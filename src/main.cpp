#include <Geode/Geode.hpp>
#include <Geode/modify/LevelSearchLayer.hpp>

using namespace geode::prelude;

void showDemonRandomizerPopup();

class $modify(MyLevelSearchLayer, LevelSearchLayer) {
  public:
    bool init(int type) {
      if (!LevelSearchLayer::init(type)) {
        return false;
      }

      auto otherFilterMenu = this->getChildByID("other-filter-menu");
      auto btnSprite = CCSprite::createWithSpriteFrameName("GJ_plainBtn_001.png");
      btnSprite->setScale(0.8f);
      auto button = CCMenuItemSpriteExtra::create(
        btnSprite,
        this,
        menu_selector(MyLevelSearchLayer::onButtonClick)
      );

      button->setID("demon_randomizer-button"_spr);
      otherFilterMenu->addChild(button);

      auto iconSprite = CCSprite::create("btn-icon.png"_spr);
      iconSprite->setScale(0.45f);
      iconSprite->setPosition(button->getContentSize() / 2);
      button->addChild(iconSprite);

      return true;
    }

    void onButtonClick(CCObject*) {
      showDemonRandomizerPopup();
    }
};