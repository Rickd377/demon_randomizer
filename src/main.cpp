#include <Geode/Geode.hpp>
#include <Geode/modify/LevelSearchLayer.hpp>

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
      if (!FLAlertLayer::init(opacity)) {
        return false;
      }

      auto winSize = CCDirector::sharedDirector()->getWinSize();
      auto popupSize = CCSizeMake(300.0f, 150.0f);

      auto bg = CCScale9Sprite::create(
        "GJ_square01.png",
        CCRectMake(0.0f, 0.0f, 80.0f, 80.0f)
      );
      bg->setContentSize(popupSize);
      bg->setPosition(ccp(winSize.width / 2.0f, winSize.height / 2.0f));
      bg->setID("demon_randomizer-popup-bg"_spr);
      m_mainLayer->addChild(bg, -1);

      auto addCorner = [&](float x, float y, bool flipX, bool flipY) {
        auto corner = CCSprite::createWithSpriteFrameName("dailyLevelCorner_001.png");
        corner->setFlipX(flipX);
        corner->setFlipY(flipY);
        corner->setAnchorPoint({0.0f, 0.0f});
        corner->setPosition({ x, y });
        m_mainLayer->addChild(corner, 2);
      };

      auto cornerSize = CCSprite::createWithSpriteFrameName("dailyLevelCorner_001.png")->getContentSize();
      auto center = ccp(winSize.width / 2.0f, winSize.height / 2.0f);
      auto halfW = popupSize.width / 2.0f;
      auto halfH = popupSize.height / 2.0f;
      
      auto left = center.x - halfW;
      auto right = center.x + halfW;
      auto bottom = center.y - halfH;
      auto top = center.y + halfH;

      addCorner(left, bottom, false, false);
      addCorner(right - cornerSize.width, bottom, true, false);
      addCorner(left, top - cornerSize.height, false, true);
      addCorner(right - cornerSize.width, top - cornerSize.height, true, true);

      auto closeSprite = CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
      closeSprite->setScale(0.8f);

      auto closeBtn = CCMenuItemSpriteExtra::create(
        closeSprite,
        this,
        menu_selector(DemonRandomizerPopup::onClose)
      );
      closeBtn->setPosition(ccp(
        winSize.width / 2.0f - popupSize.width / 2.0f + 9.0f,
        winSize.height / 2.0f + popupSize.height / 2.0f - 9.0f
      ));

      auto menu = CCMenu::createWithItem(closeBtn);
      menu->setPosition(CCPointZero);
      m_mainLayer->addChild(menu, 3);

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
      backActions();
    }

    void onClose(CCObject*) {
      backActions();
    }

    void backActions() {
      setKeypadEnabled(false);
      removeFromParentAndCleanup(true);
    }
};

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

      button->setID("randomizer-button");
      otherFilterMenu->addChild(button);

      auto iconSprite = CCSprite::create("btn-icon.png"_spr);
      iconSprite->setScale(0.45f);
      iconSprite->setPosition(button->getContentSize() / 2);
      button->addChild(iconSprite);

      return true;
    }

    void onButtonClick(CCObject*) {
      auto popup = DemonRandomizerPopup::create();
      if (popup) {
        popup->show();
      }
    }
};