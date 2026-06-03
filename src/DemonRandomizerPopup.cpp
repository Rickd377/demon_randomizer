#include <Geode/Geode.hpp>
#include "Util/gddlUtil.h"
#include "Util/labelUtil.h"
using namespace geode::prelude;

class DemonRandomizerPopup : public FLAlertLayer {
private:
  CCLabelBMFont* m_fetchCounter = nullptr;
  CCNode* m_mainContainer = nullptr;
  std::vector<GDDLTag> m_allTags;
  std::vector<CCMenuItemSpriteExtra*> m_filterButtons;
  int m_selectedFilter = 0;

  CCSize m_popupSize;
  CCSize m_popupInnerSize;
  float m_popupPadding;
  float m_popupGap;

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

  void registerWithTouchDispatcher() override {
      auto* dispatcher = CCDirector::sharedDirector()->getTouchDispatcher();
      dispatcher->addTargetedDelegate(this, -500, true);
  }

  bool init(int opacity) {
    if (!FLAlertLayer::init(opacity)) return false;

    auto winSize = CCDirector::sharedDirector()->getWinSize();
    m_popupPadding = 15.0f;
    m_popupGap = 10.0f;
    m_popupSize = CCSizeMake(320.0f, 180.0f);
    m_popupInnerSize = m_popupSize - CCSizeMake(m_popupPadding * 2, m_popupPadding * 2);

    auto popup = CCScale9Sprite::create("GJ_square01.png", CCRectMake(0, 0, 80, 80));
    popup->setContentSize(m_popupSize);
    popup->setPosition(winSize / 2);
    popup->setID("demon_randomizer-popup"_spr);
    m_mainLayer->addChild(popup, -1);

    m_mainContainer = CCNode::create();
    m_mainContainer->setContentSize(m_popupInnerSize);
    m_mainContainer->setAnchorPoint({0.5f, 0.5f});
    m_mainContainer->setPosition(m_popupSize / 2);
    m_mainContainer->setLayout(
      ColumnLayout::create()
        ->setGap(m_popupGap)
        ->setAxisAlignment(AxisAlignment::Between)
        ->setCrossAxisAlignment(AxisAlignment::Center)
    );
    popup->addChild(m_mainContainer);

    auto title = LabelUtil::createScaledLabel("Demon Randomizer", "goldFont.fnt", 0.75f);

    auto loader = CCSprite::create("loadingCircle.png");
    if (loader) {
      loader->setScale(0.6f);
      loader->setLayoutOptions(
        AxisLayoutOptions::create()
          ->setAutoScale(false)
          ->setLength(loader->getContentHeight() * loader->getScale())
      );
      loader->runAction(CCRepeatForever::create(CCRotateBy::create(1.0f, 360.0f)));
    }

    auto fetchLabel = LabelUtil::createScaledLabel(
      "Downloading your demon data...", "bigFont.fnt", 0.5f, m_popupInnerSize.width
    );
    m_fetchCounter = LabelUtil::createScaledLabel("0 / 0 levels processed", "bigFont.fnt", 0.35f);
    auto fetchNote = LabelUtil::createScaledLabel(
      "This process might take a while", "goldFont.fnt", 0.4f, m_popupInnerSize.width
    );

    m_mainContainer->addChild(fetchNote);
    m_mainContainer->addChild(m_fetchCounter);
    m_mainContainer->addChild(fetchLabel);
    if (loader) m_mainContainer->addChild(loader);
    m_mainContainer->addChild(title);
    m_mainContainer->updateLayout();

    auto closeSprite = CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
    closeSprite->setScale(0.8f);
    auto closeBtn = CCMenuItemSpriteExtra::create(
      closeSprite, this, menu_selector(DemonRandomizerPopup::backActions)
    );
    closeBtn->setPosition({8.0f, m_popupSize.height - 8.0f});
    auto closeMenu = CCMenu::createWithItem(closeBtn);
    closeMenu->setPosition(CCPointZero);
    popup->addChild(closeMenu, 1);

    this->startGDDLFetch();
    return true;
  }

  void startGDDLFetch() {
    Ref<DemonRandomizerPopup> safeSelf = this;

    fetchAllCompletedDemonTags(
      [safeSelf](int current, int total) {
        if (!safeSelf || !safeSelf->m_fetchCounter) return;
        safeSelf->m_fetchCounter->setString(
          fmt::format("{}/{} levels processed", current, total).c_str()
        );
      },
      [safeSelf]() {
        if (!safeSelf) return;
        if (safeSelf->m_fetchCounter)
          safeSelf->m_fetchCounter->setString("Loading GDDL tags...");

        fetchAllAvailableTags([safeSelf](std::vector<GDDLTag> tags) {
          if (!safeSelf) return;
          safeSelf->m_allTags = tags;
          safeSelf->m_mainContainer->removeAllChildrenWithCleanup(true);
          safeSelf->buildRandomizerUI();
        });
      }
    );
  }

  void buildRandomizerUI() {
    if (!m_mainContainer) return;

    m_mainContainer->removeAllChildren();
    m_mainContainer->setLayout(
      ColumnLayout::create()
        ->setAxisAlignment(AxisAlignment::Between)
        ->setCrossAxisAlignment(AxisAlignment::Center)
    );

    auto title = LabelUtil::createScaledLabel("Demon Randomizer", "goldFont.fnt", 0.75f);

    float buttonWidth = (m_popupInnerSize.width - m_popupGap) / 2.0f;

    auto makeActionButton = [&](const char* texture, const char* label) -> CCNode* {
      auto bg = CCScale9Sprite::create(texture);
      bg->setContentSize({buttonWidth, 30});

      auto lbl = CCLabelBMFont::create(label, "bigFont.fnt");
      lbl->setPosition(bg->getContentSize() / 2);
      lbl->setAnchorPoint({0.5f, 0.5f});
      lbl->setScale(0.5f);
      bg->addChild(lbl);

      auto wrapper = CCNode::create();
      wrapper->setContentSize({buttonWidth, 35});
      wrapper->setAnchorPoint({0.5f, 0.5f});
      bg->setPosition(wrapper->getContentSize() / 2);
      wrapper->addChild(bg);
      return wrapper;
    };

    auto buttonOneWrapper = makeActionButton("GJ_button_02.png", "Generate");
    auto buttonTwoWrapper = makeActionButton("GJ_button_03.png", "Challenge");

    auto buttonOne = CCMenuItemSpriteExtra::create(
      buttonOneWrapper, this,
      menu_selector(DemonRandomizerPopup::onButtonOneClick)
    );
    auto buttonTwo = CCMenuItemSpriteExtra::create(
      buttonTwoWrapper, this,
      menu_selector(DemonRandomizerPopup::onButtonTwoClick)
    );

    auto buttonWrapper = CCMenu::create();
    buttonWrapper->setContentSize({m_popupInnerSize.width, 35});
    buttonWrapper->setLayout(
      RowLayout::create()
        ->setGap(m_popupGap)
        ->setAxisAlignment(AxisAlignment::Center)
        ->setCrossAxisAlignment(AxisAlignment::Center)
    );
    buttonWrapper->addChild(buttonOne);
    buttonWrapper->addChild(buttonTwo);
    buttonWrapper->updateLayout();

    m_filterButtons.clear();

    auto filterBg = CCScale9Sprite::create("square02b_001.png", CCRectMake(0, 0, 80, 80));
    filterBg->setContentSize({m_popupInnerSize.width, 70});
    filterBg->setColor({116, 56, 29});

    auto filterMenu = CCMenu::create();
    filterMenu->setContentSize({m_popupInnerSize.width, 50});
    filterMenu->ignoreAnchorPointForPosition(false);
    filterMenu->setAnchorPoint({0.5f, 0.5f});
    filterMenu->setLayout(
      RowLayout::create()
        ->setAxisAlignment(AxisAlignment::Between)
        ->setCrossAxisAlignment(AxisAlignment::Center)
        ->setPadding({m_popupGap, 0, m_popupGap, 0})
    );

    auto makeFilter = [&](const char* frame, int index) {
      auto sprite = CCSprite::createWithSpriteFrameName(frame);
      sprite->setScale(0.8f);

      auto btn = CCMenuItemSpriteExtra::create(
        sprite, this,
        menu_selector(DemonRandomizerPopup::onFilterClick)
      );
      btn->setTag(index);
      btn->setOpacity(150);

      m_filterButtons.push_back(btn);
      return btn;
    };

    filterMenu->addChild(makeFilter("difficulty_06_btn_001.png",  0));
    filterMenu->addChild(makeFilter("difficulty_07_btn2_001.png", 1));
    filterMenu->addChild(makeFilter("difficulty_08_btn2_001.png", 2));
    filterMenu->addChild(makeFilter("difficulty_06_btn2_001.png", 3));
    filterMenu->addChild(makeFilter("difficulty_09_btn2_001.png", 4));
    filterMenu->addChild(makeFilter("difficulty_10_btn2_001.png", 5));
    filterMenu->updateLayout();

    auto filterWrapper = CCNode::create();
    filterWrapper->setContentSize({m_popupInnerSize.width, 50});
    filterWrapper->setAnchorPoint({0.5f, 0.5f});
    filterBg->setPosition(filterWrapper->getContentSize() / 2);
    filterMenu->setPosition(filterWrapper->getContentSize() / 2);
    filterWrapper->addChild(filterBg);
    filterWrapper->addChild(filterMenu);

    m_mainContainer->addChild(buttonWrapper);
    m_mainContainer->addChild(filterWrapper);
    m_mainContainer->addChild(title);

    selectFilter(0);
    m_mainContainer->updateLayout();
  }

  void onButtonOneClick(CCObject*) {}
  void onButtonTwoClick(CCObject*) {}

  void onFilterClick(CCObject* sender) {
    auto btn = static_cast<CCMenuItemSpriteExtra*>(sender);
    selectFilter(btn->getTag());
  }

  void selectFilter(int index) {
    m_selectedFilter = index;
    for (int i = 0; i < (int)m_filterButtons.size(); i++) {
      if (!m_filterButtons[i]) continue;
      m_filterButtons[i]->setOpacity(i == index ? 255 : 150);
    }
  }

  void onEnter() override {
    FLAlertLayer::onEnter();
    cocos::handleTouchPriority(this);
  }

  void keyBackClicked() override {
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