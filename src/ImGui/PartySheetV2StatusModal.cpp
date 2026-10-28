#include "../../includes/ImGui/PartySheetV2StatusModal.h"
#include "../../includes/ImGui/Renderer.h"
#include "../../includes/Settings.h"
#include "../../includes/TextureManager.h"
#include "../../includes/LocalizationManager.h"
#include "../../includes/PlayerStatus.h"
#include "../../includes/Utils/HUDDraw.h"

#include <imgui.h>
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <string>
#include <vector>
#include <cstdio>

namespace PartySheetV2 {

    static bool s_StatusOpen = false;

    void OpenStatusModal()  { s_StatusOpen = true;  }
    void CloseStatusModal() { s_StatusOpen = false; }
    bool IsStatusModalOpen(){ return s_StatusOpen;  }

    static constexpr ImU32 kAccent    = IM_COL32(212, 175,  55, 255);
    static constexpr ImU32 kTextMain  = IM_COL32(240, 240, 240, 255);
    static constexpr ImU32 kTextMuted = IM_COL32(136, 136, 136, 255);
    static constexpr ImU32 kBorderDim = IM_COL32( 50,  50,  60, 255);
    static constexpr ImU32 kSubMuted  = IM_COL32(150, 150, 150, 200);

    static HUDDraw::IconFx IconFxDefault() {
        auto* s = Settings::GetSingleton();
        HUDDraw::IconFx fx{};
        fx.duotone      = s->psV2IconDuotone;
        fx.shadow       = s->psV2IconShadow;
        fx.outline      = false;
        fx.outlineWidth = s->psV2IconOutlineWidth;
        fx.outlineColor = s->psV2IconOutlineColor;
        fx.glow         = false;
        fx.gloss        = false;
        return fx;
    }

    static constexpr ImU32 kFactionLeader = IM_COL32(255, 215, 100, 255);
    static constexpr ImU32 kFactionMember = IM_COL32(220, 220, 220, 255);
    static constexpr ImU32 kChampion      = IM_COL32(200, 100, 255, 255);
    static constexpr ImU32 kThane         = IM_COL32(100, 200, 150, 255);

    static float s_EffectiveScale = 1.0f;
    static float FontBody()    { auto* s = Settings::GetSingleton(); return s->psV2FontBody    * s_EffectiveScale; }
    static float FontSmall()   { auto* s = Settings::GetSingleton(); return s->psV2FontSmall   * s_EffectiveScale; }
    static float FontHeading() { auto* s = Settings::GetSingleton(); return s->psV2FontHeading * s_EffectiveScale; }

    struct StatusItem {
        std::string iconKey;
        std::string title;
        std::string subtitle;
        ImU32       titleColor;
    };

    static std::vector<StatusItem> BuildChampions(const PlayerStatus::PlayerStatusData& data) {
        std::vector<StatusItem> out;
        for (const auto& c : data.champions) {
            StatusItem it;
            it.iconKey    = c.key;
            it.title      = c.displayName;
            it.subtitle   = !c.leaderTitle.empty() ? c.leaderTitle : LocalizationManager::Get("champion");
            it.titleColor = kChampion;
            out.push_back(std::move(it));
        }
        return out;
    }

    static std::vector<StatusItem> BuildThanes(const PlayerStatus::PlayerStatusData& data) {
        std::vector<StatusItem> out;
        for (const auto& t : data.thaneHolds) {
            StatusItem it;
            it.iconKey    = t.key;
            it.title      = t.displayName;
            it.subtitle   = "";
            it.titleColor = kThane;
            out.push_back(std::move(it));
        }
        return out;
    }

    static std::vector<StatusItem> BuildFactions(const PlayerStatus::PlayerStatusData& data) {
        std::vector<StatusItem> out;
        for (const auto& f : data.factions) {
            StatusItem it;
            it.iconKey    = f.key;
            it.title      = f.displayName;
            it.subtitle   = f.isLeader ? f.leaderTitle : LocalizationManager::Get("member");
            it.titleColor = f.isLeader ? kFactionLeader : kFactionMember;
            out.push_back(std::move(it));
        }
        return out;
    }

    static void RenderSection(ImDrawList* dl,
                              const char* headerText,
                              const std::vector<StatusItem>& items,
                              float bodyW,
                              float sc) {
        ImGui::SetWindowFontScale(FontSmall());
        ImVec2 hp = ImGui::GetCursorScreenPos();
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), hp, kAccent, headerText);
        float lineH = ImGui::GetTextLineHeight();
        float ulY = hp.y + lineH + 2.0f * sc;
        dl->AddRectFilledMultiColor(
            ImVec2(hp.x, ulY),
            ImVec2(hp.x + bodyW * 0.5f, ulY + 1.0f),
            kAccent, IM_COL32(212,175,55,0), IM_COL32(0,0,0,0), IM_COL32(0,0,0,0));
        ImGui::Dummy(ImVec2(bodyW, lineH + 8.0f * sc));
        ImGui::SetWindowFontScale(1.0f);

        if (items.empty()) {
            ImGui::SetWindowFontScale(FontSmall());
            ImVec2 ep = ImGui::GetCursorScreenPos();
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(ep.x + 8.0f * sc, ep.y), kTextMuted,
                LocalizationManager::Get("no_major_factions").c_str());
            ImGui::Dummy(ImVec2(bodyW, ImGui::GetTextLineHeight() + 10.0f * sc));
            ImGui::SetWindowFontScale(1.0f);
            return;
        }

        const int   cols     = 2;
        const float colGap   = 10.0f * sc;
        const float rowGap   = 8.0f  * sc;
        const float cardW    = (bodyW - colGap * (cols - 1)) / cols;
        const float iconSz   = 48.0f * sc;
        const float cardH    = iconSz + 10.0f * sc;
        const float iconPad  = 6.0f  * sc;
        const float textGapX = 10.0f * sc;

        ImVec2 origin = ImGui::GetCursorScreenPos();
        int rows = static_cast<int>((items.size() + cols - 1) / cols);

        for (int i = 0; i < static_cast<int>(items.size()); ++i) {
            int r = i / cols;
            int c = i % cols;
            float cx = origin.x + c * (cardW + colGap);
            float cy = origin.y + r * (cardH + rowGap);

            dl->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH),
                IM_COL32(22, 22, 30, 200), 3.0f);
            dl->AddRect(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH),
                kBorderDim, 3.0f, 0, 1.0f);

            float iconX = cx + iconPad;
            float iconY = cy + (cardH - iconSz) * 0.5f;
            std::string iconPath = std::string("Data\\Interface\\PartySheet\\Status\\") + items[i].iconKey + ".png";
            if (auto* tex = TextureManager::GetTexture(iconPath)) {
                HUDDraw::DrawIconStyled(dl, ImVec2(iconX, iconY), iconSz,
                    reinterpret_cast<ImTextureID>(tex), 0xFFFFFFFF, IconFxDefault());
            }

            ImGui::SetWindowFontScale(FontBody());
            float titleH = ImGui::GetTextLineHeight();
            ImGui::SetWindowFontScale(FontSmall());
            float subH   = ImGui::GetTextLineHeight();

            float textBlockH = titleH + (items[i].subtitle.empty() ? 0.0f : (subH + 2.0f * sc));
            float textX = iconX + iconSz + textGapX;
            float textY = cy + (cardH - textBlockH) * 0.5f;

            ImGui::SetWindowFontScale(FontBody());
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(textX, textY), items[i].titleColor, items[i].title.c_str());

            if (!items[i].subtitle.empty()) {
                ImGui::SetWindowFontScale(FontSmall());
                dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(textX, textY + titleH + 2.0f * sc),
                    kSubMuted, items[i].subtitle.c_str());
            }

            ImGui::SetWindowFontScale(1.0f);
        }

        ImGui::Dummy(ImVec2(bodyW, rows * cardH + (rows - 1) * rowGap + 12.0f * sc));
    }

    void RenderStatusModal(RE::Actor* actor) {
        if (!s_StatusOpen) return;
        if (!actor || !actor->IsPlayerRef()) return;

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return;

        auto* settings = Settings::GetSingleton();
        auto& io       = ImGui::GetIO();
        const float fitScale = (std::min)(io.DisplaySize.x * 0.95f / 640.0f, io.DisplaySize.y * 0.90f / 600.0f);
        const float sc = (std::min)(settings->psV2Scale, fitScale);
        s_EffectiveScale = sc;

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("##psv2_status_overlay", nullptr,
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
        const float modalH = 600.0f * sc;
        ImVec2 modalPos = ImVec2((io.DisplaySize.x - modalW) * 0.5f,
                                 (io.DisplaySize.y - modalH) * 0.5f);

        ImGui::SetNextWindowPos(modalPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(modalW, modalH), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(1.0f);
        ImGui::SetNextWindowFocus();

        ImGui::PushStyleColor(ImGuiCol_WindowBg,             ImVec4( 9/255.f,  9/255.f, 11/255.f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_Border,               ImVec4(50/255.f, 50/255.f, 60/255.f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,          ImVec4( 9/255.f,  9/255.f, 11/255.f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,        ImVec4(80/255.f, 65/255.f, 20/255.f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(212/255.f,175/255.f,55/255.f,0.6f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,     ImVec2(16.0f * sc, 14.0f * sc));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,  1.0f);

        ImGui::Begin("##psv2_status_modal", nullptr,
            ImGuiWindowFlags_NoTitleBar  | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove      | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoNav);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 ws = ImGui::GetWindowSize();

        {
            ImFont* hf = ImGuiRenderer::GetHeadingFont();
            if (hf) ImGui::PushFont(hf);
            ImGui::SetWindowFontScale(0.85f);
            char title[160];
            snprintf(title, sizeof(title), "%s - %s",
                actor->GetDisplayFullName() ? actor->GetDisplayFullName() : "?",
                LocalizationManager::Get("header_status").c_str());
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
                CloseStatusModal();
            ImGui::SetWindowFontScale(1.0f);
        }

        float sepY = wp.y + 38.0f * sc;
        dl->AddRectFilledMultiColor(
            ImVec2(wp.x + 16.0f * sc, sepY),
            ImVec2(wp.x + ws.x * 0.7f, sepY + 1.0f),
            kAccent, IM_COL32(212,175,55,0), IM_COL32(0,0,0,0), IM_COL32(0,0,0,0));

        ImGui::Dummy(ImVec2(ws.x - 32.0f * sc, 42.0f * sc));

        const float scrollbarW = 14.0f;
        const float bodyW      = ws.x - 32.0f * sc - scrollbarW;
        float listH = (std::max)(1.0f, ws.y - (ImGui::GetCursorScreenPos().y - wp.y) - 14.0f * sc);

        ImGui::PushStyleColor(ImGuiCol_ChildBg,              ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,          ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,        ImVec4(212/255.f,175/255.f,55/255.f,0.4f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(212/255.f,175/255.f,55/255.f,0.7f));
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize,     10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 3.0f);
        ImGui::BeginChild("##status_list", ImVec2(bodyW + scrollbarW, listH), false, ImGuiWindowFlags_NoNav);

        {
            float wheel = ImGuiRenderer::ConsumeScrollDelta();
            if (wheel != 0.0f)
                ImGui::SetScrollY(ImGui::GetScrollY() - wheel * 40.0f * sc);
        }

        auto data       = PlayerStatus::GetPlayerStatus(player);
        auto champions  = BuildChampions(data);
        auto thanes     = BuildThanes(data);
        auto factions   = BuildFactions(data);

        RenderSection(dl, LocalizationManager::Get("header_champion_status").c_str(), champions, bodyW, sc);
        RenderSection(dl, LocalizationManager::Get("header_thane_status").c_str(),    thanes,    bodyW, sc);
        RenderSection(dl, LocalizationManager::Get("header_faction_status").c_str(),  factions,  bodyW, sc);

        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(5);
        ImGui::End();

        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            CloseStatusModal();
    }

}
