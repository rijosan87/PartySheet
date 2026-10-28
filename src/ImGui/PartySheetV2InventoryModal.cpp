#include "../../includes/ImGui/PartySheetV2InventoryModal.h"
#include "../../includes/ImGui/Renderer.h"
#include "../../includes/Settings.h"
#include "../../includes/TextureManager.h"
#include "../../includes/LocalizationManager.h"
#include "../../includes/Utils/HUDDraw.h"

#include <imgui.h>
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>

namespace logger = SKSE::log;

namespace PartySheetV2 {

    static bool s_InvOpen   = false;
    static int  s_InvFilter = 0;

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
        s_LastActorFormID  = 0;
        s_NullActorStreak  = 0;
        s_LastItemCount    = -1;
        s_LastVisibleCount = -1;
    }
    void CloseInventoryModal() {
        s_InvOpen = false;
    }
    bool IsInventoryModalOpen() { return s_InvOpen; }

    static constexpr ImU32 kAccent    = IM_COL32(212, 175,  55, 255);
    static constexpr ImU32 kTextMain  = IM_COL32(240, 240, 240, 255);
    static constexpr ImU32 kTextMuted = IM_COL32(136, 136, 136, 255);
    static constexpr ImU32 kBorderDim = IM_COL32( 50,  50,  60, 255);

    // ── Font scale helpers ────────────────────────────────────────────────────
    static float s_EffectiveScale = 1.0f;
    static float FontBody()  { auto* s = Settings::GetSingleton(); return s->psV2FontBody  * s_EffectiveScale; }
    static float FontSmall() { auto* s = Settings::GetSingleton(); return s->psV2FontSmall * s_EffectiveScale; }

    enum class InvItemType { Weapon, Armor, Potion, Food, Ingredient, Book, Scroll, Misc };

    struct InvEntry {
        RE::TESForm*                    form    = nullptr;
        std::string                     name;
        std::string                     typeLabel;
        std::string                     hint;
        InvItemType                     type    = InvItemType::Misc;
        float                           weight  = 0.0f;
        int                             value   = 0;
        int                             count   = 1;
    };

    static InvItemType ClassifyForm(RE::TESForm* form, std::string& outHint, std::string& outTypeLabel) {
        static std::string s_weapon, s_armor, s_potion, s_food, s_ammo, s_ingredient, s_soulgem, s_key, s_book, s_scroll, s_misc;
        if (s_misc.empty()) {
            s_weapon     = LocalizationManager::Get("Inventory.TypeWeapon");
            s_armor      = LocalizationManager::Get("Inventory.TypeArmor");
            s_potion     = LocalizationManager::Get("Inventory.TypePotion");
            s_food       = LocalizationManager::Get("Inventory.TypeFood");
            s_ammo       = LocalizationManager::Get("Inventory.TypeAmmo");
            s_ingredient = LocalizationManager::Get("Inventory.TypeIngredient");
            s_soulgem    = LocalizationManager::Get("Inventory.TypeSoulGem");
            s_key        = LocalizationManager::Get("Inventory.TypeKey");
            s_book       = LocalizationManager::Get("Inventory.TypeBook");
            s_scroll     = LocalizationManager::Get("Inventory.TypeScroll");
            s_misc       = LocalizationManager::Get("Inventory.TypeMisc");
        }
        if (!form) { outHint = "misc"; outTypeLabel = s_misc; return InvItemType::Misc; }

        if (form->As<RE::TESObjectWEAP>()) {
            auto* w = form->As<RE::TESObjectWEAP>();
            using WT = RE::WEAPON_TYPE;
            switch (w->GetWeaponType()) {
                case WT::kHandToHandMelee: outHint = "fist";     break;
                case WT::kOneHandSword:    outHint = "sword";    break;
                case WT::kOneHandDagger:   outHint = "dagger";   break;
                case WT::kOneHandAxe:      outHint = "axe";      break;
                case WT::kOneHandMace:     outHint = "mace";     break;
                case WT::kTwoHandSword:    outHint = "greatsword"; break;
                case WT::kTwoHandAxe:      outHint = "battleaxe"; break;
                case WT::kBow:             outHint = "bow";      break;
                case WT::kStaff:           outHint = "staff";    break;
                case WT::kCrossbow:        outHint = "crossbow"; break;
                default:                   outHint = "sword";    break;
            }
            outTypeLabel = s_weapon;
            return InvItemType::Weapon;
        }
        if (form->As<RE::TESObjectARMO>()) {
            auto* a = form->As<RE::TESObjectARMO>();
            using SL = RE::BIPED_MODEL::BipedObjectSlot;
            if      (a->HasPartOf(SL::kHead))    outHint = "helmet";
            else if (a->HasPartOf(SL::kBody))    outHint = "cuirass";
            else if (a->HasPartOf(SL::kHands))   outHint = "gauntlets";
            else if (a->HasPartOf(SL::kFeet))    outHint = "boots";
            else if (a->HasPartOf(SL::kShield))  outHint = "shield";
            else if (a->HasPartOf(SL::kAmulet))  outHint = "amulet";
            else if (a->HasPartOf(SL::kRing))    outHint = "ring";
            else if (a->HasPartOf(SL::kCirclet)) outHint = "circlet";
            else                                 outHint = "cuirass";
            outTypeLabel = s_armor;
            return InvItemType::Armor;
        }
        if (auto* alch = form->As<RE::AlchemyItem>()) {
            if (alch->IsFood()) {
                outHint = "food";
                outTypeLabel = s_food;
                return InvItemType::Food;
            }
            outHint = "potion";
            outTypeLabel = s_potion;
            return InvItemType::Potion;
        }
        if (form->As<RE::TESAmmo>()) {
            outHint = "arrow";
            outTypeLabel = s_ammo;
            return InvItemType::Misc;
        }
        if (form->As<RE::IngredientItem>()) {
            outHint = "ingredient";
            outTypeLabel = s_ingredient;
            return InvItemType::Ingredient;
        }
        if (form->As<RE::TESSoulGem>()) {
            outHint = "soulgem";
            outTypeLabel = s_soulgem;
            return InvItemType::Misc;
        }
        if (form->As<RE::TESKey>()) {
            outHint = "key";
            outTypeLabel = s_key;
            return InvItemType::Misc;
        }
        if (form->As<RE::TESObjectBOOK>()) {
            outHint = "book";
            outTypeLabel = s_book;
            return InvItemType::Book;
        }
        if (form->As<RE::ScrollItem>()) {
            outHint = "scroll";
            outTypeLabel = s_scroll;
            return InvItemType::Scroll;
        }
        outHint = "misc";
        outTypeLabel = s_misc;
        return InvItemType::Misc;
    }

    static std::vector<InvEntry> BuildInventory(RE::Actor* actor) {
        std::vector<InvEntry> out;
        if (!actor) return out;

        auto inv = actor->GetInventory([](const RE::TESBoundObject& obj) {
            return obj.GetFormID() != 0xF;
        });

        for (auto& [form, data] : inv) {
            if (!form) continue;
            auto&  entry = data.second;
            int    count = data.first;
            if (count <= 0) continue;

            const char* nm = entry ? entry->GetDisplayName() : nullptr;
            auto isMissing = [](const char* s) {
                return !s || !s[0] || std::strcmp(s, "<Missing Name>") == 0;
            };
            if (isMissing(nm)) nm = form->GetName();
            if (isMissing(nm)) continue;

            InvEntry e;
            e.form  = form;
            e.name  = nm;
            e.count = count;
            e.type  = ClassifyForm(form, e.hint, e.typeLabel);

            if (entry) {
                e.weight = entry->GetWeight();
                e.value  = entry->GetValue();
            } else {
                if (auto* w = form->As<RE::TESWeightForm>()) e.weight = w->weight;
                if (auto* v = form->As<RE::TESValueForm>())  e.value  = v->value;
            }

            out.push_back(std::move(e));
        }

        std::sort(out.begin(), out.end(), [](const InvEntry& a, const InvEntry& b) {
            if (a.type != b.type) return static_cast<int>(a.type) < static_cast<int>(b.type);
            return a.name < b.name;
        });
        return out;
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

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("##psv2_inv_overlay", nullptr,
            ImGuiWindowFlags_NoTitleBar            | ImGuiWindowFlags_NoResize     |
            ImGuiWindowFlags_NoMove                | ImGuiWindowFlags_NoScrollbar  |
            ImGuiWindowFlags_NoCollapse            | ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav        |
            ImGuiWindowFlags_NoInputs);
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(0, 0), io.DisplaySize, IM_COL32(0, 0, 0, 160));
        ImGui::End();
        ImGui::PopStyleVar();

        const float modalW = 640.0f * sc;
        const float modalH = 520.0f * sc;
        ImVec2 modalPos = ImVec2((io.DisplaySize.x - modalW) * 0.5f,
                                 (io.DisplaySize.y - modalH) * 0.5f);

        ImGui::SetNextWindowPos(modalPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(modalW, modalH), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(1.0f);
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
            ImGuiWindowFlags_NoMove      | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoNav);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wp      = ImGui::GetWindowPos();
        ImVec2 ws      = ImGui::GetWindowSize();

        {
            ImFont* hf = ImGuiRenderer::GetHeadingFont();
            if (hf) ImGui::PushFont(hf);
            ImGui::SetWindowFontScale(0.85f);
            char title[128];
            snprintf(title, sizeof(title), "%s - Inventory",
                actor->GetDisplayFullName() ? actor->GetDisplayFullName() : "?");
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(wp.x + 16.0f * sc, wp.y + 12.0f * sc), kAccent, title);
            ImGui::SetWindowFontScale(1.0f);
            if (hf) ImGui::PopFont();

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
        dl->AddRectFilledMultiColor(
            ImVec2(wp.x + 16.0f * sc, sepY),
            ImVec2(wp.x + ws.x * 0.7f, sepY + 1.0f),
            kAccent, IM_COL32(212,175,55,0), IM_COL32(0,0,0,0), IM_COL32(0,0,0,0));

        ImGui::Dummy(ImVec2(ws.x - 32.0f * sc, 42.0f * sc));

        static std::string s_tabAll, s_tabWeapons, s_tabArmor, s_tabPotions, s_tabFood, s_tabIngredients, s_tabBooks, s_tabMisc;
        static std::string s_colName, s_colType, s_colWgt, s_colVal, s_colQty, s_emptyMsg;
        if (s_tabAll.empty()) {
            s_tabAll         = LocalizationManager::Get("Inventory.TabAll");
            s_tabWeapons     = LocalizationManager::Get("Inventory.TabWeapons");
            s_tabArmor       = LocalizationManager::Get("Inventory.TabArmor");
            s_tabPotions     = LocalizationManager::Get("Inventory.TabPotions");
            s_tabFood        = LocalizationManager::Get("Inventory.TabFood");
            s_tabIngredients = LocalizationManager::Get("Inventory.TabIngredients");
            s_tabBooks       = LocalizationManager::Get("Inventory.TabBooks");
            s_tabMisc        = LocalizationManager::Get("Inventory.TabMisc");
            s_colName        = LocalizationManager::Get("Inventory.ColName");
            s_colType        = LocalizationManager::Get("Inventory.ColType");
            s_colWgt         = LocalizationManager::Get("Inventory.ColWeight");
            s_colVal         = LocalizationManager::Get("Inventory.ColValue");
            s_colQty         = LocalizationManager::Get("Inventory.ColQty");
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
            bool active = (s_InvFilter == i);
            ImGui::PushStyleColor(ImGuiCol_Button,
                active ? ImVec4(212/255.f,175/255.f,55/255.f,0.25f)
                       : ImVec4(22/255.f, 22/255.f, 30/255.f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(212/255.f,175/255.f,55/255.f,0.15f));
            ImGui::PushStyleColor(ImGuiCol_Border,
                active ? ImVec4(212/255.f,175/255.f,55/255.f,0.6f)
                       : ImVec4(50/255.f, 50/255.f, 60/255.f, 1.f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            char btnId[32]; snprintf(btnId, sizeof(btnId), "%s##inv_tab_%d", kTabLabels[i], i);
            if (ImGui::Button(btnId, ImVec2(0, 0))) s_InvFilter = i;
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
            if (i < kNumTabs - 1) ImGui::SameLine();
        }
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleVar(2);
        ImGui::Dummy(ImVec2(1, 6.0f * sc));

        const float iconCol  = 34.0f * sc;
        const float scrollbarW = 14.0f;
        const float bodyW    = ws.x - 32.0f * sc - scrollbarW;
        const float wgtCol   = 52.0f * sc;
        const float valCol   = 52.0f * sc;
        const float qtyCol   = 32.0f * sc;
        const float typeCol  = 72.0f * sc;
        const float nameCol  = bodyW - iconCol - typeCol - wgtCol - valCol - qtyCol;

        ImGui::SetWindowFontScale(FontSmall());
        {
            ImVec2 hdr = ImGui::GetCursorScreenPos();
            float x = hdr.x + iconCol;
            float lineH = ImGui::GetTextLineHeight();
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(x, hdr.y), kTextMuted, s_colName.c_str());
            x += nameCol;
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(x, hdr.y), kTextMuted, s_colType.c_str());
            x += typeCol;
            auto RightText = [&](float colW, const char* txt) {
                float tw = ImGui::CalcTextSize(txt).x;
                dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(x + colW - tw, hdr.y), kTextMuted, txt);
                x += colW;
            };
            RightText(wgtCol, s_colWgt.c_str());
            RightText(valCol, s_colVal.c_str());
            RightText(qtyCol, s_colQty.c_str());
            ImGui::Dummy(ImVec2(bodyW, lineH + 4.0f * sc));
        }

        {
            ImVec2 divPos = ImGui::GetCursorScreenPos();
            dl->AddLine(divPos, ImVec2(divPos.x + bodyW, divPos.y), kBorderDim, 1.0f);
            ImGui::Dummy(ImVec2(bodyW, 3.0f * sc));
        }
        ImGui::SetWindowFontScale(1.0f);

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

        auto items = BuildInventory(actor);

        int curItemCount = static_cast<int>(items.size());
        if (curItemCount != s_LastItemCount) {
            s_LastItemCount = curItemCount;
        }

        const float rowH   = 28.0f * sc;
        const float iconSz = 24.0f * sc;
        bool anyShown = false;
        int  visibleCount = 0;

        ImGui::SetWindowFontScale(FontBody());
        for (auto& e : items) {
            bool show = false;
            switch (s_InvFilter) {
                case 0: show = true; break;
                case 1: show = (e.type == InvItemType::Weapon);     break;
                case 2: show = (e.type == InvItemType::Armor);      break;
                case 3: show = (e.type == InvItemType::Potion);     break;
                case 4: show = (e.type == InvItemType::Food);       break;
                case 5: show = (e.type == InvItemType::Ingredient); break;
                case 6: show = (e.type == InvItemType::Book || e.type == InvItemType::Scroll); break;
                case 7: show = (e.type == InvItemType::Misc);       break;
            }
            if (!show) continue;
            anyShown = true;
            ++visibleCount;

            ImVec2 rp = ImGui::GetCursorScreenPos();

            bool hovered = ImGui::IsMouseHoveringRect(rp, ImVec2(rp.x + bodyW, rp.y + rowH));
            if (hovered)
                ImGui::GetWindowDrawList()->AddRectFilled(rp, ImVec2(rp.x + bodyW, rp.y + rowH),
                    IM_COL32(212, 175, 55, 18));

            float x = rp.x;

            {
                auto* tex = TextureManager::GetItemIcon(e.form, e.hint);
                if (tex) {
                    float iconOff = (rowH - iconSz) * 0.5f;
                    HUDDraw::DrawIconStyled(ImGui::GetWindowDrawList(),
                        ImVec2(x, rp.y + iconOff), iconSz,
                        reinterpret_cast<ImTextureID>(tex), 0xFFFFFFFF, IconFxDefault());
                }
            }
            x += iconCol;

            float textY = rp.y + (rowH - ImGui::GetTextLineHeight()) * 0.5f;
            ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(x, textY), kTextMain, e.name.c_str());
            x += nameCol;

            ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(x, textY), kTextMuted, e.typeLabel.c_str());
            x += typeCol;

            {
                char buf[16];
                snprintf(buf, sizeof(buf), "%.1f", e.weight);
                float tw = ImGui::CalcTextSize(buf).x;
                ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(x + wgtCol - tw, textY), kTextMuted, buf);
                x += wgtCol;
            }
            {
                char buf[16];
                snprintf(buf, sizeof(buf), "%d", e.value);
                float tw = ImGui::CalcTextSize(buf).x;
                ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(x + valCol - tw, textY), kTextMuted, buf);
                x += valCol;
            }
            {
                char buf[12];
                snprintf(buf, sizeof(buf), "%d", e.count);
                float tw = ImGui::CalcTextSize(buf).x;
                ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(x + qtyCol - tw, textY),
                    e.count > 1 ? kAccent : kTextMuted, buf);
            }

            ImGui::Dummy(ImVec2(bodyW, rowH));
        }

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
