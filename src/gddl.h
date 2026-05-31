#pragma once
#include <Geode/Geode.hpp>
#include <functional>
#include <vector>
using namespace geode::prelude;

void fetchAllCompletedDemonTags(
  std::function<void(int current, int total)> onProgress,
  std::function<void()> onComplete
);