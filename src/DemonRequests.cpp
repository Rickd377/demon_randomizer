#include <Geode/Geode.hpp>
#include "Util/gddlUtil.h"
#include <Geode/utils/web.hpp>
#include <Geode/binding/GameLevelManager.hpp>
#include <Geode/binding/LevelInfoLayer.hpp>
#include <Geode/utils/string.hpp>
#include <thread>
#include <chrono>
#include <mutex>
#include <random>
#include <fstream>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>

using namespace geode::prelude;

static std::mt19937 s_rng(
  std::random_device{}() ^
  static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count())
);

static std::string getApiKey() {
  return Mod::get()->getSettingValue<std::string>("api-key");
}

static auto getCacheFilePath() {
  return Mod::get()->getSaveDir() / "saved.json";
}

static std::unordered_map<int, std::vector<std::string>> s_tagCache;
static bool s_cacheLoaded = false;

static void ensureCacheLoaded() {
  if (s_cacheLoaded) return;
  s_cacheLoaded = true;

  std::ifstream file(getCacheFilePath());
  if (!file.is_open()) return;

  auto result = matjson::Value::parse(file);
  if (!result.isOk()) return;

  auto root = result.unwrap();
  if (!root.isObject()) return;

  for (auto& [key, val] : root) {
    auto idResult = geode::utils::numFromString<int>(key);
    if (!idResult) continue;
    int id = idResult.unwrap();

    std::vector<std::string> tags;
    if (val.isArray()) {
      for (auto const& tagVal : val) {
        auto str = tagVal.asString();
        if (str.isOk()) tags.push_back(str.unwrap());
      }
    }
    s_tagCache[id] = std::move(tags);
  }
}

static void flushCacheToDisk() {
  auto obj = matjson::Value::object();
  for (auto const& [id, tags] : s_tagCache) {
    auto arr = matjson::Value::array();
    for (auto const& tag : tags)
      arr.push(matjson::Value(tag));
    obj.set(std::to_string(id), arr);
  }

  std::ofstream file(getCacheFilePath());
  if (!file.is_open()) return;
  file << obj.dump();
}

bool isCached(int levelID) {
  return Mod::get()->hasSavedValue("tags_" + std::to_string(levelID));
}

void writeCacheEntry(int levelID, std::vector<std::string> tags) {
  if (tags.empty()) return;
  std::string joined;
  for (size_t i = 0; i < tags.size(); i++) {
    if (i > 0) joined += ",";
    joined += tags[i];
  }
  Mod::get()->setSavedValue("tags_" + std::to_string(levelID), joined);
}

std::vector<std::string> readCacheEntry(int levelID) {
  std::string raw = Mod::get()->getSavedValue<std::string>("tags_" + std::to_string(levelID));
  std::vector<std::string> result;
  if (raw.empty()) return result;
  std::stringstream ss(raw);
  std::string token;
  while (std::getline(ss, token, ','))
    result.push_back(token);
  return result;
}

void fetchAllCompletedDemonTags(
  std::function <void(int current, int total)> onProgress,
  std::function <void()> onComplete
) {
  if (getApiKey().empty()) {
    onComplete();
    return;
  }

  auto * glm = GameLevelManager::sharedState();
  auto * levels = glm -> getSavedLevels(false, 0);
  if (!levels) {
    onComplete();
    return;
  }

  std::vector <int> toFetch;
  for (int i = 0; i <(int) levels -> count(); i++) {
    auto* level = static_cast <GJGameLevel*> (levels -> objectAtIndex(i));
    if (!level) continue;
    if (level -> m_demon.value() == 0) continue;
    if (level -> m_normalPercent.value() < 100) continue;
    int id = level -> m_levelID.value();
    if (!isCached(id)) toFetch.push_back(id);
  }

  if (toFetch.empty()) {
    log::debug("All demon levels already cached");
    onComplete();
    return;
  }

  int total = (int) toFetch.size();
  log::debug("Need to fetch {} levels", total);

  std::thread([toFetch, total, onProgress, onComplete]() {
    int count = 0;
    int requestsThisMinute = 0;
    auto windowStart = std::chrono::steady_clock::now();

    for (int id: toFetch) {
      auto now = std::chrono::steady_clock::now();
      auto elapsed = std::chrono::duration_cast < std::chrono::seconds > (now - windowStart).count();
      if (elapsed >= 60) {
        requestsThisMinute = 0;
        windowStart = now;
      }
      if (requestsThisMinute >= 90) {
        int waitSeconds = 60 - (int) elapsed + 1;
        log::debug("Rate limit reached, sleeping {}s", waitSeconds);
        std::this_thread::sleep_for(std::chrono::seconds(waitSeconds));
        requestsThisMinute = 0;
        windowStart = std::chrono::steady_clock::now();
      }

      std::mutex mtx;
      std::condition_variable cv;
      bool done = false;
      std::vector < std::string > tags;

      std::string url = "https://gdladder.com/api/level/" + std::to_string(id) + "/tags";
      auto req = web::WebRequest();
      req.header("Authorization", "Bearer " + getApiKey());

      async::spawn(
        req.get(url),
        [ & mtx, & cv, & done, & tags, id](web::WebResponse resp) {
          if (resp.ok()) {
            auto json = resp.json();
            if (json.isOk()) {
              auto val = json.unwrap();
              if (val.isArray()) {
                for (auto const& tag: val.asArray().unwrap()) {
                  auto tagObj = tag["Tag"];
                  if (tagObj.contains("Name"))
                    tags.push_back(tagObj["Name"].asString().unwrapOrDefault());
                }
              }
            }
          } else {
            log::warn("Failed for level {}: HTTP {}", id, resp.code());
          }
          std::unique_lock < std::mutex > lock(mtx);
          done = true;
          cv.notify_one();
        }
      );

      std::unique_lock < std::mutex > lock(mtx);
      cv.wait(lock, [ & ] {
        return done;
      });
      std::this_thread::sleep_for(std::chrono::milliseconds(700));

      requestsThisMinute++;
      count++;

      int capturedId = id;
      int capturedCount = count;
      std::vector < std::string > capturedTags = tags;
      Loader::get() -> queueInMainThread([capturedId, capturedTags, capturedCount, total, onProgress, onComplete]() {
        writeCacheEntry(capturedId, capturedTags);
        onProgress(capturedCount, total);
        if (capturedCount >= total) onComplete();
      });

      log::debug("Fetched {}/{}", count, total);
    }
  }).detach();
}

static char const* demonDifficultyParam(int filterIndex) {
  if (filterIndex == 0) {
    int random = std::uniform_int_distribution<int>(1, 5)(s_rng);
    switch (random) {
      case 1: return "1";
      case 2: return "2";
      case 3: return "3";
      case 4: return "4";
      default: return "5";
    }
  }
  switch (filterIndex) {
    case 1: return "1";
    case 2: return "2";
    case 3: return "3";
    case 4: return "4";
    case 5: return "5";
    default: return "1";
  }
}

void generateRandomDemon(
  int filterIndex,
  bool challengeMode,
  std::function <void(int levelID)> onResult,
  std::function <void(std::string)> onError
) {
  if (getApiKey().empty()) {
    onError("No API key set. Add your GDDL API key in the mod settings.");
    return;
  }

  auto* glm = GameLevelManager::sharedState();
  auto* saved = glm -> getSavedLevels(false, 0);

  std::unordered_set <int> completedIDs;
  std::unordered_map <std::string, int> tagFreq;

  if (saved) {
    for (int i = 0; i <(int) saved -> count(); i++) {
      auto* lvl = static_cast < GJGameLevel * > (saved -> objectAtIndex(i));
      if (!lvl || lvl -> m_demon.value() == 0 || lvl -> m_normalPercent.value() < 100) continue;
      completedIDs.insert(lvl -> m_levelID.value());
      for (auto const& tag: readCacheEntry(lvl -> m_levelID.value()))
        tagFreq[tag]++;
    }
  }

  std::string url = "https://gdladder.com/api/level/search?limit=1&excludeUnrated=true&page=0";
  char const* diff = demonDifficultyParam(filterIndex);
  if (diff) url += "&difficulty=" + std::string(diff);

  auto countReq = web::WebRequest();
  countReq.header("Authorization", "Bearer " + getApiKey());

  async::spawn(
    countReq.get(url),
    [completedIDs, tagFreq, challengeMode, onResult, onError, diffStr = diff ? std::string(diff) : ""](web::WebResponse countResp) {
      if (!countResp.ok()) {
        Loader::get()->queueInMainThread([onError, code = countResp.code()]() {
          onError(fmt::format("Search failed: HTTP {}", code));
        });
        return;
      }

      auto countJson = countResp.json();
      if (!countJson.isOk()) {
        Loader::get()->queueInMainThread([onError]() {
          onError("Failed to parse search response");
        });
        return;
      }

      auto countRoot = countJson.unwrap();
      int total = countRoot["total"].asInt().unwrapOrDefault();
      int maxPage = std::max(0, (total - 25) / 25);
      int randomPage = std::uniform_int_distribution<int>(0, maxPage)(s_rng);

      std::string pageUrl = "https://gdladder.com/api/level/search?limit=25&excludeUnrated=true&page="
        + std::to_string(randomPage);
      if (!diffStr.empty()) pageUrl += "&difficulty=" + diffStr;

      auto req = web::WebRequest();
      req.header("Authorization", "Bearer " + getApiKey());

      async::spawn(
        req.get(pageUrl),
        [completedIDs, tagFreq, challengeMode, onResult, onError](web::WebResponse resp) {
          if (!resp.ok()) {
            Loader::get()->queueInMainThread([onError, code = resp.code()]() {
              onError(fmt::format("Search failed: HTTP {}", code));
            });
            return;
          }

          auto json = resp.json();
          if (!json.isOk()) {
            Loader::get()->queueInMainThread([onError]() {
              onError("Failed to parse search response");
            });
            return;
          }

          auto root = json.unwrap();
          if (!root.contains("levels") || !root["levels"].isArray()) {
            Loader::get()->queueInMainThread([onError]() {
              onError("No levels found for this filter.");
            });
            return;
          }

          std::vector<int> candidates;
          for (auto const& lvl : root["levels"].asArray().unwrap()) {
            int id = lvl["ID"].asInt().unwrapOrDefault();
            if (id == 0 || completedIDs.count(id)) continue;
            candidates.push_back(id);
          }

          if (candidates.empty()) {
            Loader::get()->queueInMainThread([onError]() {
              onError("All levels on this page were already completed. Try again!");
            });
            return;
          }

          struct TagResult {
            int id;
            int score;
          };

          auto results = std::make_shared<std::vector<TagResult>>();
          auto remaining = std::make_shared<int>((int)candidates.size());
          auto mu = std::make_shared<std::mutex>();

          for (int i = 0; i < (int)candidates.size(); i++) {
            int id = candidates[i];

            std::string tagUrl = "https://gdladder.com/api/level/" + std::to_string(id) + "/tags";
            auto tagReq = web::WebRequest();
            tagReq.header("Authorization", "Bearer " + getApiKey());

            async::spawn(
              tagReq.get(tagUrl),
              [id, tagFreq, challengeMode, results, remaining, mu, onResult, onError](web::WebResponse tagResp) {
                  int score = 0;

                  if (tagResp.ok()) {
                    auto tagJson = tagResp.json();
                    if (tagJson.isOk()) {
                      auto tagVal = tagJson.unwrap();
                      if (tagVal.isArray()) {
                        for (auto const& tagEl : tagVal.asArray().unwrap()) {
                          std::string name;
                          if (tagEl.contains("Tag") && tagEl["Tag"].contains("Name"))
                            name = tagEl["Tag"]["Name"].asString().unwrapOrDefault();
                          auto it = tagFreq.find(name);
                          if (it != tagFreq.end())
                            score += it->second;
                        }
                      }
                    }
                  }

                  std::lock_guard<std::mutex> guard(*mu);
                  results->push_back({id, score});
                  (*remaining)--;

                  if (*remaining == 0) {
                    std::sort(results->begin(), results->end(), [](TagResult const& a, TagResult const& b) {
                      return a.score > b.score;
                    });

                    int poolSize = std::max(1, (int)(results->size() / 4));
                    std::uniform_int_distribution<int> dist(0, poolSize - 1);
                    int pick;
                    if (challengeMode) {
                      int startIdx = (int)results->size() - poolSize;
                      pick = (*results)[startIdx + dist(s_rng)].id;
                    } else {
                      pick = (*results)[dist(s_rng)].id;
                    }

                    Loader::get()->queueInMainThread([onResult, pick]() {
                      onResult(pick);
                    });
                  }
              }
            );
          }
        }
      );
    }
  );
}