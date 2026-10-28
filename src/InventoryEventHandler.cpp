// STATUS: Working
// Processes inventory changes and updates caches.

#include "../includes/InventoryEventHandler.h"
#include "../includes/ImGui/WidgetPinnedItemsInternal.h"
#include "../includes/ImGui/WidgetCollectiblesInternal.h"
#include "../includes/ImGui/PlayerHUDEquipment.h"
#include "../includes/UIAnimation.h"
#include "../includes/HUDVisibility.h"

namespace InventoryEvents {
    RE::BSEventNotifyControl ContainerChangedSink::ProcessEvent(
        const RE::TESContainerChangedEvent* event,
        RE::BSTEventSource<RE::TESContainerChangedEvent>*)
    {
        if (!event) return RE::BSEventNotifyControl::kContinue;
        if (!WidgetPinnedItems::g_Loaded && !WidgetCollectibles::g_Loaded)
            return RE::BSEventNotifyControl::kContinue;
        if (WidgetPinnedItems::g_PinnedItems.empty() && WidgetCollectibles::g_Collectibles.empty())
            return RE::BSEventNotifyControl::kContinue;
        RE::FormID changedForm = event->baseObj;
        if (changedForm == 0) return RE::BSEventNotifyControl::kContinue;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return RE::BSEventNotifyControl::kContinue;
        RE::FormID playerFormID = player->GetFormID();
        if (playerFormID == 0) return RE::BSEventNotifyControl::kContinue;
        bool itemAdded   = (event->newContainer == playerFormID) && (event->itemCount > 0);
        bool itemRemoved = (event->oldContainer == playerFormID) && (event->itemCount > 0);
        if (!itemAdded && !itemRemoved) return RE::BSEventNotifyControl::kContinue;
        HUDVisibility::Trigger(HUDVisibility::WidgetID::PlayerResources);
        PlayerHUDEquipment::MarkSlotsDirty();
        bool pinnedAdded = false;
        if (auto* changed = RE::TESForm::LookupByID(changedForm);
            changed && changed->Is(RE::FormType::AlchemyItem)) {
            WidgetPinnedItems::RefreshPotionSlots();
            if (itemAdded) {
                if (auto slotId = WidgetPinnedItems::MatchPotionSlotId(changed->As<RE::AlchemyItem>());
                    !slotId.empty()) {
                    UIAnimation::TriggerBounce("pt_bounce_" + slotId);
                    pinnedAdded = true;
                }
            }
        }
        for (size_t i = 0; i < WidgetPinnedItems::g_PinnedItems.size(); ++i) {
            auto& entry = WidgetPinnedItems::g_PinnedItems[i];
            if (!entry.enabled) continue;
            if (entry.id == "health_potion" || entry.id == "magicka_potion" ||
                entry.id == "stamina_potion")
                continue;
            for (const auto& item : entry.items) {
                if (item.resolvedFormID != 0 && item.resolvedFormID == changedForm) {
                    if (i < WidgetPinnedItems::g_CachedCounts.size()) {
                        if (itemAdded) {
                            WidgetPinnedItems::g_CachedCounts[i] += event->itemCount;
                            UIAnimation::TriggerBounce("pt_bounce_" + entry.id);
                            pinnedAdded = true;
                        } else {
                            WidgetPinnedItems::g_CachedCounts[i] -= event->itemCount;
                            if (WidgetPinnedItems::g_CachedCounts[i] < 0)
                                WidgetPinnedItems::g_CachedCounts[i] = 0;
                        }
                    }
                    break;
                }
            }
        }
        if (pinnedAdded)
            HUDVisibility::Trigger(HUDVisibility::WidgetID::PinnedItems);
        bool collectibleChanged = false;
        if (WidgetCollectibles::g_Loaded && !WidgetCollectibles::g_Collectibles.empty()) {
            for (size_t i = 0; i < WidgetCollectibles::g_Collectibles.size(); ++i) {
                auto& entry = WidgetCollectibles::g_Collectibles[i];
                if (!entry.enabled) continue;
                for (const auto& item : entry.items) {
                    if (item.resolvedFormID != 0 && item.resolvedFormID == changedForm) {
                        if (i < WidgetCollectibles::g_CachedCounts.size()) {
                            if (itemAdded) {
                                WidgetCollectibles::g_CachedCounts[i] += event->itemCount;
                                UIAnimation::TriggerBounce("cc_bounce_" + entry.id);
                            } else {
                                WidgetCollectibles::g_CachedCounts[i] -= event->itemCount;
                                if (WidgetCollectibles::g_CachedCounts[i] < 0)
                                    WidgetCollectibles::g_CachedCounts[i] = 0;
                            }
                            collectibleChanged = true;
                        }
                        break;
                    }
                }
            }
        }
        if (collectibleChanged)
            HUDVisibility::Trigger(HUDVisibility::WidgetID::Collectibles);
        {
            auto* form = RE::TESForm::LookupByID(changedForm);
            if (form) {
                auto ft = form->GetFormType();
                if (ft == RE::FormType::Armor) {
                    HUDVisibility::Trigger(HUDVisibility::WidgetID::PlayerResist);
                }
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }

    void RegisterEventSinks() {
        auto* scriptEventSource = RE::ScriptEventSourceHolder::GetSingleton();
        if (!scriptEventSource) {
            logger::error("InventoryEvents: Failed to get ScriptEventSourceHolder");
            return;
        }
        scriptEventSource->AddEventSink(ContainerChangedSink::GetSingleton());
        logger::info("InventoryEvents: ContainerChangedSink registered");
    }
}
