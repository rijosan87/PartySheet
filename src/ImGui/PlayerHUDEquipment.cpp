#include "../../includes/ImGui/PlayerHUDEquipment.h"
#include "../../includes/ImGui/Renderer.h"
#include "../../includes/Settings.h"
#include "../../includes/TextureManager.h"
#include "../../includes/BackgroundManager.h"
#include "../../includes/MenuUtils.h"
#include "../../includes/Utils/ColorUtils.h"
#include "../../includes/Utils/EquipmentStatResolver.h"
#include "../../includes/Utils/HUDDraw.h"
#include "../../includes/Utils/HUDFlow.h"
#include "../../includes/HUDVisibility.h"
#include "../../includes/LocalizationManager.h"
#include "../../includes/UIAnimation.h"
#include "../../includes/PNGIIBridge.h"
#include "../../includes/HUDSymptoms.h"
#include "../../includes/ImGui/WidgetDrawHelpers.h"
#include "../../includes/Utils/EffectIconRenderer.h"
#include "../../includes/Utils/EffectBadges.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <RE/Skyrim.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <vector>

namespace PlayerHUDEquipment {

    enum class SlotType { LeftHand, RightHand, Shout };

    struct SlotDef {
        SlotType    type;
        const char* badge;
        const char* fallbackHint;
    };

    static constexpr SlotDef k_Slots[] = {
        { SlotType::RightHand, "R", "weapon" },
        { SlotType::LeftHand,  "L", "weapon" },
        { SlotType::Shout,     "Z", "shout"  },
    };
    static constexpr int k_SlotCount = static_cast<int>(std::size(k_Slots));

    using EffectBadge = EffectBadges::Badge;

    struct SlotData {
        std::string                       name;
        RE::TESForm*                      form          = nullptr;
        ID3D11ShaderResourceView*         icon          = nullptr;
        ID3D11ShaderResourceView*         glyph         = nullptr; // PNGII pictogram (peIconStyle 1); null = draw icon
        bool                              isEmpty       = true;
        int                               ammoCount     = -1;
        float                             weaponDamage  = -1.0f;
        float                             enchantCharge = -1.0f;
        float                             enchantChargeMax = -1.0f;
        float                             enchantDamage = 0.0f;
        float                             castCharge    = -1.0f;
        float                             shoutCooldown = -1.0f;
        float                             durabilityPct = -1.0f;
        std::string                       enchantType;
        int                               auraType      = 0;
        bool                              symptom       = false;
        std::vector<EffectBadge>          effects;
    };

    static HUDDraw::BadgeStyle MakeBadgeStyle(std::uint32_t textColor) {
        const auto* s = Settings::GetSingleton();
        HUDDraw::BadgeStyle bs;
        bs.fontScale = s->peBadgeFontScale;
        const std::uint32_t bg = s->peBadgeBgColor;
        const std::uint8_t a = (bg >> 24) & 0xFF, r = (bg >> 16) & 0xFF,
                           g = (bg >> 8) & 0xFF,  b =  bg        & 0xFF;
        auto lift = [](std::uint8_t c) { return static_cast<std::uint32_t>((std::min)(255.0f, c * 1.20f + 14.0f)); };
        auto sink = [](std::uint8_t c) { return static_cast<std::uint32_t>(c * 0.55f); };
        bs.bgTop    = (static_cast<std::uint32_t>(a) << 24) | (lift(r) << 16) | (lift(g) << 8) | lift(b);
        bs.bgBottom = 0xFF000000u | (sink(r) << 16) | (sink(g) << 8) | sink(b);
        bs.textColor = textColor;
        return bs;
    }

    static bool        g_ForcePos = false;
    static std::string g_LastDebugKey;
    static RE::TESForm* g_PrevRH   = nullptr;
    static RE::TESForm* g_PrevLH   = nullptr;
    static RE::TESForm* g_PrevPow  = nullptr;
    static RE::TESAmmo* g_PrevAmmo = nullptr;
    static SlotData     g_CachedSlots[k_SlotCount];
    static bool         g_SlotsDirty = true;
    static float        g_ShoutCooldownMax = 0.0f;
    static bool         g_ShoutWasReady = true;

    static std::chrono::steady_clock::time_point g_ShoutDeniedAt{};
    static constexpr float k_ShoutDeniedDur = 0.45f;

    // ── Master scale helpers ────────────────────────────────────────────────
    static float MasterScale()
    {
        const auto* settings = Settings::GetSingleton();
        return settings
            ? std::clamp(static_cast<float>(settings->peScale) / 100.0f, 0.5f, 2.0f)
            : 1.0f;
    }

    static float ScalePx(float value)
    {
        return value * MasterScale();
    }

    void Init() {}
    void ForceReposition() { g_ForcePos = true; }
    void MarkSlotsDirty()  { g_SlotsDirty = true; }

    void NotifyShoutPressed()
    {
        HUDVisibility::Trigger(HUDVisibility::WidgetID::PlayerEquip);

        for (std::size_t i = 0; i < k_SlotCount; ++i) {
            if (k_Slots[i].type != SlotType::Shout)
                continue;
            const float cd = g_CachedSlots[i].shoutCooldown;
            if (cd >= 0.0f && cd < 1.0f) {
                g_ShoutDeniedAt = std::chrono::steady_clock::now();
                UIAnimation::TriggerShake("peq_shout_denied");
            }
            break;
        }
    }

    static ID3D11ShaderResourceView* ResolveIcon(RE::TESForm* form, const char* hint)
    {
        return TextureManager::GetHeldIcon(form, hint);
    }

    static int GetAmmoInfo(RE::PlayerCharacter* player, RE::TESForm* weaponForm,
                           RE::TESAmmo*& outAmmo, bool& outIsBolt)
    {
        outAmmo  = nullptr;
        outIsBolt = false;
        if (!weaponForm) return -1;
        auto* weap = weaponForm->As<RE::TESObjectWEAP>();
        if (!weap) return -1;
        auto wt = weap->GetWeaponType();
        if (wt != RE::WEAPON_TYPE::kBow && wt != RE::WEAPON_TYPE::kCrossbow) return -1;

        outIsBolt = (wt == RE::WEAPON_TYPE::kCrossbow);
        outAmmo   = player->GetCurrentAmmo();
        if (!outAmmo) return 0;
        return player->GetItemCount(outAmmo);
    }

    static float ResolveEnchantCharge(RE::PlayerCharacter* player, RE::TESForm* form, bool isRightHand)
    {
        if (!form) return -1.0f;
        auto* weap = form->As<RE::TESObjectWEAP>();
        if (!weap) return -1.0f;

        if (!EquipmentStatResolver::GetWeaponEnchantment(player, weap, !isRightHand)) return -1.0f;

        if (auto* avo = player->AsActorValueOwner()) {
            const auto av = isRightHand ? RE::ActorValue::kRightItemCharge : RE::ActorValue::kLeftItemCharge;
            const float liveCharge = avo->GetActorValue(av);
            if (liveCharge >= 0.0f) return liveCharge;
        }

        auto* entry = player->GetEquippedEntryData(!isRightHand);

        if (!entry) return -1.0f;
        if (entry->object != weap) return -1.0f;

        auto charge = entry->GetEnchantmentCharge();
        if (!charge.has_value()) return -1.0f;
        return static_cast<float>(charge.value());
    }

    static float ResolveEnchantChargeMax(RE::PlayerCharacter* player, RE::TESForm* form, bool isRightHand)
    {
        if (!form) return -1.0f;
        auto* enchantable = form->As<RE::TESEnchantableForm>();
        if (!enchantable) return -1.0f;

        float maxCharge = -1.0f;

        if (enchantable->amountofEnchantment > 0) {
            maxCharge = static_cast<float>(enchantable->amountofEnchantment);
        } else {
            auto* entry = player->GetEquippedEntryData(!isRightHand);
            if (entry && entry->object == form && entry->extraLists) {
                for (auto* xl : *entry->extraLists) {
                    if (!xl) continue;
                    auto* xe = xl->GetByType<RE::ExtraEnchantment>();
                    if (xe && xe->charge > 0) {
                        maxCharge = static_cast<float>(xe->charge);
                        break;
                    }
                }
            }
        }

        return maxCharge;
    }

    static float ResolveDurability(RE::PlayerCharacter* player, RE::TESForm* form, bool isLeftHand)
    {
        if (!form) return -1.0f;
        const bool isWeapon = form->IsWeapon();
        const bool isArmor  = form->IsArmor();
        if (!isWeapon && !isArmor) return -1.0f;

        auto healthToDurability = [](float h) -> float {
            if (h >= 1.0f) return 1.0f;  // baseline pristine or tempered above baseline
            if (h <= 0.9f) return 0.0f;  // broken / minimum durability floor
            return std::clamp((h - 0.9f) / 0.1f, 0.0f, 1.0f);
        };

        auto isMatchingExtra = [&](RE::ExtraDataList* xl) -> bool {
            if (!xl) return false;
            if (isArmor) {
                return xl->HasType(RE::ExtraDataType::kWorn) || xl->HasType(RE::ExtraDataType::kWornLeft);
            }
            return xl->HasType(isLeftHand ? RE::ExtraDataType::kWornLeft : RE::ExtraDataType::kWorn);
        };

        if (isArmor) {
            auto* changes = player->GetInventoryChanges();
            if (!changes || !changes->entryList) return -1.0f;
            for (auto* entry : *changes->entryList) {
                if (!entry || !entry->extraLists) continue;
                if (entry->object != form) continue;
                for (auto* xl : *entry->extraLists) {
                    if (!isMatchingExtra(xl)) continue;
                    auto* xh = xl->GetByType<RE::ExtraHealth>();
                    if (!xh) return 1.0f;
                    return healthToDurability(xh->health);
                }
            }
            return -1.0f;
        }

        auto* entry = player->GetEquippedEntryData(isLeftHand);
        if (!entry || !entry->extraLists) return -1.0f;
        for (auto* xl : *entry->extraLists) {
            if (!isMatchingExtra(xl)) continue;
            auto* xh = xl->GetByType<RE::ExtraHealth>();
            if (!xh) return 1.0f;
            return healthToDurability(xh->health);
        }
        return -1.0f;
    }

    static float ResolveCastCharge(RE::PlayerCharacter* player, RE::MagicSystem::CastingSource src, RE::TESForm* form)
    {
        if (!form) return -1.0f;
        auto* spell = form->As<RE::SpellItem>();
        if (!spell) return -1.0f;
        auto* caster = player->GetMagicCaster(src);
        if (!caster) return -1.0f;
        auto st = caster->state.get();
        const int srcIdx = static_cast<int>(src) & 3;

        static float s_ChargeDur[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        static int   s_LastState[4] = { -1, -1, -1, -1 };
        if (static_cast<int>(st) != s_LastState[srcIdx]) {
            logger::debug("[EquipProgDbg] cast src={} state {} -> {} timer={:.3f}",
                srcIdx, s_LastState[srcIdx], static_cast<int>(st), caster->castingTimer);
            if (st == RE::MagicCaster::State::kUnk02)
                s_ChargeDur[srcIdx] = (std::max)(caster->castingTimer, 0.001f);
            s_LastState[srcIdx] = static_cast<int>(st);
        }

        if (st == RE::MagicCaster::State::kNone ||
            st == RE::MagicCaster::State::kUnk01) return -1.0f;
        if (st == RE::MagicCaster::State::kUnk02) {
            const float dur = s_ChargeDur[srcIdx] > 0.001f ? s_ChargeDur[srcIdx] : 1.0f;
            return std::clamp(1.0f - caster->castingTimer / dur, 0.0f, 1.0f);
        }
        return 1.0f;
    }

    static int ResolveWeaponAura(RE::PlayerCharacter* player, RE::TESForm* form, bool isLeftHand)
    {
        if (!form || !form->IsWeapon()) return 0;
        auto* entry = player->GetEquippedEntryData(isLeftHand);
        if (entry && entry->object == form->As<RE::TESBoundObject>() && entry->IsPoisoned()) return 1;
        auto* ench = EquipmentStatResolver::GetWeaponEnchantment(player, form, isLeftHand);
        if (!ench) return 0;
        for (std::uint32_t i = 0; i < ench->effects.size(); ++i) {
            auto* effect = ench->effects[i];
            if (!effect || !effect->baseEffect) continue;
            switch (effect->baseEffect->data.resistVariable) {
            case RE::ActorValue::kResistFire:   return 2;
            case RE::ActorValue::kResistFrost:  return 3;
            case RE::ActorValue::kResistShock:  return 4;
            case RE::ActorValue::kPoisonResist: return 1;
            default: break;
            }
        }
        return 5;
    }

    // PNGII scope: weapons, ammo, torches, shields, spells/scrolls in hand, shouts and powers.
    static bool IsPictogramForm(RE::TESForm* form)
    {
        if (!form) return false;
        if (form->As<RE::TESObjectWEAP>() || form->As<RE::TESAmmo>() || form->As<RE::TESObjectLIGH>() ||
            form->As<RE::SpellItem>() || form->As<RE::TESShout>()) return true;
        auto* arm = form->As<RE::TESObjectARMO>();
        return arm && arm->IsShield();
    }

    // Glyph tint from the slot element, same poison-wins auraType and palette as the aura wash.
    static std::uint32_t SlotElementArgb(const Settings* s, const SlotData& data)
    {
        switch (data.auraType) {
        case 1:  return s->peAuraPoisonColor | 0xFF000000u;
        case 2:  return s->peAuraFireColor   | 0xFF000000u;
        case 3:  return s->peAuraFrostColor  | 0xFF000000u;
        case 4:  return s->peAuraShockColor  | 0xFF000000u;
        case 5:  return s->peAuraOtherColor  | 0xFF000000u;
        default: return 0;
        }
    }

    static void GatherSlots(RE::PlayerCharacter* player, SlotData out[k_SlotCount])
    {
        RE::TESForm* rh = player->GetEquippedObject(false);
        RE::TESForm* lh = player->GetEquippedObject(true);
        if (lh && lh == rh) {
            bool rhIs2H = false;
            if (auto* rw = rh->As<RE::TESObjectWEAP>()) {
                auto wt = rw->GetWeaponType();
                rhIs2H = (wt == RE::WEAPON_TYPE::kTwoHandSword || wt == RE::WEAPON_TYPE::kTwoHandAxe ||
                          wt == RE::WEAPON_TYPE::kBow || wt == RE::WEAPON_TYPE::kCrossbow);
            }
            bool leftOccupied = (player->GetEquippedEntryData(true) != nullptr);
            if (rhIs2H && !leftOccupied) lh = nullptr;
        }

        RE::TESForm* power = player->GetActorRuntimeData().selectedPower;

        RE::TESAmmo* ammoForm  = nullptr;
        bool         isBolt    = false;
        int          ammoCount = GetAmmoInfo(player, rh, ammoForm, isBolt);
        bool         rightIsBow = (ammoCount >= 0);

        for (int i = 0; i < k_SlotCount; ++i) {
            const auto& def = k_Slots[i];
            SlotData&   d   = out[i];
            d = SlotData{};

            switch (def.type) {
            case SlotType::LeftHand:
                if (rightIsBow) {
                    d.form      = ammoForm;
                    d.isEmpty   = (ammoForm == nullptr);
                    d.ammoCount = ammoCount;
                    if (ammoForm) {
                        d.name = ammoForm->GetName();
                        d.icon = TextureManager::GetItemIcon(ammoForm, isBolt ? "type_bolt" : "type_quiver");
                    } else {
                        d.name.clear(); d.icon = nullptr;
                    }
                } else {
                    d.form      = lh;
                    d.isEmpty   = (lh == nullptr);
                    d.ammoCount = -1;
                    if (lh) { d.name = lh->GetName(); d.icon = ResolveIcon(lh, def.fallbackHint); }
                    else    { d.name.clear(); d.icon = nullptr; }
                    d.castCharge    = ResolveCastCharge(player, RE::MagicSystem::CastingSource::kLeftHand, lh);
                    d.enchantCharge = ResolveEnchantCharge(player, lh, false);
                    d.enchantChargeMax = ResolveEnchantChargeMax(player, lh, false);
                    if (lh) {
                        auto stats = EquipmentStatResolver::ResolveHandWeaponStats(player, lh, true);
                        d.weaponDamage  = stats.hitDamage;
                        d.enchantDamage = stats.enchantTotal;
                        d.enchantType   = stats.enchantType;
                        d.auraType      = ResolveWeaponAura(player, lh, true);
                    }
                }
                break;
            case SlotType::RightHand:
                d.form    = rh;
                d.isEmpty = (rh == nullptr);
                if (rh) { d.name = rh->GetName(); d.icon = ResolveIcon(rh, def.fallbackHint); }
                else    { d.name.clear(); d.icon = nullptr; }
                d.castCharge    = ResolveCastCharge(player, RE::MagicSystem::CastingSource::kRightHand, rh);
                d.enchantCharge = ResolveEnchantCharge(player, rh, true);
                d.enchantChargeMax = ResolveEnchantChargeMax(player, rh, true);
                if (rh) {
                    auto stats = EquipmentStatResolver::ResolveHandWeaponStats(player, rh, false);
                    d.weaponDamage  = stats.hitDamage;
                    d.enchantDamage = stats.enchantTotal;
                    d.enchantType   = stats.enchantType;
                    d.auraType      = ResolveWeaponAura(player, rh, false);
                }
                break;
            case SlotType::Shout:
                d.form    = power;
                d.isEmpty = (power == nullptr);
                if (power) {
                    d.name = power->GetName();
                    const char* hint = def.fallbackHint;
                    if (auto* sp = power->As<RE::SpellItem>()) {
                        auto t = sp->GetSpellType();
                        if (t == RE::MagicSystem::SpellType::kPower ||
                            t == RE::MagicSystem::SpellType::kLesserPower)
                            hint = "power";
                    }
                    d.icon = TextureManager::GetItemIcon(power, hint ? hint : "");
                } else {
                    d.name.clear(); d.icon = nullptr;
                }
                if (power && power->As<RE::TESShout>()) {
                    auto* proc = player->GetActorRuntimeData().currentProcess;
                    float remaining = proc ? proc->GetVoiceRecoveryTime() : 0.0f;
                    if (remaining > 0.0f) {
                        if (remaining > g_ShoutCooldownMax)
                            g_ShoutCooldownMax = remaining;
                        d.shoutCooldown = std::clamp(1.0f - (remaining / g_ShoutCooldownMax), 0.0f, 1.0f);
                    } else {
                        g_ShoutCooldownMax = 0.0f;
                        d.shoutCooldown = -1.0f;
                    }
                } else {
                    d.shoutCooldown = -2.0f;
                }
                break;
            }
        }

        const auto* s = Settings::GetSingleton();
        if (s->peIconStyle == 1 && PNGIIBridge::Available()) {
            std::vector<RE::TESForm*> pendingForms;
            for (int i = 0; i < k_SlotCount; ++i) {
                SlotData& d = out[i];
                if (!IsPictogramForm(d.form)) continue;
                std::string rel;
                if (PNGIIBridge::TryGetPath(d.form->GetFormID(), rel)) {
                    if (!rel.empty()) {
                        std::ranges::replace(rel, '/', '\\');
                        d.glyph = TextureManager::GetTexture("Data\\" + rel);  // nullptr on missing/decode failure -> item art
                    }
                } else {
                    pendingForms.push_back(d.form);
                }
            }
            if (!pendingForms.empty()) PNGIIBridge::RequestPaths(std::move(pendingForms));
        }
    }

    static float SlotScale(const Settings* s, SlotType type) {
        float scale = 1.0f;
        switch (type) {
        case SlotType::RightHand: scale = s->peSlotScale0; break;
        case SlotType::LeftHand:  scale = s->peSlotScale1; break;
        case SlotType::Shout:     scale = s->peSlotScale2; break;
        }
        return std::clamp(scale, 0.3f, 2.5f);
    }

    static float SlotWidth(const Settings* s, SlotType type) {
        const float scale = (s->peLayout == 2) ? 1.0f : SlotScale(s, type);
        return ScalePx(static_cast<float>(s->peIconSize) * scale);
    }

    static int SlotShape(const Settings* s, SlotType type) {
        int shape = -1;
        switch (type) {
        case SlotType::RightHand: shape = s->peSlotShape0; break;
        case SlotType::LeftHand:  shape = s->peSlotShape1; break;
        case SlotType::Shout:     shape = s->peSlotShape2; break;
        }
        return std::clamp(shape < 0 ? s->peIconBgShape : shape, 0, 2);
    }

    static ImRect AnchorRect(ImVec2 pos, float w, float h, int shape)
    {
        if (shape != 1 && shape != 2)
            return ImRect(pos, ImVec2(pos.x + w, pos.y + h));
        const float top = pos.y + (h - w) * 0.5f;
        return ImRect(ImVec2(pos.x, top), ImVec2(pos.x + w, top + w));
    }

    static float ShapeHeight(float w, float h, int shape) { return (shape == 1 || shape == 2) ? w : h; }

    static ImVec2 OnCircle(ImVec2 c, float dist, float deg)
    {
        const float a = deg * 3.14159265f / 180.0f;
        return ImVec2(c.x + std::cos(a) * dist, c.y + std::sin(a) * dist);
    }

    static ImVec2 BoxFromCenter(ImVec2 c, float w, float h) { return ImVec2(c.x - w * 0.5f, c.y - h * 0.5f); }

    static void LayoutCluster(const float w[3], const float h[3], float gap, float ang1, float ang2, ImVec2 out[3])
    {
        const ImVec2 c(0.0f, 0.0f);
        out[0] = BoxFromCenter(c, w[0], h[0]);
        out[1] = BoxFromCenter(OnCircle(c, w[0] * 0.5f + w[1] * 0.5f + gap, ang1), w[1], h[1]);
        out[2] = BoxFromCenter(OnCircle(c, w[0] * 0.5f + w[2] * 0.5f + gap, ang2), w[2], h[2]);
    }

    static void LayoutCross(const float w[3], const float h[3], float gap, ImVec2 out[3])
    {
        const float maxR = (std::max)({ w[0], w[1], w[2] }) * 0.5f;
        const float d    = maxR + gap * 0.5f;
        out[0] = BoxFromCenter(ImVec2(0.0f, -d), w[0], h[0]);
        out[1] = BoxFromCenter(ImVec2(-d, 0.0f), w[1], h[1]);
        out[2] = BoxFromCenter(ImVec2(d, 0.0f), w[2], h[2]);
    }

    static void LayoutStack(const float w[3], const float h[3], const float shapeH[3],
                            float overlapPct, float shiftX, float gap, ImVec2 out[3])
    {
        float centerY = 0.0f;
        for (int i = 0; i < 3; ++i) {
            if (i > 0)
                centerY -= (shapeH[i - 1] * 0.5f + shapeH[i] * 0.5f) * (1.0f - overlapPct * 0.01f) + gap;
            out[i] = BoxFromCenter(ImVec2(shiftX * i, centerY), w[i], h[i]);
        }
    }

    static ImVec2 PlaceName(const ImRect& a, ImVec2 textSize, int placement, int align, float gap)
    {
        return HUDFlow::PlaceLabel(a.Min, a.Max, textSize, static_cast<HUDFlow::Side>(placement),
                                   static_cast<HUDFlow::Align>(std::clamp(align, 0, 2)), gap);
    }

    static int NamePlacementFor(const Settings* s)
    {
        const int p = std::clamp(s->peNamePlacement, 0, 3);
        return (s->peLayout == 2 && p >= 2) ? 0 : p;
    }

    static int SidePlacement(int placement, ImVec2 outward)
    {
        const Settings* s = Settings::GetSingleton();
        if (placement < 2 || (s->peLayout != 3 && s->peLayout != 4)) return placement;
        if (std::fabs(outward.x) < 0.5f) return s->peCenterNameSide == 0 ? 2 : 3;
        if (s->peSatelliteNamesBelow) return outward.y < -0.5f ? 1 : 0;
        return outward.x < 0.0f ? 2 : 3;
    }

    static float NameGap() { return ScalePx(static_cast<float>(std::clamp(Settings::GetSingleton()->peNameGap, -20, 60))); }

    static float NameWrapWidth(const Settings* s, float slotW, float pitchW)
    {
        if (NamePlacementFor(s) >= 2)
            return ScalePx(static_cast<float>(std::clamp(s->peNameSideWidth, 40, 240)));
        return (pitchW > 0.0f) ? pitchW : slotW;
    }

    static std::string DisplayName(const SlotData& data, const Settings* s)
    {
        std::string name = data.name;
        const int maxChars = s->peNameMaxChars;
        if (maxChars > 3 && name.size() > static_cast<std::size_t>(maxChars)) {
            name.resize(static_cast<std::size_t>(maxChars) - 3);
            name += "...";
        }
        return name;
    }

    static constexpr std::uint32_t kSymptomRed = WidgetDraw::kSymptomRedArgb;

    using HandMarkList = std::array<const HUDSymptoms::Mark*, 3>;
    static HandMarkList HandMarks(const HUDSymptoms::Equip& e, RE::TESForm* form) {
        if (!form) return {};
        if (auto* arm = form->As<RE::TESObjectARMO>(); arm && arm->IsShield()) return { &e.shield };
        auto* w = form->As<RE::TESObjectWEAP>();
        if (!w) return {};
        using WT = RE::WEAPON_TYPE;
        switch (w->GetWeaponType()) {
        case WT::kOneHandSword: case WT::kOneHandDagger: case WT::kOneHandAxe: case WT::kOneHandMace:
            return { &e.oneHanded, &e.melee, &e.any };
        case WT::kTwoHandSword: case WT::kTwoHandAxe:
            return { &e.twoHanded, &e.melee, &e.any };
        case WT::kBow: case WT::kCrossbow:
            return { &e.ranged, &e.any };
        default:
            return {};
        }
    }

    static bool HandSymptom(const HUDSymptoms::Equip& e, RE::TESForm* form) {
        for (const auto* m : HandMarks(e, form))
            if (m && m->on) return true;
            return false;
        }

    static void AddEffectBadges(std::vector<EffectBadge>& out, const HUDSymptoms::Causes& from, bool buff) {
        EffectBadges::AddFromCauses(out, from, buff);
    }

    static std::uint32_t DamageColor(const SlotData& d) {
        if (d.symptom) return kSymptomRed;
        const bool buffed = std::any_of(d.effects.begin(), d.effects.end(), [](const EffectBadge& e) { return e.buff; });
        return buffed ? EffectBadges::kBuffGreen : 0xFFFFEBA0;
    }

    static int DamageTrend(const SlotData& d) {
        if (d.symptom) return -1;
        const bool buffed = std::any_of(d.effects.begin(), d.effects.end(), [](const EffectBadge& e) { return e.buff; });
        return buffed ? 1 : 0;
    }

    static EffectBadges::ShownGroups s_shownBadges;

    bool ShowsAsBadge(const EffectAttribution::GroupKey& key) {
        return s_shownBadges.Contains(key);
    }

    static void DrawEffectBadges(ImDrawList* dl, const ImRect& anchor, const ImRect& nameRect, const SlotData& data) {
        const auto* s = Settings::GetSingleton();
        if (!s->peShowEffectBadges || data.effects.empty()) return;

        EffectBadges::RowStyle style;
        style.preset    = s->peEffectBadgeStyle;
        style.size      = (std::max)(anchor.GetWidth() * s->peEffectBadgeSize * 0.01f, ScalePx(12.0f));
        style.max       = s->peEffectBadgeMax;
        style.fontScale = s->peBadgeFontScale;
        style.gap       = ScalePx(2.0f);
        const auto row  = EffectBadges::Measure(data.effects, style);

        const bool  onLabel = s->peEffectBadgePlace == 1 && nameRect.Min.x < nameRect.Max.x;
        const ImRect host   = onLabel ? nameRect : anchor;
        float x = host.GetCenter().x - row.total.x * 0.5f;
        if (onLabel && s->peNameAlignment == 0) x = host.Min.x;
        if (onLabel && s->peNameAlignment == 2) x = host.Max.x - row.total.x;
        const float top = host.Min.y - style.gap - row.total.y;

        EffectBadges::Draw(dl, ImVec2(x, top), data.effects, row, style,
            [](const EffectBadge& e) { s_shownBadges.Remember(e.key); });
    }

    static float DrawSlot(
        ImDrawList*      dl,
        ImVec2           screenPos,
        const SlotData&  data,
        const SlotDef&   def,
        const Settings*  s,
        float            nameMaxWidth = 0.0f,
        float            slotW        = -1.0f,
        ImVec2           outward      = ImVec2(-1.0f, 0.0f))
    {
        const float w  = (slotW > 0.0f) ? slotW : ScalePx(static_cast<float>(s->peIconSize));
        const float h  = w * (200.0f / 144.0f);
        const float ov = ScalePx(static_cast<float>(s->peIconBgPadding)); // overflow px: icon extends beyond bg by this amount per side

        float deniedT = -1.0f;
        if (def.type == SlotType::Shout && g_ShoutDeniedAt.time_since_epoch().count() != 0)
            deniedT = std::chrono::duration<float>(std::chrono::steady_clock::now() - g_ShoutDeniedAt).count();
        const bool shaking = deniedT >= 0.0f && deniedT < k_ShoutDeniedDur;
        if (shaking)
            screenPos.x += UIAnimation::GetShakeOffsetX("peq_shout_denied") * MasterScale();
        HUDDraw::ScopedTextNoSnap textGlide(dl, shaking);
        const int    shape  = SlotShape(s, def.type);
        const ImRect anchor = AnchorRect(screenPos, w, h, shape);

        float       contentBottom = screenPos.y + h;

        if (s->peShowNames && s->peStripStyle > 0 && !data.isEmpty && !data.name.empty()) {
            const int placement = SidePlacement(NamePlacementFor(s), outward);
            const std::string displayName = DisplayName(data, s);
            const float  wrapW    = NameWrapWidth(s, w, nameMaxWidth);
            const ImVec2 textSize = ImGui::CalcTextSize(displayName.c_str(), nullptr, false, wrapW);
            const ImVec2 namePos  = PlaceName(anchor, textSize, placement, s->peNameAlignment, NameGap());
            if (placement < 2) {
                const float pad   = ScalePx(8.0f);
                const float half  = (std::max)(textSize.x * 0.5f + pad, ScalePx(static_cast<float>(s->peStripWidth)) * 0.5f);
                const float midX  = namePos.x + textSize.x * 0.5f;
                const float midY  = namePos.y + textSize.y * 0.5f;
                const float halfH = textSize.y * 0.5f + ScalePx(3.0f);
                if (s->peStripStyle == HUDDraw::StripStyle_Rounded) {
                    HUDDraw::DrawStrip(dl, ImVec2(midX - half, midY - halfH), ImVec2(midX + half, midY + halfH),
                                       s->peStripStyle, s->peStripTint, true);
                } else {
                    HUDDraw::DrawStrip(dl, ImVec2(midX - half, midY - halfH), ImVec2(midX, midY + halfH),
                                       s->peStripStyle, s->peStripTint, false);
                    HUDDraw::DrawStrip(dl, ImVec2(midX, midY - halfH), ImVec2(midX + half, midY + halfH),
                                       s->peStripStyle, s->peStripTint, true);
                }
            } else {
                const bool  toRight = placement == 3;
                const float pad     = ScalePx(6.0f);
                const float reach   = (std::max)(textSize.x + NameGap() + pad * 2.0f, ScalePx(static_cast<float>(s->peStripWidth)));
                const float midY    = namePos.y + textSize.y * 0.5f;
                const float halfH   = textSize.y * 0.5f + ScalePx(3.0f);
                float tuck = ScalePx(2.0f);
                if (shape == 2) {
                    const float halfD = anchor.GetWidth() * 0.5f;
                    const float dy    = std::fabs(midY - anchor.GetCenter().y) + halfH;
                    tuck += halfD * (std::min)(1.0f, dy / halfD);
                }
                const float edge    = toRight ? anchor.Max.x - tuck : anchor.Min.x + tuck;
                const float span    = reach + tuck;
                HUDDraw::DrawStrip(dl,
                    ImVec2(toRight ? edge : edge - span, midY - halfH),
                    ImVec2(toRight ? edge + span : edge, midY + halfH),
                    s->peStripStyle, s->peStripTint, toRight);
            }
        }

        static ID3D11ShaderResourceView* s_GlowTex = nullptr;
        static bool s_GlowTried = false;
        if (!s_GlowTried) {
            s_GlowTried = true;
            s_GlowTex = TextureManager::GetTexture(
                "Data/Interface/PartySheet/Backgrounds/glow.png");
        }

        if (s->peShowIcons) {
            if (s->peIconBgEnabled || s->peIconBorderWidth > 0 || s->peUseIconBgTexture) {
                ImU32 borderCol = ImGui::ColorConvertFloat4ToU32(ColorUtils::ArgbToVec4(s->peIconBorderColor));
                float bw        = ScalePx(static_cast<float>(s->peIconBorderWidth));
                float radius    = w * s->peIconBorderRadius * 0.01f;
                ImVec2 tl = screenPos;
                ImVec2 br = ImVec2(screenPos.x + w, screenPos.y + h);
                ImVec2 center = ImVec2(screenPos.x + w * 0.5f, screenPos.y + h * 0.5f);

                ID3D11ShaderResourceView* bgTex = nullptr;
                if (s->peUseIconBgTexture)
                    bgTex = BackgroundManager::GetBackground(BackgroundManager::Type::Inspectcard, s->peIconBgIndex);
                const ImU32 bgTexTint = IM_COL32(255, 255, 255,
                    std::clamp(static_cast<int>(s->peIconBgTexOpacity * 255.0f + 0.5f), 0, 255));

                auto DrawFadeFill = [&]() {
                    HUDDraw::FadeBgStyle fs;
                    fs.mode      = static_cast<HUDDraw::FadeBgMode>(s->peIconBgFadeMode);
                    fs.color     = s->peIconBgColor;
                    fs.solidFrac = s->peIconBgFadeAnchor / 100.0f;
                    fs.fadeFrac  = s->peIconBgFadeLength / 100.0f;
                    HUDDraw::DrawFadeBg(dl, tl, br, fs);
                };

                auto DrawDefaultBg = [&](int bgShape) {
                    if (bgShape == 0 && s->peIconBgStyle == 1) {
                        DrawFadeFill();
                        return;
                    }
                    const std::uint32_t c  = s->peIconBgColor;
                    const std::uint8_t  cA = (c >> 24) & 0xFF, cR = (c >> 16) & 0xFF,
                                        cG = (c >> 8) & 0xFF,  cB = c & 0xFF;
                    auto lift = [](std::uint8_t v) { return static_cast<std::uint8_t>((std::min)(255.0f, v * 1.18f + 14.0f)); };
                    auto deep = [](std::uint8_t v) { return static_cast<std::uint8_t>(v * 0.50f); };
                    const ImU32 litCol  = IM_COL32(lift(cR), lift(cG), lift(cB), 255);
                    const ImU32 deepCol = IM_COL32(deep(cR), deep(cG), deep(cB), 255);
                    const float r = w * 0.5f;

                    auto FillShape = [&](ImVec2 off, ImU32 col, float grow) {
                        switch (bgShape) {
                        default:
                        case 0:
                            dl->AddRectFilled(ImVec2(tl.x - grow + off.x, tl.y - grow + off.y),
                                              ImVec2(br.x + grow + off.x, br.y + grow + off.y),
                                              col, radius + grow);
                            break;
                        case 1:
                            dl->AddCircleFilled(ImVec2(center.x + off.x, center.y + off.y), r + grow, col);
                            break;
                        case 2: {
                            ImVec2 pts[4] = {
                                ImVec2(center.x + off.x,            center.y - r - grow + off.y),
                                ImVec2(center.x + r + grow + off.x, center.y + off.y),
                                ImVec2(center.x + off.x,            center.y + r + grow + off.y),
                                ImVec2(center.x - r - grow + off.x, center.y + off.y),
                            };
                            dl->AddConvexPolyFilled(pts, 4, col);
                            break;
                        }
                        }
                    };

                    for (int i = 2; i >= 1; --i)
                        FillShape(ImVec2(1.0f, 2.0f), IM_COL32(0, 0, 0, 30), static_cast<float>(i));

                    const float topY = (bgShape == 0) ? tl.y : center.y - r;
                    const float botY = (bgShape == 0) ? br.y : center.y + r;
                    int v0 = dl->VtxBuffer.Size;
                    FillShape(ImVec2(0, 0), IM_COL32(255, 255, 255, cA), 0.0f);
                    int v1 = dl->VtxBuffer.Size;
                    ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, v0, v1,
                        ImVec2(tl.x, topY), ImVec2(tl.x, botY), litCol, deepCol);

                    const float hlY = topY + (bgShape == 2 ? r * 0.35f : ScalePx(1.0f));
                    dl->PushClipRect(ImVec2(tl.x - ScalePx(1.0f), hlY), ImVec2(br.x + ScalePx(1.0f), hlY + ScalePx(1.5f)), true);
                    FillShape(ImVec2(0, 0), IM_COL32(255, 255, 255, 30), 0.0f);
                    dl->PopClipRect();
                };

                switch (shape) {
                default:
                case 0:
                    if (bgTex) dl->AddImageRounded(reinterpret_cast<ImTextureID>(bgTex), tl, br,
                                                   ImVec2(0, 0), ImVec2(1, 1), bgTexTint, radius);
                    else if (s->peIconBgEnabled) DrawDefaultBg(0);
                    if (bw > 0) dl->AddRect(tl, br, borderCol, radius, 0, bw);
                    break;
                case 1: {
                    float r = w * 0.5f;
                    if (bgTex) {
                        ImVec2 cMin(center.x - r, center.y - r);
                        ImVec2 cMax(center.x + r, center.y + r);
                        dl->AddImageRounded(reinterpret_cast<ImTextureID>(bgTex), cMin, cMax,
                                            ImVec2(0, 0), ImVec2(1, 1), bgTexTint, r);
                    } else if (s->peIconBgEnabled) {
                        DrawDefaultBg(1);
                    }
                    if (bw > 0) dl->AddCircle(center, r, borderCol, 0, bw);
                    break;
                }
                case 2: {
                    float r = w * 0.5f;
                    ImVec2 pts[4] = {
                        ImVec2(center.x,     center.y - r),
                        ImVec2(center.x + r, center.y    ),
                        ImVec2(center.x,     center.y + r),
                        ImVec2(center.x - r, center.y    ),
                    };
                    if (bgTex) {
                        dl->AddImageQuad(reinterpret_cast<ImTextureID>(bgTex),
                            pts[0], pts[1], pts[2], pts[3],
                            ImVec2(0, 0), ImVec2(1, 0), ImVec2(1, 1), ImVec2(0, 1), bgTexTint);
                    } else if (s->peIconBgEnabled) {
                        DrawDefaultBg(2);
                    }
                    if (bw > 0) dl->AddPolyline(pts, 4, borderCol, ImDrawFlags_Closed, bw);
                    break;
                }
                }
            }

            if (false && s->peShowAura && data.auraType > 0) {
                std::uint32_t auraArgb;
                switch (std::clamp(data.auraType, 1, 5)) {
                case 1: auraArgb = s->peAuraPoisonColor; break;
                case 2: auraArgb = s->peAuraFireColor;   break;
                case 3: auraArgb = s->peAuraFrostColor;  break;
                case 4: auraArgb = s->peAuraShockColor;  break;
                default: auraArgb = s->peAuraOtherColor; break;
                }
                const int   aR = (auraArgb >> 16) & 0xFF, aG = (auraArgb >> 8) & 0xFF, aB = auraArgb & 0xFF;
                const float aA = ((auraArgb >> 24) & 0xFF) / 255.0f;
                const float pulse = UIAnimation::Pulse(2.4f, 0.44f, 1.0f);
                const int   alphas[]  = { 170, 70, 30 };
                const float expands[] = { 0.0f, 2.0f, 4.0f };
                const float radius = w * s->peIconBorderRadius * 0.01f;
                const ImVec2 tl(screenPos.x, screenPos.y);
                const ImVec2 br(screenPos.x + w, screenPos.y + h);
                const ImVec2 center(screenPos.x + w * 0.5f, screenPos.y + h * 0.5f);
                for (int p = 0; p < 3; ++p) {
                    const float e = ScalePx(expands[p]);
                    const ImU32 col = IM_COL32(aR, aG, aB, static_cast<int>(alphas[p] * pulse * aA));
                    switch (shape) {
                    default:
                    case 0:
                        dl->AddRect(ImVec2(tl.x - e, tl.y - e), ImVec2(br.x + e, br.y + e),
                                    col, radius + e, 0, ScalePx(1.6f));
                        break;
                    case 1:
                        dl->AddCircle(center, w * 0.5f + e, col, 0, ScalePx(1.6f));
                        break;
                    case 2: {
                        const float dr = w * 0.5f + e;
                        ImVec2 pts[4] = { ImVec2(center.x, center.y - dr), ImVec2(center.x + dr, center.y),
                                          ImVec2(center.x, center.y + dr), ImVec2(center.x - dr, center.y) };
                        dl->AddPolyline(pts, 4, col, ImDrawFlags_Closed, ScalePx(1.6f));
                        break;
                    }
                    }
                }
            }

            if (s_GlowTex) {
                ImVec2 center = ImVec2(screenPos.x + w * 0.5f, screenPos.y + h * 0.5f);
                constexpr struct { float scale; float maxAlpha; } kPasses[] = {
                    { 3.2f, 200.0f },
                    { 1.8f, 255.0f },
                };

                auto DrawGlow = [&](ImU32 col, float aMult = 1.0f) {
                    for (auto& p : kPasses) {
                        float halfW = (w * p.scale) * 0.5f;
                        float halfH = (h * p.scale) * 0.5f;
                        ImU32 tinted = IM_COL32(
                            (col >> IM_COL32_R_SHIFT) & 0xFF,
                            (col >> IM_COL32_G_SHIFT) & 0xFF,
                            (col >> IM_COL32_B_SHIFT) & 0xFF,
                            static_cast<int>(p.maxAlpha * aMult));
                        dl->AddImage(
                            reinterpret_cast<ImTextureID>(s_GlowTex),
                            ImVec2(center.x - halfW, center.y - halfH),
                            ImVec2(center.x + halfW, center.y + halfH),
                            ImVec2(0, 0), ImVec2(1, 1), tinted);
                    }
                };

                if (s->peShowAura && data.auraType > 0) {
                    std::uint32_t auraArgb;
                    switch (std::clamp(data.auraType, 1, 5)) {
                    case 1: auraArgb = s->peAuraPoisonColor; break;
                    case 2: auraArgb = s->peAuraFireColor;   break;
                    case 3: auraArgb = s->peAuraFrostColor;  break;
                    case 4: auraArgb = s->peAuraShockColor;  break;
                    default: auraArgb = s->peAuraOtherColor; break;
                    }
                    const int   aR = (auraArgb >> 16) & 0xFF, aG = (auraArgb >> 8) & 0xFF, aB = auraArgb & 0xFF;
                    const float aA = ((auraArgb >> 24) & 0xFF) / 255.0f;
                    const float pulse = UIAnimation::Pulse(2.4f, 0.44f, 1.0f);
                    DrawGlow(IM_COL32(aR, aG, aB, 255), 0.30f * pulse * aA);
                }

                if (data.castCharge >= 1.0f) {
                    DrawGlow(IM_COL32(140, 185, 255, 255));
                }
                if (def.type == SlotType::Shout && data.shoutCooldown == -1.0f && data.form) {
                    DrawGlow(IM_COL32(255, 200, 80, 255));
                }
            }

            // Icon image: zoom scales draw size beyond bg bounds, centered on slot.
            if (data.glyph) {
                // Pictogram: square, centered, breathing room from peGlyphScale. Zoom, focus and
                const float side = std::min(w, h) * std::clamp(s->peGlyphScale, 0.40f, 1.00f);
                const ImVec2 gMin(screenPos.x + (w - side) * 0.5f, screenPos.y + (h - side) * 0.5f);
                std::uint32_t argb = s->peGlyphColor;
                if (s->peGlyphTintByElement) {
                    if (const auto el = SlotElementArgb(s, data)) argb = el;
                }
                HUDDraw::IconFx fx;
                fx.duotone = s->peSpellIconDuotone;
                fx.shadow  = s->peSpellIconShadow;
                fx.outline = s->peSpellIconOutline;
                fx.outlineWidth = std::max(1, static_cast<int>(std::round(ScalePx(static_cast<float>(s->peSpellIconOutlineWidth)))));
                fx.glow    = false;
                fx.gloss   = false;
                HUDDraw::DrawIconStyled(
                    dl,
                    gMin,
                    ImVec2(side, side),
                    reinterpret_cast<ImTextureID>(data.glyph),
                    argb,
                    fx,
                    ImVec2(0.0f, 0.0f),
                    ImVec2(1.0f, 1.0f));
            } else if (data.icon) {
                float baseW = std::max(1.0f, w - ov * 2.0f);
                float baseH = std::max(1.0f, h - ov * 2.0f);
                bool  isSpell   = data.form && (data.form->As<RE::SpellItem>() || data.form->As<RE::TESShout>());
                float fullZoom  = (std::max)(s->peIconZoom, 1.0f);
                float zoom      = isSpell ? (1.0f + (fullZoom - 1.0f) * 0.5f) : fullZoom;
                if (isSpell) baseW = baseH = std::min(baseW, baseH);
                float imgW  = baseW * zoom;
                float imgH  = baseH * zoom;

                float anchorX = screenPos.x + w * s->peIconFocusU;
                float anchorY = screenPos.y + h * s->peIconFocusV;
                ImVec2 imgMin = ImVec2(anchorX - imgW * s->peIconFocusU,
                                       anchorY - imgH * s->peIconFocusV);
                ImVec2 imgMax = ImVec2(imgMin.x + imgW, imgMin.y + imgH);

                bool isWeapon = data.form && data.form->As<RE::TESObjectWEAP>();
                bool isSpellIcon = data.form &&
                    (data.form->As<RE::SpellItem>() || data.form->As<RE::TESShout>());
                float angle = isWeapon ? s->peWeaponRotation : 0.0f;

                if (angle != 0.0f) {
                    float cx = (imgMin.x + imgMax.x) * 0.5f;
                    float cy = (imgMin.y + imgMax.y) * 0.5f;
                    float cosA = std::cos(angle);
                    float sinA = std::sin(angle);
                    auto rot = [&](float px, float py) -> ImVec2 {
                        float dx = px - cx, dy = py - cy;
                        return ImVec2(cx + dx * cosA - dy * sinA,
                                      cy + dx * sinA + dy * cosA);
                    };
                    dl->AddImageQuad(
                        reinterpret_cast<ImTextureID>(data.icon),
                        rot(imgMin.x, imgMin.y),
                        rot(imgMax.x, imgMin.y),
                        rot(imgMax.x, imgMax.y),
                        rot(imgMin.x, imgMax.y),
                        ImVec2(0, 0), ImVec2(1, 0), ImVec2(1, 1), ImVec2(0, 1));
                } else {
                    if (isSpellIcon &&
                        (s->peSpellIconDuotone || s->peSpellIconShadow || s->peSpellIconOutline)) {
                        HUDDraw::IconFx fx;
                        fx.duotone = s->peSpellIconDuotone;
                        fx.shadow  = s->peSpellIconShadow;
                        fx.outline = s->peSpellIconOutline;
                        fx.outlineWidth = std::max(1, static_cast<int>(std::round(ScalePx(static_cast<float>(s->peSpellIconOutlineWidth)))));
                        fx.glow    = false;
                        fx.gloss   = false;
                        HUDDraw::DrawIconStyled(
                            dl,
                            imgMin,
                            ImVec2(imgW, imgH),
                            reinterpret_cast<ImTextureID>(data.icon),
                            0xFFFFFFFF,
                            fx,
                            ImVec2(0.0f, 0.0f),
                            ImVec2(1.0f, 1.0f));
                    } else {
                        dl->AddImage(reinterpret_cast<ImTextureID>(data.icon), imgMin, imgMax,
                            ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
                    }
                }
            }
        }

        auto DrawRoundedRectProgress = [&](float progress, ImU32 col, float thick, float radius) {
            radius = std::clamp(radius, 0.0f, std::min(w, h) * 0.5f);
            // 8 segments: 4 straight edges + 4 quarter-arcs.
            const float straightX = w - 2.0f * radius;
            const float straightY = h - 2.0f * radius;
            const float arcLen    = (radius > 0.0f) ? (3.14159265f * 0.5f * radius) : 0.0f;
            const float perim     = 2.0f * (straightX + straightY) + 4.0f * arcLen;
            float remaining       = std::clamp(progress, 0.0f, 1.0f) * perim;

            const float L = screenPos.x;
            const float R = screenPos.x + w;
            const float T = screenPos.y;
            const float B = screenPos.y + h;
            const float HALF_PI = 3.14159265f * 0.5f;

            auto drawArc = [&](ImVec2 c, float a0, float a1, float frac) {
                const int segs = std::max(2, (int)(8 * frac));
                dl->PathClear();
                dl->PathArcTo(c, radius, a0, a0 + (a1 - a0) * frac, segs);
                dl->PathStroke(col, 0, thick);
            };

            if (remaining > 0.0f && straightX > 0.0f) {
                float t = std::min(remaining, straightX) / straightX;
                dl->AddLine(ImVec2(L + radius,            T),
                            ImVec2(L + radius + straightX * t, T), col, thick);
                remaining -= straightX * t;
            }
            if (remaining > 0.0f && arcLen > 0.0f) {
                float t = std::min(remaining, arcLen) / arcLen;
                drawArc(ImVec2(R - radius, T + radius), -HALF_PI, 0.0f, t);
                remaining -= arcLen * t;
            }
            if (remaining > 0.0f && straightY > 0.0f) {
                float t = std::min(remaining, straightY) / straightY;
                dl->AddLine(ImVec2(R, T + radius),
                            ImVec2(R, T + radius + straightY * t), col, thick);
                remaining -= straightY * t;
            }
            if (remaining > 0.0f && arcLen > 0.0f) {
                float t = std::min(remaining, arcLen) / arcLen;
                drawArc(ImVec2(R - radius, B - radius), 0.0f, HALF_PI, t);
                remaining -= arcLen * t;
            }
            if (remaining > 0.0f && straightX > 0.0f) {
                float t = std::min(remaining, straightX) / straightX;
                dl->AddLine(ImVec2(R - radius, B),
                            ImVec2(R - radius - straightX * t, B), col, thick);
                remaining -= straightX * t;
            }
            if (remaining > 0.0f && arcLen > 0.0f) {
                float t = std::min(remaining, arcLen) / arcLen;
                drawArc(ImVec2(L + radius, B - radius), HALF_PI, 3.14159265f, t);
                remaining -= arcLen * t;
            }
            if (remaining > 0.0f && straightY > 0.0f) {
                float t = std::min(remaining, straightY) / straightY;
                dl->AddLine(ImVec2(L, B - radius),
                            ImVec2(L, B - radius - straightY * t), col, thick);
                remaining -= straightY * t;
            }
            if (remaining > 0.0f && arcLen > 0.0f) {
                float t = std::min(remaining, arcLen) / arcLen;
                drawArc(ImVec2(L + radius, T + radius), 3.14159265f, 3.14159265f + HALF_PI, t);
            }
        };

        auto DrawProgressFrame = [&](float progress, ImU32 col) {
            const float cw = ScalePx(2.5f);
            const ImU32 trackCol = IM_COL32(8, 7, 10, 170);
            ImVec2 center = ImVec2(screenPos.x + w * 0.5f, screenPos.y + h * 0.5f);

            switch (shape) {
            default:
            case 0: {
                float radius = w * s->peIconBorderRadius * 0.01f;
                dl->AddRect(screenPos, ImVec2(screenPos.x + w, screenPos.y + h), trackCol,
                            std::clamp(radius, 0.0f, std::min(w, h) * 0.5f), 0, cw + ScalePx(2.0f));
                DrawRoundedRectProgress(progress, col, cw, radius);
                break;
            }
            case 1: {
                const float r          = w * 0.5f;
                const float angleStart = -3.14159265f * 0.5f;
                const float angleEnd   = angleStart + 3.14159265f * 2.0f * progress;
                const int   numSeg     = (int)(32 * progress) + 1;
                dl->AddCircle(center, r, trackCol, 0, cw + ScalePx(2.0f));
                dl->PathArcTo(center, r, angleStart, angleEnd, numSeg);
                dl->PathStroke(col, 0, cw);
                break;
            }
            case 2: {
                const float r = w * 0.5f;
                ImVec2 pts[5] = {
                    ImVec2(center.x,     center.y - r),
                    ImVec2(center.x + r, center.y    ),
                    ImVec2(center.x,     center.y + r),
                    ImVec2(center.x - r, center.y    ),
                    ImVec2(center.x,     center.y - r),
                };
                dl->AddPolyline(pts, 4, trackCol, ImDrawFlags_Closed, cw + ScalePx(2.0f));
                const float edgeLen = r * std::sqrt(2.0f) * 2.0f;
                const float perim   = edgeLen * 4.0f;
                float remaining = progress * perim;
                for (int seg = 0; seg < 4 && remaining > 0.0f; ++seg) {
                    float t = std::min(remaining, edgeLen) / edgeLen;
                    ImVec2 segEnd = ImVec2(
                        pts[seg].x + (pts[seg + 1].x - pts[seg].x) * t,
                        pts[seg].y + (pts[seg + 1].y - pts[seg].y) * t);
                    dl->AddLine(pts[seg], segEnd, col, cw);
                    remaining -= edgeLen * t;
                }
                break;
            }
            }
        };

        auto DrawCastSweep = [&](float progress, bool showEdge = true) {
            progress = std::clamp(progress, 0.0f, 1.0f);
            if (progress >= 1.0f) return;
            const ImVec2 rectMin = screenPos;
            const ImVec2 rectMax = ImVec2(screenPos.x + w, screenPos.y + h);
            const ImVec2 center  = ImVec2(screenPos.x + w * 0.5f, screenPos.y + h * 0.5f);
            const float HALF_PI  = 1.57079633f;
            const float start    = -HALF_PI + 6.28318531f * progress;
            const float end      = -HALF_PI + 6.28318531f;
            const float R = (shape == 1) ? w * 0.5f
                                         : std::sqrt(w * w + h * h) * 0.5f + 1.0f;

            auto clipHalfPlane = [](std::vector<ImVec2>& poly, ImVec2 a, ImVec2 n) {
                std::vector<ImVec2> out;
                out.reserve(poly.size() + 2);
                const std::size_t count = poly.size();
                for (std::size_t i = 0; i < count; ++i) {
                    const ImVec2 p = poly[i];
                    const ImVec2 q = poly[(i + 1) % count];
                    const float dp = (p.x - a.x) * n.x + (p.y - a.y) * n.y;
                    const float dq = (q.x - a.x) * n.x + (q.y - a.y) * n.y;
                    if (dp <= 0.0f) out.push_back(p);
                    if ((dp < 0.0f) != (dq < 0.0f) && dp != dq) {
                        const float t = dp / (dp - dq);
                        out.push_back(ImVec2(p.x + (q.x - p.x) * t, p.y + (q.y - p.y) * t));
                    }
                }
                poly = std::move(out);
            };

            if (shape == 0) dl->PushClipRect(rectMin, rectMax, true);

            const ImU32 shade = IM_COL32(0, 0, 0, 150);
            float a = start;
            while (a < end - 0.0001f) {
                const float b    = std::min(a + HALF_PI, end);
                const int   segs = std::max(3, (int)(12.0f * (b - a) / HALF_PI));
                if (shape == 2) {
                    std::vector<ImVec2> poly;
                    poly.reserve(segs + 2);
                    poly.push_back(center);
                    for (int i = 0; i <= segs; ++i) {
                        const float ang = a + (b - a) * (float)i / (float)segs;
                        poly.push_back(ImVec2(center.x + std::cos(ang) * R,
                                              center.y + std::sin(ang) * R));
                    }
                    const float r = w * 0.5f;
                    clipHalfPlane(poly, ImVec2(center.x, center.y - r), ImVec2( 1.0f, -1.0f));
                    clipHalfPlane(poly, ImVec2(center.x, center.y - r), ImVec2(-1.0f, -1.0f));
                    clipHalfPlane(poly, ImVec2(center.x, center.y + r), ImVec2( 1.0f,  1.0f));
                    clipHalfPlane(poly, ImVec2(center.x, center.y + r), ImVec2(-1.0f,  1.0f));
                    if (poly.size() >= 3)
                        dl->AddConvexPolyFilled(poly.data(), (int)poly.size(), shade);
                } else {
                    dl->PathClear();
                    dl->PathLineTo(center);
                    dl->PathArcTo(center, R, a, b, segs);
                    dl->PathFillConvex(shade);
                }
                a = b;
            }

            if (showEdge) {
                const float ca = std::cos(start), sa = std::sin(start);
                float edgeLen = R;
                if (shape == 2)
                    edgeLen = (w * 0.5f) / std::max(std::fabs(ca) + std::fabs(sa), 0.0001f);
                dl->AddLine(center,
                            ImVec2(center.x + ca * edgeLen, center.y + sa * edgeLen),
                            IM_COL32(100, 160, 255, 230), ScalePx(2.0f));
            }

            if (shape == 0) dl->PopClipRect();
        };

        if (data.shoutCooldown >= 0.0f && data.castCharge < 0.0f)
            DrawCastSweep(data.shoutCooldown, false);
        (void)DrawProgressFrame;

        if (float k = UIAnimation::AgeFade(deniedT, 0.0f, 0.0f, k_ShoutDeniedDur); k > 0.0f) {
            k *= k;
            const ImU32  red = IM_COL32(225, 45, 35, static_cast<int>(150.0f * k));
            const ImVec2 br(screenPos.x + w, screenPos.y + h);
            const ImVec2 center(screenPos.x + w * 0.5f, screenPos.y + h * 0.5f);
            switch (shape) {
            default:
            case 0:
                dl->AddRectFilled(screenPos, br, red, w * s->peIconBorderRadius * 0.01f);
                break;
            case 1:
                dl->AddCircleFilled(center, w * 0.5f, red);
                break;
            case 2: {
                const float dr = w * 0.5f;
                ImVec2 quad[4] = { ImVec2(center.x, center.y - dr), ImVec2(center.x + dr, center.y),
                                   ImVec2(center.x, center.y + dr), ImVec2(center.x - dr, center.y) };
                dl->AddConvexPolyFilled(quad, 4, red);
                break;
            }
            }
        }

        if (data.castCharge >= 0.0f)
            DrawCastSweep(data.castCharge);

        if (s->peShowSlotBadge) {
            std::string localizedBadge;
            switch (def.type) {
                case SlotType::LeftHand:  localizedBadge = LocalizationManager::Get("Equipment.BadgeLeft");  break;
                case SlotType::RightHand: localizedBadge = LocalizationManager::Get("Equipment.BadgeRight"); break;
                case SlotType::Shout:     localizedBadge = LocalizationManager::Get("Equipment.BadgeShout"); break;
            }
            if (localizedBadge == "Equipment.BadgeLeft"  ||
                localizedBadge == "Equipment.BadgeRight" ||
                localizedBadge == "Equipment.BadgeShout")
                localizedBadge = def.badge;
            const HUDDraw::BadgeStyle badgeStyle = MakeBadgeStyle(0xDCC8C8C8);
            const ImVec2 bsz = HUDDraw::MeasureBadge(localizedBadge.c_str(), badgeStyle);
            const float  ox  = ScalePx(static_cast<float>(s->peBadgeOffsetX));
            const float  oy  = ScalePx(static_cast<float>(s->peBadgeOffsetY));
            using HUDFlow::Spot;
            const ImVec2 straddle(0.5f, 0.5f), inside(0.0f, 0.0f);
            auto at = [&](Spot spot, ImVec2 over, float dx, float dy) {
                const ImVec2 p = HUDFlow::PlaceBadge(anchor.Min, anchor.Max, bsz, spot, over);
                return ImVec2(p.x + dx, p.y + dy);
            };
            const ImVec2 topCenter    = at(Spot::Top, straddle, 0.0f, oy);
            const ImVec2 bottomCenter = at(Spot::Bottom, straddle, 0.0f, -oy);
            ImVec2 badgePos;
            switch (s->peBadgeAnchor) {
            case 1:  badgePos = topCenter; break;
            case 2:  badgePos = bottomCenter; break;
            case 3:
            case 4:  badgePos = at(Spot::Left, straddle, ox, 0.0f); break;
            case 5:  badgePos = at(Spot::Right, straddle, -ox, 0.0f); break;
            default: badgePos = at(Spot::TopLeft, inside, ScalePx(2.0f) + ox, ScalePx(2.0f) + oy); break;
            }
            HUDDraw::DrawBadge(dl, badgePos, 0, localizedBadge.c_str(), badgeStyle);
        }

        if (data.enchantCharge >= 0.0f && s->peChargeStyle != 2) {
            const bool barStyle = (s->peChargeStyle == 1) && (data.enchantChargeMax > 0.0f);
            if (barStyle) {
                // Vertical charge bar, full HUDDraw::DrawTimerBarV layer stack (recessed
                const float barW = ScalePx(static_cast<float>(s->peChargeBarWidth));
                const float barH = h * s->peChargeBarHeight;

                HUDDraw::TimerBarStyle tb;
                tb.width         = barW;
                tb.height        = barH;
                tb.fraction      = std::clamp(data.enchantCharge / data.enchantChargeMax, 0.0f, 1.0f);
                tb.fillColor     = 0xFF50AAFF;
                tb.edgeLightSize = barH * 0.4f;
                HUDDraw::DrawTimerBarV(dl,
                    ImVec2(screenPos.x + w - barW - ScalePx(3.0f + s->peBadgeOffsetX),
                           anchor.Min.y + ScalePx(3.0f + s->peBadgeOffsetY)), tb);
            } else {
                char chargeStr[16];
                if (data.enchantChargeMax > 0.0f) {
                    const float pct = std::clamp(data.enchantCharge / data.enchantChargeMax, 0.0f, 1.0f) * 100.0f;
                    snprintf(chargeStr, sizeof(chargeStr), "%.0f%%", pct);
                } else {
                    snprintf(chargeStr, sizeof(chargeStr), "%.0f", data.enchantCharge);
                }
                HUDDraw::DrawBadge(dl,
                    ImVec2(screenPos.x + w - ScalePx(2.0f + s->peBadgeOffsetX), anchor.Min.y + ScalePx(2.0f + s->peBadgeOffsetY)),
                    1, chargeStr, MakeBadgeStyle(0xFF64C8FF));
            }
        }

        {
            const ImVec2 blAnchor(screenPos.x + ScalePx(2.0f + s->peBadgeOffsetX),
                                  anchor.Max.y - ScalePx(2.0f + s->peBadgeOffsetY));
            if (s->peShowDamage && data.weaponDamage >= 0.0f && data.ammoCount < 0) {
                char damageStr[16];
                snprintf(damageStr, sizeof(damageStr), "%.0f", data.weaponDamage + data.enchantDamage);
                auto damageStyle = MakeBadgeStyle(DamageColor(data));
                damageStyle.trend = DamageTrend(data);
                HUDDraw::DrawBadge(dl, blAnchor, 3, damageStr, damageStyle);
            }
        }

        if (s->peShowDurability && data.durabilityPct >= 0.0f && data.ammoCount < 0) {
            constexpr int   kPipCount = 5;
            static const ImU32 tierColors[kPipCount + 1] = {
                IM_COL32(220,  50,  40, 255),
                IM_COL32(220,  50,  40, 255),
                IM_COL32(235, 120,  40, 255),
                IM_COL32(230, 200,  60, 255),
                IM_COL32(120, 200,  70, 255),
                IM_COL32( 70, 220, 100, 255),
            };
            const int litCount = std::clamp(
                static_cast<int>(data.durabilityPct * kPipCount + 0.5f), 0, kPipCount);
            const ImU32 litColor = tierColors[litCount];

            const float sc     = std::clamp(s->peDurabilityScale, 0.3f, 3.0f);
            const float pipW   = ScalePx(4.0f) * sc;
            const float pipGap = ScalePx(2.0f) * sc;
            const float pipH   = h * 0.22f * sc;
            // Anchor to the slot's bottom-RIGHT corner: the right edge and bottom
            const float rightEdge = screenPos.x + w - ScalePx(3.0f + s->peBadgeOffsetX);
            const float pipBot    = anchor.Max.y - ScalePx(9.0f + s->peBadgeOffsetY);
            const float pipTop    = pipBot - pipH;

            for (int i = 0; i < kPipCount; ++i) {
                const float px2 = rightEdge - (kPipCount - 1 - i) * (pipW + pipGap);
                const float px1 = px2 - pipW;
                const bool  lit = (i < litCount);
                const ImU32 col = lit ? litColor : IM_COL32(40, 42, 48, 200);
                dl->AddRectFilled(ImVec2(px1, pipTop), ImVec2(px2, pipBot), col, 1.0f);
            }
        }

        if (s->peShowAmmoCount && data.ammoCount >= 0) {
            char ammoStr[16];
            snprintf(ammoStr, sizeof(ammoStr), "x%d", data.ammoCount);
            HUDDraw::DrawBadge(dl,
                ImVec2(screenPos.x + w - ScalePx(2.0f), anchor.Max.y - ScalePx(2.0f)),
                2, ammoStr, MakeBadgeStyle(0xFFFFDC64));
        }

        ImRect nameRect(0.0f, 0.0f, 0.0f, 0.0f);

        if (s->peShowNames && !data.isEmpty && !data.name.empty()) {
            const float bpad      = ScalePx(4.0f);
            const ImU32 bgCol     = ImGui::ColorConvertFloat4ToU32(ColorUtils::ArgbToVec4(s->peNameBgColor));
            const ImU32 textCol   = ImGui::ColorConvertFloat4ToU32(ColorUtils::ArgbToVec4(s->peNameColor));
            const std::string displayName = DisplayName(data, s);
            const int    placement = SidePlacement(NamePlacementFor(s), outward);
            const float  wrapW     = NameWrapWidth(s, w, nameMaxWidth);
            const ImVec2 textSize  = ImGui::CalcTextSize(displayName.c_str(), nullptr, false, wrapW);
            const ImVec2 namePos   = PlaceName(anchor, textSize, placement, s->peNameAlignment, NameGap());
                if ((s->peNameBgColor >> 24) > 0)
                    dl->AddRectFilled(ImVec2(namePos.x - bpad, namePos.y - bpad),
                                  ImVec2(namePos.x + textSize.x + bpad, namePos.y + textSize.y + bpad), bgCol, 2.0f);
            const bool centered = (s->peNameAlignment == 1);
            const ImVec2 drawPos = centered ? ImVec2(namePos.x - (wrapW - textSize.x) * 0.5f, namePos.y) : namePos;
            HUDDraw::DrawTextStyledWrapped(dl, drawPos, wrapW, textCol, displayName.c_str(), HUDDraw::TextFx{}, centered);
                contentBottom = (std::max)(contentBottom, namePos.y + textSize.y);
            nameRect = ImRect(namePos, ImVec2(namePos.x + textSize.x, namePos.y + textSize.y));
            }

        DrawEffectBadges(dl, anchor, nameRect, data);

        return contentBottom;
    }

    static void ComputeSlotExtents(const Settings* s, const int slotOrder[k_SlotCount],
                                   float wArr[k_SlotCount], float hArr[k_SlotCount],
                                   float& maxW, float& maxH)
    {
        maxW = 0.0f;
        maxH = 0.0f;
        for (int i = 0; i < k_SlotCount; ++i) {
            SlotType t = k_Slots[slotOrder[i]].type;
            wArr[i] = SlotWidth(s, t);
            hArr[i] = wArr[i] * (200.0f / 144.0f);
            maxW = (std::max)(maxW, wArr[i]);
            maxH = (std::max)(maxH, hArr[i]);
        }
    }

    static ImVec2 SlotScreenPos(ImVec2 origin, int idx, const Settings* s)
    {
        const float w   = ScalePx(static_cast<float>(s->peIconSize));
        const float h   = w * (200.0f / 144.0f);
        const float gap = ScalePx(static_cast<float>(s->peSlotSpacing));

        int col = idx % 2;
        int row = idx / 2;
        return ImVec2(origin.x + col * (w + gap), origin.y + row * (h + gap));
    }

    static ImVec2 WidgetSize(const Settings* s)
    {
        const float gap   = ScalePx(static_cast<float>(s->peSlotSpacing));
        const float nameH = s->peShowNames ? (ImGui::GetTextLineHeight() + ScalePx(4.0f)) : 0.0f;

        const int slotOrder[k_SlotCount] = {
            std::clamp(s->peSlotOrder0, 0, k_SlotCount - 1),
            std::clamp(s->peSlotOrder1, 0, k_SlotCount - 1),
            std::clamp(s->peSlotOrder2, 0, k_SlotCount - 1),
        };
        float wArr[k_SlotCount], hArr[k_SlotCount], maxW, maxH;
        ComputeSlotExtents(s, slotOrder, wArr, hArr, maxW, maxH);

        switch (s->peLayout) {
        default:
        case 0: {
            float sumW = 0.0f;
            for (int i = 0; i < k_SlotCount; ++i) sumW += wArr[i];
            return ImVec2(sumW + (k_SlotCount - 1) * gap, maxH + nameH);
        }
        case 1: {
            float sumH = 0.0f;
            for (int i = 0; i < k_SlotCount; ++i) sumH += hArr[i];
            return ImVec2(maxW, sumH + (k_SlotCount - 1) * gap + nameH);
        }
        case 2: {
            const float w = ScalePx(static_cast<float>(s->peIconSize));
            const float h = w * (200.0f / 144.0f);
            int cols = 2;
            int rows = (k_SlotCount + cols - 1) / cols;
            return ImVec2(cols * w + (cols - 1) * gap, rows * h + (rows - 1) * gap + nameH);
        }
        }
    }

    static void LogSlots(RE::PlayerCharacter* player, const SlotData slots[k_SlotCount])
    {
        std::string key;
        for (int i = 0; i < k_SlotCount; ++i) key += "|" + slots[i].name;
        if (key == g_LastDebugKey) return;
        g_LastDebugKey = key;

        logger::debug("[PlayerHUDEquipment] --- equipment changed ---");
        for (int i = 0; i < k_SlotCount; ++i) {
            logger::debug("[PlayerHUDEquipment] Slot[{}] badge='{}' name='{}' empty={} ammo={}",
                i, k_Slots[i].badge,
                slots[i].name.empty() ? "(empty)" : slots[i].name,
                slots[i].isEmpty,
                slots[i].ammoCount);
        }
    }

    void RenderContent(bool allowDrag)
    {
        auto* settings = Settings::GetSingleton();
        if (!settings->enablePlayerHud || !settings->enablePlayerEquipWidget) return;

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->Is3DLoaded()) return;
        if (MenuUtils::ShouldBlockOverlay()) return;

        if (g_SlotsDirty) {
            GatherSlots(player, g_CachedSlots);
            LogSlots(player, g_CachedSlots);
            g_SlotsDirty = false;
        }

        {
            const auto& equip = HUDSymptoms::GetEquip(player);
            for (int i = 0; i < k_SlotCount; ++i) {
                auto& slot = g_CachedSlots[i];
                auto* form = slot.form;
                slot.effects.clear();
                if (k_Slots[i].type == SlotType::Shout) {
                    const bool isShout = form && form->Is(RE::FormType::Shout);
                    slot.symptom = equip.shout.on && isShout;
                    if (isShout) {
                        AddEffectBadges(slot.effects, equip.shout.cuts, false);
                        AddEffectBadges(slot.effects, equip.shout.buffs, true);
                    }
                } else if (auto* sp = form ? form->As<RE::SpellItem>() : nullptr) {
                    const auto& cuts = HUDSymptoms::GetSkillCauses(player, sp->GetAssociatedSkill());
                    slot.symptom = !cuts.empty();
                    AddEffectBadges(slot.effects, cuts, false);
                    AddEffectBadges(slot.effects, HUDSymptoms::GetSkillBuffs(player, sp->GetAssociatedSkill()), true);
                } else {
                    const HandMarkList marks = HandMarks(equip, form);
                    slot.symptom = HandSymptom(equip, form);
                    for (const auto* m : marks) if (m) AddEffectBadges(slot.effects, m->cuts, false);
                    for (const auto* m : marks) if (m) AddEffectBadges(slot.effects, m->buffs, true);
                }
            }
        }

        {
            auto* proc = player->GetActorRuntimeData().currentProcess;
            RE::TESForm* rh = player->GetEquippedObject(false);
            RE::TESForm* lh = player->GetEquippedObject(true);
        if (lh && lh == rh) {
            bool rhIs2H = false;
            if (auto* rw = rh->As<RE::TESObjectWEAP>()) {
                auto wt = rw->GetWeaponType();
                rhIs2H = (wt == RE::WEAPON_TYPE::kTwoHandSword || wt == RE::WEAPON_TYPE::kTwoHandAxe ||
                          wt == RE::WEAPON_TYPE::kBow || wt == RE::WEAPON_TYPE::kCrossbow);
            }
            bool leftOccupied = (player->GetEquippedEntryData(true) != nullptr);
            if (rhIs2H && !leftOccupied) lh = nullptr;
        }
            for (int i = 0; i < k_SlotCount; ++i) {
                if (k_Slots[i].type == SlotType::RightHand) {
                    auto stats = EquipmentStatResolver::ResolveHandWeaponStats(player, rh, false);
                    g_CachedSlots[i].weaponDamage  = stats.hitDamage;
                    g_CachedSlots[i].enchantDamage = stats.enchantTotal;
                    g_CachedSlots[i].enchantType   = stats.enchantType;
                    g_CachedSlots[i].castCharge    = ResolveCastCharge(player, RE::MagicSystem::CastingSource::kRightHand, rh);
                    g_CachedSlots[i].enchantCharge = ResolveEnchantCharge(player, rh, true);
                    g_CachedSlots[i].enchantChargeMax = ResolveEnchantChargeMax(player, rh, true);
                    g_CachedSlots[i].durabilityPct = ResolveDurability(player, rh, false);
                    g_CachedSlots[i].auraType      = ResolveWeaponAura(player, rh, false);
                } else if (k_Slots[i].type == SlotType::LeftHand) {
                    auto stats = EquipmentStatResolver::ResolveHandWeaponStats(player, g_CachedSlots[i].form, true);
                    g_CachedSlots[i].weaponDamage  = stats.hitDamage;
                    g_CachedSlots[i].enchantDamage = stats.enchantTotal;
                    g_CachedSlots[i].enchantType   = stats.enchantType;
                    g_CachedSlots[i].castCharge    = ResolveCastCharge(player, RE::MagicSystem::CastingSource::kLeftHand, lh);
                    g_CachedSlots[i].enchantCharge = ResolveEnchantCharge(player, lh, false);
                    g_CachedSlots[i].enchantChargeMax = ResolveEnchantChargeMax(player, lh, false);
                    g_CachedSlots[i].durabilityPct = ResolveDurability(player, lh, true);
                    g_CachedSlots[i].auraType      = ResolveWeaponAura(player, lh, true);
                } else if (k_Slots[i].type == SlotType::Shout) {
                    RE::TESForm* pow = player->GetActorRuntimeData().selectedPower;
                    if (pow && pow->As<RE::TESShout>()) {
                        float remaining = proc ? proc->GetVoiceRecoveryTime() : 0.0f;
                        if (remaining > 0.0f) {
                            if (g_ShoutWasReady)
                                HUDVisibility::Trigger(HUDVisibility::WidgetID::PlayerEquip);
                            g_ShoutWasReady = false;
                            if (remaining > g_ShoutCooldownMax)
                                g_ShoutCooldownMax = remaining;
                            g_CachedSlots[i].shoutCooldown = std::clamp(1.0f - (remaining / g_ShoutCooldownMax), 0.0f, 1.0f);
                        } else {
                            g_ShoutWasReady = true;
                            g_ShoutCooldownMax = 0.0f;
                            g_CachedSlots[i].shoutCooldown = -1.0f;
                        }
                    } else {
                        g_CachedSlots[i].shoutCooldown = -2.0f;
                    }
                }
            }
        }

        const SlotData* slots = g_CachedSlots;

        {
            const float maxScale_ = (settings->peLayout == 2) ? 1.0f
                : (std::max)({ std::clamp(settings->peSlotScale0, 0.3f, 2.5f),
                               std::clamp(settings->peSlotScale1, 0.3f, 2.5f),
                               std::clamp(settings->peSlotScale2, 0.3f, 2.5f) });
            const float w_    = ScalePx(static_cast<float>(settings->peIconSize)) * maxScale_;
            const float h_    = w_ * (200.0f / 144.0f);
            const float ov_   = ScalePx(static_cast<float>(settings->peIconBgPadding));
            const float zoom_ = (std::max)(settings->peIconZoom, 1.0f);
            const float overflowW = (w_ - ov_ * 2.0f) * zoom_ * 0.5f - w_ * 0.5f;
            const float overflowH = (h_ - ov_ * 2.0f) * zoom_ * 0.5f - h_ * 0.5f;
            // Glow extends 3.2× the slot size: account for the extra bleed
            const float glowBleedW = w_ * 3.2f * 0.5f - w_ * 0.5f;
            const float glowBleedH = h_ * 3.2f * 0.5f - h_ * 0.5f;
            const float marginX = (std::max)({overflowW, glowBleedW, 0.0f}) + ScalePx(2.0f);
            const float marginY = (std::max)({overflowH, glowBleedH, 0.0f}) + ScalePx(2.0f);

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(marginX, marginY));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(0, 0));

        const ImVec2 targetPos(
            static_cast<float>(settings->pePosX) - marginX,
            static_cast<float>(settings->pePosY) - marginY);
        const bool forcedThisFrame = g_ForcePos;
        if (!allowDrag || g_ForcePos) {
            ImGui::SetNextWindowPos(targetPos, ImGuiCond_Always);
            g_ForcePos = false;
        } else {
            ImGui::SetNextWindowPos(targetPos, ImGuiCond_FirstUseEver);
        }

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar      |
                                 ImGuiWindowFlags_NoResize         |
                                 ImGuiWindowFlags_NoScrollbar      |
                                 ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoBackground;
        if (!allowDrag) flags |= ImGuiWindowFlags_NoMove;

        ImGui::Begin("PlayerHUDEquipment##equip", nullptr, flags);
        ImGui::SetWindowFontScale(settings->peNameFontScale);

        ImDrawList* dl     = ImGui::GetWindowDrawList();
        ImVec2      origin = ImGui::GetCursorScreenPos();

        // Name label width is computed per-slot in the draw loop below (each slot
        const float gap          = ScalePx(static_cast<float>(settings->peSlotSpacing));
        const float kNameWrapPad = ScalePx(24.0f);

        const int slotOrder[k_SlotCount] = {
            std::clamp(settings->peSlotOrder0, 0, k_SlotCount - 1),
            std::clamp(settings->peSlotOrder1, 0, k_SlotCount - 1),
            std::clamp(settings->peSlotOrder2, 0, k_SlotCount - 1),
        };

        float wArr[k_SlotCount], hArr[k_SlotCount], maxW, maxH;
        ComputeSlotExtents(settings, slotOrder, wArr, hArr, maxW, maxH);

        ImVec2 local[k_SlotCount];
        const int layout = settings->peLayout;
        if (layout >= 3 && layout <= 5) {
            float shapeH[k_SlotCount];
            for (int i = 0; i < k_SlotCount; ++i)
                shapeH[i] = ShapeHeight(wArr[i], hArr[i], SlotShape(settings, k_Slots[slotOrder[i]].type));
            if (layout == 3)
                LayoutCluster(wArr, hArr, gap, static_cast<float>(settings->peClusterAngle1),
                              static_cast<float>(settings->peClusterAngle2), local);
            else if (layout == 4)
                LayoutCross(wArr, hArr, gap, local);
            else
                LayoutStack(wArr, hArr, shapeH, static_cast<float>(std::clamp(settings->peStackOverlap, 0, 80)),
                            ScalePx(static_cast<float>(settings->peStackShiftX)), gap, local);
        } else {

        float cursor = 0.0f;
        for (int i = 0; i < k_SlotCount; ++i) {
                switch (layout) {
            case 1:
                    local[i] = ImVec2((maxW - wArr[i]) * 0.5f, cursor);
                cursor += hArr[i] + gap;
                break;
            case 2:
                    local[i] = SlotScreenPos(ImVec2(0.0f, 0.0f), i, settings);
                break;
            default:
                    local[i] = ImVec2(cursor, (maxH - hArr[i]) * 0.5f);
                cursor += wArr[i] + gap;
                break;
            }
            }
        }

        local[1].x += ScalePx(static_cast<float>(settings->peSlotOffsetX1));
        local[1].y += ScalePx(static_cast<float>(settings->peSlotOffsetY1));
        local[2].x += ScalePx(static_cast<float>(settings->peSlotOffsetX2));
        local[2].y += ScalePx(static_cast<float>(settings->peSlotOffsetY2));

        float nameMaxW[k_SlotCount];
        for (int i = 0; i < k_SlotCount; ++i)
            nameMaxW[i] = (layout == 0 || layout == 2) ? (std::max)(wArr[i], wArr[i] + gap - kNameWrapPad) : wArr[i];

        ImRect shapes[k_SlotCount];
        for (int i = 0; i < k_SlotCount; ++i)
            shapes[i] = AnchorRect(local[i], wArr[i], hArr[i], SlotShape(settings, k_Slots[slotOrder[i]].type));

        ImVec2 layoutMid(0.0f, 0.0f);
        for (int i = 0; i < k_SlotCount; ++i) {
            const ImVec2 c = shapes[i].GetCenter();
            layoutMid.x += c.x / k_SlotCount;
            layoutMid.y += c.y / k_SlotCount;
        }
        ImVec2 outward[k_SlotCount];
        for (int i = 0; i < k_SlotCount; ++i) {
            outward[i] = ImVec2(-1.0f, 0.0f);
            if (layout == 3 && i == 0) {
                outward[i] = ImVec2(0.0f, 0.0f);
            } else if (layout == 3 || layout == 4) {
                const ImVec2 c = shapes[i].GetCenter();
                outward[i] = ImVec2(c.x - (layout == 3 ? shapes[0].GetCenter().x : layoutMid.x),
                                    c.y - (layout == 3 ? shapes[0].GetCenter().y : layoutMid.y));
            }
        }

        ImRect bounds(FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX);
        const int placementPicked = NamePlacementFor(settings);
        for (int i = 0; i < k_SlotCount; ++i) {
            bounds.Add(shapes[i]);
            const int placement = SidePlacement(placementPicked, outward[i]);
            const SlotData& d = slots[slotOrder[i]];
            if (settings->peShowNames) {
                const ImVec2 reserve(NameWrapWidth(settings, wArr[i], nameMaxW[i]), ImGui::GetTextLineHeight());
                const ImVec2 rp = PlaceName(shapes[i], reserve, placement, settings->peNameAlignment, NameGap());
                bounds.Add(ImRect(rp, ImVec2(rp.x + reserve.x, rp.y + reserve.y)));
            }
            if (settings->peShowNames && !d.isEmpty && !d.name.empty()) {
                const std::string nm = DisplayName(d, settings);
                const ImVec2 ts = ImGui::CalcTextSize(nm.c_str(), nullptr, false,
                                                      NameWrapWidth(settings, wArr[i], nameMaxW[i]));
                const ImVec2 np = PlaceName(shapes[i], ts, placement, settings->peNameAlignment, NameGap());
                bounds.Add(ImRect(np, ImVec2(np.x + ts.x, np.y + ts.y)));
            }
        }
        if (layout <= 2) {
            const ImVec2 base = WidgetSize(settings);
            bounds.Add(ImRect(ImVec2(0.0f, 0.0f), base));
        }

        const ImVec2 shift(origin.x - bounds.Min.x, origin.y - bounds.Min.y);
        for (int i = 0; i < k_SlotCount; ++i) {
            const int si = slotOrder[i];
            const ImVec2 pos(local[i].x + shift.x, local[i].y + shift.y);
            DrawSlot(dl, pos, slots[si], k_Slots[si], settings, nameMaxW[i], wArr[i], outward[i]);
        }

        ImGui::Dummy(ImVec2(bounds.GetWidth(), bounds.GetHeight()));

        ImGui::SetWindowFontScale(1.0f);

        if (allowDrag && !forcedThisFrame) {
            ImVec2 winPos = ImGui::GetWindowPos();
            const float dx = winPos.x - targetPos.x;
            const float dy = winPos.y - targetPos.y;
            if (std::fabs(dx) > 1.5f || std::fabs(dy) > 1.5f) {
                settings->pePosX = static_cast<int>(std::lround(winPos.x + marginX));
                settings->pePosY = static_cast<int>(std::lround(winPos.y + marginY));
            }
        }

        ImGui::End();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
        }
    }

    bool Render()
    {
        auto* settings = Settings::GetSingleton();
        if (!settings->enablePlayerEquipWidget) return false;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->Is3DLoaded() || MenuUtils::ShouldBlockOverlay()) return false;

        {
            auto* proc = player->GetActorRuntimeData().currentProcess;
            RE::TESForm* rh  = player->GetEquippedObject(false);
            RE::TESForm* lh  = player->GetEquippedObject(true);
        if (lh && lh == rh) {
            bool rhIs2H = false;
            if (auto* rw = rh->As<RE::TESObjectWEAP>()) {
                auto wt = rw->GetWeaponType();
                rhIs2H = (wt == RE::WEAPON_TYPE::kTwoHandSword || wt == RE::WEAPON_TYPE::kTwoHandAxe ||
                          wt == RE::WEAPON_TYPE::kBow || wt == RE::WEAPON_TYPE::kCrossbow);
            }
            bool leftOccupied = (player->GetEquippedEntryData(true) != nullptr);
            if (rhIs2H && !leftOccupied) lh = nullptr;
        }
            RE::TESForm* pow  = player->GetActorRuntimeData().selectedPower;
            RE::TESAmmo* ammo = player->GetCurrentAmmo();
            if (rh != g_PrevRH || lh != g_PrevLH || pow != g_PrevPow || ammo != g_PrevAmmo) {
                g_PrevRH     = rh;
                g_PrevLH     = lh;
                g_PrevPow    = pow;
                g_PrevAmmo   = ammo;
                g_SlotsDirty = true;
                logger::debug("[PlayerHUDEquipment] equip change detected — triggering visibility");
                HUDVisibility::Trigger(HUDVisibility::WidgetID::PlayerEquip);
            }
        }

        float hvAlpha = HUDVisibility::GetAlpha(HUDVisibility::WidgetID::PlayerEquip);
        static float s_PrevHvAlpha = 1.0f;
        const float prevAlpha = s_PrevHvAlpha;
        const bool justShown = (prevAlpha <= 0.001f && hvAlpha > 0.001f);
        s_PrevHvAlpha = hvAlpha;
        if (!HUDVisibility::IsVisible(HUDVisibility::WidgetID::PlayerEquip)) return false;
        if (justShown) {
            if (!HUDVisibility::ConsumeEntrancePopSuppress(HUDVisibility::WidgetID::PlayerEquip))
                UIAnimation::TriggerBounce("phequip_show");
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, hvAlpha);
        RenderContent(false);
        ImGui::PopStyleVar();

        ImGui::EndFrame();
        ImGui::Render();
        HUDVisibility::ApplyAlphaToDrawData(hvAlpha);
        HUDVisibility::ApplyOffsetToDrawData(0.0f, UIAnimation::GetBounceOffsetY("phequip_show") * MasterScale());
        ImGuiRenderer::RenderToBoundTarget(ImGui::GetDrawData());
        return true;
    }

}
