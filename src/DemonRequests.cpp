#include <Geode/Geode.hpp>
using namespace geode::prelude;

std::string getCompletedDemonLevelIDs() {
  auto* glm = GameLevelManager::sharedState();
  auto* levels = glm->getSavedLevels(false, 0);

  std::string result;
  if (!levels) return result;

  for (int i = 0; i < levels->count(); i++) {
    auto* level = static_cast<GJGameLevel*>(levels->objectAtIndex(i));
    if (!level) continue;

    bool isDemon    = level->m_demon.value() != 0;
    bool isComplete = level->m_normalPercent.value() >= 100;

    if (isDemon && isComplete) {
        if (!result.empty()) result += ", ";
        result += std::to_string(level->m_levelID.value());
    }
  }

  return result;
}