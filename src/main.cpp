#include <Geode/Geode.hpp>
#include <Geode/modify/SecretRewardsLayer.hpp>
#include <Geode/ui/ScrollLayer.hpp>

using namespace geode::prelude;

static constexpr float BULK_BUTTON_Y = 30.f;
static constexpr float INFO_LABEL_Y = 22.f;
static constexpr const char* STAT_DEMON_KEYS = "21";
static constexpr int REWARD_LAYER_TAG = 9999;

class $modify(BulkChestLayer, SecretRewardsLayer) {

    struct Fields {
        std::vector<int> selectedChests;
        std::map<int, CCMenuItemSpriteExtra*> chestButtons;
        CCMenu* bulkOpenMenu = nullptr;

        int totalOrbs = 0;
        int totalDiamonds = 0;
        int totalDemonKeys = 0;
        std::map<SpecialRewardItem, int> totalShards;
        std::vector<std::pair<UnlockType, int>>unlockedItems;

        int chestCount = 0;
        bool isOpening = false;
    };

    void updateBulkButton() {
        if (m_fields->bulkOpenMenu) {
            m_fields->bulkOpenMenu->setVisible(!m_fields->selectedChests.empty());
        }
    }

    void highlightChest(CCMenuItemSpriteExtra* button, bool selected) {
        if (!button) return;

        auto sprite = typeinfo_cast<CCSprite*>(button->getNormalImage());
        if (!sprite) return;

        if (selected) {
            sprite->setColor({150, 255, 150});

            auto check = CCSprite::createWithSpriteFrameName("GJ_completesIcon_001.png");
            auto size = sprite->getContentSize();
            check->setPosition({size.width / 2.f, size.height / 2.f});
            check->setScale(1.3f);
            check->setID("selection-check"_spr);
            sprite->addChild(check, 100);
        } else {
            sprite->setColor({255, 255, 255});
            if (auto check = sprite->getChildByID("selection-check"_spr)) {
                check->removeFromParent();
            }
        }
    }

    bool init(bool fromShop) {
        if (!SecretRewardsLayer::init(fromShop)) return false;

        m_fields->selectedChests.clear();
        m_fields->chestButtons.clear();

        auto bulkSprite = ButtonSprite::create("Open All", "goldFont.fnt", "GJ_button_01.png", 0.8f);
        bulkSprite->setScale(0.7f);
        auto bulkButton = CCMenuItemSpriteExtra::create(bulkSprite, this, menu_selector(BulkChestLayer::onBulkOpen));
        bulkButton->setID("bulk-open-btn"_spr);

        m_fields->bulkOpenMenu = CCMenu::create();
        m_fields->bulkOpenMenu->addChild(bulkButton);
        m_fields->bulkOpenMenu->setPosition({CCScene::get()->getContentWidth() / 2.f, BULK_BUTTON_Y});
        m_fields->bulkOpenMenu->setVisible(false);
        this->addChild(m_fields->bulkOpenMenu, 100);

        auto infoLabel = CCLabelBMFont::create("Shift+Click to select chests", "chatFont.fnt");
        infoLabel->setScale(0.4f);
        infoLabel->setPosition({CCScene::get()->getContentWidth() / 2.f, INFO_LABEL_Y});
        infoLabel->setColor({180, 180, 180});
        infoLabel->setID("bulk-info-label"_spr);
        this->addChild(infoLabel, 100);

        return true;
    }

    void collectRewardStats(GJRewardItem* reward) {
        if (!reward || !reward->m_rewardObjects) return;

        for (auto* rewardObject : CCArrayExt<GJRewardObject*>(reward->m_rewardObjects)) {
            if (!rewardObject) continue;

            switch (rewardObject->m_specialRewardItem) {
                case SpecialRewardItem::Orbs:
                    m_fields->totalOrbs += rewardObject->m_total;
                    break;
                case SpecialRewardItem::Diamonds:
                    m_fields->totalDiamonds += rewardObject->m_total;
                    break;
                case SpecialRewardItem::BonusKey:
                    m_fields->totalDemonKeys += rewardObject->m_total;
                    break;
                default:
                    m_fields->totalShards[rewardObject->m_specialRewardItem] += rewardObject->m_total;
                    break;
            }

            if (rewardObject->m_specialRewardItem == SpecialRewardItem::CustomItem && rewardObject->m_itemID > 0) {
                m_fields->unlockedItems.push_back({rewardObject->m_unlockType, rewardObject->m_itemID});
            }
        }
    }

    void onSelectItem(CCObject* sender) {
        auto button = typeinfo_cast<CCMenuItemSpriteExtra*>(sender);
        if (!button) return;

        int chestId = button->getTag();
        m_fields->chestButtons[chestId] = button;

        auto keyboard = CCDirector::get()->getKeyboardDispatcher();
        bool shiftHeld = keyboard && keyboard->getShiftKeyPressed();

        if (shiftHeld) {
            trySelectChest(button, chestId);
            updateBulkButton();
            return;
        }

        if (!m_fields->selectedChests.empty()) {
            auto iterator = std::find(m_fields->selectedChests.begin(), m_fields->selectedChests.end(), chestId);
            if (iterator == m_fields->selectedChests.end()) {
                m_fields->selectedChests.push_back(chestId);
            }
            openAllSelected();
        } else {
            SecretRewardsLayer::onSelectItem(sender);
        }
    }

    void hideButtonsRecursively(CCNode* node) {
        if (!node) return;

        if (auto button = typeinfo_cast<CCMenuItemSpriteExtra*>(node)) {
            button->setVisible(false);
        }

        if (auto children = node->getChildren()) {
            for (auto* child : CCArrayExt<CCNode*>(children)) {
                hideButtonsRecursively(child);
            }
        }
    }

    void hideRewardClaimButton() {
        auto scene = CCDirector::sharedDirector()->getRunningScene();
        if (!scene) return;

        if (auto rewardLayer = scene->getChildByTag(REWARD_LAYER_TAG)) {
            for (auto* child : CCArrayExt<CCNode*>(rewardLayer->getChildren())) {
                if (auto menu = typeinfo_cast<CCMenu*>(child)) {
                    for (auto* menuItem : CCArrayExt<CCNode*>(menu->getChildren())) {
                        menuItem->setVisible(false);
                    }
                }
            }
        }
    }

    bool trySelectChest(CCMenuItemSpriteExtra* button, int chestId) {
        if (!button) return false;

        auto& selected = m_fields->selectedChests;
        auto iterator = std::find(selected.begin(), selected.end(), chestId);

        if (iterator != selected.end()) {
            selected.erase(iterator);
            highlightChest(button, false);
            return true;
        }

        auto gameStats = GameStatsManager::sharedState();
        int currentKeys = gameStats->getStat(STAT_DEMON_KEYS);

        int totalCost = gameStats->keyCostForSecretChest(chestId);
        for (int id : selected) {
            totalCost += gameStats->keyCostForSecretChest(id);
        }

        if (gameStats->isSecretChestUnlocked(chestId)) {
            FLAlertLayer::create("Already Open", "This chest has already been opened!", "OK")->show();
            return false;
        }

        if (currentKeys < totalCost) {
            FLAlertLayer::create("Not Enough Keys", "You don't have enough demon keys for all selected chests!", "OK")->show();
            return false;
        }

        selected.push_back(chestId);
        highlightChest(button, true);
        return true;
    }

    void onBulkOpen(CCObject*) {
        if (!m_fields->selectedChests.empty()) {
            openAllSelected();
        }
    }

    void showSummary();
    void showCombinedReward(std::vector<GJRewardItem*>& rewards, int chestCount);

    void openAllSelected() {
        auto& selected = m_fields->selectedChests;
        if (selected.empty() || m_fields->isOpening) return;

        m_fields->isOpening = true;
        auto gameStats = GameStatsManager::sharedState();

        int totalCost = 0;
        for (int id : selected) {
            totalCost += gameStats->keyCostForSecretChest(id);
        }

        int currentKeys = gameStats->getStat(STAT_DEMON_KEYS);
        if (currentKeys < totalCost) {
            FLAlertLayer::create(
                "Not Enough Keys",
                fmt::format("Need {} keys, you have {}", totalCost, currentKeys).c_str(),
                "OK"
            )->show();
            m_fields->isOpening = false;
            return;
        }

        m_fields->totalOrbs = 0;
        m_fields->totalDiamonds = 0;
        m_fields->totalDemonKeys = 0;
        m_fields->totalShards.clear();
        m_fields->unlockedItems.clear();

        int keysBefore = gameStats->getStat(STAT_DEMON_KEYS);
        std::vector<GJRewardItem*> rewards;
        int expectedKeysSpent = 0;

        for (int chestId : selected) {
            expectedKeysSpent += gameStats->keyCostForSecretChest(chestId);

            if (auto reward = gameStats->unlockSecretChest(chestId)) {
                rewards.push_back(reward);
                collectRewardStats(reward);
            }

            auto iterator = m_fields->chestButtons.find(chestId);
            if (iterator != m_fields->chestButtons.end() && iterator->second && iterator->second->getParent()) {
                this->switchToOpenedState(iterator->second);
            }
        }

        // fix for gd not always deducting keys properly
        int keysAfter = gameStats->getStat(STAT_DEMON_KEYS);
        int actualSpent = keysBefore - keysAfter;
        if (actualSpent < expectedKeysSpent) {
            int correction = expectedKeysSpent - actualSpent;
            gameStats->setStat(STAT_DEMON_KEYS, keysAfter - correction);
        }

        for (int id : selected) {
            auto iterator = m_fields->chestButtons.find(id);
            if (iterator != m_fields->chestButtons.end() && iterator->second) {
                highlightChest(iterator->second, false);
            }
        }
        selected.clear();
        updateBulkButton();

        if (!rewards.empty()) {
            showCombinedReward(rewards, static_cast<int>(rewards.size()));
        }

        m_fields->isOpening = false;
        this->updateUnlockedLabel();
    }

    void onExit() {
        SecretRewardsLayer::onExit();
    }
};

class BulkRewardSummary : public geode::Popup<> {
protected:
    int m_orbs;
    int m_diamonds;
    int m_keys;
    int m_chestCount;
    std::map<SpecialRewardItem, int> m_shards;
    std::vector<std::pair<UnlockType, int>>m_unlocks;

    const char* shardName(SpecialRewardItem item) {
        switch (item) {
            case SpecialRewardItem::FireShard: return "Fire";
            case SpecialRewardItem::IceShard: return "Ice";
            case SpecialRewardItem::PoisonShard: return "Poison";
            case SpecialRewardItem::ShadowShard: return "Shadow";
            case SpecialRewardItem::LavaShard: return "Lava";
            case SpecialRewardItem::EarthShard: return "Earth";
            case SpecialRewardItem::BloodShard: return "Blood";
            case SpecialRewardItem::MetalShard: return "Metal";
            case SpecialRewardItem::LightShard: return "Light";
            case SpecialRewardItem::SoulShard: return "Soul";
            default: return "Shard";
        }
    }

    CurrencySpriteType shardToCurrencyType(SpecialRewardItem item) {
        switch (item) {
            case SpecialRewardItem::FireShard: return CurrencySpriteType::FireShard;
            case SpecialRewardItem::IceShard: return CurrencySpriteType::IceShard;
            case SpecialRewardItem::PoisonShard: return CurrencySpriteType::PoisonShard;
            case SpecialRewardItem::ShadowShard: return CurrencySpriteType::ShadowShard;
            case SpecialRewardItem::LavaShard: return CurrencySpriteType::LavaShard;
            case SpecialRewardItem::EarthShard: return CurrencySpriteType::EarthShard;
            case SpecialRewardItem::BloodShard: return CurrencySpriteType::BloodShard;
            case SpecialRewardItem::MetalShard: return CurrencySpriteType::MetalShard;
            case SpecialRewardItem::LightShard: return CurrencySpriteType::LightShard;
            case SpecialRewardItem::SoulShard: return CurrencySpriteType::SoulShard;
            default: return CurrencySpriteType::FireShard;
        }
    }

    void onClaim(CCObject*) {
        FMODAudioEngine::sharedEngine()->playEffect("reward01.ogg");

        auto scene = CCDirector::sharedDirector()->getRunningScene();
        if (scene) {
            if (auto rewardLayer = scene->getChildByTag(REWARD_LAYER_TAG)) {
                rewardLayer->removeFromParent();
            }
        }
        this->onClose(nullptr);
    }

    bool setup() override {
        this->setTitle(fmt::format("{} Chests Opened!", m_chestCount).c_str());
        if (m_closeBtn) m_closeBtn->setVisible(false);

        float contentWidth = 360.f;
        float scrollHeight = 180.f;
        float rowHeight = 35.f;
        std::vector<CCNode*> rows;

        bool hasCurrency = (m_orbs > 0 || m_diamonds > 0 || m_keys > 0);
        if (hasCurrency) {
            auto header = CCLabelBMFont::create("= Currency =", "goldFont.fnt");
            header->setScale(0.5f);
            rows.push_back(header);

            auto addCurrency = [&](CurrencySpriteType type, const char* name, int amount, ccColor3B color) {
                if (amount <= 0) return;
                auto row = CCNode::create();
                row->setContentSize({contentWidth, rowHeight});

                auto sprite = CurrencySprite::create(type, false);
                if (sprite) {
                    sprite->setScale(0.7f);
                    sprite->setPosition({25.f, rowHeight / 2.f});
                    row->addChild(sprite);
                }

                auto label = CCLabelBMFont::create(fmt::format("{}: +{}", name, amount).c_str(), "bigFont.fnt");
                label->setScale(0.4f);
                label->setAnchorPoint({0.f, 0.5f});
                label->setPosition({55.f, rowHeight / 2.f});
                label->setColor(color);
                row->addChild(label);
                rows.push_back(row);
            };

            addCurrency(CurrencySpriteType::Orb, "Orbs", m_orbs, {255, 200, 50});
            addCurrency(CurrencySpriteType::Diamond, "Diamonds", m_diamonds, {100, 220, 255});
            addCurrency(CurrencySpriteType::DemonKey, "Demon Keys", m_keys, {255, 100, 100});
        }

        bool hasShards = false;
        for (auto& [type, count] : m_shards) if (count > 0) hasShards = true;

        if (hasShards) {
            auto header = CCLabelBMFont::create("= Shards =", "goldFont.fnt");
            header->setScale(0.5f);
            rows.push_back(header);

            for (auto& [type, count] : m_shards) {
                if (count <= 0) continue;
                auto row = CCNode::create();
                row->setContentSize({contentWidth, rowHeight});

                auto sprite = CurrencySprite::create(shardToCurrencyType(type), false);
                if (sprite) {
                    sprite->setScale(0.7f);
                    sprite->setPosition({25.f, rowHeight / 2.f});
                    row->addChild(sprite);
                }

                auto label = CCLabelBMFont::create(fmt::format("{}: +{}", shardName(type), count).c_str(), "bigFont.fnt");
                label->setScale(0.4f);
                label->setAnchorPoint({0.f, 0.5f});
                label->setPosition({55.f, rowHeight / 2.f});
                label->setColor({200, 150, 255});
                row->addChild(label);
                rows.push_back(row);
            }
        }

        if (!m_unlocks.empty()) {
            auto header = CCLabelBMFont::create("= Items Unlocked =", "goldFont.fnt");
            header->setScale(0.5f);
            rows.push_back(header);

            int iconsPerRow = 5;
            float iconSize = 60.f;

            for (size_t index = 0; index < m_unlocks.size(); index += iconsPerRow) {
                auto row = CCNode::create();
                row->setContentSize({contentWidth, iconSize});

                for (size_t column = index; column < std::min(index + iconsPerRow, m_unlocks.size()); column++) {
                    auto& [type, id] = m_unlocks[column];
                    auto icon = GJItemIcon::createBrowserItem(type, id);
                    if (icon) {
                        icon->setScale(1.3f);
                        float xPosition = ((column - index) + 0.5f) * (contentWidth / iconsPerRow);
                        icon->setPosition({xPosition, iconSize / 2.f});
                        row->addChild(icon);
                    }
                }
                rows.push_back(row);
            }
        }

        float totalHeight = 10.f;
        for (auto row : rows) {
            if (auto label = dynamic_cast<CCLabelBMFont*>(row)) totalHeight += 30.f;
            else totalHeight += row->getContentSize().height;
        }
        totalHeight = std::max(totalHeight, scrollHeight);

        auto scrollLayer = ScrollLayer::create({contentWidth, scrollHeight});
        scrollLayer->setPosition({20.f, 55.f});
        m_mainLayer->addChild(scrollLayer);

        float yPosition = totalHeight - 10.f;
        for (auto row : rows) {
            float height;
            if (auto label = dynamic_cast<CCLabelBMFont*>(row)) {
                height = 30.f;
                label->setPosition({contentWidth / 2.f, yPosition - height / 2.f});
            } else {
                height = row->getContentSize().height;
                row->setAnchorPoint({0.f, 0.f});
                row->setPosition({0.f, yPosition - height});
            }
            scrollLayer->m_contentLayer->addChild(row);
            yPosition -= height;
        }

        scrollLayer->m_contentLayer->setContentSize({contentWidth, totalHeight});
        scrollLayer->scrollToTop();

        auto claimSprite = ButtonSprite::create("Claim", "goldFont.fnt", "GJ_button_01.png", 0.8f);
        auto claimButton = CCMenuItemSpriteExtra::create(claimSprite, this, menu_selector(BulkRewardSummary::onClaim));
        auto buttonMenu = CCMenu::create();
        buttonMenu->addChild(claimButton);
        buttonMenu->setPosition({200.f, 30.f});
        m_mainLayer->addChild(buttonMenu);

        FMODAudioEngine::sharedEngine()->playEffect("gold01.ogg");
        return true;
    }

public:
    static BulkRewardSummary* create(int orbs, int diamonds, int keys,
                                     std::map<SpecialRewardItem, int>& shards,
                                     std::vector<std::pair<UnlockType, int>>& unlocks,
                                     int chestCount) {
        auto instance = new BulkRewardSummary();
        instance->m_orbs = orbs;
        instance->m_diamonds = diamonds;
        instance->m_keys = keys;
        instance->m_shards = shards;
        instance->m_unlocks = unlocks;
        instance->m_chestCount = chestCount;

        if (instance->initAnchored(400.f, 290.f)) {
            instance->autorelease();
            return instance;
        }
        CC_SAFE_DELETE(instance);
        return nullptr;
    }
};

void BulkChestLayer::showSummary() {
    auto popup = BulkRewardSummary::create(
        m_fields->totalOrbs,
        m_fields->totalDiamonds,
        m_fields->totalDemonKeys,
        m_fields->totalShards,
        m_fields->unlockedItems,
        m_fields->chestCount
    );
    if (popup) popup->show();
}

void BulkChestLayer::showCombinedReward(std::vector<GJRewardItem*>& rewards, int chestCount) {
    m_fields->chestCount = chestCount;

    static constexpr int MAX_DISPLAY_ITEMS = 8;
    auto combinedObjects = CCArray::create();
    int itemCount = 0;

    for (auto* reward : rewards) {
        if (!reward || !reward->m_rewardObjects) continue;

        for (auto* rewardObject : CCArrayExt<GJRewardObject*>(reward->m_rewardObjects)) {
            if (rewardObject && itemCount < MAX_DISPLAY_ITEMS) {
                combinedObjects->addObject(rewardObject);
                itemCount++;
            }
        }
    }

    auto combinedReward = GJRewardItem::createWithObjects(GJRewardType::LargeTreasure, combinedObjects);
    if (!combinedReward && !rewards.empty() && rewards[0]) {
        combinedReward = rewards[0];
    }

    auto rewardLayer = RewardUnlockLayer::create(static_cast<int>(GJRewardType::LargeTreasure), nullptr);
    if (rewardLayer && combinedReward) {
        rewardLayer->setTag(REWARD_LAYER_TAG);
        rewardLayer->show();
        rewardLayer->showCollectReward(combinedReward);

        this->runAction(CCSequence::create(
            CCDelayTime::create(0.5f),
            CCCallFunc::create(this, callfunc_selector(BulkChestLayer::hideRewardClaimButton)),
            nullptr
        ));
    }

    this->runAction(CCSequence::create(
        CCDelayTime::create(3.5f),
        CCCallFunc::create(this, callfunc_selector(BulkChestLayer::showSummary)),
        nullptr
    ));
}

$on_mod(Loaded) {
    log::info("bulk chest open loaded");
}