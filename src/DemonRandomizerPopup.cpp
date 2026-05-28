#include <Geode/Geode.hpp>

using namespace geode::prelude;

namespace {
    constexpr float PopupWidth = 420.0f;
    constexpr float PopupHeight = 260.0f;
    constexpr float PopupOpacity = 75.0f;
    constexpr float TitleOffsetY = 75.0f;
    constexpr float CloseOffset = 20.0f;

    CCSprite* createCloseSprite() {
      auto closeSprite = CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
      if (!closeSprite) {
        closeSprite = CCSprite::createWithSpriteFrameName("GJ_plainBtn_001.png");
      }
      if (!closeSprite) {
        closeSprite = CCSprite::create();
      }

      closeSprite->setScale(0.8f);
      return closeSprite;
    }

    class DemonRandomizerPopup : public FLAlertLayer {
      public:
        static DemonRandomizerPopup* create() {
          auto ret = new DemonRandomizerPopup();
          if (ret->init(PopupOpacity)) {
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
          auto popupSize = CCSizeMake(PopupWidth, PopupHeight);

          auto bg = CCScale9Sprite::create(
            "GJ_square01.png",
            CCRectMake(0.0f, 0.0f, 80.0f, 80.0f)
          );
          bg->setContentSize(popupSize);
          bg->setPosition(ccp(winSize.width / 2.0f, winSize.height / 2.0f));
          bg->setID("demon_randomizer-popup-bg"_spr);
          m_mainLayer->addChild(bg, -1);

          auto title = CCLabelBMFont::create("Demon Randomizer", "goldFont.fnt");
          title->setPosition(ccp(winSize.width / 2.0f, winSize.height / 2.0f + TitleOffsetY));
          m_mainLayer->addChild(title);

          auto closeBtn = CCMenuItemSpriteExtra::create(
            createCloseSprite(),
            this,
            menu_selector(DemonRandomizerPopup::onClose)
          );
          closeBtn->setPosition(ccp(
            winSize.width / 2.0f - popupSize.width / 2.0f + CloseOffset,
            winSize.height / 2.0f + popupSize.height / 2.0f - CloseOffset
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
}

void showDemonRandomizerPopup() {
  auto popup = DemonRandomizerPopup::create();
  if (popup) {
    popup->show();
  }
}
