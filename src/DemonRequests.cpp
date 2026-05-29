#include <Geode/Geode.hpp>
#include "gddl.h"
#include <Geode/utils/web.hpp>
using namespace geode::prelude;

static const std::string GDDL_API_KEY = "22619628f6630ae8613e936a8b91325d742bbb198d9cd4a11820620fa88babba";

void fetchDemonTags(int levelID, std::function<void(std::string)> callback) {
  std::string url = "https://gdladder.com/api/v2/levels/" + std::to_string(levelID);

  auto req = web::WebRequest();
  req.header("Authorization", "Bearer " + GDDL_API_KEY);
  req.header("Content-Type", "application/json");

  async::spawn(
    req.get(url),
    [callback](web::WebResponse resp) {
      if (!resp.ok()) {
        callback("Request failed: " + std::to_string(resp.code()));
        return;
      }

      std::string tagList;
      auto json = resp.json();
      if (json.isOk()) {
        auto obj = json.unwrap();
        if (obj.contains("tags") && obj["tags"].isArray()) {
          for (auto const& tag : obj["tags"].asArray().unwrap()) {
            if (!tagList.empty()) tagList += ", ";
            if (tag.contains("name")) {
              tagList += tag["name"].asString().unwrapOrDefault();
            }
          }
        }
      }

      if (tagList.empty()) {
        tagList = "(no tags found)";
      }
      callback(tagList);
    }
  );
}

void fetchFirstCompletedDemonTags(std::function<void(int, std::string)> callback) {
  auto* glm = GameLevelManager::sharedState();
  auto* levels = glm->getSavedLevels(false, 0);
  if (!levels || levels->count() == 0) {
    callback(-1, "No completed demon levels found");
    return;
  }

  for (int i = 0; i < levels->count(); i++) {
    auto* level = static_cast<GJGameLevel*>(levels->objectAtIndex(i));
    if (!level) continue;

    bool isDemon = level->m_demon.value() != 0;
    bool isComplete = level->m_normalPercent.value() >= 100;

    if (isDemon && isComplete) {
      int id = level->m_levelID.value();
      fetchDemonTags(id, [callback, id](std::string tags) {
        callback(id, tags);
      });
      return;
    }
  }

  callback(-1, "No completed demon levels found");
}