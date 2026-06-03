#include <Geode/Geode.hpp>
#include "Util/gddlUtil.h"
#include <Geode/utils/web.hpp>
#include <Geode/binding/GameLevelManager.hpp>
#include <thread>
#include <chrono>
#include <mutex>
using namespace geode::prelude;

static const std::string GDDL_API_KEY = "22619628f6630ae8613e936a8b91325d742bbb198d9cd4a11820620fa88babba";

static std::mutex s_cacheMutex;

bool isCached(int levelID) {
  return Mod::get()->hasSavedValue("tags_" + std::to_string(levelID));
}

void writeCacheEntry(int levelID, std::vector<std::string> tags) {
  std::string joined;
  for (size_t i = 0; i < tags.size(); i++) {
    if (i > 0) joined += ",";
    joined += tags[i];
  }
  Mod::get()->setSavedValue("tags_" + std::to_string(levelID), joined);
}

void fetchAllAvailableTags(std::function<void(std::vector<GDDLTag>)> onComplete) {
  std::string url = "https://gdladder.com";
  auto req = web::WebRequest();
  req.header("Authorization", "Bearer " + GDDL_API_KEY);

  log::debug("Fetching all global GDDL tags...");

  async::spawn(
    req.get(url),
    [onComplete](web::WebResponse resp) {
      std::vector<GDDLTag> tagsList;

      if (resp.ok()) {
        auto json = resp.json();
        if (json.isOk()) {
          auto val = json.unwrap();

          if (val.isArray()) {
            for (auto const& tagElement : val.asArray().unwrap()) {
              GDDLTag tag;

              if (tagElement.contains("id")) {
                tag.id = tagElement["id"].asInt().unwrapOrDefault();
              }
              if (tagElement.contains("name")) {
                tag.name = tagElement["name"].asString().unwrapOrDefault();
              }

              tagsList.push_back(tag);
            }
          }
        }
      } else {
        log::warn("Failed to fetch GDDL tags: HTTP {}", resp.code());
      }

      Loader::get()->queueInMainThread([onComplete, tagsList]() {
        onComplete(tagsList);
      });
    }
  );
}

void fetchAllCompletedDemonTags(
  std::function<void(int current, int total)> onProgress,
  std::function<void()> onComplete
) {
  auto* glm = GameLevelManager::sharedState();
  auto* levels = glm->getSavedLevels(false, 0);
  if (!levels) { onComplete(); return; }

  std::vector<int> toFetch;
  for (int i = 0; i < levels->count(); i++) {
    auto* level = static_cast<GJGameLevel*>(levels->objectAtIndex(i));
    if (!level) continue;
    if (level->m_demon.value() == 0) continue;
    if (level->m_normalPercent.value() < 100) continue;
    int id = level->m_levelID.value();
    if (!isCached(id)) toFetch.push_back(id);
  }

  if (toFetch.empty()) {
    log::debug("All demon levels already cached");
    onComplete();
    return;
  }

  int total = (int)toFetch.size();
  log::debug("Need to fetch {} levels", total);

  std::thread([toFetch, total, onProgress, onComplete]() {
    int count = 0;
    int requestsThisMinute = 0;
    auto windowStart = std::chrono::steady_clock::now();

    for (int id : toFetch) {
      auto now = std::chrono::steady_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - windowStart).count();
      if (elapsed >= 60) {
        requestsThisMinute = 0;
        windowStart = now;
      }

      if (requestsThisMinute >= 90) {
        int waitSeconds = 60 - (int)elapsed + 1;
        log::debug("Rate limit reached, sleeping {}s", waitSeconds);
        std::this_thread::sleep_for(std::chrono::seconds(waitSeconds));
        requestsThisMinute = 0;
        windowStart = std::chrono::steady_clock::now();
      }

      std::mutex mtx;
      std::condition_variable cv;
      bool done = false;
      std::vector<std::string> tags;

      std::string url = "https://gdladder.com/api/level/" + std::to_string(id) + "/tags";
      auto req = web::WebRequest();
      req.header("Authorization", "Bearer " + GDDL_API_KEY);

      async::spawn(
        req.get(url),
        [&mtx, &cv, &done, &tags, id](web::WebResponse resp) {
          if (resp.ok()) {
            auto json = resp.json();
            if (json.isOk()) {
              auto val = json.unwrap();
              if (val.isArray()) {
                for (auto const& tag : val.asArray().unwrap()) {
                  auto tagObj = tag["Tag"];
                  if (tagObj.contains("Name")) {
                    tags.push_back(tagObj["Name"].asString().unwrapOrDefault());
                  }
                }
              }
            }
          } else {
            log::warn("Failed for level {}: HTTP {}", id, resp.code());
          }
          std::unique_lock<std::mutex> lock(mtx);
          done = true;
          cv.notify_one();
        }
      );

      std::unique_lock<std::mutex> lock(mtx);
      cv.wait(lock, [&] { return done; });

      std::this_thread::sleep_for(std::chrono::milliseconds(700));

      requestsThisMinute++;
      count++;

      int capturedId = id;
      int capturedCount = count;
      std::vector<std::string> capturedTags = tags;
      Loader::get()->queueInMainThread([capturedId, capturedTags, capturedCount, total, onProgress, onComplete]() {
        writeCacheEntry(capturedId, capturedTags);
        onProgress(capturedCount, total);
        if (capturedCount >= total) onComplete();
      });

      log::debug("Fetched {}/{}", count, total);
    }
  }).detach();
}