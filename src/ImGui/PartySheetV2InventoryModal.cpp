#include "../../includes/ImGui/PartySheetV2InventoryModal.h"
#include "../../includes/ImGui/PartySheetV2Tooltip.h"
#include "../../includes/ImGui/Renderer.h"
#include "../../includes/ImGui/FollowerHUD.h"
#include "../../includes/PNGIIBridge.h"
#include "../../includes/Settings.h"
#include "../../includes/TextureManager.h"
#include "../../includes/LocalizationManager.h"
#include "../../includes/Utils/GearCatalog.h"
#include "../../includes/Utils/GearSwitcher.h"
#include "../../includes/Utils/HUDDraw.h"

#include <array>
#include <bit>

#include <imgui.h>
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <string>
#include <vector>
#include <algorithm>
#include <optional>
#include <chrono>
#include <cfloat>
#include <climits>
#include <memory>
#include "../../includes/UIAnimation.h"

namespace logger = SKSE::log;

namespace PartySheetV2 {

    static bool s_InvOpen   = false;
    static int  s_InvFilter = 0;
    static std::optional<GearCatalog::Card> s_SlotCard;
    static std::optional<GearCatalog::Card> s_SpellHand;
    static bool s_SpellWindow = false;
    static GearCatalog::Card s_BrowseHand = GearCatalog::Card::RightHand;
    struct PendingSend {
        bool                     openPopup = false;
        RE::FormID               to        = 0;
        GearCatalog::InstanceKey key;
        std::string              name;
        int                      max       = 1;
        int                      amount    = 1;
    };
    static PendingSend s_PendingSend;
    static GearCatalog::SlotTag s_SlotTag = GearCatalog::SlotTag::None;
    static GearCatalog::PotionTag s_PotionTag = GearCatalog::PotionTag::None;

    static HUDDraw::IconFx IconFxDefault() {
        auto* s = Settings::GetSingleton();
        HUDDraw::IconFx fx{};
        fx.duotone      = s->psV2IconDuotone;
        fx.shadow       = s->psV2IconShadow;
        fx.outline      = s->psV2IconOutline;
        fx.outlineWidth = s->psV2IconOutlineWidth;
        fx.outlineColor = s->psV2IconOutlineColor;
        fx.glow         = false;
        fx.gloss        = false;
        return fx;
    }

    static std::uint64_t s_FrameCounter      = 0;
    static std::uint32_t s_LastActorFormID   = 0;
    static int           s_NullActorStreak   = 0;
    static int           s_LastItemCount     = -1;
    static int           s_LastVisibleCount  = -1;

    struct GhostTile {
        GearCatalog::InventoryItem             item;
        int                                   slot = 0;
        std::chrono::steady_clock::time_point at;
    };
    static constexpr float kGhostSeconds = 0.35f;
    static std::vector<GhostTile>                          s_Ghosts;
    static std::shared_ptr<const GearCatalog::ActorGear>   s_PrevGear;
    static std::vector<int>                                s_PrevShownRows;
    static std::uint64_t                                   s_PrevViewSig = 0;

    static void ClearTileFeedback() {
        s_Ghosts.clear();
        s_PrevGear.reset();
        s_PrevShownRows.clear();
        s_PrevViewSig = 0;
    }

    static bool SameIgnoringPoison(const GearCatalog::InstanceKey& a, const GearCatalog::InstanceKey& b) {
        return a.baseID == b.baseID && a.health == b.health && a.enchantID == b.enchantID &&
               a.customName == b.customName;
    }

    static std::string TileKey(const char* part, RE::FormID actorID, const GearCatalog::InstanceKey& k) {
        char buf[96];
        snprintf(buf, sizeof(buf), "psv2inv_%s_%08X_%08X_%.1f_%08X_", part, actorID, k.baseID, k.health, k.enchantID);
        return std::string(buf) + k.customName;
    }

    void OpenInventoryModal()  {
        s_InvOpen = true;
        PNGIIBridge::Clear();
        ClearTileFeedback();
        s_InvFilter = 0;
        s_PotionTag = GearCatalog::PotionTag::None;
        s_SlotCard.reset();
        s_SpellHand.reset();
        s_SpellWindow = false;
        s_BrowseHand  = GearCatalog::Card::RightHand;
        s_LastActorFormID  = 0;
        s_NullActorStreak  = 0;
        s_LastItemCount    = -1;
        s_LastVisibleCount = -1;
    }
    void OpenInventoryModal(GearCatalog::Card card) {
        OpenInventoryModal();
        s_SlotCard = card;
        s_SlotTag  = GearCatalog::SlotTag::None;
    }
    void OpenSpellPicker(GearCatalog::Card hand) {
        OpenInventoryModal();
        s_SpellHand = hand;
        ResetSpellPicker();
    }
    void OpenSpellWindow() {
        OpenInventoryModal();
        s_SpellWindow = true;
        ResetSpellPicker();
    }
    bool IsSpellWindowOpen() { return s_InvOpen && s_SpellWindow; }
    void CloseInventoryModal() {
        s_InvOpen = false;
    }
    bool IsInventoryModalOpen() { return s_InvOpen; }

    static constexpr ImU32 kAccent    = IM_COL32(212, 175,  55, 255);
    static constexpr ImU32 kTextMain  = IM_COL32(240, 240, 240, 255);
    static constexpr ImU32 kTextMuted = IM_COL32(136, 136, 136, 255);
    static constexpr ImU32 kEnchanted = IM_COL32(150, 140, 255, 255);

    // ── Font scale helpers ────────────────────────────────────────────────────
    static float s_EffectiveScale = 1.0f;
    static float FontBody()  { auto* s = Settings::GetSingleton(); return s->psV2FontBody  * s_EffectiveScale; }
    static float FontHeading() { auto* s = Settings::GetSingleton(); return s->psV2FontHeading * s_EffectiveScale; }
    static float FontSmall() { auto* s = Settings::GetSingleton(); return s->psV2FontSmall * s_EffectiveScale; }

    using GearCatalog::InventoryItem;
    using GearCatalog::ItemTab;
    using GearCatalog::ItemType;

    static const std::string& TypeLabel(ItemType type) {
        static std::string s_labels[11];
        static bool        s_loaded = false;
        if (!s_loaded) {
            s_labels[static_cast<int>(ItemType::Weapon)]     = LocalizationManager::Get("Inventory.TypeWeapon");
            s_labels[static_cast<int>(ItemType::Armor)]      = LocalizationManager::Get("Inventory.TypeArmor");
            s_labels[static_cast<int>(ItemType::Potion)]     = LocalizationManager::Get("Inventory.TypePotion");
            s_labels[static_cast<int>(ItemType::Food)]       = LocalizationManager::Get("Inventory.TypeFood");
            s_labels[static_cast<int>(ItemType::Ammo)]       = LocalizationManager::Get("Inventory.TypeAmmo");
            s_labels[static_cast<int>(ItemType::Ingredient)] = LocalizationManager::Get("Inventory.TypeIngredient");
            s_labels[static_cast<int>(ItemType::SoulGem)]    = LocalizationManager::Get("Inventory.TypeSoulGem");
            s_labels[static_cast<int>(ItemType::Key)]        = LocalizationManager::Get("Inventory.TypeKey");
            s_labels[static_cast<int>(ItemType::Book)]       = LocalizationManager::Get("Inventory.TypeBook");
            s_labels[static_cast<int>(ItemType::Scroll)]     = LocalizationManager::Get("Inventory.TypeScroll");
            s_labels[static_cast<int>(ItemType::Misc)]       = LocalizationManager::Get("Inventory.TypeMisc");
            s_loaded = true;
        }
        return s_labels[static_cast<int>(type)];
            }

    static bool ListView() { return !s_SlotCard && (s_InvFilter == 3 || s_InvFilter == 4 || s_InvFilter == 5); }

    struct EffectChip { std::string text; ImU32 color; };

    static std::vector<EffectChip> EffectChips(RE::TESForm* form) {
        std::vector<EffectChip> chips;
        auto* magic = form ? form->As<RE::MagicItem>() : nullptr;
        if (!magic) return chips;
        auto* ingredient = form->As<RE::IngredientItem>();
        int unknown = 0;
        for (std::uint32_t i = 0; i < magic->effects.size(); ++i) {
            auto* eff  = magic->effects[i];
            auto* base = eff ? eff->baseEffect : nullptr;
            if (!base || base->data.flags.any(RE::EffectSetting::EffectSettingData::Flag::kHideInUI)) continue;
            if (ingredient && !(ingredient->gamedata.knownEffectFlags & (1u << i))) { ++unknown; continue; }
            const char* name = base->GetFullName();
            if (!name || !name[0]) continue;
            std::string text = name;
            const float mag = Tip::DisplayMagnitude(base, eff->effectItem.magnitude);
            if (!ingredient && mag >= 1.0f && mag < 1000.0f) text += " " + std::to_string(static_cast<int>(mag + 0.5f));
            const bool bad = EffectCategories::IsDebuff(base, eff->effectItem.magnitude);
            chips.push_back({ std::move(text), bad ? Tip::kDebuff : Tip::kBuff });
            if (!ingredient && eff->effectItem.duration > 1)
                chips.push_back({ std::to_string(eff->effectItem.duration) + "s", kTextMuted });
        }
        if (unknown > 0) {
            static const std::string s_unknown = LocalizationManager::Get("Inventory.UnknownEffects");
            std::string text = s_unknown;
            if (auto at = text.find("%1"); at != std::string::npos) text.replace(at, 2, std::to_string(unknown));
            chips.push_back({ std::move(text), kTextMuted });
        }
        return chips;
    }

    static bool IsUsableRow(const InventoryItem& e) {
        if (e.type == ItemType::Weapon || e.type == ItemType::Armor || e.type == ItemType::Potion ||
            e.type == ItemType::Food || e.type == ItemType::Ammo)
            return true;
        auto* form = RE::TESForm::LookupByID(e.key.baseID);
        return form && form->Is(RE::FormType::Light);
        }

    static GearCatalog::Card CardForRow(const InventoryItem& e) {
        if (s_SlotCard) return *s_SlotCard;
        return e.type == ItemType::Ammo ? GearCatalog::Card::LeftHand : s_BrowseHand;
        }

    static void UseRow(RE::Actor* actor, const InventoryItem& e) {
        const int count = e.type == ItemType::Ammo ? e.count : 1;
        const bool queued = GearSwitcher::Equip(actor, e.key, CardForRow(e), count);
        logger::info("[InventoryModal] use {:08X} on {:08X}, queued {}", e.key.baseID, actor->GetFormID(), queued);
            }

    static std::vector<RE::Actor*> SendTargets(RE::Actor* from) {
        std::vector<RE::Actor*> out;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (player && player != from) out.push_back(player);
        for (auto* ally : FollowerHUD::GetAllies())
            if (ally && ally != from && ally->IsPlayerTeammate()) out.push_back(ally);
        return out;
        }

    static bool RowInTab(const InventoryItem& e, int filter) {
        switch (filter) {
            case 1: return e.tab == ItemTab::Weapon;
            case 2: return e.tab == ItemTab::Armor;
            case 3: return e.tab == ItemTab::Potion;
            case 4: return e.tab == ItemTab::Food;
            case 5: return e.tab == ItemTab::Ingredient;
            case 6: return e.tab == ItemTab::Book || e.tab == ItemTab::Scroll;
            case 7: return e.tab == ItemTab::Misc;
            default: return true;
        }
    }

    static void DrawSendAmountPopup(RE::Actor* actor, float sc) {
        if (s_PendingSend.openPopup) {
            ImGui::OpenPopup("##send_amount");
            s_PendingSend.openPopup = false;
        }
        if (!ImGui::BeginPopup("##send_amount")) return;

        static std::string s_okLabel;
        if (s_okLabel.empty()) s_okLabel = LocalizationManager::Get("Inventory.SendOk");
        auto* target = RE::TESForm::LookupByID<RE::Actor>(s_PendingSend.to);
        const char* targetName = target ? target->GetDisplayFullName() : nullptr;
        ImGui::SetWindowFontScale(FontSmall());
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(kAccent), "%s", s_PendingSend.name.c_str());
        if (targetName) ImGui::TextDisabled("%s", targetName);
        ImGui::SetNextItemWidth(200.0f * sc);
        ImGui::SliderInt("##send_amount_slider", &s_PendingSend.amount, 1, s_PendingSend.max);
        ImGui::SameLine();
        if (ImGui::Button(s_okLabel.c_str())) {
            if (target) GearSwitcher::Send(actor, target, s_PendingSend.key, s_PendingSend.amount);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetWindowFontScale(1.0f);
        ImGui::EndPopup();
    }

    void RenderInventoryModal(RE::Actor* actor) {
        if (!s_InvOpen) return;
        ++s_FrameCounter;

        if (!actor) {
            if (s_NullActorStreak == 0)
                logger::warn("InventoryModal: NULL actor begin (frame={}, lastActor=0x{:08X})",
                    s_FrameCounter, s_LastActorFormID);
            ++s_NullActorStreak;
            return;
        }
        if (s_NullActorStreak > 0) {
            logger::warn("InventoryModal: NULL actor end (frame={}, streakLen={}, recoveredActor=0x{:08X})",
                s_FrameCounter, s_NullActorStreak, actor->GetFormID());
            s_NullActorStreak = 0;
        }

        std::uint32_t curID = actor->GetFormID();
        if (curID != s_LastActorFormID) {
            s_LastActorFormID = curID;
            s_LastItemCount    = -1;
            s_LastVisibleCount = -1;
        }

        auto* settings = Settings::GetSingleton();
        auto& io       = ImGui::GetIO();
        const float fitScale = (std::min)(io.DisplaySize.x * 0.95f / 640.0f, io.DisplaySize.y * 0.90f / 520.0f);
        const float sc = (std::min)(settings->psV2Scale, fitScale);
        s_EffectiveScale = sc;

        const float modalW = 480.0f * sc;
        const float modalH = 540.0f * sc;
        ImVec2 modalPos = ImVec2((io.DisplaySize.x - modalW) * 0.5f, io.DisplaySize.y * 0.18f);
        if (settings->psV2InvPosX >= 0 && settings->psV2InvPosY >= 0) {
            modalPos.x = (std::min)(static_cast<float>(settings->psV2InvPosX), (std::max)(0.0f, io.DisplaySize.x - modalW));
            modalPos.y = (std::min)(static_cast<float>(settings->psV2InvPosY), (std::max)(0.0f, io.DisplaySize.y - modalH));
        }

        ImGui::SetNextWindowPos(modalPos, ImGuiCond_Appearing);
        ImGui::SetNextWindowSize(ImVec2(modalW, modalH), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(1.0f);
        if (!ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
        ImGui::SetNextWindowFocus();

        ImGui::PushStyleColor(ImGuiCol_WindowBg,    ImVec4( 9/255.f,  9/255.f, 11/255.f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_Border,      ImVec4(50/255.f, 50/255.f, 60/255.f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4( 9/255.f,  9/255.f, 11/255.f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,      ImVec4(80/255.f, 65/255.f, 20/255.f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(212/255.f,175/255.f,55/255.f,0.6f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(16.0f * sc, 14.0f * sc));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

        ImGui::Begin("##psv2_inv_modal", nullptr,
            ImGuiWindowFlags_NoTitleBar  | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse  |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoNav);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wp      = ImGui::GetWindowPos();
        ImVec2 ws      = ImGui::GetWindowSize();

        HUDDraw::DrawChip(dl, wp, ImVec2(wp.x + ws.x, wp.y + ws.y),
            HUDDraw::PanelChipFromColor(HUDDraw::PackedToArgb(Tip::g_slotChipBase), Tip::kPanelChipDarken, 0.0f, 4.0f * sc));

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            const int px = static_cast<int>(wp.x), py = static_cast<int>(wp.y);
            if (px != settings->psV2InvPosX || py != settings->psV2InvPosY) {
                settings->psV2InvPosX = px;
                settings->psV2InvPosY = py;
                settings->SaveInventoryModalPos();
            }
        }

        const auto gear = GearCatalog::Get(actor);

        {
            const float padX = 16.0f * sc;
            ImGui::PushFont(nullptr, ImGuiRenderer::GetHeadingFontSize());
            ImGui::SetWindowFontScale(FontHeading());
            const char* name = actor->GetDisplayFullName() ? actor->GetDisplayFullName() : "?";
            const float nameH = ImGui::GetFontSize();
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(wp.x + padX, wp.y + 12.0f * sc), kAccent, name);
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopFont();

            static std::string s_summaryFmt, s_spellSummaryFmt;
            if (s_summaryFmt.empty()) {
                s_summaryFmt      = LocalizationManager::Get("Inventory.Summary");
                s_spellSummaryFmt = LocalizationManager::Get("Inventory.SpellSummary");
            }
            const bool spellView = s_SpellHand || s_SpellWindow;
            std::string summary = spellView ? s_spellSummaryFmt : s_summaryFmt;
            const auto Fill = [&summary](const char* tag, const std::string& v) {
                if (auto at = summary.find(tag); at != std::string::npos) summary.replace(at, 2, v);
            };
            if (spellView) {
                Fill("%1", std::to_string(KnownSpellCount(actor)));
            } else {
                Fill("%1", std::to_string(gear ? gear->items.size() : 0));
                Fill("%2", std::to_string(static_cast<int>(actor->GetWeightInContainer() + 0.5f)));
                Fill("%3", std::to_string(static_cast<int>(actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kCarryWeight) + 0.5f)));
            }
            ImGui::SetWindowFontScale(FontSmall());
            const float summaryY = wp.y + 14.0f * sc + nameH;
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(wp.x + padX, summaryY), kTextMuted, summary.c_str());
            const float summaryH = ImGui::GetFontSize();

            static std::string s_leftLabel, s_rightLabel;
            if (s_leftLabel.empty()) {
                s_leftLabel  = LocalizationManager::Get("CharacterSheet.slot_left_hand");
                s_rightLabel = LocalizationManager::Get("CharacterSheet.slot_right_hand");
            }
            const std::optional<GearCatalog::Card> fixedHand = s_SpellHand ? s_SpellHand : s_SlotCard;
            const float closeR = 11.0f * sc;
            const float right  = wp.x + ws.x - padX - closeR * 2.0f - 8.0f * sc;
            const float handY  = wp.y + 12.0f * sc + closeR - summaryH * 0.5f;
            if (fixedHand) {
                static std::array<std::string, GearCatalog::kCardCount> s_cardLabels;
                if (s_cardLabels[0].empty()) {
                    const char* keys[GearCatalog::kCardCount] = {
                        "CharacterSheet.slot_right_hand", "CharacterSheet.slot_left_hand", "CharacterSheet.slot_head",
                        "CharacterSheet.slot_chest", "CharacterSheet.slot_feet", "CharacterSheet.slot_hands",
                        "CharacterSheet.slot_amulet", "CharacterSheet.slot_ring" };
                    for (std::size_t i = 0; i < GearCatalog::kCardCount; ++i) s_cardLabels[i] = LocalizationManager::Get(keys[i]);
                }
                const std::string& label = s_cardLabels[static_cast<std::size_t>(*fixedHand)];
                const float segPadX  = 10.0f * sc;
                const float pillPadY = 3.0f * sc;
                const float rounding = 6.0f * sc;
                const ImVec2 pillA(right - ImGui::CalcTextSize(label.c_str()).x - segPadX * 2.0f, handY - pillPadY);
                const ImVec2 pillB(right, handY + summaryH + pillPadY);
                dl->AddRectFilled(pillA, pillB, IM_COL32(16, 17, 22, 235), rounding);
                dl->AddRectFilled(pillA, pillB, IM_COL32(212, 175, 55, 50), rounding);
                dl->AddRect(pillA, pillB, IM_COL32(58, 62, 70, 255), rounding, 0, 1.0f);
                dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(pillA.x + segPadX, handY), kAccent, label.c_str());
            } else {
                const float segPadX  = 10.0f * sc;
                const float pillPadY = 3.0f * sc;
                const float rounding = 6.0f * sc;
                const float leftW  = ImGui::CalcTextSize(s_leftLabel.c_str()).x + segPadX * 2.0f;
                const float rightW = ImGui::CalcTextSize(s_rightLabel.c_str()).x + segPadX * 2.0f;
                const ImVec2 pillA(right - leftW - rightW, handY - pillPadY);
                const ImVec2 pillB(right, handY + summaryH + pillPadY);
                dl->AddRectFilled(pillA, pillB, IM_COL32(16, 17, 22, 235), rounding);

                const float splitX = pillA.x + leftW;
                for (const auto hand : { GearCatalog::Card::LeftHand, GearCatalog::Card::RightHand }) {
                    const bool isLeft = hand == GearCatalog::Card::LeftHand;
                    const std::string& label = isLeft ? s_leftLabel : s_rightLabel;
                    const ImVec2 a(isLeft ? pillA.x : splitX, pillA.y), b(isLeft ? splitX : pillB.x, pillB.y);
                    const bool active  = s_BrowseHand == hand;
                    const bool hovered = ImGui::IsMouseHoveringRect(a, b);
                    const ImDrawFlags corners = isLeft ? ImDrawFlags_RoundCornersLeft : ImDrawFlags_RoundCornersRight;
                    if (active)
                        dl->AddRectFilled(a, b, IM_COL32(212, 175, 55, 50), rounding, corners);
                    else if (hovered)
                        dl->AddRectFilled(a, b, IM_COL32(255, 255, 255, 10), rounding, corners);
                    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(a.x + segPadX, handY),
                        active ? kAccent : hovered ? kTextMain : kTextMuted, label.c_str());
                    if (hovered && ImGui::IsMouseClicked(0)) s_BrowseHand = hand;
                }
                dl->AddLine(ImVec2(splitX, pillA.y + 1.0f * sc), ImVec2(splitX, pillB.y - 1.0f * sc), IM_COL32(58, 62, 70, 255), 1.0f);
                dl->AddRect(pillA, pillB, IM_COL32(58, 62, 70, 255), rounding, 0, 1.0f);
            }
            ImGui::SetWindowFontScale(1.0f);

            const float btnR = 11.0f * sc;
            const ImVec2 btnC(wp.x + ws.x - padX - btnR, wp.y + 12.0f * sc + btnR);
            const bool hovered = ImGui::IsMouseHoveringRect(ImVec2(btnC.x - btnR, btnC.y - btnR), ImVec2(btnC.x + btnR, btnC.y + btnR));
            dl->AddCircleFilled(btnC, btnR, hovered ? IM_COL32(80, 30, 30, 220) : IM_COL32(20, 20, 26, 255));
            dl->AddCircle(btnC, btnR, IM_COL32(58, 62, 70, 255), 0, 1.0f);
            const float xr = btnR * 0.35f;
            const ImU32 xCol = hovered ? kTextMain : kTextMuted;
            dl->AddLine(ImVec2(btnC.x - xr, btnC.y - xr), ImVec2(btnC.x + xr, btnC.y + xr), xCol, 1.5f);
            dl->AddLine(ImVec2(btnC.x - xr, btnC.y + xr), ImVec2(btnC.x + xr, btnC.y - xr), xCol, 1.5f);
            if (hovered && ImGui::IsMouseClicked(0))
                CloseInventoryModal();

            const float ruleY = wp.y + 22.0f * sc + nameH + summaryH;
            HUDDraw::DrawFadeRule(dl, wp.x + padX, wp.x + ws.x - padX, ruleY, kAccent, HUDDraw::RuleFade::Center);
            ImGui::Dummy(ImVec2(ws.x - 32.0f * sc, ruleY - wp.y + 6.0f * sc - ImGui::GetStyle().WindowPadding.y));
        }

        if (s_SpellHand || s_SpellWindow) {
            DrawSpellPickerBody(actor, s_SpellHand ? *s_SpellHand : s_BrowseHand, sc, wp, ws);
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(5);
            ImGui::End();
            return;
        }

        static std::string s_tabAll, s_tabWeapons, s_tabArmor, s_tabPotions, s_tabFood, s_tabIngredients, s_tabBooks, s_tabMisc;
        static std::string s_emptyMsg;
        if (s_tabAll.empty()) {
            s_tabAll         = LocalizationManager::Get("Inventory.TabAll");
            s_tabWeapons     = LocalizationManager::Get("Inventory.TabWeapons");
            s_tabArmor       = LocalizationManager::Get("Inventory.TabArmor");
            s_tabPotions     = LocalizationManager::Get("Inventory.TabPotions");
            s_tabFood        = LocalizationManager::Get("Inventory.TabFood");
            s_tabIngredients = LocalizationManager::Get("Inventory.TabIngredients");
            s_tabBooks       = LocalizationManager::Get("Inventory.TabBooks");
            s_tabMisc        = LocalizationManager::Get("Inventory.TabMisc");
            s_emptyMsg       = LocalizationManager::Get("Inventory.Empty");
        }
        struct TabButton {
            const char* label; bool active; int tab; GearCatalog::SlotTag tag;
            GearCatalog::PotionTag potionTag = GearCatalog::PotionTag::None;
            const char* icon = nullptr;
        };
        std::vector<TabButton> tabs;
        if (s_SlotCard) {
            const std::uint32_t mask = gear ? gear->cards[static_cast<std::size_t>(*s_SlotCard)].tagMask : 0;
            if (std::popcount(mask) > 1) {
                tabs.push_back({ s_tabAll.c_str(), s_SlotTag == GearCatalog::SlotTag::None, -1, GearCatalog::SlotTag::None });
                static std::array<std::string, static_cast<std::size_t>(GearCatalog::SlotTag::None)> s_tagLabels;
                for (std::uint32_t t = 0; t < s_tagLabels.size(); ++t) {
                    if (!(mask & (1u << t))) continue;
                    const auto tag = static_cast<GearCatalog::SlotTag>(t);
                    if (s_tagLabels[t].empty()) s_tagLabels[t] = LocalizationManager::Get(GearCatalog::SlotTagKey(tag));
                    tabs.push_back({ s_tagLabels[t].c_str(), s_SlotTag == tag, -1, tag });
                }
            }
        } else {
        const char* kTabLabels[] = {
            s_tabAll.c_str(),    s_tabWeapons.c_str(), s_tabArmor.c_str(), s_tabPotions.c_str(),
            s_tabFood.c_str(),   s_tabIngredients.c_str(), s_tabBooks.c_str(), s_tabMisc.c_str()
        };
            const char* kTabIcons[] = { nullptr, "weapon", "armor", "source_potion", "source_food", "source_ingredient", "books", "misc" };
            for (int i = 0; i < 8; ++i)
                tabs.push_back({ kTabLabels[i], s_InvFilter == i, i, GearCatalog::SlotTag::None, GearCatalog::PotionTag::None, kTabIcons[i] });
        }

        std::vector<TabButton> potionTabs;
        if (!s_SlotCard && s_InvFilter == 3 && gear) {
            std::uint32_t mask = 0;
            for (const auto& e : gear->items)
                if (e.tab == ItemTab::Potion && e.potionTag != GearCatalog::PotionTag::None)
                    mask |= 1u << static_cast<std::uint32_t>(e.potionTag);
            if (std::popcount(mask) > 1) {
                potionTabs.push_back({ s_tabAll.c_str(), s_PotionTag == GearCatalog::PotionTag::None, 3,
                                       GearCatalog::SlotTag::None, GearCatalog::PotionTag::None });
                static std::array<std::string, static_cast<std::size_t>(GearCatalog::PotionTag::None)> s_potionLabels;
                const char* kPotionIcons[] = { "health", "magicka", "stamina", "fortify", "resistant", "disease_resist", "poison" };
                for (std::uint32_t t = 0; t < s_potionLabels.size(); ++t) {
                    if (!(mask & (1u << t))) continue;
                    const auto tag = static_cast<GearCatalog::PotionTag>(t);
                    if (s_potionLabels[t].empty()) s_potionLabels[t] = LocalizationManager::Get(GearCatalog::PotionTagKey(tag));
                    potionTabs.push_back({ s_potionLabels[t].c_str(), s_PotionTag == tag, 3, GearCatalog::SlotTag::None, tag, kPotionIcons[t] });
                }
            }
        }
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(4.0f * sc, 0));
        ImGui::SetWindowFontScale(FontSmall());
        const float kIconTabH = 36.0f * sc;
        auto IconOf = [](const TabButton& tb) { return tb.icon ? TextureManager::GetPartyIcon(tb.icon) : nullptr; };
        auto TabWidth = [&](const TabButton& tb) {
            return ImGui::CalcTextSize(tb.label).x + 20.0f * sc;
        };
        auto DrawTabRow = [&](const std::vector<TabButton>& tabs, bool potionRow) {
        float stretchW = 0.0f;
        for (const auto& tb : tabs) if (IconOf(tb)) { stretchW = 1.0f; break; }
        if (stretchW > 0.0f) {
            const float avail = (wp.x + ws.x - 16.0f * sc) - ImGui::GetCursorScreenPos().x;
            stretchW = (avail - 4.0f * sc * static_cast<float>(tabs.size() - 1)) / static_cast<float>(tabs.size());
        }
        for (size_t i = 0; i < tabs.size(); ++i) {
            const auto& tb = tabs[i];
            auto* iconTex = IconOf(tb);
            char btnId[64];
            snprintf(btnId, sizeof(btnId), "##inv_tab_%d_%zu", potionRow ? 1 : 0, i);
            const ImVec2 size = stretchW > 0.0f ? ImVec2(stretchW, kIconTabH) : ImVec2(TabWidth(tb), ImGui::GetFrameHeight());
            const bool clicked = ImGui::InvisibleButton(btnId, size);
            const ImVec2 bMin = ImGui::GetItemRectMin(), bMax = ImGui::GetItemRectMax();
            ImDrawList* tabDl = ImGui::GetWindowDrawList();
            if (stretchW > 0.0f) HUDDraw::DrawControlTab(tabDl, bMin, bMax, tb.active, ImGui::IsItemHovered(), sc);
            else                 HUDDraw::DrawControlTag(tabDl, bMin, bMax, tb.label, tb.active, ImGui::IsItemHovered(), sc);
            if (stretchW > 0.0f && !iconTex) {
                const ImVec2 ls = ImGui::CalcTextSize(tb.label);
                const ImVec2 lp(std::round((bMin.x + bMax.x - ls.x) * 0.5f), std::round((bMin.y + bMax.y - ls.y) * 0.5f));
                const bool   lit = tb.active || ImGui::IsItemHovered();
                tabDl->AddText(lp, lit ? IM_COL32(240, 220, 160, 255) : IM_COL32(200, 200, 200, 170), tb.label);
            }
            if (iconTex) {
                const float  side = (bMax.y - bMin.y) * 0.8f;
                const ImVec2 c((bMin.x + bMax.x) * 0.5f, (bMin.y + bMax.y) * 0.5f);
                const bool   lit = tb.active || ImGui::IsItemHovered();
                tabDl->AddImage(reinterpret_cast<ImTextureID>(iconTex),
                    ImVec2(c.x - side * 0.5f, c.y - side * 0.5f), ImVec2(c.x + side * 0.5f, c.y + side * 0.5f),
                    ImVec2(0, 0), ImVec2(1, 1), lit ? IM_COL32(240, 220, 160, 255) : IM_COL32(200, 200, 200, 170));
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tb.label);
            }
            if (clicked) {
                if (potionRow) s_PotionTag = tb.potionTag;
                else if (s_SlotCard) s_SlotTag = tb.tag;
                else { s_InvFilter = tb.tab; s_PotionTag = GearCatalog::PotionTag::None; }
            }
            if (i + 1 < tabs.size()) {
                const float lastRight = ImGui::GetItemRectMax().x;
                const float nextW = TabWidth(tabs[i + 1]);
                if (stretchW > 0.0f || lastRight + 4.0f * sc + nextW < wp.x + ws.x - 16.0f * sc) ImGui::SameLine();
                else ImGui::Dummy(ImVec2(1, 4.0f * sc));
            }
        }
        };
        DrawTabRow(tabs, false);
        if (!potionTabs.empty()) {
            ImGui::Dummy(ImVec2(1, 4.0f * sc));
            DrawTabRow(potionTabs, true);
        }
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleVar(2);
        ImGui::Dummy(ImVec2(1, 6.0f * sc));
        const float scrollbarW = 14.0f;
        const float bodyW    = ws.x - 32.0f * sc - scrollbarW;
        const float gap     = 6.0f * sc;
        const bool  listView = ListView();
        const int   columns = listView ? 1 : 3;
        const float tileW   = (bodyW - gap * (columns - 1)) / columns;
        const float baseFs  = ImGui::GetFont()->LegacySize;
        const float tileH   = listView ? baseFs * (FontBody() + FontSmall()) + 16.0f * sc : tileW;

        // ── Scrollable item list ──────────────────────────────────────────────
        float listH = (std::max)(1.0f, ws.y - (ImGui::GetCursorScreenPos().y - wp.y) - 14.0f * sc);
        ImGui::PushStyleColor(ImGuiCol_ChildBg,              ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,          ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,        ImVec4(212/255.f,175/255.f,55/255.f,0.4f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(212/255.f,175/255.f,55/255.f,0.7f));
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize,    10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 3.0f);
        ImGui::BeginChild("##inv_list", ImVec2(bodyW + scrollbarW, listH), false, ImGuiWindowFlags_NoNav);

        {
            float wheel = ImGuiRenderer::ConsumeScrollDelta();
            if (wheel != 0.0f)
                ImGui::SetScrollY(ImGui::GetScrollY() - wheel * 40.0f * sc);
        }

        static const std::vector<InventoryItem> kNoItems;
        const std::vector<InventoryItem>& items = gear ? gear->items : kNoItems;

        int curItemCount = static_cast<int>(items.size());
        if (curItemCount != s_LastItemCount) {
            s_LastItemCount = curItemCount;
        }

        const GearCatalog::CardList* slotList = (gear && s_SlotCard) ? &gear->cards[static_cast<std::size_t>(*s_SlotCard)] : nullptr;
        auto RowInSlot = [&](const InventoryItem& e) {
            return std::any_of(slotList->items.begin(), slotList->items.end(),
                [&](const GearCatalog::GearItem& g) {
                    return g.key == e.key && (s_SlotTag == GearCatalog::SlotTag::None || g.tag == s_SlotTag); });
        };
        int  visibleCount = 0;
        const bool popupOpen = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);

        std::vector<int> shownRows;
        for (int rowIndex = 0; rowIndex < static_cast<int>(items.size()); ++rowIndex) {
            const auto& e = items[rowIndex];
            if (s_SlotCard ? !(slotList && RowInSlot(e)) : !RowInTab(e, s_InvFilter)) continue;
            if (!s_SlotCard && s_PotionTag != GearCatalog::PotionTag::None && e.potionTag != s_PotionTag) continue;
            shownRows.push_back(rowIndex);
        }
        const bool anyShown = !shownRows.empty();

        const auto now = std::chrono::steady_clock::now();
        const std::uint64_t viewSig = (static_cast<std::uint64_t>(curID) << 32) |
            (static_cast<std::uint64_t>(s_InvFilter) << 8) | (static_cast<std::uint64_t>(s_SlotTag) << 16) |
            (static_cast<std::uint64_t>(s_PotionTag) << 24) | (s_SlotCard ? static_cast<std::uint64_t>(*s_SlotCard) + 1 : 0);
        if (gear) {
            if (viewSig != s_PrevViewSig) {
                s_Ghosts.clear();
            } else if (s_PrevGear && gear != s_PrevGear) {
                for (int slot = 0; slot < static_cast<int>(s_PrevShownRows.size()); ++slot) {
                    const auto& old = s_PrevGear->items[s_PrevShownRows[slot]];
                    const bool stillThere = std::any_of(items.begin(), items.end(),
                        [&](const InventoryItem& n) { return SameIgnoringPoison(n.key, old.key); });
                    if (!stillThere) s_Ghosts.push_back({ old, slot, now });
                }
            }
            s_PrevGear      = gear;
            s_PrevShownRows = shownRows;
            s_PrevViewSig   = viewSig;
        }
        std::erase_if(s_Ghosts, [now](const GhostTile& g) {
            return std::chrono::duration<float>(now - g.at).count() > kGhostSeconds;
        });

        const float pad = 4.0f * sc;
        const float rounding = 4.0f * sc;
        ImDrawList* tdl = ImGui::GetWindowDrawList();
        int slot = 0;
        auto PlaceTile = [&]() {
            if (slot % columns != 0) ImGui::SameLine();
            ++slot;
        };

        std::vector<RE::TESForm*> pictogramMisses;
        const auto* st = Settings::GetSingleton();
        const bool usePictograms = st->psV2InvPictograms && PNGIIBridge::Available();
        auto PictogramFor = [&](RE::TESForm* form) -> ID3D11ShaderResourceView* {
            if (!usePictograms || !form) return nullptr;
            if (st->psV2InvKeepItemArt && (form->As<RE::TESObjectWEAP>() || form->As<RE::TESObjectARMO>() || form->As<RE::TESAmmo>())) return nullptr;
            std::string rel;
            if (!PNGIIBridge::TryGetPath(form->GetFormID(), rel)) {
                pictogramMisses.push_back(form);
                return nullptr;
            }
            if (rel.empty()) return nullptr;
            std::ranges::replace(rel, '/', '\\');
            return TextureManager::GetTexture("Data\\" + rel);
        };

        auto DrawListRowContent = [&](const InventoryItem& e, RE::TESForm* form, ImVec2 tp, ImVec2 tq, ImU32 a) {
            const float h = tq.y - tp.y;
            const float is = h - 2.0f * pad;
            ID3D11ShaderResourceView* tex = PictogramFor(form);
            std::uint32_t rgb = 0x00FFFFFF;
            if (tex) PNGIIBridge::TryGetColor(form->GetFormID(), rgb);
            else if (form) tex = TextureManager::GetItemIcon(form, e.iconHint);
            if (tex)
                HUDDraw::DrawIconStyled(tdl, ImVec2(tp.x + pad, tp.y + pad), is, reinterpret_cast<ImTextureID>(tex),
                                        (rgb & 0x00FFFFFF) | a, IconFxDefault());

            const float fs     = ImGui::GetFontSize();
            const float nameFs = baseFs * FontBody();
            const float nameX  = tp.x + is + 3.0f * pad;
            const float nameY  = tp.y + 1.5f * pad;
            const float nameMax = tq.x - ImGui::CalcTextSize("x9999").x - 4.0f * pad - nameX;
            const ImU32 nameCol = (e.enchanted ? (kEnchanted & 0x00FFFFFF) : 0x00E6E6E6) | a;
            ImFont* font = ImGui::GetFont();
            std::string name = e.name;
            if (font->CalcTextSizeA(nameFs, FLT_MAX, 0.0f, name.c_str()).x > nameMax) {
                while (!name.empty() && font->CalcTextSizeA(nameFs, FLT_MAX, 0.0f, (name + "...").c_str()).x > nameMax) {
                    name.pop_back();
                    while (!name.empty() && (static_cast<unsigned char>(name.back()) & 0xC0) == 0x80) name.pop_back();
                    if (!name.empty() && (static_cast<unsigned char>(name.back()) & 0xC0) == 0xC0) name.pop_back();
                }
                name += "...";
            }
            tdl->AddText(font, nameFs, ImVec2(nameX, nameY), nameCol, name.c_str());

            const float chipPad = pad * 0.4f;
            const float chipH   = fs + 2.0f * chipPad;
            const float chipY   = tq.y - pad - chipH;
            const float chipEnd = tq.x - 2.0f * pad;
            float x = nameX;
            for (const auto& chip : EffectChips(form)) {
                const float tw = ImGui::CalcTextSize(chip.text.c_str()).x;
                const float cw = tw + 2.0f * pad * 1.5f;
                if (x + cw > chipEnd) break;
                tdl->AddRectFilled(ImVec2(x, chipY), ImVec2(x + cw, chipY + chipH), IM_COL32(42, 44, 52, 255) & (0x00FFFFFF | a), chipH * 0.5f);
                tdl->AddText(font, fs, ImVec2(x + pad * 1.5f, chipY + chipPad), (chip.color & 0x00FFFFFF) | a, chip.text.c_str());
                x += cw + pad;
            }
        };

        auto DrawTileContent = [&](const InventoryItem& e, ImVec2 tp, ImVec2 tq, float alpha) -> RE::TESForm* {
            const ImU32 a = static_cast<ImU32>(255.0f * alpha) << IM_COL32_A_SHIFT;
            auto* form = RE::TESForm::LookupByID(e.key.baseID);
            if (listView) {
                DrawListRowContent(e, form, tp, tq, a);
                return form;
            }
            const float w  = tq.x - tp.x;
            const float is = w * 0.40f;
            if (auto* glyph = PictogramFor(form)) {
                std::uint32_t rgb = 0x00D8D2C4;
                PNGIIBridge::TryGetColor(form->GetFormID(), rgb);
                const float gs = is * 0.85f;
                HUDDraw::DrawIconStyled(tdl, ImVec2(tp.x + (w - gs) * 0.5f, tp.y + (tq.y - tp.y) * 0.18f + (is - gs) * 0.5f), gs,
                    reinterpret_cast<ImTextureID>(glyph), (rgb & 0x00FFFFFF) | a, IconFxDefault());
            } else if (auto* tex = form ? TextureManager::GetItemIcon(form, e.iconHint) : nullptr) {
                float tw = 0.0f, th = 0.0f;
                if (!TextureManager::GetTextureSize(tex, tw, th)) tw = th = 1.0f;
                const float bandTop    = tp.y + pad + ImGui::GetTextLineHeight();
                const float bandBottom = tq.y - pad - HUDDraw::TileCaptionHeight();
                const float wanted = is * (std::max)(st->psV2InvItemArtScale, 10) / 100.0f;
                const float fit = (std::min)({ wanted / (std::max)(tw, th), (w - 2.0f * pad) / tw, (bandBottom - bandTop) / th });
                const ImVec2 size(tw * fit, th * fit);
                const float centerY = tp.y + (tq.y - tp.y) * 0.18f + is * 0.5f;
                const float top = std::clamp(centerY - size.y * 0.5f, bandTop, (std::max)(bandTop, bandBottom - size.y));
                const ImVec2 at(tp.x + (w - size.x) * 0.5f, top);
                HUDDraw::DrawIconStyled(tdl, at, size, reinterpret_cast<ImTextureID>(tex), 0x00FFFFFF | a, IconFxDefault());
            }
            if (e.statValue >= 0.0f) {
                char num[12];
                snprintf(num, sizeof(num), "%d", static_cast<int>(e.statValue + 0.5f));
                tdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(tp.x + pad, tp.y + pad),
                    (kTextMain & 0x00FFFFFF) | a, num);
            }
            const ImU32 nameCol = (e.enchanted ? (kEnchanted & 0x00FFFFFF) : 0x00C8C8C8) | a;
            HUDDraw::DrawTileCaption(tdl, tp, tq, pad, nameCol, e.name.c_str());
            return form;
        };

        std::size_t nextGhost = 0;
        std::sort(s_Ghosts.begin(), s_Ghosts.end(), [](const GhostTile& a, const GhostTile& b) { return a.slot < b.slot; });
        auto DrawGhostsUpTo = [&](int upToSlot) {
            while (nextGhost < s_Ghosts.size() && s_Ghosts[nextGhost].slot <= upToSlot) {
                const auto& g = s_Ghosts[nextGhost++];
                PlaceTile();
                ImGui::Dummy(ImVec2(tileW, tileH));
                const float t = UIAnimation::Clamp(std::chrono::duration<float>(now - g.at).count() / kGhostSeconds, 0.0f, 1.0f);
                const float alpha  = 1.0f - UIAnimation::EaseOut(t);
                const float shrink = tileW * 0.08f * UIAnimation::EaseOut(t);
                const ImVec2 tp(ImGui::GetItemRectMin().x + shrink, ImGui::GetItemRectMin().y + shrink);
                const ImVec2 tq(ImGui::GetItemRectMax().x - shrink, ImGui::GetItemRectMax().y - shrink);
                tdl->AddRectFilled(tp, tq, IM_COL32(18, 18, 24, static_cast<int>(255 * alpha)), rounding);
                tdl->AddRect(tp, tq, IM_COL32(232, 103, 106, static_cast<int>(200 * alpha)), rounding, 0, 1.5f * sc);
                DrawTileContent(g.item, tp, tq, alpha);
            }
        };

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(gap, gap));
        ImGui::SetWindowFontScale(FontSmall());
        for (const int rowIndex : shownRows) {
            const auto& e = items[rowIndex];
            DrawGhostsUpTo(slot);
            PlaceTile();
            ++visibleCount;

            const bool usable = IsUsableRow(e);
            ImGui::PushID(rowIndex);
            const bool clicked = ImGui::InvisibleButton("##tile", ImVec2(tileW, tileH));
            const bool hovered = ImGui::IsItemHovered();
            const bool pressed = ImGui::IsItemActive();
            ImVec2 tp = ImGui::GetItemRectMin();
            ImVec2 tq = ImGui::GetItemRectMax();
            if (!e.quest && ImGui::BeginPopupContextItem("##send")) {
                for (auto* target : SendTargets(actor)) {
                    const char* nm = target->GetDisplayFullName();
                    if (ImGui::MenuItem(nm && nm[0] ? nm : "?")) {
                        if (e.count > 1) s_PendingSend = { true, target->GetFormID(), e.key, e.name, e.count, e.count };
                        else GearSwitcher::Send(actor, target, e.key);
                    }
                }
                ImGui::EndPopup();
            }
            ImGui::PopID();
            if (clicked && usable) {
                if (e.worn) GearSwitcher::Unequip(actor, e.key);
                else UseRow(actor, e);
            }

            const float hoverAmt = UIAnimation::SmoothStep(UIAnimation::Step(TileKey("hov", curID, e.key), hovered, 0.12f, 0.20f));
            const float pressAmt = UIAnimation::SmoothStep(UIAnimation::Step(TileKey("prs", curID, e.key), pressed && usable, 0.05f, 0.15f));
            const float sink = 2.5f * sc * pressAmt;
            tp = ImVec2(tp.x + sink, tp.y + sink);
            tq = ImVec2(tq.x - sink, tq.y - sink);

            HUDDraw::DrawChip(tdl, tp, tq, HUDDraw::PanelChipFromColor(HUDDraw::PackedToArgb(Tip::g_slotChipBase),
                usable ? Tip::kTipChipDarken : Tip::kTileChipDarkenDim, hoverAmt, rounding));
            const float fillAlpha = 35.0f * pressAmt;
            if (fillAlpha > 0.5f)
                tdl->AddRectFilled(tp, tq, IM_COL32(212, 175, 55, static_cast<int>(fillAlpha)), rounding);

            const auto wornWatch = UIAnimation::Watch(TileKey("worn", curID, e.key), e.worn ? 1.0f : 0.0f, 0.7f, 0.5f);
            if (e.worn) {
                tdl->AddRect(tp, tq, kAccent, rounding, 0, 1.5f);
                if (wornWatch.direction > 0 && wornWatch.flash > 0.0f)
                    HUDDraw::DrawGlowRect(tdl, tp, tq, IM_COL32(255, 226, 122, 255), 1.5f * sc, 3, rounding, true, wornWatch.flash);
            } else {
                if (wornWatch.direction < 0 && wornWatch.flash > 0.0f)
                    tdl->AddRect(tp, tq, IM_COL32(212, 175, 55, static_cast<int>(255 * wornWatch.flash)), rounding, 0, 1.5f);
            }

            const std::string countKey = TileKey("cnt", curID, e.key);
            const auto countWatch = UIAnimation::Watch(countKey, static_cast<float>(e.count), 0.6f, 0.5f);
            if (countWatch.changed && countWatch.direction < 0) UIAnimation::TriggerBounce(countKey, 0.45f);
            if (countWatch.direction < 0 && countWatch.flash > 0.0f)
                tdl->AddRect(tp, tq, IM_COL32(232, 103, 106, static_cast<int>(230 * countWatch.flash)), rounding, 0, 2.0f * sc);

            RE::TESForm* form = DrawTileContent(e, tp, tq, 1.0f);

            if (e.count > 1 || countWatch.flash > 0.0f) {
                char qty[12];
                snprintf(qty, sizeof(qty), "x%d", e.count);
                const float qw = ImGui::CalcTextSize(qty).x;
                const float hop = UIAnimation::GetBounceOffsetY(countKey) * sc;
                const float qy = listView ? tp.y + 1.5f * pad + baseFs * FontBody() - ImGui::GetFontSize() : tp.y + pad;
                const ImU32 qtyCol = UIAnimation::FlashColor(countWatch, kAccent, IM_COL32(125, 220, 122, 255),
                                                             IM_COL32(232, 103, 106, 255));
                tdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(tq.x - 2.0f * pad - qw, qy + hop), qtyCol, qty);
            }

            if (e.poisonDoses > 0) DrawPoisonMark(tdl, ImVec2(tq.x - pad * 0.5f, tp.y + pad * 0.5f), e.poisonDoses, sc);

            if (hovered && !popupOpen && form) {
                auto* glyph = PictogramFor(form);
                auto* tex = glyph ? glyph : TextureManager::GetItemIcon(form, e.iconHint);
                std::uint32_t rgb = 0x00FFFFFF;
                if (glyph) PNGIIBridge::TryGetColor(form->GetFormID(), rgb);
                auto* enchant = e.key.enchantID ? RE::TESForm::LookupByID<RE::EnchantmentItem>(e.key.enchantID) : nullptr;
                DrawInventoryItemTooltip(actor, form, e.name.c_str(), enchant, TypeLabel(e.type).c_str(), tex, sc,
                                         e.key.poisonID, e.poisonDoses, 0xFF000000u | (rgb & 0x00FFFFFF), e.quest);
            }
            }
        DrawGhostsUpTo(INT_MAX);
        DrawSendAmountPopup(actor, sc);
        if (!pictogramMisses.empty()) PNGIIBridge::RequestPaths(std::move(pictogramMisses));
        ImGui::PopStyleVar();

        if (!anyShown) {
            ImVec2 ep = ImGui::GetCursorScreenPos();
            ImGui::SetWindowFontScale(FontSmall());
            const char* msg = s_emptyMsg.c_str();
            float tw = ImGui::CalcTextSize(msg).x;
            ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(ep.x + (bodyW - tw) * 0.5f, ep.y + 20.0f * sc), kTextMuted, msg);
            ImGui::SetWindowFontScale(1.0f);
        }

        if (visibleCount != s_LastVisibleCount) {
            s_LastVisibleCount = visibleCount;
        }

        ImGui::SetWindowFontScale(1.0f);
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(5);
        ImGui::End();
    }

}
