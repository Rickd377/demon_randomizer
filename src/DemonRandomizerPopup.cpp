#include <Geode/Geode.hpp>
#include "gddl.h"
using namespace geode::prelude;

class DemonRandomizerPopup : public FLAlertLayer {
public:
  static DemonRandomizerPopup* create() {
    auto ret = new DemonRandomizerPopup();
    if (ret->init(75)) {
      ret->autorelease();
      return ret;
    }
    delete ret;
    return nullptr;
  }

  bool init(int opacity) {
    if (!FLAlertLayer::init(opacity)) return false;

    auto winSize = CCDirector::sharedDirector()->getWinSize();
    auto popupSize = CCSizeMake(380.0f, 180.0f);

    auto bg = CCScale9Sprite::create("GJ_square01.png", CCRectMake(0, 0, 80, 80));
    bg->setContentSize(popupSize);
    bg->setPosition(ccp(winSize.width / 2, winSize.height / 2));
    bg->setID("demon_randomizer-popup-bg"_spr);
    m_mainLayer->addChild(bg, -1);

    auto textLabel = CCLabelBMFont::create("Loading... 0/0", "bigFont.fnt");
    textLabel->setScale(0.35f);
    textLabel->setAnchorPoint(ccp(0.5f, 0.5f));
    textLabel->setPosition(ccp(winSize.width / 2, winSize.height / 2 - 5));
    textLabel->setID("demon_randomizer-tags-label"_spr);
    m_mainLayer->addChild(textLabel);

    Ref<CCNode> layerRef = m_mainLayer;

    fetchAllCompletedDemonTags(
      [layerRef](int current, int total) {
        if (!layerRef) return;
        if (auto label = typeinfo_cast<CCLabelBMFont*>(
          layerRef->getChildByID("demon_randomizer-tags-label"_spr)
        )) {
          label->setString(
            ("Fetching... " + std::to_string(current) + "/" + std::to_string(total)).c_str()
          );
        }
      },
      [layerRef]() {
        if (!layerRef) return;
        if (auto label = typeinfo_cast<CCLabelBMFont*>(
          layerRef->getChildByID("demon_randomizer-tags-label"_spr)
        )) {
          label->setString("Done!");
        }
      }
    );

    auto closeSprite = CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
    closeSprite->setScale(0.8f);
    auto closeBtn = CCMenuItemSpriteExtra::create(
      closeSprite, this, menu_selector(DemonRandomizerPopup::backActions)
    );
    closeBtn->setPosition(ccp(
      winSize.width / 2 - popupSize.width / 2 + 9,
      winSize.height / 2 + popupSize.height / 2 - 9
    ));

    auto menu = CCMenu::createWithItem(closeBtn);
    menu->setPosition(CCPointZero);
    m_mainLayer->addChild(menu, 1);

    return true;
  }

  void onEnter() override {
    FLAlertLayer::onEnter();
    cocos::handleTouchPriority(this);
  }

  void show() override {
    FLAlertLayer::show();
    cocos::handleTouchPriority(this);
  }

  void keyBackClicked() override {
    FLAlertLayer::keyBackClicked();
    backActions(nullptr);
  }

  void backActions(CCObject*) {
    setKeypadEnabled(false);
    removeFromParentAndCleanup(true);
  }
};

void showDemonRandomizerPopup() {
  auto popup = DemonRandomizerPopup::create();
  if (popup) popup->show();
}