#include "../../includes/Utils/GearCatalog.h"
#include "../../includes/Utils/PerkModifiers.h"
#include "../../includes/TextureManager.h"
#include "../../includes/EffectCategories.h"

#include <SKSE/SKSE.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

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

        std::string FillQuestTokens(const char* name, RE::ExtraDataList* xl) {
            if (!std::strchr(name, '<')) return name;
            auto* text = xl ? xl->GetByType<RE::ExtraTextDisplayData>() : nullptr;
            RE::TESQuest* quest = text ? text->ownerQuest : nullptr;
            std::string out = name;
            if (quest) {
                using ParseString_t = void (*)(RE::BSString*, const RE::TESQuest*, std::uint32_t);
                static REL::Relocation<ParseString_t> parseString{ REL::RelocationID(23429, 23897) };
                const auto instance = static_cast<std::int32_t>(text->ownerInstance.underlying());
                RE::BSString formatted{ name };
                parseString(&formatted, quest, instance >= 0 ? static_cast<std::uint32_t>(instance) : quest->currentInstanceID);
                if (formatted.c_str() && formatted.c_str()[0]) out = formatted.c_str();
            }
            for (std::size_t open = out.find("<Alias"); open != std::string::npos; open = out.find("<Alias", open)) {
                const std::size_t close = out.find('>', open);
                if (close == std::string::npos) break;
                out.erase(open, close - open + 1);
            }
            return out;
        }

        const char* RawNameOf(RE::InventoryEntryData& tmp, RE::TESBoundObject* obj) {
            const char* name = tmp.GetDisplayName();
            if (IsMissingName(name)) name = obj->GetName();
            return IsMissingName(name) ? nullptr : name;
        }

        int PoisonDoses(const Instance& inst) {
            auto* p = inst.list ? inst.list->GetByType<RE::ExtraPoison>() : nullptr;
            return p && p->poison ? static_cast<int>(p->count) : 0;
        }

        bool FillItem(GearItem& item, const PreviewContext& ctx, RE::TESBoundObject* obj, const Instance& inst) {
            RE::InventoryEntryData tmp(obj, 1);
            if (inst.list) tmp.AddExtraList(inst.list);

            const char* name = RawNameOf(tmp, obj);
            if (!name) return false;

            item.key = inst.key;
            item.name = FillQuestTokens(name, inst.list);
            item.count = inst.count;
            item.poisonDoses = PoisonDoses(inst);
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

        SlotTag TagFor(Card card, RE::TESForm* form) {
            if (auto* weapon = form->As<RE::TESObjectWEAP>()) {
                using WT = RE::WEAPON_TYPE;
                switch (weapon->GetWeaponType()) {
                    case WT::kOneHandDagger: return SlotTag::Daggers;
                    case WT::kBow:           return SlotTag::Bows;
                    case WT::kCrossbow:      return SlotTag::Crossbows;
                    case WT::kStaff:         return SlotTag::Staves;
                    default: return IsTwoHandedWeapon(weapon) ? SlotTag::TwoHanded : SlotTag::OneHanded;
                }
            }
            if (auto* ammo = form->As<RE::TESAmmo>()) return TextureManager::IsBoltAmmo(ammo) ? SlotTag::Bolts : SlotTag::Arrows;
            if (form->As<RE::TESObjectLIGH>()) return SlotTag::Torches;
            auto* armor = form->As<RE::TESObjectARMO>();
            if (!armor) return SlotTag::None;
            if (armor->IsShield()) return SlotTag::Shields;
            if (card == Card::Amulet || card == Card::Ring) return SlotTag::None;
            const bool rated = armor->IsHeavyArmor() || armor->IsLightArmor();
            if (card == Card::Head) {
                using SL = RE::BIPED_MODEL::BipedObjectSlot;
                const bool circletOnly = armor->HasPartOf(SL::kCirclet) && !armor->HasPartOf(SL::kHead) && !armor->HasPartOf(SL::kHair);
                if (circletOnly) return SlotTag::Circlets;
                return rated ? SlotTag::Helmets : SlotTag::Hoods;
            }
            if (armor->IsHeavyArmor()) return SlotTag::Heavy;
            if (armor->IsLightArmor()) return SlotTag::Light;
            return SlotTag::Clothing;
        }

        void AddToCard(ActorGear& gear, Card card, RE::TESForm* form, GearItem item, bool equipped) {
            item.equipped = equipped;
            item.tag = TagFor(card, form);
            auto& list = gear.cards[static_cast<std::size_t>(card)];
            list.categoryMask |= 1u << static_cast<std::uint32_t>(item.category);
            if (item.tag != SlotTag::None) list.tagMask |= 1u << static_cast<std::uint32_t>(item.tag);
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
            AddToCard(gear, Card::RightHand, weapon, item, inst.wornRight);

            const bool oneHandCategory = item.category != Category::Staves && item.category != Category::Bows;
            if (leftMode == LeftMode::OneHandAndOffHand && oneHandCategory && IsOneHandedWeapon(weapon))
                AddToCard(gear, Card::LeftHand, weapon, item, inst.wornLeft);
        }

        void AddArmorInstance(ActorGear& gear, const GearItem& item, const Instance& inst, RE::TESObjectARMO* armor, LeftMode leftMode) {
            if (item.category == Category::OffHand) {
                if (leftMode == LeftMode::OneHandAndOffHand) AddToCard(gear, Card::LeftHand, armor, item, inst.wornRight || inst.wornLeft);
                return;
            }
            using SL = RE::BIPED_MODEL::BipedObjectSlot;
            for (const auto& cardSlot : kArmorCards) {
                bool fits = armor->HasPartOf(cardSlot.slot);
                if (cardSlot.card == Card::Head && !fits)
                    fits = (armor->HasPartOf(SL::kHair) || armor->HasPartOf(SL::kCirclet)) && !armor->HasPartOf(SL::kBody);
                if (fits) AddToCard(gear, cardSlot.card, armor, item, inst.wornRight);
            }
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
            AddToCard(gear, Card::LeftHand, ammo, std::move(item), ammo == ctx.actor->GetCurrentAmmo());
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
                        AddToCard(gear, Card::LeftHand, obj, std::move(item), inst.wornRight || inst.wornLeft);
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
                else {
                    set(ItemTab::Potion, ItemType::Potion, "potion");
                    row.potionTag = ClassifyPotion(alch);
                    static std::unordered_set<RE::FormID> s_logged;
                    if (s_logged.insert(alch->GetFormID()).second)
                        logger::debug("[PotionTag] {:08X} '{}' tag={}", alch->GetFormID(), alch->GetName(),
                                      static_cast<int>(row.potionTag));
                }
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

            const char* name = RawNameOf(tmp, obj);
            if (!name) return false;

            row.key = inst.key;
            row.name = FillQuestTokens(name, inst.list);
            row.count = inst.count;
            row.poisonDoses = PoisonDoses(inst);
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
                if (count == 1 && entry && entry->extraLists) {
                    int lists = 0;
                    for (auto* xl : *entry->extraLists) {
                        if (xl && ++lists == 1) whole.list = xl;
                    }
                    if (lists != 1) whole.list = nullptr;
                }
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
            if (armor->HasPartOf(SL::kBody))    return "cuirass";
            if (armor->HasPartOf(SL::kHead) || armor->HasPartOf(SL::kHair)) return "helmet";
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

    PotionTag ClassifyPotion(RE::AlchemyItem* potion) {
        if (!potion || potion->IsFood()) return PotionTag::None;
        if (potion->IsPoison()) return PotionTag::Poison;

        RE::EffectSetting* base = nullptr;
        for (auto* effect : potion->effects) {
            if (effect && effect->baseEffect) { base = effect->baseEffect; break; }
        }
        if (!base) return PotionTag::None;

        using Arch = RE::EffectArchetypes::ArchetypeID;
        const auto arch = base->GetArchetype();
        if (arch == Arch::kCureDisease || arch == Arch::kCurePoison) return PotionTag::Cure;

        using AV = RE::ActorValue;
        switch (base->data.primaryAV) {
            case AV::kHealth:  case AV::kHealRate:    case AV::kHealRateMult:    return PotionTag::Health;
            case AV::kMagicka: case AV::kMagickaRate: case AV::kMagickaRateMult: return PotionTag::Magicka;
            case AV::kStamina: case AV::kStaminaRate: case AV::kStaminaRateMult: return PotionTag::Stamina;
            case AV::kResistFire: case AV::kResistFrost: case AV::kResistShock:
            case AV::kResistMagic: case AV::kPoisonResist: case AV::kResistDisease: return PotionTag::Resist;
            case AV::kCarryWeight: case AV::kDamageResist: case AV::kAttackDamageMult:
            case AV::kWeaponSpeedMult: case AV::kSpeedMult: return PotionTag::Fortify;
            default: break;
        }
        if (EffectCategories::GetSkillTarget(base->data.primaryAV) != AV::kNone) return PotionTag::Fortify;
        return PotionTag::None;
    }

    const char* PotionTagKey(PotionTag tag) {
        switch (tag) {
            case PotionTag::Health:  return "Inventory.TagHealth";
            case PotionTag::Magicka: return "Inventory.TagMagicka";
            case PotionTag::Stamina: return "Inventory.TagStamina";
            case PotionTag::Fortify: return "Inventory.TagFortify";
            case PotionTag::Resist:  return "Inventory.TagResist";
            case PotionTag::Cure:    return "Inventory.TagCure";
            case PotionTag::Poison:  return "Inventory.TagPoison";
            default:                 return "";
        }
    }

    const char* SlotTagKey(SlotTag tag) {
        switch (tag) {
            case SlotTag::OneHanded: return "Inventory.TagOneHanded";
            case SlotTag::TwoHanded: return "Inventory.TagTwoHanded";
            case SlotTag::Daggers:   return "Inventory.TagDaggers";
            case SlotTag::Bows:      return "Inventory.TagBows";
            case SlotTag::Crossbows: return "Inventory.TagCrossbows";
            case SlotTag::Staves:    return "Inventory.TagStaves";
            case SlotTag::Shields:   return "Inventory.TagShields";
            case SlotTag::Torches:   return "Inventory.TagTorches";
            case SlotTag::Arrows:    return "Inventory.TagArrows";
            case SlotTag::Bolts:     return "Inventory.TagBolts";
            case SlotTag::Helmets:   return "Inventory.TagHelmets";
            case SlotTag::Hoods:     return "Inventory.TagHoods";
            case SlotTag::Circlets:  return "Inventory.TagCirclets";
            case SlotTag::Heavy:     return "Inventory.TagHeavy";
            case SlotTag::Light:     return "Inventory.TagLight";
            case SlotTag::Clothing:  return "Inventory.TagClothing";
            default:                 return "";
        }
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
