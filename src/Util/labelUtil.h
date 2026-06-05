#pragma once
#include <Geode/Geode.hpp>

namespace LabelUtil {
  using namespace geode::prelude;

  inline CCLabelBMFont * createScaledLabel(char const* text,
    char const* font, float scale, float wrapWidth = 0.0f) {
    auto label = CCLabelBMFont::create(text, font);
    if (!label) return nullptr;

    label -> setScale(scale);

    if (wrapWidth > 0.0f) {
      label -> setWidth(wrapWidth / scale);
      label -> setAlignment(CCTextAlignment::kCCTextAlignmentCenter);
    }

    label -> setLayoutOptions(
      AxisLayoutOptions::create()
        ->setAutoScale(false)
        ->setLength(label -> getContentHeight() * scale)
    );

    return label;
  }
}