#include <Geode/Geode.hpp>
#include "Util/gddlUtil.h"
#include <Geode/utils/web.hpp>
#include <Geode/binding/GameLevelManager.hpp>
#include <Geode/binding/LevelInfoLayer.hpp>
#include <Geode/utils/string.hpp>
#include <thread>
#include <chrono>
#include <mutex>
#include <fstream>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>

using namespace geode::prelude;

static std::string getApiKey() {
  return Mod::get()->getSettingValue<std::string>("api-key");
}

static auto getCacheFilePath() {
  return Mod::get()->getSaveDir() / "tags_cache.txt";
}

static std::unordered_map<int, std::vector<std::string>> s_tagCache;
static bool s_cacheLoaded = false;

static void ensureCacheLoaded() {
  if (s_cacheLoaded) return;
  s_cacheLoaded = true;

  auto path = getCacheFilePath();
  std::ifstream file(path);
  if (!file.is_open()) return;

  std::string line;
  while (std::getline(file, line)) {
    auto colonPos = line.find(':');
    if (colonPos == std::string::npos) continue;

    auto idResult = geode::utils::numFromString<int>(line.substr(0, colonPos));
    if (!idResult) return;
    int id = idResult.unwrap();

    std::string tagStr = line.substr(colonPos + 1);

    std::vector<std::string> tags;
    std::stringstream ss(tagStr);
    std::string tag;
    while (std::getline(ss, tag, ','))
      if (!tag.empty()) tags.push_back(tag);

    s_tagCache[id] = std::move(tags);
  }
}

static void flushCacheToDisk() {
  std::ofstream file(getCacheFilePath());
  if (!file.is_open()) return;

  for (auto const& [id, tags] : s_tagCache) {
    file << id << ":";
    for (size_t i = 0; i < tags.size(); i++) {
      if (i > 0) file << ",";
      file << tags[i];
    }
    file << "\n";
  }
}

bool isCached(int levelID) {
  ensureCacheLoaded();
  return s_tagCache.count(levelID) > 0;
}

void writeCacheEntry(int levelID, std::vector<std::string> tags) {
  ensureCacheLoaded();
  s_tagCache[levelID] = std::move(tags);
  flushCacheToDisk();
}

std::vector<std::string> readCacheEntry(int levelID) {
  ensureCacheLoaded();
  auto it = s_tagCache.find(levelID);
  if (it == s_tagCache.end()) return {};
  return it->second;
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
  switch (filterIndex) {
    case 1:
      return "1";
    case 2:
      return "2";
    case 3:
      return "3";
    case 4:
      return "4";
    case 5:
      return "5";
    default:
      return nullptr;
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

  int randomPage = std::rand() % 50;
  std::string url = "https://gdladder.com/api/level/search?limit=25&excludeUnrated=true&page=" + std::to_string(randomPage);
  char const* diff = demonDifficultyParam(filterIndex);
  if (diff) url += "&difficulty=" + std::string(diff);

  auto req = web::WebRequest();
  req.header("Authorization", "Bearer " + getApiKey());

  async::spawn(
    req.get(url),
    [completedIDs, tagFreq, challengeMode, onResult, onError](web::WebResponse resp) {
      if (!resp.ok()) {
        Loader::get() -> queueInMainThread([onError, code = resp.code()]() {
          onError(fmt::format("Search failed: HTTP {}", code));
        });
        return;
      }

      auto json = resp.json();
      if (!json.isOk()) {
        Loader::get() -> queueInMainThread([onError]() {
          onError("Failed to parse search response");
        });
        return;
      }

      auto root = json.unwrap();
      if (!root.contains("levels") || !root["levels"].isArray()) {
        Loader::get() -> queueInMainThread([onError]() {
          onError("No levels found for this filter.");
        });
        return;
      }

      std::vector <int> candidates;
      for (auto const& lvl: root["levels"].asArray().unwrap()) {
        int id = lvl["ID"].asInt().unwrapOrDefault();
        if (id == 0 || completedIDs.count(id)) continue;
        candidates.push_back(id);
      }

      if (candidates.empty()) {
        Loader::get() -> queueInMainThread([onError]() {
          onError("All levels on this page were already completed. Try again!");
        });
        return;
      }

      struct TagResult {
        int id;
        int score;
      };

      auto results = std::make_shared <std::vector <TagResult>> ();
      auto remaining = std::make_shared <int> ((int) candidates.size());
      auto mu = std::make_shared <std::mutex> ();

      for (int i = 0; i <(int) candidates.size(); i++) {
        int id = candidates[i];

        std::thread([id, i, tagFreq, challengeMode, results, remaining, mu, onResult, onError]() {
          std::this_thread::sleep_for(std::chrono::milliseconds(i * 10));

          std::string tagUrl = "https://gdladder.com/api/level/" + std::to_string(id) + "/tags";
          auto tagReq = web::WebRequest();
          tagReq.header("Authorization", "Bearer " + getApiKey());

          std::mutex localMtx;
          std::condition_variable cv;
          bool done = false;
          int score = 0;

          async::spawn(
            tagReq.get(tagUrl),
            [ & localMtx, & cv, & done, & score, & tagFreq](web::WebResponse tagResp) {
              if (tagResp.ok()) {
                auto tagJson = tagResp.json();
                if (tagJson.isOk()) {
                  auto tagVal = tagJson.unwrap();
                  if (tagVal.isArray()) {
                    for (auto const& tagEl: tagVal.asArray().unwrap()) {
                      std::string name;
                      if (tagEl.contains("Tag") && tagEl["Tag"].contains("Name"))
                        name = tagEl["Tag"]["Name"].asString().unwrapOrDefault();
                      auto it = tagFreq.find(name);
                      if (it != tagFreq.end())
                        score += it -> second;
                    }
                  }
                }
              }
              std::unique_lock < std::mutex > lock(localMtx);
              done = true;
              cv.notify_one();
            }
          );

          std::unique_lock < std::mutex > lock(localMtx);
          cv.wait(lock, [ & ] {
            return done;
          });

          {
            std::lock_guard < std::mutex > guard( * mu);
            results -> push_back({
              id,
              score
            });
            (*remaining) --;

            if (*remaining == 0) {
              std::sort(results -> begin(), results -> end(), [](TagResult const& a,
                TagResult const& b) {
                return a.score > b.score;
              });

              int poolSize = std::max(1, (int)(results -> size() / 4));
              int pick;
              if (challengeMode) {
                int startIdx = (int) results -> size() - poolSize;
                pick = (* results)[startIdx + (std::rand() % poolSize)].id;
              } else {
                pick = (* results)[std::rand() % poolSize].id;
              }

              Loader::get() -> queueInMainThread([onResult, pick]() {
                onResult(pick);
              });
            }
          }
        }).detach();
      }
    }
  );
}