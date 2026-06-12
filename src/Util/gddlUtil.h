#pragma once
#include <Geode/Geode.hpp>
#include <functional>
#include <vector>

void fetchAllCompletedDemonTags(
  std::function <void(int current, int total)> onProgress,
  std::function <void()> onComplete
);

void generateRandomDemon(
  int filterIndex,
  bool challengeMode,
  std::function <void(int levelID)> onResult,
  std::function <void(std::string reason)> onError
);