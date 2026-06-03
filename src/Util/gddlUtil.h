#pragma once
#include <Geode/Geode.hpp>
#include <functional>
#include <vector>
#include <string>

struct GDDLTag {
  int id;
  std::string name;
};

void fetchAllCompletedDemonTags(
  std::function<void(int current, int total)> onProgress,
  std::function<void()> onComplete
);

void fetchAllAvailableTags(
  std::function<void(std::vector<GDDLTag>)> onComplete
);