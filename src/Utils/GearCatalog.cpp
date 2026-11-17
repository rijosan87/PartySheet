#include "../../includes/Utils/GearCatalog.h"
#include "../../includes/Utils/PerkModifiers.h"
#include "../../includes/TextureManager.h"

#include <SKSE/SKSE.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <mutex>
#include <unordered_map>

namespace GearCatalog {

    namespace {
        using Clock = std::chrono::steady_clock;

        constexpr auto kRefreshInterval = std::chrono::milliseconds(1000);

        struct Entry {
            std::shared_ptr<const ActorGear> gear;
            Clock::time_point                requested{};
            bool                             pending = false;
            bool                             dirty = false;
        };

        std::mutex                               g_lock;
        std::unordered_map<RE::FormID, Entry>    g_cache;

        bool IsOneHandedWeapon(RE::TESObjectWEAP* weapon) {
            return weapon->IsOneHandedSword() || weapon->IsOneHandedDagger() ||
                   weapon->IsOneHandedAxe() || weapon->IsOneHandedMace();
        }

        bool IsRangedWeapon(RE::TESObjectWEAP* weapon) {
            return weapon->IsBow() || weapon->IsCrossbow();
        }

        bool IsTwoHandedWeapon(RE::TESObjectWEAP* weapon) {
            return IsRangedWeapon(weapon) || weapon->IsTwoHandedSword() || weapon->IsTwoHandedAxe();
        }

        bool IsMissingName(const char* name) {
            return !name || !name[0] || std::strcmp(name, "<Missing Name>") == 0;
        }

        bool IsListable(RE::TESBoundObject* obj) {
            if (auto* weapon = obj->As<RE::TESObjectWEAP>()) return weapon->GetPlayable() && !weapon->IsBound();
            if (obj->As<RE::TESObjectARMO>()) return (obj->GetFormFlags() & RE::TESForm::RecordFlags::kNonPlayable) == 0;
            if (auto* ammo = obj->As<RE::TESAmmo>()) return ammo->GetPlayable();
            if (auto* light = obj->As<RE::TESObjectLIGH>()) return light->CanBeCarried();
            return false;
        }

        struct Instance {
            InstanceKey        key;
            RE::ExtraDataList* list = nullptr;
            int                count = 0;
            bool               wornRight = false;
            bool               wornLeft = false;
        };

        void MergeInstance(std::vector<Instance>& instances, RE::TESBoundObject* obj, RE::ExtraDataList* xl,
                           int count, bool wornRight, bool wornLeft) {
            const InstanceKey key = KeyOf(obj, xl);
            for (auto& existing : instances) {
                if (existing.key == key) {
                    existing.count += count;
                    existing.wornRight |= wornRight;
                    existing.wornLeft |= wornLeft;
                    return;
                }
            }
            instances.push_back({ key, xl, count, wornRight, wornLeft });
        }

        std::vector<Instance> CollectInstances(RE::TESBoundObject* obj, RE::InventoryEntryData* entry, int totalCount) {
            std::vector<Instance> instances;
            int listed = 0;
            if (entry && entry->extraLists) {
                for (auto* xl : *entry->extraLists) {
                    if (!xl) continue;
                    int copies = 1;
                    if (auto* extraCount = xl->GetByType<RE::ExtraCount>()) copies = (std::max)(1, static_cast<int>(extraCount->count));
                    MergeInstance(instances, obj, xl, copies, xl->HasType<RE::ExtraWorn>(), xl->HasType<RE::ExtraWornLeft>());
                    listed += copies;
                }
            }
            if (totalCount > listed) MergeInstance(instances, obj, nullptr, totalCount - listed, false, false);
            return instances;
        }

        struct PreviewContext {
            RE::Actor*           actor = nullptr;
            RE::PlayerCharacter* player = nullptr;
            bool                 isPlayer = false;
            float                ammoBase = 0.0f;
        };

        void FillWeaponPreview(GearItem& item, const PreviewContext& ctx, RE::TESObjectWEAP* weapon, RE::InventoryEntryData& tmp) {
            const float ammoBase = IsRangedWeapon(weapon) ? ctx.ammoBase : 0.0f;
            if (ctx.isPlayer && ctx.player) {
                item.statValue = ctx.player->GetDamage(&tmp) + ammoBase;
            } else {
                item.statValue = EquipmentStatResolver::FormulaDamage(ctx.actor, weapon, PerkModifiers::AttackPerkMult(ctx.actor, weapon), ammoBase);
            }

            RE::EnchantmentItem* enchant = tmp.GetEnchantment();
            if (!enchant) enchant = weapon->formEnchanting;
            SpellDamageResolver::CollectDamageEffects(enchant, item.enchants);
        }

        bool FillItem(GearItem& item, const PreviewContext& ctx, RE::TESBoundObject* obj, const Instance& inst) {
            RE::InventoryEntryData tmp(obj, 1);
            if (inst.list) tmp.AddExtraList(inst.list);

            const char* name = tmp.GetDisplayName();
            if (IsMissingName(name)) name = obj->GetName();
            if (IsMissingName(name)) return false;

            item.key = inst.key;
            item.name = name;
            item.count = inst.count;
            item.category = Classify(obj);
            item.iconHint = IconHint(obj);

            if (auto* weapon = obj->As<RE::TESObjectWEAP>()) {
                item.twoHanded = IsTwoHandedWeapon(weapon);
                FillWeaponPreview(item, ctx, weapon, tmp);
            } else if (auto* armor = obj->As<RE::TESObjectARMO>()) {
                item.statValue = EquipmentStatResolver::ArmorValue(ctx.actor, armor, &tmp);
            }
            return true;
        }

        void AddToCard(ActorGear& gear, Card card, GearItem item, bool equipped) {
            item.equipped = equipped;
            auto& list = gear.cards[static_cast<std::size_t>(card)];
            list.categoryMask |= 1u << static_cast<std::uint32_t>(item.category);
            list.items.push_back(std::move(item));
        }

        struct ArmorCardSlot {
            Card                             card;
            RE::BIPED_MODEL::BipedObjectSlot slot;
        };
        constexpr ArmorCardSlot kArmorCards[] = {
            { Card::Head,   RE::BIPED_MODEL::BipedObjectSlot::kHead },
            { Card::Body,   RE::BIPED_MODEL::BipedObjectSlot::kBody },
            { Card::Feet,   RE::BIPED_MODEL::BipedObjectSlot::kFeet },
            { Card::Hands,  RE::BIPED_MODEL::BipedObjectSlot::kHands },
            { Card::Amulet, RE::BIPED_MODEL::BipedObjectSlot::kAmulet },
            { Card::Ring,   RE::BIPED_MODEL::BipedObjectSlot::kRing },
        };

        enum class LeftMode { OneHandAndOffHand, ArrowAmmo, BoltAmmo, Blocked };

        LeftMode LeftModeFor(RE::Actor* actor) {
            auto* rightForm = actor->GetEquippedObject(false);
            auto* weapon = rightForm ? rightForm->As<RE::TESObjectWEAP>() : nullptr;
            if (!weapon) return LeftMode::OneHandAndOffHand;
            if (weapon->IsCrossbow()) return LeftMode::BoltAmmo;
            if (weapon->IsBow()) return LeftMode::ArrowAmmo;
            if (weapon->IsTwoHandedSword() || weapon->IsTwoHandedAxe()) return LeftMode::Blocked;
            return LeftMode::OneHandAndOffHand;
        }

        void AddWeaponInstance(ActorGear& gear, const GearItem& item, const Instance& inst, RE::TESObjectWEAP* weapon, LeftMode leftMode) {
            AddToCard(gear, Card::RightHand, item, inst.wornRight);

            const bool oneHandCategory = item.category != Category::Staves && item.category != Category::Bows;
            if (leftMode == LeftMode::OneHandAndOffHand && oneHandCategory && IsOneHandedWeapon(weapon))
                AddToCard(gear, Card::LeftHand, item, inst.wornLeft);
        }

        void AddArmorInstance(ActorGear& gear, const GearItem& item, const Instance& inst, RE::TESObjectARMO* armor, LeftMode leftMode) {
            if (item.category == Category::OffHand) {
                if (leftMode == LeftMode::OneHandAndOffHand) AddToCard(gear, Card::LeftHand, item, inst.wornRight || inst.wornLeft);
                return;
            }
            for (const auto& cardSlot : kArmorCards)
                if (armor->HasPartOf(cardSlot.slot)) AddToCard(gear, cardSlot.card, item, inst.wornRight);
        }

        void AddAmmoStack(ActorGear& gear, RE::TESAmmo* ammo, int count, const PreviewContext& ctx, LeftMode leftMode) {
            const bool isBolt = TextureManager::IsBoltAmmo(ammo);
            if (leftMode == LeftMode::ArrowAmmo && isBolt) return;
            if (leftMode == LeftMode::BoltAmmo && !isBolt) return;

            Instance inst;
            inst.key = KeyOf(ammo, nullptr);
            inst.count = count;
            GearItem item;
            if (!FillItem(item, ctx, ammo, inst)) return;
            item.statValue = ammo->GetRuntimeData().data.damage;
            AddToCard(gear, Card::LeftHand, std::move(item), ammo == ctx.actor->GetCurrentAmmo());
        }

        void SortCard(CardList& list) {
            std::sort(list.items.begin(), list.items.end(), [](const GearItem& a, const GearItem& b) {
                if (a.equipped != b.equipped) return a.equipped;
                if (a.statValue != b.statValue) return a.statValue > b.statValue;
                if (a.name != b.name) return a.name < b.name;
                if (a.key.baseID != b.key.baseID) return a.key.baseID < b.key.baseID;
                return a.key.health > b.key.health;
            });
        }

        void BuildCards(RE::Actor* actor, ActorGear& gear) {
            PreviewContext ctx;
            ctx.actor = actor;
            ctx.player = RE::PlayerCharacter::GetSingleton();
            ctx.isPlayer = actor->IsPlayerRef();
            if (auto* currentAmmo = actor->GetCurrentAmmo()) ctx.ammoBase = currentAmmo->GetRuntimeData().data.damage;

            const LeftMode leftMode = LeftModeFor(actor);
            gear.cards[static_cast<std::size_t>(Card::LeftHand)].interactive = leftMode != LeftMode::Blocked;

            auto inv = actor->GetInventory([](RE::TESBoundObject& o) {
                return o.IsWeapon() || o.IsArmor() || o.IsAmmo() || o.Is(RE::FormType::Light);
            });
            for (auto& [obj, data] : inv) {
                auto& [count, entry] = data;
                if (!obj || count <= 0 || !IsListable(obj)) continue;
                if (Classify(obj) == Category::None) continue;

                if (auto* ammo = obj->As<RE::TESAmmo>()) {
                    AddAmmoStack(gear, ammo, count, ctx, leftMode);
                    continue;
                }

                for (const auto& inst : CollectInstances(obj, entry.get(), count)) {
                    GearItem item;
                    if (!FillItem(item, ctx, obj, inst)) continue;

                    if (auto* weapon = obj->As<RE::TESObjectWEAP>()) {
                        AddWeaponInstance(gear, item, inst, weapon, leftMode);
                    } else if (auto* armor = obj->As<RE::TESObjectARMO>()) {
                        AddArmorInstance(gear, item, inst, armor, leftMode);
                    } else if (leftMode == LeftMode::OneHandAndOffHand) {
                        AddToCard(gear, Card::LeftHand, std::move(item), inst.wornRight || inst.wornLeft);
                    }
                }
            }

            for (auto& card : gear.cards) SortCard(card);
        }

        void ClassifyRow(RE::TESForm* form, InventoryItem& row) {
            auto set = [&](ItemTab tab, ItemType type, const char* hint) { row.tab = tab; row.type = type; row.iconHint = hint; };
            if (form->As<RE::TESObjectWEAP>())      set(ItemTab::Weapon, ItemType::Weapon, IconHint(form));
            else if (form->As<RE::TESObjectARMO>()) set(ItemTab::Armor, ItemType::Armor, IconHint(form));
            else if (auto* alch = form->As<RE::AlchemyItem>()) {
                if (alch->IsFood()) set(ItemTab::Food, ItemType::Food, "food");
                else                set(ItemTab::Potion, ItemType::Potion, "potion");
            }
            else if (form->As<RE::TESAmmo>())       set(ItemTab::Misc, ItemType::Ammo, IconHint(form));
            else if (form->As<RE::IngredientItem>()) set(ItemTab::Ingredient, ItemType::Ingredient, "ingredient");
            else if (form->As<RE::TESSoulGem>())    set(ItemTab::Misc, ItemType::SoulGem, "soulgem");
            else if (form->As<RE::TESKey>())        set(ItemTab::Misc, ItemType::Key, "key");
            else if (form->As<RE::TESObjectBOOK>()) set(ItemTab::Book, ItemType::Book, "book");
            else if (form->As<RE::ScrollItem>())    set(ItemTab::Scroll, ItemType::Scroll, "scroll");
            else                                    set(ItemTab::Misc, ItemType::Misc, "misc");
        }

        bool FillRow(InventoryItem& row, const PreviewContext& ctx, RE::TESBoundObject* obj, const Instance& inst) {
            RE::InventoryEntryData tmp(obj, 1);
            if (inst.list) tmp.AddExtraList(inst.list);

            const char* name = tmp.GetDisplayName();
            if (IsMissingName(name)) name = obj->GetName();
            if (IsMissingName(name)) return false;

            row.key = inst.key;
            row.name = name;
            row.count = inst.count;
            row.weight = tmp.GetWeight();
            row.value = tmp.GetValue();
            row.quest = tmp.IsQuestObject();
            row.worn = inst.wornRight || inst.wornLeft;
            row.tempered = inst.key.health > 1.0f;
            ClassifyRow(obj, row);

            if (auto* weapon = obj->As<RE::TESObjectWEAP>()) {
                row.enchanted = inst.key.enchantID != 0 || weapon->formEnchanting;
                GearItem preview;
                FillWeaponPreview(preview, ctx, weapon, tmp);
                row.statValue = preview.statValue;
            } else if (auto* armor = obj->As<RE::TESObjectARMO>()) {
                row.enchanted = inst.key.enchantID != 0 || armor->formEnchanting;
                row.statValue = EquipmentStatResolver::ArmorValue(ctx.actor, armor, &tmp);
            } else if (auto* ammo = obj->As<RE::TESAmmo>()) {
                row.statValue = ammo->GetRuntimeData().data.damage;
            }
            return true;
        }

        void BuildItems(RE::Actor* actor, ActorGear& gear) {
            PreviewContext ctx;
            ctx.actor = actor;
            ctx.player = RE::PlayerCharacter::GetSingleton();
            ctx.isPlayer = actor->IsPlayerRef();
            if (auto* currentAmmo = actor->GetCurrentAmmo()) ctx.ammoBase = currentAmmo->GetRuntimeData().data.damage;

            constexpr RE::FormID kGold = 0xF;
            auto inv = actor->GetInventory([](RE::TESBoundObject& o) { return o.GetFormID() != kGold; });
            for (auto& [obj, data] : inv) {
                auto& [count, entry] = data;
                if (!obj || count <= 0) continue;

                const bool splitCopies = obj->IsWeapon() || obj->IsArmor() || obj->Is(RE::FormType::Light);
                if (splitCopies) {
                    for (const auto& inst : CollectInstances(obj, entry.get(), count)) {
                        InventoryItem row;
                        if (FillRow(row, ctx, obj, inst)) gear.items.push_back(std::move(row));
                    }
                    continue;
                }

                Instance whole;
                whole.key = KeyOf(obj, nullptr);
                whole.count = count;
                InventoryItem row;
                if (!FillRow(row, ctx, obj, whole)) continue;
                if (entry) {
                    row.quest = entry->IsQuestObject();
                    row.worn = obj->IsAmmo() && obj == actor->GetCurrentAmmo();
                }
                gear.items.push_back(std::move(row));
            }

            std::sort(gear.items.begin(), gear.items.end(), [](const InventoryItem& a, const InventoryItem& b) {
                if (a.tab != b.tab) return a.tab < b.tab;
                if (a.name != b.name) return a.name < b.name;
                return a.statValue > b.statValue;
            });
        }

        void Rebuild(RE::ActorHandle handle, RE::FormID actorID) {
            std::shared_ptr<ActorGear> gear;
            if (auto actorPtr = handle.get()) {
                auto* actor = actorPtr.get();
                gear = std::make_shared<ActorGear>();
                gear->canEquip = actor->Is3DLoaded() && (actor->IsPlayerRef() || !actor->IsInCombat());
                BuildCards(actor, *gear);
                BuildItems(actor, *gear);
            }

            std::lock_guard lk(g_lock);
            auto it = g_cache.find(actorID);
            if (it == g_cache.end()) return;
            if (gear) it->second.gear = std::move(gear);
            it->second.pending = false;
        }
    }

    Category Classify(RE::TESForm* form) {
        if (!form) return Category::None;

        if (auto* weapon = form->As<RE::TESObjectWEAP>()) {
            using WT = RE::WEAPON_TYPE;
            switch (weapon->GetWeaponType()) {
                case WT::kOneHandSword:
                case WT::kTwoHandSword:    return Category::Swords;
                case WT::kOneHandDagger:   return Category::Daggers;
                case WT::kOneHandAxe:      return Category::Axes;
                case WT::kTwoHandAxe:      return TextureManager::IsWarhammer(weapon) ? Category::Maces : Category::Axes;
                case WT::kOneHandMace:     return Category::Maces;
                case WT::kBow:
                case WT::kCrossbow:        return Category::Bows;
                case WT::kStaff:           return Category::Staves;
                default:                   return Category::None;
            }
        }
        if (auto* armor = form->As<RE::TESObjectARMO>()) return armor->IsShield() ? Category::OffHand : Category::Armor;
        if (form->As<RE::TESAmmo>()) return Category::Ammo;
        if (auto* light = form->As<RE::TESObjectLIGH>()) return light->CanBeCarried() ? Category::OffHand : Category::None;
        return Category::None;
    }

    const char* IconHint(RE::TESForm* form) {
        if (!form) return "misc";

        if (auto* weapon = form->As<RE::TESObjectWEAP>()) {
            using WT = RE::WEAPON_TYPE;
            switch (weapon->GetWeaponType()) {
                case WT::kHandToHandMelee: return "fist";
                case WT::kOneHandSword:    return "sword";
                case WT::kOneHandDagger:   return "dagger";
                case WT::kOneHandAxe:      return "axe";
                case WT::kOneHandMace:     return "mace";
                case WT::kTwoHandSword:    return "greatsword";
                case WT::kTwoHandAxe:      return "battleaxe";
                case WT::kBow:             return "bow";
                case WT::kStaff:           return "staff";
                case WT::kCrossbow:        return "crossbow";
                default:                   return "sword";
            }
        }
        if (auto* armor = form->As<RE::TESObjectARMO>()) {
            using SL = RE::BIPED_MODEL::BipedObjectSlot;
            if (armor->HasPartOf(SL::kHead))    return "helmet";
            if (armor->HasPartOf(SL::kBody))    return "cuirass";
            if (armor->HasPartOf(SL::kHands))   return "gauntlets";
            if (armor->HasPartOf(SL::kFeet))    return "boots";
            if (armor->HasPartOf(SL::kShield))  return "shield";
            if (armor->HasPartOf(SL::kAmulet))  return "amulet";
            if (armor->HasPartOf(SL::kRing))    return "ring";
            if (armor->HasPartOf(SL::kCirclet)) return "circlet";
            return "cuirass";
        }
        if (form->As<RE::TESAmmo>()) return "arrow";
        return "misc";
    }

    const char* CategoryIcon(Category category) {
        switch (category) {
            case Category::Swords:   return "sword";
            case Category::Daggers:  return "dagger";
            case Category::Axes:     return "axe";
            case Category::Maces:    return "mace";
            case Category::Bows:     return "bow";
            case Category::Staves:   return "staff";
            case Category::OffHand:  return "shield";
            case Category::Armor:    return "armor";
            case Category::Ammo:     return "bolt";
            default:                 return "";
        }
    }

    std::shared_ptr<const ActorGear> Get(RE::Actor* actor) {
        if (!actor) return nullptr;

        const RE::FormID actorID = actor->GetFormID();
        const auto       now = Clock::now();

        std::shared_ptr<const ActorGear> snapshot;
        bool                             queue = false;
        {
            std::lock_guard lk(g_lock);
            auto& e = g_cache[actorID];
            snapshot = e.gear;
            const bool stale = !e.gear || e.dirty || now - e.requested >= kRefreshInterval;
            if (!e.pending && stale) {
                e.pending = true;
                e.dirty = false;
                e.requested = now;
                queue = true;
            }
        }

        if (queue) {
            auto* tasks = SKSE::GetTaskInterface();
            if (tasks) {
                tasks->AddTask([handle = actor->GetHandle(), actorID]() { Rebuild(handle, actorID); });
            } else {
                std::lock_guard lk(g_lock);
                g_cache[actorID].pending = false;
            }
        }
        return snapshot;
    }

    void Invalidate(RE::FormID actorID) {
        std::lock_guard lk(g_lock);
        if (auto it = g_cache.find(actorID); it != g_cache.end()) it->second.dirty = true;
    }

    void Clear() {
        std::lock_guard lk(g_lock);
        g_cache.clear();
    }

    InstanceKey KeyOf(RE::TESBoundObject* obj, RE::ExtraDataList* xl) {
        InstanceKey k;
        k.baseID = obj ? obj->GetFormID() : 0;
        if (!xl) return k;
        if (auto* h = xl->GetByType<RE::ExtraHealth>()) {
            k.health = h->health;
            k.hasExtra = true;
        }
        if (auto* e = xl->GetByType<RE::ExtraEnchantment>()) {
            k.enchantID = e->enchantment ? e->enchantment->GetFormID() : 0;
            k.hasExtra = true;
        }
        if (auto* p = xl->GetByType<RE::ExtraPoison>()) {
            k.poisonID = p->poison ? p->poison->GetFormID() : 0;
            k.hasExtra = true;
        }
        if (auto* t = xl->GetByType<RE::ExtraTextDisplayData>()) {
            k.customName = t->displayName.c_str();
            k.hasExtra = true;
        }
        return k;
    }

    RE::ExtraDataList* ResolveExtraList(RE::Actor* actor, const InstanceKey& key, bool& outFound) {
        outFound = false;
        if (!actor) return nullptr;

        if (!key.hasExtra) {
            outFound = true;
            return nullptr;
        }

        auto inv = actor->GetInventory([&](RE::TESBoundObject& o) { return o.GetFormID() == key.baseID; });
        for (auto& [obj, data] : inv) {
            auto& entry = data.second;
            if (!entry || !entry->extraLists) continue;
            for (auto* xl : *entry->extraLists) {
                if (xl && KeyOf(obj, xl) == key) {
                    outFound = true;
                    return xl;
                }
            }
        }
        return nullptr;
    }
}
