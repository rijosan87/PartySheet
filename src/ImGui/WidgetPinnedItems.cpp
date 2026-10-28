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
#include "../../includes/PotionAnimationBridge.h"

#include <imgui.h>
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
                dl->AddCircleFilled(p, rad, IM_COL32(r8, g8, b8, a), 12);
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
            if (id == "health_potion")  { key = s->potionHealthKey;  mod = s->potionHealthModifier;  return true; }
            if (id == "magicka_potion") { key = s->potionMagickaKey; mod = s->potionMagickaModifier; return true; }
            if (id == "stamina_potion") { key = s->potionStaminaKey; mod = s->potionStaminaModifier; return true; }
            return false;
        }

        static std::string GetModifierWord(int mod)
        {
            switch (mod) {
                case 1: return LocalizationManager::Get("Hotkeys.ModAlt");
                case 2: return LocalizationManager::Get("Hotkeys.ModCtrl");
                case 3: return LocalizationManager::Get("Hotkeys.ModShift");
                default: return "";
            }
        }

        static HUDDraw::BadgeStyle MakeBadgeStyle(std::uint32_t bgArgb, std::uint32_t textArgb,
                                                  ImFont* font, float fontScale)
        {
            HUDDraw::BadgeStyle bs;
            bs.font      = font;
            bs.fontScale = fontScale;
            bs.padX      = ScalePx(4.0f);
            bs.padY      = ScalePx(2.5f);
            const std::uint8_t a = (bgArgb >> 24) & 0xFF, r = (bgArgb >> 16) & 0xFF,
                               g = (bgArgb >> 8) & 0xFF,  b =  bgArgb        & 0xFF;
            auto lift = [](std::uint8_t c) { return static_cast<std::uint32_t>((std::min)(255.0f, c * 1.20f + 14.0f)); };
            auto sink = [](std::uint8_t c) { return static_cast<std::uint32_t>(c * 0.55f); };
            bs.bgTop    = (static_cast<std::uint32_t>(a) << 24) | (lift(r) << 16) | (lift(g) << 8) | lift(b);
            bs.bgBottom = 0xFF000000u | (sink(r) << 16) | (sink(g) << 8) | sink(b);
            bs.textColor = textArgb;
            return bs;
        }

        static void DrawHotkeyBadge(float screenX, float screenY,
                                    int key, int mod, float scale, std::uint32_t bgArgb)
        {
            if (key < 1) return;

            std::string label = GetModifierWord(mod);
            if (!label.empty()) label += " + ";
            label += ImGuiRenderer::DIKToKeyName(key);

            auto bs = MakeBadgeStyle(bgArgb, 0xFFFFFFFF, nullptr, scale);
            bs.rounding = 0.0f;
            bs.padX = ScalePx(5.0f); bs.padY = ScalePx(3.0f);
            HUDDraw::DrawBadge(ImGui::GetForegroundDrawList(),
                               ImVec2(screenX, screenY), 0, label.c_str(), bs);
        }

        static void RenderPinnedWindow(bool allowDrag)
        {
            auto* settings = Settings::GetSingleton();
            if (!settings || !settings->enablePinnedWidget) return;
            if (MenuUtils::ShouldBlockOverlay()) return;

            int visibleCount = 0;
            for (const auto& e : g_PinnedItems) {
                if (e.enabled) ++visibleCount;
            }
            if (visibleCount == 0) return;

            ImVec2 targetPos(static_cast<float>(settings->ptPosX),
                             static_cast<float>(settings->ptPosY));
            if (g_ForcePinnedPos) {
                ImGui::SetNextWindowPos(targetPos, ImGuiCond_Always);
                g_ForcePinnedPos = false;
            } else {
                ImGui::SetNextWindowPos(targetPos, ImGuiCond_FirstUseEver);
            }

            ImGuiWindowFlags flags =
                ImGuiWindowFlags_NoTitleBar      |
                ImGuiWindowFlags_NoResize        |
                ImGuiWindowFlags_AlwaysAutoResize|
                ImGuiWindowFlags_NoScrollbar     |
                ImGuiWindowFlags_NoSavedSettings;
            if (!allowDrag) flags |= ImGuiWindowFlags_NoMove;

            ImGui::SetNextWindowBgAlpha(0.0f);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

            if (ImGui::Begin("##pinneditems", nullptr, flags)) {

                ImGui::SetWindowFontScale(settings->ptLabelScale);

                bool    kBadgeCountStyle = (settings->ptCountStyle >= 1);
                ImFont* headingFont      = ImGuiRenderer::GetHeadingFont();
                ImFont* countFont        = headingFont ? headingFont : ImGui::GetFont();
                // Height of a count-font number (used for card sizing in badge mode; falls back to base font for CJK)
                float badgeNumH = countFont
                    ? (countFont->FontSize * settings->ptCountScale + ScalePx(4.0f))
                    : ImGui::GetTextLineHeightWithSpacing();

                // padY must be large enough that a badge overlapping the top of the icon isn't clipped
                float padY = ScalePx(8.0f);
                if (kBadgeCountStyle) {
                    float badgeOverhang = badgeNumH * 0.4f;
                    if (badgeOverhang > padY) padY = badgeOverhang;
                }

                IconRenderer::Style iconTemplate;
                IconRenderer::SetZoom(iconTemplate, settings->ptIconZoom, settings->ptIconFocusU, settings->ptIconFocusV);

                float  gap        = ScalePx(static_cast<float>(settings->ptRowSpacing));
                float  padX       = ScalePx(8.0f);
                float  padBottom  = ScalePx(static_cast<float>(settings->ptPaddingBottom));
                float  padRight   = ScalePx(10.0f);
                float  itemGap    = ScalePx(5.0f);
                float  iconSz     = ScalePx(static_cast<float>(settings->ptIconSize));
                float  lineH      = ImGui::GetTextLineHeightWithSpacing();
                float  itemW      = ScalePx(static_cast<float>(settings->ptItemWidth));
                bool   rowFlow    = (settings->ptDirection == 1);
                bool   vertical   = (settings->ptCardStyle == 1) || rowFlow;
                float  curCardX   = 0.0f;
                ImDrawList* dl    = ImGui::GetWindowDrawList();
                ImVec2 winPos     = ImGui::GetWindowPos();
                ImVec4 iconBgVec4 = ColorUtils::ArgbToVec4(settings->ptIconBgColor);
                ImU32  iconBgCol  = ImGui::ColorConvertFloat4ToU32(iconBgVec4);
                ImVec4 iconBdrVec4 = ColorUtils::ArgbToVec4(settings->ptIconBorderColor);
                ImU32  iconBdrCol  = ImGui::ColorConvertFloat4ToU32(iconBdrVec4);
                const HUDDraw::BadgeStyle countBadgeStyle = MakeBadgeStyle(
                    settings->ptBadgeBgColor, 0xFFFFFFFF, headingFont, settings->ptCountScale);

                const std::string drinkingId = PotionAnimationBridge::ActiveEntryId();

                bool firstVisible = true;
                float maxCardHeight = 0.0f;
                for (size_t i = 0; i < g_PinnedItems.size(); ++i) {
                    auto& e = g_PinnedItems[i];
                    if (!e.enabled) continue;

                    int  count    = (i < g_CachedCounts.size()) ? g_CachedCounts[i] : 0;
                    bool hasGoal  = (e.goal > 0);
                    bool goalMet  = hasGoal && (count >= e.goal);

                    ID3D11ShaderResourceView* dispIcon = e.icon;
                    ImVec4 textCol = ColorUtils::RgbToVec4(goalMet ? settings->ptGoalMetColor : settings->ptTextColor);

                    // Badge style: all items show count in heading font (or base font when heading is null, e.g. CJK)
                    bool useBadge = kBadgeCountStyle;

                    char countBuf[32];
                    if (hasGoal && !kBadgeCountStyle)
                        snprintf(countBuf, sizeof(countBuf), "%d / %d", count, e.goal);
                    else
                        snprintf(countBuf, sizeof(countBuf), "%d", count);

                    if (rowFlow) {
                        if (!firstVisible) curCardX += gap;
                        ImGui::SetCursorPos(ImVec2(curCardX, 0.0f));
                    } else {
                        if (!firstVisible) ImGui::SetCursorPosY(ImGui::GetCursorPosY() + gap);
                    }
                    firstVisible = false;

                    float cardStartY = rowFlow ? 0.0f : ImGui::GetCursorPosY();

                    if (vertical) {
                        float contentW  = std::max(iconSz, itemW);
                        float cardWidth = contentW + padX + padRight;

                        float labelH = 0.0f;
                        if (settings->ptShowLabel) {
                            ImVec2 wrappedSz = ImGui::CalcTextSize(e.label.c_str(), nullptr, false, itemW);
                            labelH = wrappedSz.y + itemGap;
                        }
                        float countRowH = useBadge && e.icon ? 0.0f : (useBadge ? badgeNumH : lineH);
                        bool iconNeedsGap = e.icon && (settings->ptShowLabel || countRowH > 0.0f);
                        float cardHeight = padY
                            + (e.icon ? iconSz + (iconNeedsGap ? itemGap : 0.0f) : 0.0f)
                            + labelH + countRowH + padBottom;

                        ImGui::Dummy(ImVec2(rowFlow ? curCardX + cardWidth : cardWidth, 0.0f));

                        ImVec2 cMin = ImVec2(winPos.x + curCardX, winPos.y + cardStartY);
                        ImVec2 cMax = ImVec2(cMin.x + cardWidth, cMin.y + cardHeight);

                        float curY    = cardStartY + padY;
                        float centerX = curCardX + (cardWidth - iconSz) * 0.5f;

                        bool hasContentAfterIcon = settings->ptShowLabel || (!useBadge || !e.icon);
                        if (e.icon) {
                            float screenIconX = winPos.x + centerX;
                            float screenIconY = winPos.y + curY;
                            IconRenderer::Style icon;
                            icon.texture      = dispIcon;
                            icon.size         = iconSz;
                            icon.bgColor      = iconBgCol;
                            icon.chip         = true;
                            icon.glowEnabled  = settings->ptIconGlowEnabled;
                            icon.borderColor  = iconBdrCol;
                            icon.borderWidth  = ScalePxInt0(settings->ptIconBorderWidth);
                            icon.borderRadius = iconSz * settings->ptIconBorderRadius * 0.01f;
                            icon.uv0          = iconTemplate.uv0;
                            icon.uv1          = iconTemplate.uv1;
                            // Pickup bounce plus the decline dip, SUMMED: a pickup landing
                            icon.bounceOffsetY = ScalePx(UIAnimation::GetBounceOffsetY("pt_bounce_" + e.id))
                                               + DeclineOffsetY(e.id, iconSz);
                            const bool drinking = (!drinkingId.empty() && drinkingId == e.id) ||
                                                   InVanillaCooldown(e.id);
                            const ImU32 fxTint  = drinking ? DrinkFxTint(e.id) : 0;
                            ImVec2 iconAt(screenIconX, screenIconY + icon.bounceOffsetY);
                            if (drinking) DrawDrinkHalo(dl, iconAt, iconSz, fxTint);
                            ImGui::SetCursorScreenPos(ImVec2(screenIconX, screenIconY));
                            IconRenderer::Draw(dl, ImVec2(screenIconX, screenIconY), icon);
                            if (drinking) DrawDrinkBubbles(dl, iconAt, iconSz, fxTint);
                            if (const float dt = DeclineProgress(e.id); dt > 0.0f) {
                                const float k = std::sin(dt * 3.14159265f);
                                dl->AddRectFilled(ImVec2(screenIconX, screenIconY),
                                                  ImVec2(screenIconX + iconSz, screenIconY + iconSz),
                                                  IM_COL32(10, 12, 16, static_cast<int>(120.0f * k)),
                                                  icon.borderRadius);
                            }
                            curY += iconSz + (hasContentAfterIcon ? itemGap : 0.0f);
                        }

                        // Row: push text 10px down from bg top stripe
                        if (rowFlow) curY += ScalePx(10.0f);

                        if (settings->ptShowLabel) {
                            float labelX = curCardX + (cardWidth - itemW) * 0.5f;
                            ImVec2 lblSz = ImGui::CalcTextSize(e.label.c_str(), nullptr, false, itemW);
                            float centeredX = labelX + (itemW - std::min(lblSz.x, itemW)) * 0.5f;
                            ImVec2 labelPos = ImVec2(winPos.x + centeredX, winPos.y + curY);
                            ImU32 labelCol = ImGui::ColorConvertFloat4ToU32(textCol);
                            dl->AddText(ImVec2(labelPos.x + 1.0f, labelPos.y + 1.0f), IM_COL32(0, 0, 0, 255), e.label.c_str());
                            dl->AddText(labelPos, labelCol, e.label.c_str());
                            ImGui::SetCursorPos(ImVec2(centeredX, curY));
                            ImGui::Dummy(ImVec2(lblSz.x, lblSz.y));
                            curY += labelH;
                        }

                        if (useBadge && e.icon) {
                            ImVec2 pill = HUDDraw::MeasureBadge(countBuf, countBadgeStyle);
                            float  iconScreenX = winPos.x + centerX;
                            float  iconScreenY = winPos.y + (cardStartY + padY);
                            // Position pill so it overlaps top-left of icon
                            float  badgeX = iconScreenX - pill.x * 0.3f + ScalePxSigned(settings->ptBadgeOffsetX);
                            float  badgeY = iconScreenY - pill.y * 0.4f + ScalePxSigned(settings->ptBadgeOffsetY);
                            HUDDraw::DrawBadge(ImGui::GetForegroundDrawList(),
                                               ImVec2(badgeX, badgeY), 0, countBuf, countBadgeStyle);
                        } else if (useBadge) {
                            ImVec2 pill = HUDDraw::MeasureBadge(countBuf, countBadgeStyle);
                            float  badgeX  = winPos.x + curCardX + (cardWidth - pill.x) * 0.5f + ScalePxSigned(settings->ptBadgeOffsetX);
                            float  badgeY  = winPos.y + curY + ScalePxSigned(settings->ptBadgeOffsetY);
                            HUDDraw::DrawBadge(ImGui::GetForegroundDrawList(),
                                               ImVec2(badgeX, badgeY), 0, countBuf, countBadgeStyle);
                            ImGui::Dummy(ImVec2(pill.x, pill.y));
                        } else {
                            ImGui::SetWindowFontScale(settings->ptCountScale);
                            float textW = ImGui::CalcTextSize(countBuf).x;
                            float textH = ImGui::GetTextLineHeight();
                            ImVec2 countPos = ImVec2(winPos.x + curCardX + (cardWidth - textW) * 0.5f, winPos.y + curY);
                            ImU32 countCol = ImGui::ColorConvertFloat4ToU32(textCol);
                            dl->AddText(ImVec2(countPos.x + 1.0f, countPos.y + 1.0f), IM_COL32(0, 0, 0, 255), countBuf);
                            dl->AddText(countPos, countCol, countBuf);
                            ImGui::SetCursorPos(ImVec2(curCardX + (cardWidth - textW) * 0.5f, curY));
                            ImGui::Dummy(ImVec2(textW, textH));
                            ImGui::SetWindowFontScale(settings->ptLabelScale);
                        }

                        if (settings->ptShowHotkeyBadge) {
                            int hkKey, hkMod;
                            if (GetEntryHotkey(e.id, settings, hkKey, hkMod))
                                DrawHotkeyBadge(
                                    winPos.x + curCardX + ScalePxSigned(settings->ptHotkeyBadgeOffsetX),
                                    winPos.y + cardStartY + ScalePxSigned(settings->ptHotkeyBadgeOffsetY),
                                    hkKey, hkMod, settings->ptHotkeyBadgeScale,
                                    settings->ptHotkeyBadgeBgColor);
                        }

                        if (rowFlow) { curCardX += cardWidth; maxCardHeight = std::max(maxCardHeight, cardHeight); }
                        else ImGui::SetCursorPosY(cardStartY + cardHeight);

                    } else {
                        // ---- HORIZONTAL layout ----
                        float textX      = e.icon ? (padX + iconSz + ScalePx(8.0f)) : padX;
                        float hBarW      = itemW;
                        float cardWidth  = textX + hBarW + padRight;

                        float labelH = 0.0f;
                        if (settings->ptShowLabel) {
                            ImVec2 wrappedSz = ImGui::CalcTextSize(e.label.c_str(), nullptr, false, hBarW);
                            labelH = wrappedSz.y + itemGap;
                        }
                        float countRowH  = useBadge && e.icon ? 0.0f : (useBadge ? badgeNumH : lineH);
                        float textRows   = labelH + countRowH;
                        float cardHeight = std::max(iconSz, textRows) + padY + padBottom;

                        ImGui::Dummy(ImVec2(rowFlow ? curCardX + cardWidth : cardWidth, 0.0f));

                        ImVec2 cMin = ImVec2(winPos.x + curCardX, winPos.y + cardStartY);
                        ImVec2 cMax = ImVec2(cMin.x + cardWidth, cMin.y + cardHeight);

                        float rowStartY = cardStartY + padY;

                        if (e.icon) {
                            float screenIconX = winPos.x + curCardX + padX;
                            float screenIconY = winPos.y + rowStartY;
                            IconRenderer::Style icon;
                            icon.texture      = dispIcon;
                            icon.size         = iconSz;
                            icon.bgColor      = iconBgCol;
                            icon.chip         = true;
                            icon.glowEnabled  = settings->ptIconGlowEnabled;
                            icon.borderColor  = iconBdrCol;
                            icon.borderWidth  = ScalePxInt0(settings->ptIconBorderWidth);
                            icon.borderRadius = iconSz * settings->ptIconBorderRadius * 0.01f;
                            icon.uv0          = iconTemplate.uv0;
                            icon.uv1          = iconTemplate.uv1;
                            // Pickup bounce plus the decline dip, SUMMED: a pickup landing
                            icon.bounceOffsetY = ScalePx(UIAnimation::GetBounceOffsetY("pt_bounce_" + e.id))
                                               + DeclineOffsetY(e.id, iconSz);
                            const bool drinking = (!drinkingId.empty() && drinkingId == e.id) ||
                                                  InVanillaCooldown(e.id);
                            const ImU32 fxTint  = drinking ? DrinkFxTint(e.id) : 0;
                            ImVec2 iconAt(screenIconX, screenIconY + icon.bounceOffsetY);
                            if (drinking) DrawDrinkHalo(dl, iconAt, iconSz, fxTint);
                            ImGui::SetCursorScreenPos(ImVec2(screenIconX, screenIconY));
                            IconRenderer::Draw(dl, ImVec2(screenIconX, screenIconY), icon);
                            if (drinking) DrawDrinkBubbles(dl, iconAt, iconSz, fxTint);
                            if (const float dt = DeclineProgress(e.id); dt > 0.0f) {
                                const float k = std::sin(dt * 3.14159265f);
                                dl->AddRectFilled(ImVec2(screenIconX, screenIconY),
                                                  ImVec2(screenIconX + iconSz, screenIconY + iconSz),
                                                  IM_COL32(10, 12, 16, static_cast<int>(120.0f * k)),
                                                  icon.borderRadius);
                            }
                        }

                        float adjustedTextX = curCardX + textX;
                        float textBaseY     = rowStartY;
                        float textCurY = textBaseY;
                        if (settings->ptShowLabel) {
                            float offsetY = e.icon ? (iconSz - textRows) * 0.5f : 0.0f;
                            textCurY = textBaseY + (offsetY > 0.0f ? offsetY : 0.0f);
                            ImVec2 labelPos = ImVec2(winPos.x + adjustedTextX, winPos.y + textCurY);
                            ImU32 labelCol = ImGui::ColorConvertFloat4ToU32(textCol);
                            ImVec2 lblSz = ImGui::CalcTextSize(e.label.c_str(), nullptr, false, hBarW);
                            dl->AddText(ImVec2(labelPos.x + 1.0f, labelPos.y + 1.0f), IM_COL32(0, 0, 0, 255), e.label.c_str());
                            dl->AddText(labelPos, labelCol, e.label.c_str());
                            ImGui::SetCursorPos(ImVec2(adjustedTextX, textCurY));
                            ImGui::Dummy(ImVec2(lblSz.x, lblSz.y));
                            textCurY += labelH;
                        } else if (e.icon) {
                            float offsetY = (iconSz - countRowH) * 0.5f;
                            textCurY = textBaseY + (offsetY > 0.0f ? offsetY : 0.0f);
                        }

                        if (useBadge && e.icon) {
                            ImVec2 pill = HUDDraw::MeasureBadge(countBuf, countBadgeStyle);
                            float  iconScreenX = winPos.x + curCardX + padX;
                            float  iconScreenY = winPos.y + rowStartY;
                            float  badgeX = iconScreenX - pill.x * 0.3f + ScalePxSigned(settings->ptBadgeOffsetX);
                            float  badgeY = iconScreenY - pill.y * 0.4f + ScalePxSigned(settings->ptBadgeOffsetY);
                            HUDDraw::DrawBadge(ImGui::GetForegroundDrawList(),
                                               ImVec2(badgeX, badgeY), 0, countBuf, countBadgeStyle);
                        } else if (useBadge) {
                            ImVec2 pill = HUDDraw::MeasureBadge(countBuf, countBadgeStyle);
                            HUDDraw::DrawBadge(ImGui::GetForegroundDrawList(),
                                               ImVec2(winPos.x + adjustedTextX + ScalePxSigned(settings->ptBadgeOffsetX),
                                                      winPos.y + textCurY + ScalePxSigned(settings->ptBadgeOffsetY)),
                                               0, countBuf, countBadgeStyle);
                            ImGui::Dummy(ImVec2(pill.x, pill.y));
                        } else {
                            ImGui::SetWindowFontScale(settings->ptCountScale);
                            float textW = ImGui::CalcTextSize(countBuf).x;
                            float textH = ImGui::GetTextLineHeight();
                            ImVec2 countPos = ImVec2(winPos.x + adjustedTextX, winPos.y + textCurY);
                            ImU32 countCol = ImGui::ColorConvertFloat4ToU32(textCol);
                            dl->AddText(ImVec2(countPos.x + 1.0f, countPos.y + 1.0f), IM_COL32(0, 0, 0, 255), countBuf);
                            dl->AddText(countPos, countCol, countBuf);
                            ImGui::SetCursorPos(ImVec2(adjustedTextX, textCurY));
                            ImGui::Dummy(ImVec2(textW, textH));
                            ImGui::SetWindowFontScale(settings->ptLabelScale);
                        }

                        if (settings->ptShowHotkeyBadge) {
                            int hkKey, hkMod;
                            if (GetEntryHotkey(e.id, settings, hkKey, hkMod))
                                DrawHotkeyBadge(
                                    winPos.x + curCardX + ScalePxSigned(settings->ptHotkeyBadgeOffsetX),
                                    winPos.y + cardStartY + ScalePxSigned(settings->ptHotkeyBadgeOffsetY),
                                    hkKey, hkMod, settings->ptHotkeyBadgeScale,
                                    settings->ptHotkeyBadgeBgColor);
                        }

                        if (rowFlow) { curCardX += cardWidth; maxCardHeight = std::max(maxCardHeight, cardHeight); }
                        else ImGui::SetCursorPosY(cardStartY + cardHeight);
                    }
                }

                if (rowFlow && maxCardHeight > 0.0f)
                    ImGui::SetCursorPosY(maxCardHeight);

                ImGui::SetWindowFontScale(1.0f);

                if (allowDrag) {
                    ImVec2 pos = ImGui::GetWindowPos();
                    settings->ptPosX = static_cast<int>(pos.x);
                    settings->ptPosY = static_cast<int>(pos.y);
                }
            }
            ImGui::End();

            ImGui::PopStyleVar();
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
            if (!alch) return ConsumeResult::kNone;

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
