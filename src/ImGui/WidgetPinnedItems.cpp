#include "../../includes/ImGui/WidgetPinnedItems.h"
#include "../../includes/Utils/ColorUtils.h"
#include "../../includes/Utils/IconRenderer.h"
#include "../../includes/ImGui/WidgetDrawHelpers.h"
#include "../../includes/ImGui/Renderer.h"
#include "../../includes/ImGui/WidgetPinnedItemsInternal.h"
#include "../../includes/Utils/WidgetJsonParser.h"
#include "../../includes/MenuUtils.h"
#include "../../includes/Settings.h"
#include "../../includes/SettingsIni.h"
#include "../../includes/TextureManager.h"
#include "../../includes/PlayerStatus.h"
#include "../../includes/LocalizationManager.h"
#include "../../includes/UIAnimation.h"
#include "../../includes/HUDVisibility.h"
#include "../../includes/InputHandler.h"
#include "../../includes/Utils/HUDDraw.h"
#include "../../includes/Utils/HUDFlow.h"
#include "../../includes/PotionAnimationBridge.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <Windows.h>

#include <cfloat>
#include <chrono>
#include <cmath>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <cstdint>

namespace WidgetPinnedItems {

    std::vector<PinnedEntry> g_PinnedItems;
    std::vector<int>         g_CachedCounts;
    bool                     g_ForcePinnedPos = true;
    bool                     g_Loaded         = false;

    void ForceReposition()
    {
        g_ForcePinnedPos = true;
    }

    namespace {

        // ── Master scale helpers ────────────────────────────────────────────────
        static float MasterScale()
        {
            const auto* settings = Settings::GetSingleton();
            return settings
                ? std::clamp(static_cast<float>(settings->ptScale) / 100.0f, 0.5f, 2.0f)
                : 1.0f;
        }

        static float ScalePx(float value)
        {
            return value * MasterScale();
        }

        static int ScalePxInt0(int value)
        {
            if (value <= 0) return 0;
            return (std::max)(1, static_cast<int>(std::lround(ScalePx(static_cast<float>(value)))));
        }

        static int ScalePxSigned(int value)
        {
            return static_cast<int>(std::lround(ScalePx(static_cast<float>(value))));
        }

        // =====================================================================
        // Item Count
        // =====================================================================
        static RE::FormID SlotRestoreMgef(const std::string& id)
        {
            if (id == "health_potion")  return 0x0003EB15;
            if (id == "magicka_potion") return 0x0003EB17;
            if (id == "stamina_potion") return 0x0003EB16;
            return 0;
        }

        static constexpr RE::FormID kRestoreHealth  = 0x0003EB15;
        static constexpr RE::FormID kRestoreMagicka = 0x0003EB17;
        static constexpr RE::FormID kRestoreStamina = 0x0003EB16;
        static constexpr RE::FormID kFortifyHealth  = 0x0003EAF3;
        static constexpr RE::FormID kFortifyMagicka = 0x0003EAF8;
        static constexpr RE::FormID kFortifyStamina = 0x0003EAF9;

        static RE::FormID SlotFortifyMgef(RE::FormID slotMgef)
        {
            if (slotMgef == kRestoreHealth)  return kFortifyHealth;
            if (slotMgef == kRestoreMagicka) return kFortifyMagicka;
            if (slotMgef == kRestoreStamina) return kFortifyStamina;
            return 0;
        }

        static const char* SlotIdForMgef(RE::FormID slotMgef)
        {
            if (slotMgef == kRestoreHealth)  return "health_potion";
            if (slotMgef == kRestoreMagicka) return "magicka_potion";
            if (slotMgef == kRestoreStamina) return "stamina_potion";
            return nullptr;
        }

        static bool FormIsListedForSlot(RE::FormID slotMgef, RE::FormID formID)
        {
            const char* slotId = SlotIdForMgef(slotMgef);
            if (!slotId || formID == 0) return false;
            for (const auto& entry : g_PinnedItems) {
                if (entry.id != slotId) continue;
                for (const auto& ref : entry.items)
                    if (ref.resolvedFormID == formID) return true;
                return false;
            }
            return false;
        }

        static void LogPotionReject(RE::AlchemyItem* alch, RE::FormID slotMgef,
                                    const char* reason, RE::FormID culpritMgef)
        {
            static std::set<std::pair<RE::FormID, RE::FormID>> s_logged;
            if (!alch) return;
            if (!s_logged.insert({ alch->GetFormID(), slotMgef }).second) return;

            const char* nm = alch->GetName();
            if (culpritMgef) {
                auto* mg = RE::TESForm::LookupByID<RE::EffectSetting>(culpritMgef);
                logger::debug("[PotionReject] '{}' (0x{:08X}) slot=0x{:05X} — {}: 0x{:08X} '{}'",
                    (nm && *nm) ? nm : "?", alch->GetFormID(), slotMgef, reason, culpritMgef,
                    (mg && mg->GetName() && *mg->GetName()) ? mg->GetName() : "?");
            } else {
                logger::debug("[PotionReject] '{}' (0x{:08X}) slot=0x{:05X} — {}",
                    (nm && *nm) ? nm : "?", alch->GetFormID(), slotMgef, reason);
            }
        }

        static bool PotionMatchesSlot(RE::AlchemyItem* alch, RE::FormID slotMgef)
        {
            if (!alch) return false;
            if (alch->IsPoison()) { LogPotionReject(alch, slotMgef, "is a poison", 0); return false; }
            if (alch->IsFood())   { LogPotionReject(alch, slotMgef, "is food", 0);     return false; }

            if (FormIsListedForSlot(slotMgef, alch->GetFormID())) {
                static std::set<std::pair<RE::FormID, RE::FormID>> s_logged;
                if (s_logged.insert({ alch->GetFormID(), slotMgef }).second) {
                    const char* nm = alch->GetName();
                    logger::debug("[PotionAccept] '{}' (0x{:08X}) slot=0x{:05X} — JSON list",
                                  (nm && *nm) ? nm : "?", alch->GetFormID(), slotMgef);
                }
                return true;
            }

            const RE::FormID fortify = SlotFortifyMgef(slotMgef);

            bool hasSlot = false;
            for (auto* eff : alch->effects) {
                if (!eff || !eff->baseEffect) continue;
                if (eff->effectItem.magnitude <= 0.0f && eff->effectItem.duration <= 0) continue;
                const RE::FormID id = eff->baseEffect->GetFormID();
                if (id == slotMgef) {
                    hasSlot = true;
                } else if (id != fortify) {
                    LogPotionReject(alch, slotMgef, "extra live effect", id);
                    return false;
                }
            }
            if (!hasSlot) LogPotionReject(alch, slotMgef, "no live restore effect for this slot", 0);
            return hasSlot;
        }

        static RE::ActorValue SlotActorValue(RE::FormID slotMgef)
        {
            if (slotMgef == kRestoreHealth)  return RE::ActorValue::kHealth;
            if (slotMgef == kRestoreMagicka) return RE::ActorValue::kMagicka;
            if (slotMgef == kRestoreStamina) return RE::ActorValue::kStamina;
            return RE::ActorValue::kNone;
        }

        static float SlotEffectMagnitude(RE::AlchemyItem* alch, RE::FormID slotMgef)
        {
            if (!alch) return 0.0f;
            for (auto* eff : alch->effects) {
                if (eff && eff->baseEffect && eff->baseEffect->GetFormID() == slotMgef)
                    return eff->effectItem.magnitude;
            }

            const RE::ActorValue av = SlotActorValue(slotMgef);
            if (av == RE::ActorValue::kNone) return 0.0f;
            float best = 0.0f;
            for (auto* eff : alch->effects) {
                if (!eff || !eff->baseEffect) continue;
                const auto& d = eff->baseEffect->data;
                if (d.archetype != RE::EffectSetting::Archetype::kValueModifier) continue;
                if (d.primaryAV != av) continue;
                best = (std::max)(best, eff->effectItem.magnitude);
            }
            return best;
        }

        static float SlotDeficit(RE::FormID slotMgef)
        {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) return 0.0f;
            auto* avo = player->AsActorValueOwner();
            if (!avo) return 0.0f;
            const RE::ActorValue av = SlotActorValue(slotMgef);
            if (av == RE::ActorValue::kNone) return 0.0f;
            const float full = avo->GetPermanentActorValue(av);
            const float cur  = avo->GetActorValue(av);
            return (std::max)(0.0f, full - cur);
        }

        static RE::AlchemyItem* BestHealPotion(RE::FormID slotMgef)
        {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player || slotMgef == 0) return nullptr;
            const float deficit = SlotDeficit(slotMgef);

            RE::AlchemyItem* smallestCovering = nullptr; float coverMag = FLT_MAX;
            RE::AlchemyItem* strongest        = nullptr; float strongMag = -1.0f;
            RE::AlchemyItem* smallest         = nullptr; float smallMag  = FLT_MAX;

            auto inv = player->GetInventory([](const RE::TESBoundObject& obj) {
                return obj.Is(RE::FormType::AlchemyItem);
            });
            for (auto& [form, data] : inv) {
                if (data.first <= 0) continue;
                auto* alch = form ? form->As<RE::AlchemyItem>() : nullptr;
                if (!PotionMatchesSlot(alch, slotMgef)) continue;
                const float mag = SlotEffectMagnitude(alch, slotMgef);
                if (mag > strongMag) { strongMag = mag; strongest = alch; }
                if (mag < smallMag)  { smallMag  = mag; smallest  = alch; }
                if (mag >= deficit && mag < coverMag) { coverMag = mag; smallestCovering = alch; }
            }
            if (deficit < 1.0f) return smallest;
            if (smallestCovering) return smallestCovering;
            return strongest;
        }

        static int CountOwnedPotions(RE::FormID slotMgef)
        {
            int total = 0;
            int scanned = 0, matched = 0;
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player || slotMgef == 0) return 0;
            auto inv = player->GetInventory([](const RE::TESBoundObject& obj) {
                return obj.Is(RE::FormType::AlchemyItem);
            });
            for (auto& [form, data] : inv) {
                if (data.first <= 0) continue;
                auto* alch = form ? form->As<RE::AlchemyItem>() : nullptr;
                ++scanned;
                if (PotionMatchesSlot(alch, slotMgef)) { total += data.first; ++matched; }
            }

            {
                static std::unordered_map<RE::FormID, std::pair<int, int>> s_last;
                auto& prev = s_last[slotMgef];
                if (prev.first != matched || prev.second != scanned) {
                    prev = { matched, scanned };
                    logger::debug("[PotionScan] slot=0x{:05X} alchemyFormsSeen={} matchedForms={} totalCount={}",
                                  slotMgef, scanned, matched, total);
                }
            }
            return total;
        }

        static int GetItemCount(const PinnedEntry& e)
        {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) return 0;
            if (auto mgef = SlotRestoreMgef(e.id)) return CountOwnedPotions(mgef);
            int total = 0;
            for (const auto& item : e.items) {
                if (item.resolvedFormID == 0) continue;
                auto* form = RE::TESForm::LookupByID<RE::TESBoundObject>(item.resolvedFormID);
                if (form) total += player->GetItemCount(form);
            }
            return total;
        }

        static float DrinkFxTime()
        {
            using clock = std::chrono::steady_clock;
            static const clock::time_point s_start = clock::now();
            return std::chrono::duration<float>(clock::now() - s_start).count();
        }

        static ImU32 DrinkFxTint(const std::string& id)
        {
            if (id == "health_potion")  return IM_COL32(255,  80,  80, 255);
            if (id == "magicka_potion") return IM_COL32( 90, 150, 255, 255);
            if (id == "stamina_potion") return IM_COL32( 90, 220, 120, 255);
            return IM_COL32(255, 200, 120, 255);
        }

        static void DrawDrinkHalo(ImDrawList* dl, ImVec2 iconPos, float iconSz, ImU32 tint)
        {
            dl->PushClipRect(ImVec2(0.0f, 0.0f), ImGui::GetIO().DisplaySize, false);
            const float  t      = DrinkFxTime();
            const ImVec2 center = ImVec2(iconPos.x + iconSz * 0.5f, iconPos.y + iconSz * 0.5f);
            const float  pulse  = 1.0f + 0.10f * std::sin(t * 7.0f);
            const int    r8 = (tint >> IM_COL32_R_SHIFT) & 0xFF;
            const int    g8 = (tint >> IM_COL32_G_SHIFT) & 0xFF;
            const int    b8 = (tint >> IM_COL32_B_SHIFT) & 0xFF;
            constexpr struct { float scale; int alpha; } kLayers[] = {
                { 1.05f, 26 }, { 0.85f, 40 }, { 0.66f, 58 },
            };
            for (const auto& l : kLayers)
                dl->AddCircleFilled(center, iconSz * l.scale * pulse,
                                    IM_COL32(r8, g8, b8, l.alpha), 32);
            const float cyc   = std::fmod(t, 0.7f) / 0.7f;
            const int   ringA = static_cast<int>((1.0f - cyc) * 110.0f);
            dl->AddCircle(center, iconSz * (0.45f + 0.55f * cyc),
                          IM_COL32(r8, g8, b8, ringA), 32, ScalePx(2.0f));
            dl->PopClipRect();
        }

        static void DrawDrinkBubbles(ImDrawList* dl, ImVec2 iconPos, float iconSz, ImU32 tint)
        {
            const float t  = DrinkFxTime();
            const int   r8 = (tint >> IM_COL32_R_SHIFT) & 0xFF;
            const int   g8 = (tint >> IM_COL32_G_SHIFT) & 0xFF;
            const int   b8 = (tint >> IM_COL32_B_SHIFT) & 0xFF;
            // Bubbles may rise slightly above the slot; clip with headroom so
            dl->PushClipRect(ImVec2(iconPos.x - ScalePx(4.0f), iconPos.y - iconSz * 0.35f),
                             ImVec2(iconPos.x + iconSz + ScalePx(4.0f), iconPos.y + iconSz + ScalePx(2.0f)), false);
            constexpr int kBubbles = 7;
            for (int i = 0; i < kBubbles; ++i) {
                const float phase = std::fmod(t / 1.1f + i * 0.1731f, 1.0f);
                const float laneX = iconPos.x + iconSz * (0.22f + 0.56f * ((i * 0.381f) - std::floor(i * 0.381f)));
                const float wob   = std::sin(t * 3.0f + i * 2.1f) * iconSz * 0.05f;
                const float y     = iconPos.y + iconSz * (1.0f - 1.2f * phase);
                const float aF    = std::sin(phase * 3.14159265f);
                const int   a     = static_cast<int>(aF * 200.0f);
                const float rad   = iconSz * (0.035f + 0.025f * ((i % 3) * 0.5f));
                const ImVec2 p    = ImVec2(laneX + wob, y);
                dl->AddCircleFilled(p, rad, IM_COL32(r8, g8, b8, a), 0);
                dl->AddCircleFilled(ImVec2(p.x - rad * 0.3f, p.y - rad * 0.3f),
                                    rad * 0.35f, IM_COL32(255, 255, 255, a * 3 / 4), 8);
            }
            dl->PopClipRect();
        }

        // ===== Vanilla drink cooldown =====
        constexpr float k_VanillaDrinkCooldown = 1.0f;
        std::unordered_map<std::string, float> g_VanillaDrinkEnd;

        bool InVanillaCooldown(const std::string& entryId)
        {
            auto it = g_VanillaDrinkEnd.find(entryId);
            return it != g_VanillaDrinkEnd.end() && DrinkFxTime() < it->second;
        }

        void StartVanillaCooldown(const std::string& entryId)
        {
            g_VanillaDrinkEnd[entryId] = DrinkFxTime() + k_VanillaDrinkCooldown;
        }

        // ===== Full-resource decline =====
        constexpr float k_DeclineDur = 0.32f;
        std::unordered_map<std::string, float> g_DeclineStart;

        void StartDecline(const std::string& entryId)
        {
            g_DeclineStart[entryId] = DrinkFxTime();
        }

        float DeclineProgress(const std::string& entryId)
        {
            auto it = g_DeclineStart.find(entryId);
            if (it == g_DeclineStart.end()) return 0.0f;
            const float t = (DrinkFxTime() - it->second) / k_DeclineDur;
            return (t >= 0.0f && t < 1.0f) ? t : 0.0f;
        }

        float DeclineOffsetY(const std::string& entryId, float iconSz)
        {
            const float t = DeclineProgress(entryId);
            if (t <= 0.0f) return 0.0f;
            return std::sin(t * 3.14159265f) * iconSz * 0.07f;
        }

        // =====================================================================
        // Quick-Consume Hotkey Badge
        // =====================================================================
        static bool GetEntryHotkey(const std::string& id, const Settings* s, int& key, int& mod)
        {
            if (id == "health_potion")       { key = s->potionHealthKey;  mod = s->potionHealthModifier; }
            else if (id == "magicka_potion") { key = s->potionMagickaKey; mod = s->potionMagickaModifier; }
            else if (id == "stamina_potion") { key = s->potionStaminaKey; mod = s->potionStaminaModifier; }
            else return false;
            return key >= 1;
        }

        static int PlainModifier(int mod)
        {
            if (mod >= 4 && mod <= 9) return 1 + (mod - 4) / 2;
            return mod;
        }

        static std::string GetModifierWord(int mod)
        {
            switch (PlainModifier(mod)) {
                case 1: return LocalizationManager::Get("Hotkeys.ModAlt");
                case 2: return LocalizationManager::Get("Hotkeys.ModCtrl");
                case 3: return LocalizationManager::Get("Hotkeys.ModShift");
                default: return "";
            }
        }

        static HUDDraw::BadgeStyle MakeBadgeStyle(int preset, std::uint32_t tint, float fontScale)
        {
            HUDDraw::BadgeStyle bs = HUDDraw::BadgePresetStyle(preset);
            HUDDraw::TintBadge(bs, tint);
            bs.fontScale = ScalePx(fontScale);
            return bs;
        }

        static std::string HotkeyLabel(int key, int mod)
        {

            std::string label = GetModifierWord(mod);
            if (!label.empty()) label += " + ";
            label += ImGuiRenderer::DIKToKeyName(key);
            return label;
        }

        static ImTextureID ModifierIcon(int mod)
        {
            mod = PlainModifier(mod);
            const char* name = mod == 1 ? "alt" : mod == 2 ? "ctrl" : mod == 3 ? "shift" : nullptr;
            if (!name) return 0;
            return reinterpret_cast<ImTextureID>(TextureManager::GetPartyIcon(name));
        }

        static std::string CountText(int count, int goal)
        {
            char buf[32];
            if (goal > 0) snprintf(buf, sizeof(buf), "%d / %d", count, goal);
            else          snprintf(buf, sizeof(buf), "%d", count);
            return buf;
        }

        static void DrawHotkeyBadge(float screenX, float screenY,
                                    int key, int mod, float scale, int preset, std::uint32_t tint)
        {
            if (key < 1) return;

            auto bs = MakeBadgeStyle(preset, tint, scale);
            const ImTextureID modIcon = ModifierIcon(mod);
            const std::string label = modIcon ? ImGuiRenderer::DIKToKeyName(key) : HotkeyLabel(key, mod);
            if (modIcon) {
                bs.icon     = modIcon;
                bs.iconTint = bs.textColor;
            }
            HUDDraw::DrawBadge(ImGui::GetForegroundDrawList(),
                               ImVec2(screenX, screenY), 0, label.c_str(), bs);
        }

        static void DrawCardIcon(ImDrawList* dl, const PinnedEntry& e, ImVec2 at, float iconSz,
                                 const IconRenderer::Style& look, const std::string& drinkingId)
        {
            IconRenderer::Style icon = look;
            icon.texture = e.icon;
            icon.bounceOffsetY = ScalePx(UIAnimation::GetBounceOffsetY("pt_bounce_" + e.id))
                               + DeclineOffsetY(e.id, iconSz);
            const bool drinking = (!drinkingId.empty() && drinkingId == e.id) || InVanillaCooldown(e.id);
            const ImU32 fxTint  = drinking ? DrinkFxTint(e.id) : 0;
            const ImVec2 bounced(at.x, at.y + icon.bounceOffsetY);
            if (drinking) DrawDrinkHalo(dl, bounced, iconSz, fxTint);
            IconRenderer::Draw(dl, at, icon);
            if (drinking) DrawDrinkBubbles(dl, bounced, iconSz, fxTint);
            if (const float dt = DeclineProgress(e.id); dt > 0.0f) {
                const float k = std::sin(dt * 3.14159265f);
                dl->AddRectFilled(at, ImVec2(at.x + iconSz, at.y + iconSz),
                                  IM_COL32(10, 12, 16, static_cast<int>(120.0f * k)), icon.borderRadius);
            }
        }

        struct CardLayout {
            std::size_t index = 0;
            ImVec2      size, iconAt, labelAt, labelSize;
            ImVec2      blockAt, blockSize;
            std::string text;
            std::string endText;
            ImTextureID endIcon = 0;
            std::string endKey;
            float       endW = 0.0f;
        };

        static void RenderPinnedWindow(bool allowDrag)
        {
            auto* settings = Settings::GetSingleton();
            if (!settings || !settings->enablePinnedWidget) return;
            if (MenuUtils::ShouldBlockOverlay()) return;

            ImFont*     font       = ImGui::GetFont();
            const float labelPx    = font->LegacySize * ScalePx(settings->ptLabelScale);
            const float countPx    = ImGuiRenderer::GetHeadingFontSize() * settings->ptCountScale;
            const float iconSz     = ScalePx(static_cast<float>(settings->ptIconSize));
            const float itemW      = ScalePx(static_cast<float>(settings->ptItemWidth));
            const float cardGap    = ScalePx(static_cast<float>(settings->ptRowSpacing));
            const float labelGap   = ScalePx(5.0f);
            const auto  side       = static_cast<HUDFlow::Side>(std::clamp(settings->ptLabelPlacement, 0, 3));
            const bool  stackLabel = side == HUDFlow::Side::Below || side == HUDFlow::Side::Above;
            const bool  countBefore = settings->ptShowLabel && settings->ptCountPosition == 1;
            const bool  countEnd    = settings->ptShowLabel && !stackLabel && settings->ptCountPosition == 2;
            const bool  hotkeyEnd   = settings->ptShowLabel && !stackLabel && settings->ptHotkeyPosition == 1;
            const bool  stripOn     = settings->ptShowLabel && !stackLabel && settings->ptStripStyle > 0;
            const bool  rowLabel    = stripOn || countEnd || hotkeyEnd;
            const float stripPad    = stripOn ? ScalePx(10.0f) : 0.0f;
            const float endGap      = ScalePx(12.0f);
            const float arrowTip    = settings->ptStripStyle == HUDDraw::StripStyle_Arrow && stripOn ? labelPx * 0.6f : 0.0f;
            const float margin     = (std::max)(ScalePx(8.0f), ScalePx(countPx + 4.0f) * 0.5f);

            std::vector<CardLayout> cards;
            std::vector<ImVec2>     cardSizes;
            for (std::size_t i = 0; i < g_PinnedItems.size(); ++i) {
                const auto& e = g_PinnedItems[i];
                if (!e.enabled) continue;
                CardLayout c;
                c.index = i;
                const ImVec2 hostMin(0.0f, 0.0f), hostMax(iconSz, iconSz);
                ImVec2 boxMin = hostMin, boxMax = hostMax;
                if (settings->ptShowLabel) {
                    const int count = (i < g_CachedCounts.size()) ? g_CachedCounts[i] : 0;
                    const std::string countStr = CountText(count, e.goal);
                    c.text = countBefore ? countStr + "  " + e.label : e.label;
                    if (countEnd) c.endText = countStr;
                    int hkKey, hkMod;
                    if (hotkeyEnd && GetEntryHotkey(e.id, settings, hkKey, hkMod)) {
                        c.endText += c.endText.empty() ? "[" : "  [";
                        c.endIcon  = ModifierIcon(hkMod);
                        c.endKey   = (c.endIcon ? ImGuiRenderer::DIKToKeyName(hkKey) : HotkeyLabel(hkKey, hkMod)) + "]";
            }
                    if (!c.endText.empty())
                        c.endW = font->CalcTextSizeA(labelPx, FLT_MAX, 0.0f, c.endText.c_str()).x
                               + (c.endIcon ? labelPx : 0.0f)
                               + font->CalcTextSizeA(labelPx, FLT_MAX, 0.0f, c.endKey.c_str()).x;

                    c.labelSize = font->CalcTextSizeA(labelPx, FLT_MAX, rowLabel ? 0.0f : itemW, c.text.c_str());
                    ImVec2 block;
                    if (stackLabel) {
                        block = ImVec2((std::max)(c.labelSize.x, itemW), c.labelSize.y);
                    } else {
                        const float content = c.labelSize.x + (c.endW > 0.0f ? endGap + c.endW : 0.0f) + stripPad * 2.0f + arrowTip;
                        block = ImVec2((std::max)(content, ScalePx(static_cast<float>(settings->ptStripWidth))),
                                       c.labelSize.y + (stripOn ? ScalePx(6.0f) : 0.0f));
                    }
                    const ImVec2 blockAt = HUDFlow::PlaceLabel(hostMin, hostMax, block, side, HUDFlow::Align::Center, labelGap);
                    c.blockAt   = blockAt;
                    c.blockSize = block;
                    const float textY = blockAt.y + (block.y - c.labelSize.y) * 0.5f;
                    if (stackLabel)
                        c.labelAt = ImVec2(blockAt.x + (block.x - c.labelSize.x) * 0.5f, textY);
                    else if (side == HUDFlow::Side::Right)
                        c.labelAt = ImVec2(blockAt.x + stripPad, textY);
                    else
                        c.labelAt = ImVec2(blockAt.x + block.x - stripPad - c.labelSize.x, textY);
                    boxMin = ImVec2((std::min)(boxMin.x, blockAt.x), (std::min)(boxMin.y, blockAt.y));
                    boxMax = ImVec2((std::max)(boxMax.x, blockAt.x + block.x), (std::max)(boxMax.y, blockAt.y + block.y));
                }
                const ImVec2 shift(margin - boxMin.x, margin - boxMin.y);
                c.iconAt  = shift;
                c.labelAt = ImVec2(c.labelAt.x + shift.x, c.labelAt.y + shift.y);
                c.blockAt = ImVec2(c.blockAt.x + shift.x, c.blockAt.y + shift.y);
                c.size    = ImVec2(boxMax.x - boxMin.x + margin * 2.0f, boxMax.y - boxMin.y + margin * 2.0f);
                cards.push_back(c);
                cardSizes.push_back(c.size);
            }
            if (cards.empty()) return;

            HUDFlow::Options flow;
            flow.corner = static_cast<HUDFlow::Corner>(std::clamp(settings->ptAnchor, 0, 3));
            flow.axis   = settings->ptDirection == 1 ? HUDFlow::Axis::Horizontal : HUDFlow::Axis::Vertical;
            flow.gapX   = cardGap;
            flow.gapY   = cardGap;
            const HUDFlow::Result layout = HUDFlow::Arrange(cardSizes, flow);
            const ImVec2 anchorPt(static_cast<float>(settings->ptPosX), static_cast<float>(settings->ptPosY));
            const ImVec2 topLeft = HUDFlow::TopLeftFromAnchor(anchorPt, layout.size, flow.corner);

            ImGuiWindow* self   = ImGui::FindWindowByName("##pinneditems");
            const bool dragging = allowDrag && self && GImGui->MovingWindow == self && !g_ForcePinnedPos;
            if (!dragging) ImGui::SetNextWindowPos(topLeft, ImGuiCond_Always);
                g_ForcePinnedPos = false;
            ImGui::SetNextWindowSize(layout.size, ImGuiCond_Always);

            ImGuiWindowFlags flags =
                ImGuiWindowFlags_NoTitleBar      |
                ImGuiWindowFlags_NoResize        |
                ImGuiWindowFlags_NoScrollbar     |
                ImGuiWindowFlags_NoSavedSettings;
            if (!allowDrag) flags |= ImGuiWindowFlags_NoMove;

            ImGui::SetNextWindowBgAlpha(0.0f);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

            if (ImGui::Begin("##pinneditems", nullptr, flags)) {
                ImDrawList* dl     = ImGui::GetWindowDrawList();
                ImDrawList* fg     = ImGui::GetForegroundDrawList();
                const ImVec2 winPos = ImGui::GetWindowPos();

                IconRenderer::Style look;
                IconRenderer::SetZoom(look, settings->ptIconZoom, settings->ptIconFocusU, settings->ptIconFocusV);
                look.size         = iconSz;
                look.bgColor      = ImGui::ColorConvertFloat4ToU32(ColorUtils::ArgbToVec4(settings->ptIconBgColor));
                look.chip         = true;
                look.glowEnabled  = settings->ptIconGlowEnabled;
                look.borderColor  = ImGui::ColorConvertFloat4ToU32(ColorUtils::ArgbToVec4(settings->ptIconBorderColor));
                look.borderWidth  = ScalePxInt0(settings->ptIconBorderWidth);
                look.borderRadius = iconSz * settings->ptIconBorderRadius * 0.01f;

                const HUDDraw::BadgeStyle countBadgeStyle = MakeBadgeStyle(
                    settings->ptCountStyle, settings->ptBadgeTint, countPx / font->LegacySize);
                const auto spot = static_cast<HUDFlow::Spot>(std::clamp(settings->ptCountAnchor, 0, 8));
                const ImVec2 countOverhang(0.3f, 0.4f);

                const std::string drinkingId = PotionAnimationBridge::ActiveEntryId();

                for (std::size_t k = 0; k < cards.size(); ++k) {
                    const CardLayout& c = cards[k];
                    const auto& e = g_PinnedItems[c.index];
                    const ImVec2 origin(winPos.x + layout.offsets[k].x, winPos.y + layout.offsets[k].y);
                    const ImVec2 iconMin(origin.x + c.iconAt.x, origin.y + c.iconAt.y);
                    const ImVec2 iconMax(iconMin.x + iconSz, iconMin.y + iconSz);

                    const int  count   = (c.index < g_CachedCounts.size()) ? g_CachedCounts[c.index] : 0;
                    const bool hasGoal = e.goal > 0;
                    const bool goalMet = hasGoal && count >= e.goal;
                    const std::uint32_t textRgb = goalMet ? settings->ptGoalMetColor : settings->ptTextColor;

                    const bool toRight = side == HUDFlow::Side::Right;
                    const ImVec2 blockMin(origin.x + c.blockAt.x, origin.y + c.blockAt.y);
                    const ImVec2 blockMax(blockMin.x + c.blockSize.x, blockMin.y + c.blockSize.y);
                    if (stripOn) {
                        const float iconMidX = (iconMin.x + iconMax.x) * 0.5f;
                        HUDDraw::DrawStrip(dl,
                            ImVec2(toRight ? iconMidX : blockMin.x, blockMin.y),
                            ImVec2(toRight ? blockMax.x : iconMidX, blockMax.y),
                            settings->ptStripStyle, settings->ptStripTint, toRight);
                }

                    if (e.icon) DrawCardIcon(dl, e, iconMin, iconSz, look, drinkingId);

                    if (settings->ptShowLabel) {
                        const ImVec2 at(origin.x + c.labelAt.x, origin.y + c.labelAt.y);
                        const ImU32  col  = ImGui::ColorConvertFloat4ToU32(ColorUtils::RgbToVec4(textRgb));
                        const float  wrap = rowLabel ? 0.0f : itemW;
                        dl->AddText(font, labelPx, ImVec2(at.x + 1.0f, at.y + 1.0f), IM_COL32(0, 0, 0, 255),
                                    c.text.c_str(), nullptr, wrap);
                        dl->AddText(font, labelPx, at, col, c.text.c_str(), nullptr, wrap);

                        if (!c.endText.empty()) {
                            const float endX = toRight ? blockMax.x - stripPad - arrowTip - c.endW
                                                       : blockMin.x + stripPad + arrowTip;
                            const ImU32 endCol = (col & 0x00FFFFFFu) | IM_COL32(0, 0, 0, 220);
                            float x = endX;
                            auto endText = [&](const std::string& t) {
                                dl->AddText(font, labelPx, ImVec2(x + 1.0f, at.y + 1.0f), IM_COL32(0, 0, 0, 255), t.c_str());
                                dl->AddText(font, labelPx, ImVec2(x, at.y), endCol, t.c_str());
                                x += font->CalcTextSizeA(labelPx, FLT_MAX, 0.0f, t.c_str()).x;
                            };
                            endText(c.endText);
                            if (c.endIcon) {
                                const float inset = labelPx * 0.1f;
                                dl->AddImage(c.endIcon, ImVec2(x + inset, at.y + inset),
                                             ImVec2(x + labelPx - inset, at.y + labelPx - inset), ImVec2(0, 0), ImVec2(1, 1), endCol);
                                x += labelPx;
                            }
                            endText(c.endKey);
                        }
                    }

                    if (!countBefore && !countEnd) {
                        HUDDraw::BadgeStyle countStyle = countBadgeStyle;
                        if (goalMet || settings->ptCountStyle == HUDDraw::BadgePreset_Text)
                            countStyle.textColor = 0xFF000000u | textRgb;

                    char countBuf[32];
                        if (hasGoal && settings->ptCountStyle == HUDDraw::BadgePreset_Text)
                        snprintf(countBuf, sizeof(countBuf), "%d / %d", count, e.goal);
                    else
                        snprintf(countBuf, sizeof(countBuf), "%d", count);
                        const ImVec2 pill = HUDDraw::MeasureBadge(countBuf, countStyle);
                        const ImVec2 at   = HUDFlow::PlaceBadge(iconMin, iconMax, pill, spot, countOverhang);
                        HUDDraw::DrawBadge(fg, ImVec2(at.x + ScalePxSigned(settings->ptBadgeOffsetX),
                                                      at.y + ScalePxSigned(settings->ptBadgeOffsetY)),
                                           0, countBuf, countStyle);
                    }

                    int hkKey, hkMod;
                    if (!hotkeyEnd && settings->ptHotkeyBadgeStyle >= 0 && GetEntryHotkey(e.id, settings, hkKey, hkMod))
                        DrawHotkeyBadge(iconMin.x + ScalePxSigned(settings->ptHotkeyBadgeOffsetX),
                                        iconMin.y + ScalePxSigned(settings->ptHotkeyBadgeOffsetY),
                                        hkKey, hkMod, settings->ptHotkeyBadgeScale,
                                        settings->ptHotkeyBadgeStyle, settings->ptHotkeyBadgeTint);
                        }

                ImGui::Dummy(layout.size);

                if (dragging) {
                    const ImVec2 a = HUDFlow::AnchorFromTopLeft(ImGui::GetWindowPos(), layout.size, flow.corner);
                    settings->ptPosX = static_cast<int>(a.x);
                    settings->ptPosY = static_cast<int>(a.y);
                }
            }
            ImGui::End();

            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor();
        }

    }

    void RefreshPotionSlots()
    {
        for (size_t i = 0; i < g_PinnedItems.size(); ++i) {
            if (auto mgef = SlotRestoreMgef(g_PinnedItems[i].id)) {
                if (i < g_CachedCounts.size())
                    g_CachedCounts[i] = CountOwnedPotions(mgef);
            }
        }
    }

    std::string MatchPotionSlotId(RE::AlchemyItem* alch)
    {
        if (!alch) return {};
        for (const auto& entry : g_PinnedItems) {
            if (!entry.enabled) continue;
            if (auto mgef = SlotRestoreMgef(entry.id); mgef && PotionMatchesSlot(alch, mgef))
                return entry.id;
        }
        return {};
    }

    void Init()
    {
        g_PinnedItems.clear();
        g_CachedCounts.clear();
        g_ForcePinnedPos = true;
        g_Loaded = false;

        const char* jsonPath = "Data/SKSE/Plugins/PartySheet_Widgets.json";
        std::ifstream file(jsonPath);
        if (!file.is_open()) {
            logger::warn("WidgetPinnedItems: Could not open {}", jsonPath);
            return;
        }

        std::stringstream buf;
        buf << file.rdbuf();
        std::string content = buf.str();
        file.close();

        std::string pinnedArray = WidgetJsonParser::ExtractArray(content, "pinned");
        if (pinnedArray.empty()) {
            logger::warn("WidgetPinnedItems: No 'pinned' array in JSON");
            return;
        }

        for (const auto& obj : WidgetJsonParser::SplitObjects(pinnedArray)) {
            PinnedEntry entry;
            entry.id      = WidgetJsonParser::ExtractValue(obj, "id");
            {
                std::string jsonLabel = WidgetJsonParser::ExtractValue(obj, "label");
                std::string locKey    = "Widgets." + entry.id;
                std::string locLabel  = LocalizationManager::Get(locKey);
                entry.label = (locLabel != locKey) ? locLabel : (!jsonLabel.empty() ? jsonLabel : entry.id);
            }
            entry.enabled = WidgetJsonParser::ParseBool(WidgetJsonParser::ExtractValue(obj, "enabled"), true);

            {
                std::wstring wkey(entry.id.begin(), entry.id.end());
                wkey += L"_enabled";
                const int ini = SettingsIni::ReadPresetIntOnce(L"PinnedItems", wkey.c_str(), -1);
                if (ini != -1)
                    entry.enabled = (ini != 0);
            }

            std::string goalStr = WidgetJsonParser::ExtractValue(obj, "goal");
            try { entry.goal = std::stoi(goalStr); } catch (...) { entry.goal = 0; }

            if (entry.id.empty() || entry.label.empty()) {
                logger::warn("WidgetPinnedItems: Skipping invalid entry (id='{}' label='{}')",
                    entry.id, entry.label);
                continue;
            }

            std::string itemsArray = WidgetJsonParser::ExtractArray(obj, "items");
            for (const auto& itemObj : WidgetJsonParser::SplitObjects(itemsArray)) {
                PinnedItemRef ref;
                ref.mod         = WidgetJsonParser::ExtractValue(itemObj, "mod");
                ref.localFormID = WidgetJsonParser::ParseHexFormID(WidgetJsonParser::ExtractValue(itemObj, "formId"));
                if (!ref.mod.empty() && ref.localFormID != 0) {
                    ref.resolvedFormID = PlayerStatus::ResolveFormID(ref.mod, ref.localFormID);
                    if (ref.resolvedFormID == 0) {
                        auto* dh = RE::TESDataHandler::GetSingleton();
                        bool dataReady = dh && dh->GetLoadedModCount() > 0;
                        if (dataReady)
                            logger::warn("WidgetPinnedItems: Could not resolve item formId {:X} in '{}'",
                                ref.localFormID, ref.mod);
                        else
                            logger::trace("WidgetPinnedItems: Deferred item formId {:X} in '{}' (pre-load)",
                                ref.localFormID, ref.mod);
                    }
                    entry.items.push_back(ref);
                }
            }

            std::string iconPath = "Data\\Interface\\PartySheet\\Widgets\\" + entry.id + ".png";
            entry.icon = TextureManager::GetTexture(iconPath);

            g_PinnedItems.push_back(std::move(entry));
        }

        g_CachedCounts.resize(g_PinnedItems.size(), 0);
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (player) {
            for (size_t i = 0; i < g_PinnedItems.size(); ++i)
                g_CachedCounts[i] = GetItemCount(g_PinnedItems[i]);
        }
        g_Loaded = true;
    }

    ConsumeResult TryConsume(const std::string& entryId)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return ConsumeResult::kNone;

        if (InVanillaCooldown(entryId)) return ConsumeResult::kBlocked;

        if (auto mgef = SlotRestoreMgef(entryId)) {
            auto* alch = BestHealPotion(mgef);
            {
                const char* nm = alch ? alch->GetName() : nullptr;
                logger::debug("[PotionPress] slot='{}' mgef=0x{:05X} pick={} deficit={:.1f}",
                              entryId, mgef, (nm && *nm) ? nm : "<none owned>", SlotDeficit(mgef));
            }
            if (!alch) {
                StartDecline(entryId);
                return ConsumeResult::kNone;
            }

            if (SlotDeficit(mgef) < 1.0f) {
                StartDecline(entryId);
                return ConsumeResult::kDeclined;
            }

            if (PotionAnimationBridge::TryDrinkWithAnimation(alch, entryId)) {
                return ConsumeResult::kDrankAnimated;
            }

            if (PotionAnimationBridge::IsBusy()) {
                return ConsumeResult::kBlocked;
            }

            player->DrinkPotion(alch, nullptr);
            StartVanillaCooldown(entryId);
            return ConsumeResult::kDrankVanilla;
        }

        for (auto& e : g_PinnedItems) {
            if (e.id != entryId) continue;

            for (const auto& ref : e.items) {
                if (ref.resolvedFormID == 0) continue;
                auto* alch = RE::TESForm::LookupByID<RE::AlchemyItem>(ref.resolvedFormID);
                if (!alch) continue;
                if (player->GetItemCount(alch) <= 0) continue;

                if (PotionAnimationBridge::TryDrinkWithAnimation(alch, entryId)) {
                    return ConsumeResult::kDrankAnimated;
                }

                if (PotionAnimationBridge::IsBusy()) {
                    return ConsumeResult::kBlocked;
                }

                player->DrinkPotion(alch, nullptr);
                StartVanillaCooldown(entryId);
                return ConsumeResult::kDrankVanilla;
            }
            StartDecline(entryId);
            return ConsumeResult::kNone;
        }
        return ConsumeResult::kNone;
    }

    void RenderPreview()
    {
        RenderPinnedWindow(true);
    }

    void RenderHUD()
    {
        auto* settings = Settings::GetSingleton();
        if (!settings || !settings->enablePinnedWidget) return;
        if (MenuUtils::ShouldBlockOverlay()) return;
        if (settings->ptHideInCombat) {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (player && player->IsInCombat()) return;
        }
        if (!g_Loaded || g_PinnedItems.empty()) return;
        if (!HUDVisibility::IsVisible(HUDVisibility::WidgetID::PinnedItems)) return;

        bool anyVisible = false;
        for (const auto& e : g_PinnedItems) {
            if (e.enabled) { anyVisible = true; break; }
        }
        if (!anyVisible) return;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, HUDVisibility::GetAlpha(HUDVisibility::WidgetID::PinnedItems));
        RenderPinnedWindow(false);
        ImGui::PopStyleVar();

        ImGui::EndFrame();
        ImGui::Render();
        HUDVisibility::ApplyAlphaToDrawData(HUDVisibility::GetAlpha(HUDVisibility::WidgetID::PinnedItems));
        ImGuiRenderer::RenderToBoundTarget(ImGui::GetDrawData());
    }

}
