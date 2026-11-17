#include "../../includes/ImGui/PartySheetV2InventoryModal.h"
#include "../../includes/ImGui/Renderer.h"
#include "../../includes/ImGui/FollowerHUD.h"
#include "../../includes/Settings.h"
#include "../../includes/TextureManager.h"
#include "../../includes/LocalizationManager.h"
#include "../../includes/Utils/GearCatalog.h"
#include "../../includes/Utils/GearSwitcher.h"
#include "../../includes/Utils/HUDDraw.h"

#include <imgui.h>
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <string>
#include <vector>
#include <algorithm>
#include <optional>

namespace logger = SKSE::log;

namespace PartySheetV2 {

    static bool s_InvOpen   = false;
    static int  s_InvFilter = 0;
    static std::optional<GearCatalog::Card> s_SlotCard;

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

    void OpenInventoryModal()  {
        s_InvOpen = true;
        s_InvFilter = 0;
        s_SlotCard.reset();
        s_LastActorFormID  = 0;
        s_NullActorStreak  = 0;
        s_LastItemCount    = -1;
        s_LastVisibleCount = -1;
    }
    void OpenInventoryModal(GearCatalog::Card card) {
        OpenInventoryModal();
        s_SlotCard = card;
    }
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

    static bool IsUsableRow(const InventoryItem& e) {
        return e.type == ItemType::Weapon || e.type == ItemType::Armor || e.type == ItemType::Potion ||
               e.type == ItemType::Food || e.type == ItemType::Ammo;
        }

    static GearCatalog::Card CardForRow(const InventoryItem& e) {
        if (s_SlotCard) return *s_SlotCard;
        return e.type == ItemType::Ammo ? GearCatalog::Card::LeftHand : GearCatalog::Card::RightHand;
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

        const float modalW = 400.0f * sc;
        const float modalH = 470.0f * sc;
        ImVec2 modalPos = ImVec2((io.DisplaySize.x - modalW) * 0.5f, io.DisplaySize.y * 0.18f);

        ImGui::SetNextWindowPos(modalPos, ImGuiCond_FirstUseEver);
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

        {
            ImGui::PushFont(nullptr, ImGuiRenderer::GetHeadingFontSize());
            ImGui::SetWindowFontScale(0.85f);
            char title[128];
            snprintf(title, sizeof(title), "%s - Inventory",
                actor->GetDisplayFullName() ? actor->GetDisplayFullName() : "?");
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(wp.x + 16.0f * sc, wp.y + 12.0f * sc), kAccent, title);
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopFont();

            ImGui::SetWindowFontScale(FontBody());
            float btnSz = 20.0f * sc;
            ImVec2 btnTL = ImVec2(wp.x + ws.x - btnSz - 12.0f * sc, wp.y + 10.0f * sc);
            ImVec2 btnBR = ImVec2(btnTL.x + btnSz, btnTL.y + btnSz);
            bool hovered = ImGui::IsMouseHoveringRect(btnTL, btnBR);
            dl->AddRectFilled(btnTL, btnBR, hovered ? IM_COL32(80,30,30,200) : IM_COL32(30,30,40,180), 3.0f);
            ImVec2 xSz = ImGui::CalcTextSize("x");
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(btnTL.x + (btnSz - xSz.x) * 0.5f, btnTL.y + (btnSz - xSz.y) * 0.5f),
                kTextMuted, "x");
            if (hovered && ImGui::IsMouseClicked(0))
                CloseInventoryModal();
            ImGui::SetWindowFontScale(1.0f);
        }

        float sepY = wp.y + 38.0f * sc;
        HUDDraw::DrawFadeRule(dl, wp.x + 16.0f * sc, wp.x + ws.x * 0.7f, sepY, kAccent, HUDDraw::RuleFade::Right);

        ImGui::Dummy(ImVec2(ws.x - 32.0f * sc, 42.0f * sc));

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
        const char* kTabLabels[] = {
            s_tabAll.c_str(),    s_tabWeapons.c_str(), s_tabArmor.c_str(), s_tabPotions.c_str(),
            s_tabFood.c_str(),   s_tabIngredients.c_str(), s_tabBooks.c_str(), s_tabMisc.c_str()
        };
        constexpr int kNumTabs = 8;
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(4.0f * sc, 0));
        ImGui::SetWindowFontScale(FontSmall());
        for (int i = 0; i < kNumTabs; ++i) {
            bool active = !s_SlotCard && s_InvFilter == i;
            ImGui::PushStyleColor(ImGuiCol_Button,
                active ? ImVec4(212/255.f,175/255.f,55/255.f,0.25f)
                       : ImVec4(22/255.f, 22/255.f, 30/255.f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(212/255.f,175/255.f,55/255.f,0.15f));
            ImGui::PushStyleColor(ImGuiCol_Border,
                active ? ImVec4(212/255.f,175/255.f,55/255.f,0.6f)
                       : ImVec4(50/255.f, 50/255.f, 60/255.f, 1.f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            char btnId[32]; snprintf(btnId, sizeof(btnId), "%s##inv_tab_%d", kTabLabels[i], i);
            if (ImGui::Button(btnId, ImVec2(0, 0))) { s_InvFilter = i; s_SlotCard.reset(); }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
            if (i < kNumTabs - 1) {
                const float lastRight = ImGui::GetItemRectMax().x;
                const float nextW = ImGui::CalcTextSize(kTabLabels[i + 1]).x + ImGui::GetStyle().FramePadding.x * 2.0f;
                if (lastRight + 4.0f * sc + nextW < wp.x + ws.x - 16.0f * sc) ImGui::SameLine();
                else ImGui::Dummy(ImVec2(1, 4.0f * sc));
            }        }
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleVar(2);
        ImGui::Dummy(ImVec2(1, 6.0f * sc));
        const float scrollbarW = 14.0f;
        const float bodyW    = ws.x - 32.0f * sc - scrollbarW;
        const float gap     = 6.0f * sc;
        const int   columns = (std::max)(1, static_cast<int>((bodyW + gap) / (84.0f * sc + gap)));
        const float tileW   = (bodyW - gap * (columns - 1)) / columns;
        const float tileH   = tileW;
        const float iconSz  = tileW * 0.46f;

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
        const auto gear = GearCatalog::Get(actor);
        const std::vector<InventoryItem>& items = gear ? gear->items : kNoItems;

        int curItemCount = static_cast<int>(items.size());
        if (curItemCount != s_LastItemCount) {
            s_LastItemCount = curItemCount;
        }

        const GearCatalog::CardList* slotList = (gear && s_SlotCard) ? &gear->cards[static_cast<std::size_t>(*s_SlotCard)] : nullptr;
        auto RowInSlot = [&](const InventoryItem& e) {
            return std::any_of(slotList->items.begin(), slotList->items.end(),
                [&](const GearCatalog::GearItem& g) { return g.key == e.key; });
        };
        bool anyShown = false;
        int  visibleCount = 0;
        const bool popupOpen = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(gap, gap));
        ImGui::SetWindowFontScale(FontSmall());
        for (int rowIndex = 0; rowIndex < static_cast<int>(items.size()); ++rowIndex) {
            const auto& e = items[rowIndex];
            if (s_SlotCard ? !(slotList && RowInSlot(e)) : !RowInTab(e, s_InvFilter)) continue;
            if (visibleCount % columns != 0) ImGui::SameLine();
            anyShown = true;
            ++visibleCount;

            const bool usable = IsUsableRow(e);
            ImGui::PushID(rowIndex);
            const bool clicked = ImGui::InvisibleButton("##tile", ImVec2(tileW, tileH));
            const bool hovered = ImGui::IsItemHovered();
            const bool pressed = ImGui::IsItemActive();
            const ImVec2 tp = ImGui::GetItemRectMin();
            const ImVec2 tq = ImGui::GetItemRectMax();
            if (!e.quest && ImGui::BeginPopupContextItem("##send")) {
                for (auto* target : SendTargets(actor)) {
                    const char* nm = target->GetDisplayFullName();
                    if (ImGui::MenuItem(nm && nm[0] ? nm : "?")) GearSwitcher::Send(actor, target, e.key);
                }
                ImGui::EndPopup();
            }
            ImGui::PopID();
            if (clicked && usable) {
                if (e.worn) GearSwitcher::Unequip(actor, e.key);
                else UseRow(actor, e);
            }

            ImDrawList* tdl = ImGui::GetWindowDrawList();
            tdl->AddRectFilled(tp, tq, IM_COL32(18, 18, 24, 255), 4.0f * sc);
            if (hovered) {
                const int alpha = !usable ? 18 : pressed ? 70 : 40;
                tdl->AddRectFilled(tp, tq, IM_COL32(212, 175, 55, alpha), 4.0f * sc);
            }
            tdl->AddRect(tp, tq, e.worn ? kAccent : IM_COL32(42, 42, 52, 255), 4.0f * sc, 0, e.worn ? 1.5f : 1.0f);

            auto* form = RE::TESForm::LookupByID(e.key.baseID);
            auto* tex  = form ? TextureManager::GetItemIcon(form, e.iconHint) : nullptr;
                if (tex) {
                HUDDraw::DrawIconStyled(tdl, ImVec2(tp.x + (tileW - iconSz) * 0.5f, tp.y + tileH * 0.18f), iconSz,
                        reinterpret_cast<ImTextureID>(tex), 0xFFFFFFFF, IconFxDefault());
                }

            const float pad = 4.0f * sc;
            if (e.statValue >= 0.0f) {
                char num[12];
                snprintf(num, sizeof(num), "%d", static_cast<int>(e.statValue + 0.5f));
                tdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(tp.x + pad, tp.y + pad), kTextMain, num);
            }
            if (e.count > 1) {
                char qty[12];
                snprintf(qty, sizeof(qty), "x%d", e.count);
                const float qw = ImGui::CalcTextSize(qty).x;
                tdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(tq.x - pad - qw, tp.y + pad), kAccent, qty);
            }

            const ImU32 nameCol = e.enchanted ? kEnchanted : IM_COL32(200, 200, 200, 255);
            const float nw = ImGui::CalcTextSize(e.name.c_str()).x;
            const float nameX = nw < tileW - 2.0f * pad ? tp.x + (tileW - nw) * 0.5f : tp.x + pad;
            const float nameY = tq.y - pad - ImGui::GetTextLineHeight();
            tdl->PushClipRect(ImVec2(tp.x + pad, tp.y), ImVec2(tq.x - pad, tq.y), true);
            tdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(nameX, nameY), nameCol, e.name.c_str());
            tdl->PopClipRect();

            if (hovered && !popupOpen && form) {
                auto* enchant = e.key.enchantID ? RE::TESForm::LookupByID<RE::EnchantmentItem>(e.key.enchantID) : nullptr;
                DrawInventoryItemTooltip(actor, form, e.name.c_str(), enchant, TypeLabel(e.type).c_str(), tex, sc);
            }
            }
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

        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            CloseInventoryModal();
    }

}
