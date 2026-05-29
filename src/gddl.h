#pragma once
#include <Geode/Geode.hpp>
#include <functional>
using namespace geode::prelude;

void fetchDemonTags(int levelID, std::function<void(std::string)> callback);
void fetchFirstCompletedDemonTags(std::function<void(int, std::string)> callback);