#include "../../includes/ImGui/PartySheetV2.h"
#include "../../includes/ImGui/PartySheetV2Modal.h"
#include "../../includes/ImGui/PartySheetV2InventoryModal.h"
#include "../../includes/ImGui/PartySheetV2StatusModal.h"
#include "../../includes/ImGui/Renderer.h"
#include "../../includes/ImGui/CharacterSheet.h"
#include "../../includes/ImGui/FollowerHUD.h"
#include "../../includes/Settings.h"
#include "../../includes/MenuUtils.h"
#include "../../includes/TextureManager.h"
#include "../../includes/SpellIconManager.h"
#include "../../includes/ActorStats.h"
#include "../../includes/Utils/EquipmentStatResolver.h"
#include "../../includes/Utils/WornGearStatResolver.h"
#include "../../includes/BadgeStyle.h"
#include "../../includes/LocalizationManager.h"
#include "../../includes/Utils/TraitResolver.h"
#include "../../includes/PerfStats.h"
#include "../../includes/Utils/HUDDraw.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <Windows.h>
#include <string>
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace PartySheetV2 {

    static bool s_Visible = false;
    static WornGearStatResolver::WornGearData s_WornGearData;

    struct V2Theme {
        ImU32 bgBase       = IM_COL32(  9,   9,  11, 255);
        ImU32 accent       = IM_COL32(212, 175,  55, 255);
        ImU32 textMain     = IM_COL32(240, 240, 240, 255);
        ImU32 textMuted    = IM_COL32(136, 136, 136, 255);
        ImU32 hp           = IM_COL32(139,   0,   0, 255);
        ImU32 sp           = IM_COL32( 34, 139,  34, 255);
        ImU32 mp           = IM_COL32( 16,  78, 139, 255);
        ImU32 cardBg       = IM_COL32( 18,  18,  24, 200);
        ImU32 borderSubtle = IM_COL32( 50,  50,  60, 255);
        ImU32 hoverFill    = IM_COL32(255, 255, 255,  14);

        static constexpr ImU32 kAccentR = 212, kAccentG = 175, kAccentB = 55;
        static ImU32 AccentA(int a) { return IM_COL32(kAccentR, kAccentG, kAccentB, a); }
    };
    static const V2Theme kTheme;

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

    static HUDDraw::IconFx IconFxItemArt() {
        HUDDraw::IconFx fx = IconFxDefault();
        fx.shadow  = false;
        fx.outline = false;
        return fx;
    }

    // ── Font scale tiers (relative to active font atlas) ────────────────────
    static float s_EffectiveScale = 1.0f;
    static float FontDisplay() { auto* s = Settings::GetSingleton(); return s->psV2FontDisplay * s_EffectiveScale; }
    static float FontHeading() { auto* s = Settings::GetSingleton(); return s->psV2FontHeading * s_EffectiveScale; }
    static float FontBody()    { auto* s = Settings::GetSingleton(); return s->psV2FontBody    * s_EffectiveScale; }
    static float FontSmall()   { auto* s = Settings::GetSingleton(); return s->psV2FontSmall   * s_EffectiveScale; }

    static void AddTextShadowed(ImDrawList* dl, ImFont* font, float fontSize, const ImVec2& pos,
                                ImU32 col, const char* text, const char* textEnd = nullptr,
                                float wrapWidth = 0.0f) {
        const ImU32 sh = IM_COL32(0, 0, 0, 110);
        dl->AddText(font, fontSize, ImVec2(pos.x + 1.0f, pos.y),        sh, text, textEnd, wrapWidth);
        dl->AddText(font, fontSize, ImVec2(pos.x - 1.0f, pos.y),        sh, text, textEnd, wrapWidth);
        dl->AddText(font, fontSize, ImVec2(pos.x,        pos.y + 1.0f), sh, text, textEnd, wrapWidth);
        dl->AddText(font, fontSize, ImVec2(pos.x,        pos.y - 1.0f), sh, text, textEnd, wrapWidth);
        dl->AddText(font, fontSize, pos, col, text, textEnd, wrapWidth);
    }

    // ── Layout constants (base px, matching HTML prototype grid) ─────────────
    static constexpr float kPad        = 40.0f;
    static constexpr float kGap        = 40.0f;
    static constexpr float kSidebarW   = 110.0f;
    static constexpr float kLeftColW   = 600.0f;
    static constexpr float kCenterW    = 530.0f;
    static constexpr float kRightW     = 480.0f;
    static constexpr float kHeaderH    = 130.0f;
    static constexpr float kFooterH    =  60.0f;

    static constexpr float kBaseCanvasW = kPad * 2.0f + kSidebarW + kLeftColW + kCenterW + kRightW + kGap * 3.0f;
    static constexpr float kBaseMainH   = 730.0f;
    static constexpr float kBaseCanvasH = kPad * 2.0f + kHeaderH + kBaseMainH + kFooterH + kGap * 2.0f;

    static std::string GetActorBgStem(RE::Actor* actor) {
        if (!actor || actor->IsPlayerRef()) return {};
        auto* base = actor->GetActorBase();
        if (!base) return {};
        auto* file = base->GetFile(0);
        if (!file) return {};
        std::string plugin = file->GetFilename().data();
        if (plugin.empty()) return {};
        uint32_t localID = base->GetLocalFormID() & 0x00FFFFFF;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "-0x%X", localID);
        return plugin + buf;
    }

    static const char* GetClassBgName(RE::Actor* actor) {
        if (!actor) return "class-common";
        if (actor->IsPlayerRef()) return "class-player";

        if (auto* base = actor->GetActorBase()) {
            if (auto* race = base->GetRace()) {
                std::string raceEid = race->GetFormEditorID() ? race->GetFormEditorID() : "";
                if (raceEid.find("Vampire")    != std::string::npos) return "class-vampire";
                if (raceEid.find("Werewolf")   != std::string::npos) return "class-werewolf";
                if (raceEid.find("Skeleton")   != std::string::npos) return "class-undead";
                if (raceEid.find("Draugr")     != std::string::npos) return "class-undead";
                if (raceEid.find("Lich")       != std::string::npos) return "class-undead";
                if (raceEid.find("Ghost")      != std::string::npos) return "class-undead";
                if (raceEid.find("Dragon")     != std::string::npos) return "class-creatures";
                if (raceEid.find("Dwarven")    != std::string::npos) return "class-creatures";
                if (raceEid.find("Falmer")     != std::string::npos) return "class-creatures";
                if (raceEid.find("Atronach")   != std::string::npos) return "class-daedra";
                if (raceEid.find("Dremora")    != std::string::npos) return "class-daedra";
                if (raceEid.find("Hagraven")   != std::string::npos) return "class-creatures";
                if (raceEid.find("Spider")     != std::string::npos) return "class-creatures";
                if (raceEid.find("Giant")      != std::string::npos) return "class-creatures";
                if (raceEid.find("Horse")      != std::string::npos) return "class-creatures";
                if (raceEid.find("Mammoth")    != std::string::npos) return "class-creatures";
                if (raceEid.find("Bear")       != std::string::npos) return "class-creatures";
                if (raceEid.find("Wolf")       != std::string::npos) return "class-creatures";
            }

            if (auto* cls = base->npcClass) {
                std::string cn = cls->GetName() ? cls->GetName() : "";

                if (cn == "Warrior"      || cn == "Barbarian"     || cn == "Blade"         ||
                    cn == "Guard"        || cn == "Soldier"        || cn == "Witchblade"    ||
                    cn == "Forsworn"     || cn == "Scout"          || cn == "Orc Warrior"   ||
                    cn == "Nord Hero"    || cn == "Prisoner"       || cn == "Bandit")
                    return "class-warrior";

                if (cn == "Ranger"       || cn == "Fletcher"       ||
                    cn == "Bandit Archer"|| cn == "Alik'r Archer"  || cn == "Forsworn Missile")
                    return "class-ranged";

                if (cn == "Assassin"     || cn == "Thief"          ||
                    cn == "Nightblade"   || cn == "Rogue"          || cn == "Jailor")
                    return "class-stealth";

                if (cn == "Sorcerer"     || cn == "Conjurer"       || cn == "Priest"        ||
                    cn == "Spellsword"   || cn == "Mystic"         || cn == "Apothecary"    ||
                    cn == "Destruction Mage" || cn == "Fire Mage"  || cn == "Frost Mage"   ||
                    cn == "Shock Mage"   || cn == "Bandit Wizard"  || cn == "Forsworn Shaman")
                    return "class-mage";

                if (cn.find("Dragon Priest") != std::string::npos ||
                    cn.find("Draugr")        != std::string::npos ||
                    cn.find("Skeleton")      != std::string::npos ||
                    cn == "Vigilant")
                    return "class-undead";
            }
        }

        return "class-common";
    }

    static ID3D11ShaderResourceView* TryLoadBgTexture(const std::string& stem) {
        static std::unordered_map<std::string, ID3D11ShaderResourceView*> s_Cache;
        auto it = s_Cache.find(stem);
        if (it != s_Cache.end()) return it->second;
        std::string base = "Data\\Interface\\PartySheet\\Backgrounds\\" + stem;
        auto* t = TextureManager::GetTexture(base + ".png");
        if (!t) t = TextureManager::GetTexture(base + ".jpg");
        s_Cache[stem] = t;
        return t;
    }

    static ID3D11ShaderResourceView* GetClassBgTexture(const char* stem) {
        auto* tex = TryLoadBgTexture(stem);
        if (!tex && std::string(stem) != "class-default")
            tex = TryLoadBgTexture("class-default");
        return tex;
    }

    // ── Subject (selected party member) ─────────────────────────────────────
    static int   s_SubjectIndex  = 0;
    static float s_SidebarScroll = 0.0f;

    static std::vector<RE::Actor*> BuildSubjectList() {
        std::vector<RE::Actor*> list;
        if (auto* player = RE::PlayerCharacter::GetSingleton())
            list.push_back(player);
        for (auto* ally : FollowerHUD::GetAllies())
            if (ally) list.push_back(ally);
        return list;
    }

    enum class ActorState { Following, Waiting, Combat, Mounted, Downed };

    static RE::ActorValue GetWaitingAV() {
        static RE::ActorValue av = RE::ActorValue::kNone;
        if (av == RE::ActorValue::kNone)
            if (auto* avl = RE::ActorValueList::GetSingleton())
                av = avl->LookupActorValueByName("WaitingforPlayer");
        return av;
    }

    static ActorState GetActorState(RE::Actor* actor) {
        if (!actor) return ActorState::Following;
        if (actor->AsActorState()->IsBleedingOut()) return ActorState::Downed;
        if (actor->IsInCombat())                    return ActorState::Combat;
        if (actor->IsOnMount())                     return ActorState::Mounted;
        RE::ActorValue waitAV = GetWaitingAV();
        if (waitAV != RE::ActorValue::kNone)
            return (actor->AsActorValueOwner()->GetActorValue(waitAV) >= 1.0f)
                ? ActorState::Waiting : ActorState::Following;
        return ActorState::Following;
    }

    static void DrawDiamondPortrait(ImDrawList* dl, ID3D11ShaderResourceView* tex,
                                    float cx, float cy, float half) {
        ImVec2 top   (cx,        cy - half);
        ImVec2 right (cx + half, cy       );
        ImVec2 bottom(cx,        cy + half);
        ImVec2 left  (cx - half, cy       );

        if (tex) {
            dl->AddImageQuad(
                reinterpret_cast<ImTextureID>(tex),
                top, right, bottom, left,
                ImVec2(0.5f, 0.0f),
                ImVec2(1.0f, 0.5f),
                ImVec2(0.5f, 1.0f),
                ImVec2(0.0f, 0.5f)
            );
        } else {
            dl->AddQuadFilled(top, right, bottom, left, IM_COL32(30, 30, 40, 200));
        }
    }

    static void DrawDiamondBorder(ImDrawList* dl, float cx, float cy, float half,
                                  ImU32 color, float thickness = 1.0f) {
        ImVec2 top   (cx,        cy - half);
        ImVec2 right (cx + half, cy       );
        ImVec2 bottom(cx,        cy + half);
        ImVec2 left  (cx - half, cy       );
        dl->AddQuad(top, right, bottom, left, color, thickness);
    }

    static void DisablePlayerInput() {
        if (auto* pc = RE::PlayerControls::GetSingleton()) {
            if (pc->lookHandler)        pc->lookHandler->SetInputEventHandlingEnabled(false);
            if (pc->movementHandler)    pc->movementHandler->SetInputEventHandlingEnabled(false);
            if (pc->attackBlockHandler) pc->attackBlockHandler->SetInputEventHandlingEnabled(false);
        }
    }

    static void EnablePlayerInput() {
        if (auto* pc = RE::PlayerControls::GetSingleton()) {
            if (pc->lookHandler)        pc->lookHandler->SetInputEventHandlingEnabled(true);
            if (pc->movementHandler)    pc->movementHandler->SetInputEventHandlingEnabled(true);
            if (pc->attackBlockHandler) pc->attackBlockHandler->SetInputEventHandlingEnabled(true);
        }
    }

    bool IsVisible() {
        return s_Visible;
    }

    void PrevSubject() {
        auto subjects = BuildSubjectList();
        if (subjects.empty()) return;
        s_SubjectIndex = (s_SubjectIndex - 1 + static_cast<int>(subjects.size()))
                         % static_cast<int>(subjects.size());
    }

    void NextSubject() {
        auto subjects = BuildSubjectList();
        if (subjects.empty()) return;
        s_SubjectIndex = (s_SubjectIndex + 1) % static_cast<int>(subjects.size());
    }

    void Toggle() {
        if (MenuUtils::ShouldBlockCharacterSheet()) return;

        if (!s_Visible) {
            if (ImGuiRenderer::IsVisible())
                ImGuiRenderer::Toggle();
            auto* settings = Settings::GetSingleton();
            if (settings->enableCharacterSheet && CharacterSheet::GetSingleton()->IsVisible())
                CharacterSheet::GetSingleton()->Toggle();
        }

        bool wasVisible = s_Visible;
        s_Visible = !s_Visible;

        if (s_Visible && !wasVisible) {
            DisablePlayerInput();
        } else if (!s_Visible && wasVisible) {
            CloseEffectsModal();
            CloseInventoryModal();
            CloseStatusModal();
            if (!ImGuiRenderer::IsVisible())
                EnablePlayerInput();
        }
    }

    static void RenderSidebar(ImVec2 size) {
        static ID3D11ShaderResourceView* s_TexFollow  = nullptr;
        static ID3D11ShaderResourceView* s_TexWait    = nullptr;
        static ID3D11ShaderResourceView* s_TexCombat  = nullptr;
        static ID3D11ShaderResourceView* s_TexMounted = nullptr;
        static ID3D11ShaderResourceView* s_TexDowned  = nullptr;
        if (!s_TexFollow)  s_TexFollow  = TextureManager::GetPartyIcon("StateFollow");
        if (!s_TexWait)    s_TexWait    = TextureManager::GetPartyIcon("StateWait");
        if (!s_TexCombat)  s_TexCombat  = TextureManager::GetPartyIcon("StateCombat");
        if (!s_TexMounted) s_TexMounted = TextureManager::GetPartyIcon("StateMounted");
        if (!s_TexDowned)  s_TexDowned  = TextureManager::GetPartyIcon("StateDowned");

        std::vector<RE::Actor*> subjects = BuildSubjectList();
        if (s_SubjectIndex >= static_cast<int>(subjects.size()))
            s_SubjectIndex = 0;

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 origin  = ImGui::GetCursorScreenPos();

        const float sc      = s_EffectiveScale;
        const float halfAct = 40.0f * sc;
        const float halfInc = 32.0f * sc;
        const float gapY    = 16.0f * sc;
        const float cx      = origin.x + size.x * 0.5f;
        const float visH    = size.y;

        if (ImGui::IsWindowHovered()) {
            s_SidebarScroll -= ImGui::GetIO().MouseWheel * 60.0f * sc;
        }

        // Compute total content height to clamp scroll
        float totalH = 16.0f * sc;
        for (int i = 0; i < static_cast<int>(subjects.size()); ++i) {
            float h = (i == s_SubjectIndex ? halfAct : halfInc) * 2.0f + gapY;
            totalH += h;
        }
        float maxScroll = std::max(0.0f, totalH - visH);
        s_SidebarScroll = std::max(0.0f, std::min(s_SidebarScroll, maxScroll));

        {
            float itemY = 16.0f * sc;
            for (int i = 0; i < s_SubjectIndex; ++i)
                itemY += (i == s_SubjectIndex ? halfAct : halfInc) * 2.0f + gapY;
            float itemTop    = itemY - s_SidebarScroll;
            float itemBottom = itemTop + halfAct * 2.0f;
            if (itemBottom > visH)
                s_SidebarScroll = std::max(s_SidebarScroll, itemY + halfAct * 2.0f - visH + 8.0f * sc);
            if (itemTop < 0.0f)
                s_SidebarScroll = std::min(s_SidebarScroll, itemY);
        }

        float curY = origin.y + 16.0f * sc - s_SidebarScroll;

        ImVec2 clipMin = origin;
        ImVec2 clipMax = ImVec2(origin.x + size.x, origin.y + size.y);
        dl->PushClipRect(clipMin, clipMax, true);

        for (int i = 0; i < static_cast<int>(subjects.size()); ++i) {
            RE::Actor* actor  = subjects[i];
            bool isActive     = (i == s_SubjectIndex);
            float half        = isActive ? halfAct : halfInc;
            float cy          = curY + half;

            auto* portrait = TextureManager::GetPortrait(actor);

            if (isActive) {
                DrawDiamondBorder(dl, cx, cy, half + 5.0f * sc,
                    V2Theme::AccentA(50), 4.0f * sc);
                DrawDiamondBorder(dl, cx, cy, half + 2.0f * sc,
                    V2Theme::AccentA(120), 1.5f * sc);
            }

            ImVec2 top   (cx,        cy - half);
            ImVec2 right (cx + half, cy       );
            ImVec2 bottom(cx,        cy + half);
            ImVec2 left  (cx - half, cy       );
            dl->AddQuadFilled(top, right, bottom, left, IM_COL32(0, 0, 0, 140));

            DrawDiamondPortrait(dl, portrait, cx, cy, half);

            if (isActive) {
                static ID3D11ShaderResourceView* s_AvatarFrame = nullptr;
                if (!s_AvatarFrame) s_AvatarFrame = TextureManager::GetFrameTexture("bgframe-1");
                if (s_AvatarFrame) {
                    const float frameScale = 0.85f;
                    const float fHalf = half * frameScale;
                    ImVec2 fTL(cx - fHalf, cy - fHalf);
                    ImVec2 fBR(cx + fHalf, cy + fHalf);
                    dl->AddImage(reinterpret_cast<ImTextureID>(s_AvatarFrame), fTL, fBR);
                }
            }

            ImU32 borderColor = isActive
                ? V2Theme::AccentA(220)
                : IM_COL32(255, 255, 255,  25);
            DrawDiamondBorder(dl, cx, cy, half, borderColor, isActive ? 1.5f : 1.0f);

            {
                int level = actor->GetLevel();
                char lvlBuf[8];
                snprintf(lvlBuf, sizeof(lvlBuf), "%d", level);
                ImGui::SetWindowFontScale(FontSmall());
                ImVec2 textSz = ImGui::CalcTextSize(lvlBuf);
                ImGui::SetWindowFontScale(1.0f);

                const float bpad = 3.0f * sc;
                ImVec2 bMin(bottom.x - textSz.x * 0.5f - bpad,
                            bottom.y - 5.0f * sc - 1);
                ImVec2 bMax(bottom.x + textSz.x * 0.5f + bpad,
                            bottom.y - 5.0f * sc + textSz.y + bpad * 2 + 1);
                dl->AddRectFilled(bMin, bMax, IM_COL32(9, 9, 11, 255), 2.0f * sc);
                dl->AddRect(bMin, bMax, V2Theme::AccentA(160), 2.0f * sc, 0, 1.0f);

                ImGui::SetWindowFontScale(FontSmall());
                dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(bMin.x + bpad, bMin.y + bpad),
                    V2Theme::AccentA(255), lvlBuf);
                ImGui::SetWindowFontScale(1.0f);
            }

            {
                bool isPlayer = (actor == RE::PlayerCharacter::GetSingleton());
                ID3D11ShaderResourceView* stateTex = nullptr;
                if (!isPlayer) {
                    ActorState state = GetActorState(actor);
                    stateTex =
                        (state == ActorState::Downed)  ? s_TexDowned  :
                        (state == ActorState::Combat)  ? s_TexCombat  :
                        (state == ActorState::Mounted) ? s_TexMounted :
                        (state == ActorState::Waiting) ? s_TexWait    : s_TexFollow;
                }
                if (stateTex) {
                    const float iconHalf = 8.0f * sc;
                    ImVec2 iMin(right.x - 5.0f * sc - 1,              right.y - iconHalf - 1);
                    ImVec2 iMax(right.x - 5.0f * sc + iconHalf * 2 + 1, right.y + iconHalf + 1);
                    dl->AddRectFilled(iMin, iMax, IM_COL32(9, 9, 11, 255), 2.0f * sc);
                    ImVec2 iTl(iMin.x + 1, iMin.y + 1), iBr(iMax.x - 1, iMax.y - 1);
                    HUDDraw::DrawIconStyled(dl, iTl, ImVec2(iBr.x - iTl.x, iBr.y - iTl.y),
                        reinterpret_cast<ImTextureID>(stateTex), 0xFFFFFFFF, IconFxDefault());
                }
            }

            {
                ActorStats stats = GetActorStats(actor);
                float hp = stats.health.GetPercent();
                if (hp < 0.8f) {
                    bool crit = hp < 0.25f;
                    float speed = crit ? 5.0f : 2.5f;
                    float alpha = (sinf(static_cast<float>(ImGui::GetTime()) * speed) * 0.5f + 0.5f)
                                  * 0.35f + 0.08f;
                    ImU32 pulseCol = IM_COL32(220, 20, 20, static_cast<int>(alpha * 255));
                    dl->AddQuadFilled(top, right, bottom, left, pulseCol);
                    if (crit)
                        DrawDiamondBorder(dl, cx, cy, half, IM_COL32(255, 60, 60, 180), 1.5f * sc);
                }
            }

            ImGui::SetCursorScreenPos(ImVec2(cx - half, cy - half));
            ImGui::InvisibleButton(("##psv2_avatar_" + std::to_string(i)).c_str(),
                ImVec2(half * 2.0f, half * 2.0f));
            if (ImGui::IsItemClicked())
                s_SubjectIndex = i;

            curY += half * 2.0f + gapY;
        }

        dl->PopClipRect();

        ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + size.y));
    }

    static void RenderHeader(ImVec2 size) {
        auto subjects = BuildSubjectList();
        if (subjects.empty()) return;
        if (s_SubjectIndex >= static_cast<int>(subjects.size())) s_SubjectIndex = 0;
        RE::Actor* actor = subjects[s_SubjectIndex];
        if (!actor) return;

        const float sc  = s_EffectiveScale;
        ImDrawList* dl  = ImGui::GetWindowDrawList();
        ImVec2 origin   = ImGui::GetCursorScreenPos();
        bool isPlayer   = (actor == RE::PlayerCharacter::GetSingleton());

        float rowY = origin.y;

        ImFont* headingFont = ImGuiRenderer::GetHeadingFont();

        const char* name = actor->GetDisplayFullName();
        static std::string s_unknownName;
        if (s_unknownName.empty()) s_unknownName = LocalizationManager::Get("Common.Unknown");
        if (!name || name[0] == '\0') name = s_unknownName.c_str();
        char nameBuf[128];
        snprintf(nameBuf, sizeof(nameBuf), "%s", name);
        for (char* p = nameBuf; *p; ++p) *p = static_cast<char>(toupper(*p));

        if (headingFont) ImGui::PushFont(headingFont);
        ImGui::SetWindowFontScale(FontDisplay());
        ImVec2 nameSz = ImGui::CalcTextSize(nameBuf);
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
            ImVec2(origin.x, rowY), kTheme.textMain, nameBuf);
        ImGui::SetWindowFontScale(1.0f);
        if (headingFont) ImGui::PopFont();

        float lineB_Y = rowY + nameSz.y + 4.0f * sc;
        float penX    = origin.x;

        {
            std::string raceClass;
            if (auto* race = actor->GetRace())
                raceClass += race->GetFullName();
            if (!isPlayer) {
                if (auto* base = actor->GetActorBase()) {
                    if (auto* npc = base->As<RE::TESNPC>()) {
                        if (npc->npcClass && npc->npcClass->GetFullName()[0] != '\0') {
                            raceClass += "  |  ";
                            raceClass += npc->npcClass->GetFullName();
                        }
                    }
                }
            }
            raceClass += "  |  " + LocalizationManager::Get("Common.Level") + " ";
            raceClass += std::to_string(actor->GetLevel());
            for (char& c : raceClass) c = static_cast<char>(toupper(c));

            if (headingFont) ImGui::PushFont(headingFont);
            ImGui::SetWindowFontScale(FontHeading());
            ImVec2 rcSz = ImGui::CalcTextSize(raceClass.c_str());
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(penX, lineB_Y), kTheme.textMuted, raceClass.c_str());
            penX += rcSz.x + 16.0f * sc;
            ImGui::SetWindowFontScale(1.0f);
            if (headingFont) ImGui::PopFont();
        }

        {
            auto* base      = actor->GetActorBase();
            bool essential  = base && base->IsEssential();
            bool protected_ = base && base->IsProtected();

            std::string relTagStr;
            if (isPlayer) {
                relTagStr = LocalizationManager::Get("Common.Player");
            } else {
                auto* player = RE::PlayerCharacter::GetSingleton();
                if (player && actor->IsHostileToActor(player)) {
                    relTagStr = LocalizationManager::Get("Common.Foe");
                } else {
                    auto* playerBase = player ? player->GetActorBase() : nullptr;
                    RE::BGSRelationship* rel = nullptr;
                    if (base && playerBase) {
                        rel = RE::BGSRelationship::GetRelationship(base, playerBase);
                        if (!rel) rel = RE::BGSRelationship::GetRelationship(playerBase, base);
                    }
                    if (rel) {
                        using Level = RE::BGSRelationship::RELATIONSHIP_LEVEL;
                        switch (rel->level.get()) {
                            case Level::kLover:        relTagStr = LocalizationManager::Get("Common.Lover");        break;
                            case Level::kAlly:         relTagStr = LocalizationManager::Get("Common.Ally");         break;
                            case Level::kConfidant:    relTagStr = LocalizationManager::Get("Common.Confidant");    break;
                            case Level::kFriend:       relTagStr = LocalizationManager::Get("Common.Friend");       break;
                            case Level::kRival:        relTagStr = LocalizationManager::Get("Common.Rival");        break;
                            case Level::kFoe:          relTagStr = LocalizationManager::Get("Common.Foe");          break;
                            case Level::kEnemy:        relTagStr = LocalizationManager::Get("Common.Enemy");        break;
                            case Level::kArchnemesis:  relTagStr = LocalizationManager::Get("Common.Archnemesis");  break;
                            case Level::kAcquaintance:
                            default:                   relTagStr = LocalizationManager::Get("Common.Acquaintance"); break;
                        }
                    } else {
                        relTagStr = LocalizationManager::Get("Common.Acquaintance");
                    }
                }
            }
            for (char& c : relTagStr) c = static_cast<char>(toupper(c));
            const char* relTag = relTagStr.c_str();

            ImGui::SetWindowFontScale(FontSmall());
            const float tpad  = 4.0f * sc;
            const float tpadH = 2.0f * sc;

            ImVec2 refSz = ImGui::CalcTextSize(relTag);
            float tagY = lineB_Y;

            auto DrawTag = [&](const char* text, ImU32 border, ImU32 textCol) {
                ImVec2 sz  = ImGui::CalcTextSize(text);
                ImVec2 tMin(penX, tagY);
                ImVec2 tMax(penX + sz.x + tpad * 2.0f, tagY + sz.y + tpadH * 2.0f);
                dl->AddRect(tMin, tMax, border, 2.0f * sc, 0, 1.0f);
                dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(tMin.x + tpad, tMin.y + tpadH), textCol, text);
                penX += sz.x + tpad * 2.0f + 8.0f * sc;
            };

            auto DrawLabel = [&](const char* text, ImU32 textCol) {
                ImVec2 sz = ImGui::CalcTextSize(text);
                dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(penX, tagY + tpadH), textCol, text);
                penX += sz.x + 10.0f * sc;
            };

            DrawLabel(relTag, kTheme.accent);

            if (essential) {
                std::string esStr = LocalizationManager::Get("Common.Essential");
                for (char& c : esStr) c = static_cast<char>(toupper(c));
                DrawTag(esStr.c_str(), IM_COL32(200, 80, 80, 200), IM_COL32(200, 80, 80, 255));
            }
            if (protected_) {
                std::string prStr = LocalizationManager::Get("Common.Protected");
                for (char& c : prStr) c = static_cast<char>(toupper(c));
                DrawTag(prStr.c_str(), IM_COL32(180,140, 60, 200), IM_COL32(180,140, 60, 255));
            }

            if (!isPlayer) {
                if (auto* actorBase = actor->GetActorBase()) {
                    std::string aggrStr;
                    switch (static_cast<uint8_t>(actorBase->GetAggressionLevel())) {
                        case 0: aggrStr = LocalizationManager::Get("AI.Aggression.Unaggressive");  break;
                        case 1: aggrStr = LocalizationManager::Get("AI.Aggression.Aggressive");    break;
                        case 2: aggrStr = LocalizationManager::Get("AI.Aggression.VeryAggressive"); break;
                        case 3: aggrStr = LocalizationManager::Get("AI.Aggression.Frenzied");      break;
                        default: aggrStr = "?"; break;
                    }
                    std::string confStr;
                    switch (static_cast<uint8_t>(actorBase->GetConfidenceLevel())) {
                        case 0: confStr = LocalizationManager::Get("AI.Confidence.Cowardly");  break;
                        case 1: confStr = LocalizationManager::Get("AI.Confidence.Cautious");  break;
                        case 2: confStr = LocalizationManager::Get("AI.Confidence.Average");   break;
                        case 3: confStr = LocalizationManager::Get("AI.Confidence.Brave");     break;
                        case 4: confStr = LocalizationManager::Get("AI.Confidence.Foolhardy"); break;
                        default: confStr = "?"; break;
                    }
                    std::string assistStr;
                    switch (static_cast<uint8_t>(actorBase->GetAssistanceLevel())) {
                        case 0: assistStr = LocalizationManager::Get("AI.Assistance.Nobody");           break;
                        case 1: assistStr = LocalizationManager::Get("AI.Assistance.Allies");           break;
                        case 2: assistStr = LocalizationManager::Get("AI.Assistance.FriendsAndAllies"); break;
                        default: assistStr = "?"; break;
                    }

                    std::string aiLabelAggr   = LocalizationManager::Get("AI.Aggression.Label");
                    std::string aiLabelConf   = LocalizationManager::Get("AI.Confidence.Label");
                    std::string aiLabelAssist = LocalizationManager::Get("AI.Assistance.Label");
                    for (char& c : aiLabelAggr)   c = static_cast<char>(toupper(c));
                    for (char& c : aiLabelConf)   c = static_cast<char>(toupper(c));
                    for (char& c : aiLabelAssist) c = static_cast<char>(toupper(c));

                    struct AIEntry { const std::string& label; const std::string& value; ImU32 col; };
                    AIEntry aiEntries[] = {
                        { aiLabelAggr,   aggrStr,   IM_COL32(255, 120,  80, 255) },
                        { aiLabelConf,   confStr,   IM_COL32(100, 200, 255, 255) },
                        { aiLabelAssist, assistStr, IM_COL32(100, 220, 120, 255) },
                    };
                    constexpr float kAIColGap = 22.0f;
                    float aiBlockW = 0.0f;
                    for (int ai = 0; ai < 3; ++ai) {
                        ImVec2 ls = ImGui::CalcTextSize(aiEntries[ai].label.c_str());
                        ImVec2 vs = ImGui::CalcTextSize(aiEntries[ai].value.c_str());
                        aiBlockW += std::max(ls.x, vs.x);
                        if (ai < 2) aiBlockW += kAIColGap * sc;
                    }

                    float aiX = origin.x + size.x - aiBlockW;
                    for (const auto& ai : aiEntries) {
                        ImVec2 labelSz2 = ImGui::CalcTextSize(ai.label.c_str());
                        ImVec2 valueSz  = ImGui::CalcTextSize(ai.value.c_str());
                        float  colW     = std::max(labelSz2.x, valueSz.x);

                        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                            ImVec2(aiX, tagY), kTheme.textMuted, ai.label.c_str());
                        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                            ImVec2(aiX, tagY + labelSz2.y + 2.0f * sc), ai.col, ai.value.c_str());

                        aiX += colW + kAIColGap * sc;
                    }
                }
            }

            ImGui::SetWindowFontScale(1.0f);
        }

        if (headingFont) ImGui::PushFont(headingFont);
        ImGui::SetWindowFontScale(FontHeading());
        float lineBH = ImGui::GetTextLineHeight();
        ImGui::SetWindowFontScale(1.0f);
        if (headingFont) ImGui::PopFont();
        const float sepY = lineB_Y + lineBH + 10.0f * sc;
        dl->AddRectFilledMultiColor(
            ImVec2(origin.x,              sepY),
            ImVec2(origin.x + size.x * 0.5f, sepY + 1.0f),
            IM_COL32(0,0,0,0), IM_COL32(212,175,55,100), IM_COL32(212,175,55,100), IM_COL32(0,0,0,0));
        dl->AddRectFilledMultiColor(
            ImVec2(origin.x + size.x * 0.5f, sepY),
            ImVec2(origin.x + size.x,        sepY + 1.0f),
            IM_COL32(212,175,55,100), IM_COL32(0,0,0,0), IM_COL32(0,0,0,0), IM_COL32(212,175,55,100));

        const float barsY    = sepY + 14.0f * sc;
        const float barH     = 5.0f  * sc;
        const float labelGap = 6.0f  * sc;
        ActorStats stats     = GetActorStats(actor);

        struct VitalDef { std::string label; float cur; float max; ImU32 color; };
        VitalDef vitals[3] = {
            { LocalizationManager::Get("Stats.Health"),  stats.health.current,  stats.health.GetMax(),  kTheme.hp },
            { LocalizationManager::Get("Stats.Stamina"), stats.stamina.current, stats.stamina.GetMax(), kTheme.sp },
            { LocalizationManager::Get("Stats.Magicka"), stats.magicka.current, stats.magicka.GetMax(), kTheme.mp },
        };

        const float vGap     = 24.0f * sc;
        const float leftColW = size.x - kCenterW * sc - kRightW * sc - 2.0f * kGap * sc;
        const float barsW    = std::max(leftColW, 200.0f * sc);
        const float vitalW   = (barsW - 2.0f * vGap) / 3.0f;

        for (int v = 0; v < 3; ++v) {
            const auto& vt = vitals[v];
            float vx = origin.x + v * (vitalW + vGap);

            ImGui::SetWindowFontScale(FontBody());
            ImVec2 labelSz = ImGui::CalcTextSize(vt.label.c_str());

            char valBuf[32];
            snprintf(valBuf, sizeof(valBuf), "%.0f / %.0f", vt.cur, vt.max);
            ImVec2 valSz = ImGui::CalcTextSize(valBuf);

            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(vx, barsY), kTheme.textMuted, vt.label.c_str());
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(vx + vitalW - valSz.x, barsY), kTheme.textMain, valBuf);
            ImGui::SetWindowFontScale(1.0f);

            float barY = barsY + labelSz.y + labelGap;
            ImVec2 bMin(vx, barY);
            ImVec2 bMax(vx + vitalW, barY + barH);
            dl->AddRectFilled(bMin, bMax, IM_COL32(0, 0, 0, 130), 2.0f * sc);
            dl->AddRect(bMin, bMax, IM_COL32(255, 255, 255, 15), 2.0f * sc, 0, 1.0f);

            float pct = (vt.max > 0.0f) ? std::clamp(vt.cur / vt.max, 0.0f, 1.0f) : 0.0f;
            if (pct > 0.0f) {
                ImVec2 fMax(vx + vitalW * pct, barY + barH);
                dl->AddRectFilled(bMin, fMax, vt.color, 2.0f * sc);
                dl->AddRectFilledMultiColor(
                    bMin, ImVec2(fMax.x, barY + barH * 0.5f),
                    IM_COL32(255,255,255,40), IM_COL32(255,255,255,40),
                    IM_COL32(0,0,0,0), IM_COL32(0,0,0,0));
            }
        }

        ImGui::Dummy(size);
    }

    struct SkillTex {
        ID3D11ShaderResourceView* oneHanded   = nullptr;
        ID3D11ShaderResourceView* twoHanded   = nullptr;
        ID3D11ShaderResourceView* block       = nullptr;
        ID3D11ShaderResourceView* heavyArmor  = nullptr;
        ID3D11ShaderResourceView* lightArmor  = nullptr;
        ID3D11ShaderResourceView* smithing    = nullptr;
        ID3D11ShaderResourceView* sneak       = nullptr;
        ID3D11ShaderResourceView* archery     = nullptr;
        ID3D11ShaderResourceView* speech      = nullptr;
        ID3D11ShaderResourceView* alchemy     = nullptr;
        ID3D11ShaderResourceView* lockpicking = nullptr;
        ID3D11ShaderResourceView* pickpocket  = nullptr;
        ID3D11ShaderResourceView* conjuration = nullptr;
        ID3D11ShaderResourceView* illusion    = nullptr;
        ID3D11ShaderResourceView* alteration  = nullptr;
        ID3D11ShaderResourceView* destruction = nullptr;
        ID3D11ShaderResourceView* restoration = nullptr;
        ID3D11ShaderResourceView* enchanting  = nullptr;
    };

    static void LoadSkillTextures(SkillTex& t) {
        if (!t.oneHanded)   t.oneHanded   = TextureManager::GetPartyIcon("onehanded");
        if (!t.twoHanded)   t.twoHanded   = TextureManager::GetPartyIcon("twohanded");
        if (!t.block)       t.block       = TextureManager::GetPartyIcon("block");
        if (!t.heavyArmor)  t.heavyArmor  = TextureManager::GetPartyIcon("heavyarmor");
        if (!t.lightArmor)  t.lightArmor  = TextureManager::GetPartyIcon("lightarmor");
        if (!t.smithing)    t.smithing    = TextureManager::GetPartyIcon("smithing");
        if (!t.sneak)       t.sneak       = TextureManager::GetPartyIcon("sneak");
        if (!t.archery)     t.archery     = TextureManager::GetPartyIcon("archery");
        if (!t.speech)      t.speech      = TextureManager::GetPartyIcon("speech");
        if (!t.alchemy)     t.alchemy     = TextureManager::GetPartyIcon("alchemy");
        if (!t.lockpicking) t.lockpicking = TextureManager::GetPartyIcon("lockpicking");
        if (!t.pickpocket)  t.pickpocket  = TextureManager::GetPartyIcon("pickpocket");
        if (!t.conjuration) t.conjuration = TextureManager::GetPartyIcon("conjuration");
        if (!t.illusion)    t.illusion    = TextureManager::GetPartyIcon("illusion");
        if (!t.alteration)  t.alteration  = TextureManager::GetPartyIcon("alteration");
        if (!t.destruction) t.destruction = TextureManager::GetPartyIcon("destruction");
        if (!t.restoration) t.restoration = TextureManager::GetPartyIcon("restoration");
        if (!t.enchanting)  t.enchanting  = TextureManager::GetPartyIcon("enchanting");
    }

    static void RenderLeftColumn(ImVec2 size) {
        static SkillTex s_SkillTex;
        LoadSkillTextures(s_SkillTex);

        auto subjects = BuildSubjectList();
        if (subjects.empty()) return;
        if (s_SubjectIndex >= static_cast<int>(subjects.size())) s_SubjectIndex = 0;
        RE::Actor* actor = subjects[s_SubjectIndex];
        if (!actor) return;

        ActorStats stats = GetActorStats(actor);
        const ActorSkills& sk = stats.skills;

        const float sc  = s_EffectiveScale;
        ImDrawList* dl  = ImGui::GetWindowDrawList();
        ImVec2 origin   = ImGui::GetCursorScreenPos();

        ImFont* headingFont = ImGuiRenderer::GetHeadingFont();
        if (headingFont) ImGui::PushFont(headingFont);
        ImGui::SetWindowFontScale(FontHeading());
        std::string skillsTitle = LocalizationManager::Get("Skills.SectionTitle");
        for (char& c : skillsTitle) c = static_cast<char>(toupper(c));
        const char* titleText = skillsTitle.c_str();
        ImVec2 titleSz = ImGui::CalcTextSize(titleText);
        AddTextShadowed(dl, ImGui::GetFont(), ImGui::GetFontSize(),
            ImVec2(origin.x, origin.y), kTheme.accent, titleText);
        ImGui::SetWindowFontScale(1.0f);
        if (headingFont) ImGui::PopFont();

        const float sepY  = origin.y + titleSz.y + 4.0f * sc;
        const float lineW = size.x;
        dl->AddRectFilledMultiColor(
            ImVec2(origin.x,                  sepY),
            ImVec2(origin.x + lineW * 0.5f,   sepY + 1.0f),
            IM_COL32(0,0,0,0), IM_COL32(212,175,55,100), IM_COL32(212,175,55,100), IM_COL32(0,0,0,0));
        dl->AddRectFilledMultiColor(
            ImVec2(origin.x + lineW * 0.5f,   sepY),
            ImVec2(origin.x + lineW,           sepY + 1.0f),
            IM_COL32(212,175,55,100), IM_COL32(0,0,0,0), IM_COL32(0,0,0,0), IM_COL32(212,175,55,100));

        struct SkillEntry { std::string name; float value; ID3D11ShaderResourceView* tex; };
        const SkillEntry skills[18] = {
            { LocalizationManager::Get("Skills.OneHanded"),  sk.oneHanded.value,  s_SkillTex.oneHanded   },
            { LocalizationManager::Get("Skills.TwoHanded"),  sk.twoHanded.value,  s_SkillTex.twoHanded   },
            { LocalizationManager::Get("Skills.Block"),      sk.block.value,       s_SkillTex.block       },
            { LocalizationManager::Get("Skills.HeavyArmor"), sk.heavyArmor.value,  s_SkillTex.heavyArmor  },
            { LocalizationManager::Get("Skills.LightArmor"), sk.lightArmor.value,  s_SkillTex.lightArmor  },
            { LocalizationManager::Get("Skills.Smithing"),   sk.smithing.value,    s_SkillTex.smithing    },
            { LocalizationManager::Get("Skills.Sneak"),      sk.sneak.value,       s_SkillTex.sneak       },
            { LocalizationManager::Get("Skills.Archery"),    sk.archery.value,     s_SkillTex.archery     },
            { LocalizationManager::Get("Skills.Speech"),     sk.speech.value,      s_SkillTex.speech      },
            { LocalizationManager::Get("Skills.Alchemy"),    sk.alchemy.value,     s_SkillTex.alchemy     },
            { LocalizationManager::Get("Skills.Lockpicking"),sk.lockpicking.value, s_SkillTex.lockpicking },
            { LocalizationManager::Get("Skills.Pickpocket"), sk.pickpocket.value,  s_SkillTex.pickpocket  },
            { LocalizationManager::Get("Skills.Conjuration"),sk.conjuration.value, s_SkillTex.conjuration },
            { LocalizationManager::Get("Skills.Illusion"),   sk.illusion.value,    s_SkillTex.illusion    },
            { LocalizationManager::Get("Skills.Alteration"), sk.alteration.value,  s_SkillTex.alteration  },
            { LocalizationManager::Get("Skills.Destruction"),sk.destruction.value, s_SkillTex.destruction },
            { LocalizationManager::Get("Skills.Restoration"),sk.restoration.value, s_SkillTex.restoration },
            { LocalizationManager::Get("Skills.Enchanting"), sk.enchanting.value,  s_SkillTex.enchanting  },
        };

        // Row dimensions: icon + label LEFT, value RIGHT
        const float iconSz   = Settings::GetSingleton()->psV2IconSkill * sc;
        const float rowPadX  =  5.0f * sc;
        const float rowPadY  =  3.0f * sc;
        const float colGapX  =  8.0f * sc;
        const float rowGapY  =  8.0f * sc;

        const float colW = (size.x - colGapX * 2.0f) / 3.0f;

        ImGui::SetWindowFontScale(FontBody());
        float textLineH = ImGui::GetTextLineHeight();
        ImGui::SetWindowFontScale(1.0f);
        const float rowH = std::max(iconSz, textLineH) + rowPadY * 2.0f;

        float startY = sepY + 16.0f * sc;

        for (int i = 0; i < 18; ++i) {
            const auto& s = skills[i];

            int colIdx = i / 6;
            int rowIdx = i % 6;

            float cx = origin.x + colIdx * (colW + colGapX);
            float cy = startY + rowIdx * (rowH + rowGapY);

            ImVec2 rMin(cx, cy);
            ImVec2 rMax(cx + colW, cy + rowH);

            if (ImGui::IsMouseHoveringRect(rMin, rMax))
                dl->AddRectFilled(rMin, rMax, kTheme.hoverFill, 3.0f * sc);

            const bool skillHighlighted       = (s_WornGearData.skillHighlightMask       & (1u << i)) != 0;
            const bool situationalHighlighted = (s_WornGearData.situationalHighlightMask & (1u << i)) != 0;
            if (skillHighlighted)
                dl->AddRectFilled(rMin, rMax, BadgeStyle::Theme::SkillBlue().fill);
            else if (situationalHighlighted)
                dl->AddRectFilled(rMin, rMax, IM_COL32(80, 65, 30, 220));

            dl->AddRect(rMin, rMax, IM_COL32(255, 255, 255, 18), 3.0f * sc, 0, 1.0f);

            float midY = cy + (rowH - iconSz) * 0.5f;

            float iconX = cx + rowPadX;
            if (s.tex) {
                HUDDraw::DrawIconStyled(dl, ImVec2(iconX, midY), iconSz,
                    reinterpret_cast<ImTextureID>(s.tex),
                    HUDDraw::PackedToArgb(IM_COL32(255, 255, 255, 200)), IconFxDefault());
            } else {
                dl->AddRectFilled(ImVec2(iconX, midY),
                    ImVec2(iconX + iconSz, midY + iconSz), IM_COL32(50, 50, 60, 150), 2.0f);
            }

            float labelX = iconX + iconSz + rowPadX;
            float textY  = cy + (rowH - textLineH) * 0.5f;
            ImGui::SetWindowFontScale(FontBody());
            float maxLabelW = rMax.x - labelX - rowPadX * 4.0f;
            const bool anyHighlighted = skillHighlighted || situationalHighlighted;
            ImU32 highlightTextCol = skillHighlighted ? IM_COL32(255, 255, 255, 255) : IM_COL32(190, 165, 100, 255);
            ImU32 labelCol = anyHighlighted ? highlightTextCol : kTheme.textMuted;
            ImU32 valCol   = anyHighlighted ? highlightTextCol : kTheme.textMain;
            AddTextShadowed(dl, ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(labelX, textY), labelCol, s.name.c_str(), nullptr, maxLabelW);
            ImGui::SetWindowFontScale(1.0f);

            char valBuf[8];
            snprintf(valBuf, sizeof(valBuf), "%.0f", s.value);
            ImGui::SetWindowFontScale(FontBody());
            ImVec2 valSz = ImGui::CalcTextSize(valBuf);
            AddTextShadowed(dl, ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(rMax.x - rowPadX - valSz.x, textY),
                valCol, valBuf);
            ImGui::SetWindowFontScale(1.0f);
        }

        float usedH = (sepY + 8.0f * sc) + 6.0f * (rowH + rowGapY) - origin.y;
        ImGui::Dummy(ImVec2(size.x, usedH));
        ImGui::Dummy(ImVec2(size.x, 16.0f * sc));

        static ID3D11ShaderResourceView* s_TexArmor      = nullptr;
        static ID3D11ShaderResourceView* s_TexDmgRes     = nullptr;
        static ID3D11ShaderResourceView* s_TexMagic      = nullptr;
        static ID3D11ShaderResourceView* s_TexFire       = nullptr;
        static ID3D11ShaderResourceView* s_TexFrost      = nullptr;
        static ID3D11ShaderResourceView* s_TexShock      = nullptr;
        static ID3D11ShaderResourceView* s_TexPoison     = nullptr;
        static ID3D11ShaderResourceView* s_TexDisease    = nullptr;
        static ID3D11ShaderResourceView* s_TexWeapon     = nullptr;
        static ID3D11ShaderResourceView* s_TexMoveSpeed  = nullptr;
        static ID3D11ShaderResourceView* s_TexAtkSpeed   = nullptr;
        static ID3D11ShaderResourceView* s_TexStagger    = nullptr;
        static ID3D11ShaderResourceView* s_TexReach      = nullptr;
        if (!s_TexArmor)     s_TexArmor     = TextureManager::GetPartyIcon("armor");
        if (!s_TexDmgRes)    s_TexDmgRes    = TextureManager::GetPartyIcon("damage_resist");
        if (!s_TexMagic)     s_TexMagic     = TextureManager::GetPartyIcon("magic_resist");
        if (!s_TexFire)      s_TexFire      = TextureManager::GetPartyIcon("fire_resist");
        if (!s_TexFrost)     s_TexFrost     = TextureManager::GetPartyIcon("frost_resist");
        if (!s_TexShock)     s_TexShock     = TextureManager::GetPartyIcon("shock_resist");
        if (!s_TexPoison)    s_TexPoison    = TextureManager::GetPartyIcon("poison_resist");
        if (!s_TexDisease)   s_TexDisease   = TextureManager::GetPartyIcon("disease_resist");
        if (!s_TexWeapon)    s_TexWeapon    = TextureManager::GetPartyIcon("weapon");
        if (!s_TexMoveSpeed) s_TexMoveSpeed = TextureManager::GetPartyIcon("movespeed");
        if (!s_TexAtkSpeed)  s_TexAtkSpeed  = TextureManager::GetPartyIcon("attackspeed");
        if (!s_TexStagger)   s_TexStagger   = TextureManager::GetPartyIcon("bowstagger");
        if (!s_TexReach)     s_TexReach     = TextureManager::GetPartyIcon("reach");

        auto* avo_combat    = actor->AsActorValueOwner();
        float combatMass    = avo_combat->GetActorValue(RE::ActorValue::kMass);
        float combatMoveSpd = stats.speedMult;

        float combatWpnSpd = 0.0f;
        float combatReach  = 0.0f;
        {
            RE::TESForm* wpnForm = actor->GetEquippedObject(false);
            if (!wpnForm || !wpnForm->As<RE::TESObjectWEAP>())
                wpnForm = actor->GetEquippedObject(true);
            if (auto* weap = wpnForm ? wpnForm->As<RE::TESObjectWEAP>() : nullptr) {
                combatWpnSpd = weap->GetSpeed();
                combatReach  = weap->GetReach();
            }
        }

        float combatDamage = stats.weaponDamage;
        if (actor->IsPlayerRef()) {
            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* rh = player->GetEquippedObject(false);
            auto* lh = player->GetEquippedObject(true);
            RE::TESForm* weaponForm = nullptr;
            bool         isLeftHand = false;
            if (rh && rh->As<RE::TESObjectWEAP>()) {
                weaponForm = rh;
                isLeftHand = false;
            } else if (lh && lh->As<RE::TESObjectWEAP>()) {
                weaponForm = lh;
                isLeftHand = true;
            }
            if (weaponForm) {
                auto resolved = EquipmentStatResolver::ResolveHandWeaponStats(player, weaponForm, isLeftHand);
                if (resolved.totalDamage >= 0.0f)
                    combatDamage = resolved.totalDamage;
            }
        }

        ImVec2 dcOrigin = ImGui::GetCursorScreenPos();

        const float dcColGap = 8.0f * sc;
        const float dcColW   = (size.x - dcColGap) * 0.5f;

        const float dIconSz = Settings::GetSingleton()->psV2IconStat * sc;
        const float dPadX   =  5.0f * sc;
        const float dPadY   =  3.0f * sc;
        const float dGapY   =  4.0f * sc;
        const float dValW   = 44.0f * sc;

        ImGui::SetWindowFontScale(FontBody());
        float dTextH = ImGui::GetTextLineHeight();
        ImGui::SetWindowFontScale(1.0f);
        const float dRowH = std::max(dIconSz, dTextH) + dPadY * 2.0f;

        const int   kPips  = 6;
        const float pipR   = 3.5f * sc;
        const float pipGap = 2.0f * sc;
        const float pipsW  = kPips * pipR * 2.0f + (kPips - 1) * pipGap;

        ImU32 pipFill   = IM_COL32(212, 175,  55, 200);
        ImU32 pipBorder = IM_COL32(255, 255, 255, 25);
        ImU32 pipEmpty  = IM_COL32(255, 255, 255, 15);

        auto DrawStatRow = [&](float ox, float ry, float colW,
                               const char* label, float value,
                               float rangeMax, bool isPercent, bool isMult,
                               ID3D11ShaderResourceView* tex,
                               uint8_t resistBit = 0) {
            if (ImGui::IsMouseHoveringRect(ImVec2(ox, ry), ImVec2(ox + colW, ry + dRowH)))
                dl->AddRectFilled(ImVec2(ox, ry), ImVec2(ox + colW, ry + dRowH), kTheme.hoverFill);

            bool resistHighlighted = false;
            {
                if (resistBit && (s_WornGearData.resistHighlightMask & resistBit)) {
                    resistHighlighted = true;
                    dl->AddRectFilled(
                        ImVec2(ox, ry), ImVec2(ox + colW, ry + dRowH),
                        BadgeStyle::Theme::ResistPurple().fill);
                }
            }

            float midY  = ry + (dRowH - dIconSz) * 0.5f;
            float textY = ry + (dRowH - dTextH)  * 0.5f;

            float iconX = ox + dPadX;
            if (tex)
                HUDDraw::DrawIconStyled(dl, ImVec2(iconX, midY), dIconSz,
                    reinterpret_cast<ImTextureID>(tex),
                    HUDDraw::PackedToArgb(IM_COL32(255,255,255,200)), IconFxDefault());

            float labelX = iconX + dIconSz + dPadX;
            ImGui::SetWindowFontScale(FontBody());
            AddTextShadowed(dl, ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(labelX, textY), resistHighlighted ? IM_COL32(255,255,255,255) : kTheme.textMuted, label);
            ImGui::SetWindowFontScale(1.0f);

            float pct = (rangeMax > 0.0f)
                ? std::clamp(value / rangeMax, 0.0f, 1.0f)
                : 0.0f;
            int filledPips = static_cast<int>(std::round(pct * kPips));

            float valueFarRight = ox + colW - dPadX;
            float valColLeft    = valueFarRight - dValW;
            float pipsRight     = valColLeft - dPadX;
            float pipsLeft      = pipsRight - pipsW;
            float pipCY         = ry + dRowH * 0.5f;

            for (int p = 0; p < kPips; ++p) {
                float cx    = pipsLeft + pipR + p * (pipR * 2.0f + pipGap);
                bool filled = (p < filledPips);
                dl->AddCircleFilled(ImVec2(cx, pipCY), pipR, filled ? pipFill : pipEmpty, 12);
                dl->AddCircle(ImVec2(cx, pipCY), pipR, pipBorder, 12, 1.0f);
            }

            char valBuf[12];
            if (isPercent)       snprintf(valBuf, sizeof(valBuf), "%.0f%%", value);
            else if (isMult)     snprintf(valBuf, sizeof(valBuf), "%.2f",   value);
            else                 snprintf(valBuf, sizeof(valBuf), "%.0f",   value);

            bool isNegative = value < -0.5f;
            ImGui::SetWindowFontScale(FontBody());
            ImVec2 valSz = ImGui::CalcTextSize(valBuf);
            ImU32  valCol = isNegative ? IM_COL32(220, 80, 80, 255) : (resistHighlighted ? IM_COL32(255,255,255,255) : kTheme.textMain);
            AddTextShadowed(dl, ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(valueFarRight - valSz.x, textY), valCol, valBuf);
            ImGui::SetWindowFontScale(1.0f);
        };

        auto DrawSubTitle = [&](float ox, float y, const char* title) -> float {
            if (headingFont) ImGui::PushFont(headingFont);
            ImGui::SetWindowFontScale(FontHeading());
            ImVec2 titleSz = ImGui::CalcTextSize(title);
            AddTextShadowed(dl, ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(ox, y), kTheme.accent, title);
            ImGui::SetWindowFontScale(1.0f);
            if (headingFont) ImGui::PopFont();

            float sepY = y + titleSz.y + 4.0f * sc;
            dl->AddRectFilledMultiColor(
                ImVec2(ox,                    sepY),
                ImVec2(ox + dcColW * 0.5f,    sepY + 1.0f),
                IM_COL32(0,0,0,0), IM_COL32(212,175,55,100), IM_COL32(212,175,55,100), IM_COL32(0,0,0,0));
            dl->AddRectFilledMultiColor(
                ImVec2(ox + dcColW * 0.5f,    sepY),
                ImVec2(ox + dcColW,            sepY + 1.0f),
                IM_COL32(212,175,55,100), IM_COL32(0,0,0,0), IM_COL32(0,0,0,0), IM_COL32(212,175,55,100));
            return sepY + 8.0f * sc;
        };

        std::string defTitle = LocalizationManager::Get("Stats.Defense");
        for (char& c : defTitle) c = static_cast<char>(toupper(c));
        float defStartY = DrawSubTitle(dcOrigin.x, dcOrigin.y, defTitle.c_str());

        struct StatRowDef {
            std::string               label;
            float                     value;
            float                     rangeMax;
            bool                      isPercent;
            bool                      isMult;
            ID3D11ShaderResourceView* tex;
            uint8_t                   resistBit = 0;
        };

        const StatRowDef defRows[] = {
            { LocalizationManager::Get("Stats.Armor"),         stats.armorRating,    500.0f, false, false, s_TexArmor,   0 },
            { LocalizationManager::Get("Stats.DamageResist"),   stats.damageResistPct, 80.0f, true,  false, s_TexDmgRes,  0 },
            { LocalizationManager::Get("Stats.Magic"),         stats.resistMagic,     85.0f, true,  false, s_TexMagic,   WornGearStatResolver::kResistMagic  },
            { LocalizationManager::Get("Resistances.Fire"),    stats.resistFire,     100.0f, true,  false, s_TexFire,    WornGearStatResolver::kResistFire   },
            { LocalizationManager::Get("Resistances.Frost"),   stats.resistFrost,    100.0f, true,  false, s_TexFrost,   WornGearStatResolver::kResistFrost  },
            { LocalizationManager::Get("Resistances.Shock"),   stats.resistShock,    100.0f, true,  false, s_TexShock,   WornGearStatResolver::kResistShock  },
            { LocalizationManager::Get("Resistances.Poison"),  stats.resistPoison,   100.0f, true,  false, s_TexPoison,  WornGearStatResolver::kResistPoison },
            { LocalizationManager::Get("Resistances.Disease"), stats.resistDisease,  100.0f, true,  false, s_TexDisease, 0 },
        };

        float dY = defStartY;
        for (const auto& row : defRows) {
            DrawStatRow(dcOrigin.x, dY, dcColW,
                row.label.c_str(), row.value, row.rangeMax, row.isPercent, row.isMult, row.tex, row.resistBit);
            dY += dRowH + dGapY;
        }

        float combatOriginX = dcOrigin.x + dcColW + dcColGap;
        std::string combatTitle = LocalizationManager::Get("Stats.Combat");
        for (char& c : combatTitle) c = static_cast<char>(toupper(c));
        float combatStartY  = DrawSubTitle(combatOriginX, dcOrigin.y, combatTitle.c_str());

        const StatRowDef combatRows[] = {
            { LocalizationManager::Get("Stats.Damage"), combatDamage,  150.0f, false, false, s_TexWeapon    },
            { LocalizationManager::Get("Stats.Move"),   combatMoveSpd, 150.0f, true,  false, s_TexMoveSpeed },
            { LocalizationManager::Get("Stats.Speed"),  combatWpnSpd,   2.0f,  false, true,  s_TexAtkSpeed  },
            { LocalizationManager::Get("Stats.Stagger"),               combatMass,     10.0f,  false, true,  s_TexStagger   },
            { LocalizationManager::Get("Stats.Reach"),  combatReach,    2.0f,   false, true,  s_TexReach     },
        };

        float cY = combatStartY;
        for (const auto& row : combatRows) {
            DrawStatRow(combatOriginX, cY, dcColW,
                row.label.c_str(), row.value, row.rangeMax, row.isPercent, row.isMult, row.tex);
            cY += dRowH + dGapY;
        }

        float dcUsedH = std::max(dY, cY) - dcOrigin.y;
        ImGui::Dummy(ImVec2(size.x, dcUsedH));

        bool anyModalOpen = IsEffectsModalOpen() || IsInventoryModalOpen() || IsStatusModalOpen();

        ImGui::SetWindowFontScale(FontSmall());
        float tabH = ImGui::GetTextLineHeight() + 2.0f * 5.0f * sc;
        ImGui::SetWindowFontScale(1.0f);

        static float s_fxHover = 0.0f, s_invHover = 0.0f, s_statusHover = 0.0f;

        auto DrawTab = [&](const char* id, ImVec2 size, const char* label,
                            ID3D11ShaderResourceView* icon, float& hoverFrac) -> bool {
            ImGui::InvisibleButton(id, size);
            bool hovered = ImGui::IsItemHovered() && !anyModalOpen;
            bool active  = ImGui::IsItemActive()  && !anyModalOpen;
            bool clicked = ImGui::IsItemClicked() && !anyModalOpen;

            float target = hovered ? 1.0f : 0.0f;
            hoverFrac += (target - hoverFrac) * std::min(1.0f, ImGui::GetIO().DeltaTime * 8.0f);

            ImVec2 bMin = ImGui::GetItemRectMin();
            ImVec2 bMax = ImGui::GetItemRectMax();

            float disableMul = anyModalOpen ? 0.5f : 1.0f;
            auto ScaleAlpha = [](std::uint32_t argb, float mul) -> std::uint32_t {
                std::uint32_t a = (argb >> 24) & 0xFF;
                a = static_cast<std::uint32_t>(a * mul);
                return (a << 24) | (argb & 0x00FFFFFFu);
            };

            std::uint32_t bgTop = active ? 0xD0453D30u : 0xF03A342Cu;
            {
                std::uint32_t a = (bgTop >> 24) & 0xFF;
                std::uint32_t r = std::min<std::uint32_t>(255, ((bgTop >> 16) & 0xFF) + static_cast<std::uint32_t>(hoverFrac * 10));
                std::uint32_t g = std::min<std::uint32_t>(255, ((bgTop >> 8)  & 0xFF) + static_cast<std::uint32_t>(hoverFrac * 10));
                std::uint32_t b = std::min<std::uint32_t>(255, (bgTop        & 0xFF) + static_cast<std::uint32_t>(hoverFrac * 8));
                bgTop = (a << 24) | (r << 16) | (g << 8) | b;
            }
            HUDDraw::ChipStyle cs;
            cs.rounding    = 4.0f * sc;
            cs.topColor    = ScaleAlpha(bgTop, disableMul);
            cs.bottomColor = ScaleAlpha(0xFF12100F, disableMul);
            cs.innerBorder = ScaleAlpha(0x50968A74, disableMul);
            cs.outerBorder = ScaleAlpha(0xC8060608, disableMul);
            HUDDraw::DrawChip(dl, bMin, bMax, cs);

            float underlineAlpha = active ? 1.0f : hoverFrac;
            if (underlineAlpha > 0.01f) {
                std::uint32_t line = ScaleAlpha(V2Theme::AccentA(static_cast<int>(220 * underlineAlpha)), disableMul);
                dl->AddLine(ImVec2(bMin.x + 2.0f * sc, bMax.y - 1.0f),
                            ImVec2(bMax.x - 2.0f * sc, bMax.y - 1.0f), line, 1.0f);
            }

            float padX    = 8.0f * sc;
            ImGui::SetWindowFontScale(FontSmall());
            float iconSz  = ImGui::GetTextLineHeight();
            ImGui::SetWindowFontScale(1.0f);
            float cy      = (bMin.y + bMax.y) * 0.5f;
            float cursorX = bMin.x + padX;

            if (icon) {
                HUDDraw::IconFx ifx = IconFxDefault();
                ifx.glow  = false;
                ifx.gloss = false;
                HUDDraw::DrawIconStyled(dl, ImVec2(cursorX, cy - iconSz * 0.5f), iconSz,
                    reinterpret_cast<ImTextureID>(icon),
                    ScaleAlpha(0xFFFFFFFFu, disableMul), ifx);
                cursorX += iconSz + 5.0f * sc;
            }

            ImGui::SetWindowFontScale(FontSmall());
            ImVec2 textSz = ImGui::CalcTextSize(label);
            const float labelFontSize = ImGui::GetFontSize();
            ImGui::SetWindowFontScale(1.0f);

            HUDDraw::TextFx tfx;
            tfx.shadow  = true;
            tfx.outline = false;
            tfx.duotone = true;
            tfx.fontSize = labelFontSize;
            HUDDraw::DrawTextStyled(dl, ImVec2(cursorX, cy - textSz.y * 0.5f),
                ScaleAlpha(0xFFE8DCC0u, disableMul), label, tfx);

            return clicked;
        };

        std::string fxBtnLabel     = LocalizationManager::Get("PartySheetV2.EffectsAndSpells");
        std::string invBtnLabel    = LocalizationManager::Get("PartySheetV2.Inventory");
        std::string statusBtnLabel = LocalizationManager::Get("header_status");

        auto TabWidth = [&](const char* label) -> float {
            const float padX = 8.0f * sc;
            ImGui::SetWindowFontScale(FontSmall());
            const float iconSz = ImGui::GetTextLineHeight();
            const float textW  = ImGui::CalcTextSize(label).x;
            ImGui::SetWindowFontScale(1.0f);
            return padX + iconSz + 5.0f * sc + textW + padX;
        };

        static ID3D11ShaderResourceView* s_TexFxTab     = nullptr;
        static ID3D11ShaderResourceView* s_TexInvTab    = nullptr;
        static ID3D11ShaderResourceView* s_TexStatusTab = nullptr;
        if (!s_TexFxTab)     s_TexFxTab     = TextureManager::GetPartyIcon("magic");
        if (!s_TexInvTab)    s_TexInvTab    = TextureManager::GetPartyIcon("weight");
        if (!s_TexStatusTab) s_TexStatusTab = TextureManager::GetPartyIcon("blessing");

        ImGui::BeginDisabled(anyModalOpen);
        if (DrawTab("##psv2_fx_btn", ImVec2(TabWidth(fxBtnLabel.c_str()), tabH), fxBtnLabel.c_str(), s_TexFxTab, s_fxHover))
            OpenEffectsModal();
        ImGui::EndDisabled();

        ImGui::SameLine(0.0f, 6.0f * sc);

        ImGui::BeginDisabled(anyModalOpen);
        if (DrawTab("##psv2_inv_btn", ImVec2(TabWidth(invBtnLabel.c_str()), tabH), invBtnLabel.c_str(), s_TexInvTab, s_invHover))
            OpenInventoryModal();
        ImGui::EndDisabled();

        if (actor && actor->IsPlayerRef()) {
            ImGui::SameLine(0.0f, 6.0f * sc);
            ImGui::BeginDisabled(anyModalOpen);
            if (DrawTab("##psv2_status_btn", ImVec2(TabWidth(statusBtnLabel.c_str()), tabH), statusBtnLabel.c_str(), s_TexStatusTab, s_statusHover))
                OpenStatusModal();
            ImGui::EndDisabled();
        }
    }

    static void RenderCenter(ImVec2 size) {
        auto subjects = BuildSubjectList();
        if (subjects.empty()) return;
        if (s_SubjectIndex >= static_cast<int>(subjects.size())) s_SubjectIndex = 0;
        RE::Actor* actor = subjects[s_SubjectIndex];
        if (!actor) return;

        static SkillTex s_CenterSkillTex;
        LoadSkillTextures(s_CenterSkillTex);

        const float sc      = s_EffectiveScale;
        ImDrawList* dl      = ImGui::GetWindowDrawList();
        ImVec2      origin  = ImGui::GetCursorScreenPos();

        ActorStats stats = GetActorStats(actor);

        static ID3D11ShaderResourceView* s_TexWeaponC = nullptr;
        static ID3D11ShaderResourceView* s_TexArmorC  = nullptr;
        if (!s_TexWeaponC) s_TexWeaponC = TextureManager::GetPartyIcon("weapon");
        if (!s_TexArmorC)  s_TexArmorC  = TextureManager::GetPartyIcon("armor");

        struct StatCard { const char* labelKey; float value; ID3D11ShaderResourceView* tex; };

        RE::TESForm* rhForm = actor->GetEquippedObject(false);
        RE::TESForm* lhForm = actor->GetEquippedObject(true);
        bool hasWeapon = (rhForm && rhForm->As<RE::TESObjectWEAP>()) ||
                         (lhForm && lhForm->As<RE::TESObjectWEAP>());
        bool hasSpell  = !hasWeapon &&
                         ((rhForm && rhForm->As<RE::SpellItem>()) ||
                          (lhForm && lhForm->As<RE::SpellItem>()));

        float displayDamage = stats.weaponDamage;
        float weaponSpeed   = 0.0f;
        float spellDamage   = 0.0f;

        if (hasWeapon) {
            RE::TESForm* wpnForm = (rhForm && rhForm->As<RE::TESObjectWEAP>()) ? rhForm : lhForm;
            if (auto* weap = wpnForm->As<RE::TESObjectWEAP>())
                weaponSpeed = weap->GetSpeed();
            if (actor->IsPlayerRef()) {
                auto* player = RE::PlayerCharacter::GetSingleton();
                bool isLeft = !(rhForm && rhForm->As<RE::TESObjectWEAP>());
                auto resolved = EquipmentStatResolver::ResolveHandWeaponStats(player, wpnForm, isLeft);
                if (resolved.totalDamage >= 0.0f)
                    displayDamage = resolved.totalDamage;
            }
        } else if (hasSpell) {
            RE::SpellItem* spell = rhForm ? rhForm->As<RE::SpellItem>() : lhForm->As<RE::SpellItem>();
            if (spell) {
                auto* avEffect = spell->GetAVEffect();
                if (avEffect && avEffect->IsHostile()) {
                    for (auto* effect : spell->effects) {
                        if (effect && effect->baseEffect == avEffect) {
                            float base = effect->effectItem.magnitude;
                            float mod = 0.0f;
                            if (avEffect->data.associatedSkill == RE::ActorValue::kDestruction)
                                mod = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kDestructionModifier);
                            spellDamage = base * (1.0f + mod / 100.0f);
                            break;
                        }
                    }
                }
            }
        }

        float displayDPS = (weaponSpeed > 0.0f) ? displayDamage * weaponSpeed : displayDamage;
        const char* dmgLabelKey = hasSpell ? "Stats.Damage" : "Stats.DPS";
        float       dmgValue    = hasSpell ? spellDamage : displayDPS;

        static ID3D11ShaderResourceView* s_TexSpellC = nullptr;
        if (!s_TexSpellC) s_TexSpellC = TextureManager::GetPartyIcon("destruction");

        const StatCard kCards[2] = {
            { dmgLabelKey,   dmgValue,           hasSpell ? s_TexSpellC : s_TexWeaponC },
            { "Stats.Armor", stats.armorRating,   s_TexArmorC  },
        };

        const float cardIconSz  = Settings::GetSingleton()->psV2IconCard * sc;
        const float cardPad     = 14.0f * sc;
        const float cardGap     = 16.0f * sc;
        const float cardHorizPad = 16.0f * sc;

        ImGui::SetWindowFontScale(FontDisplay());
        const float valH = ImGui::GetTextLineHeight();
        ImGui::SetWindowFontScale(FontSmall());
        const float lblH = ImGui::GetTextLineHeight();
        ImGui::SetWindowFontScale(1.0f);

        const float cardContentH = cardIconSz + 6.0f * sc + valH + 6.0f * sc + lblH;
        const float cardH        = cardContentH + cardPad * 2.0f;
        const float cardW        = (size.x - cardHorizPad * 2.0f - cardGap) * 0.5f;

        constexpr int   kMaxTraits  = 4;
        const float     iconSz      = Settings::GetSingleton()->psV2IconTrait * sc;

        ImGui::SetWindowFontScale(FontSmall());
        const float traitLabelH = ImGui::GetTextLineHeight();
        ImGui::SetWindowFontScale(1.0f);
        const float pipSzPre    = 7.0f * sc;
        const float traitBlockH = iconSz + 4.0f * sc + traitLabelH + 4.0f * sc + pipSzPre;

        const float cardY = origin.y + size.y - traitBlockH - 64.0f * sc - cardH;

        for (int i = 0; i < 2; ++i) {
            const auto& card = kCards[i];
            float cardX = origin.x + cardHorizPad + i * (cardW + cardGap);

            const float inset = cardW * 0.08f;
            ImVec2 topL = ImVec2(cardX + inset,        cardY);
            ImVec2 topR = ImVec2(cardX + cardW - inset, cardY);
            ImVec2 botR = ImVec2(cardX + cardW,         cardY + cardH);
            ImVec2 botL = ImVec2(cardX,                 cardY + cardH);

            dl->AddQuadFilled(topL, topR, botR, botL, IM_COL32(0, 0, 0, 128));

            // Thin gold border on top + sides (1px), thicker on bottom (2px)
            dl->AddLine(topL, topR, V2Theme::AccentA(38), 1.0f);
            dl->AddLine(topL, botL, V2Theme::AccentA(38), 1.0f);
            dl->AddLine(topR, botR, V2Theme::AccentA(38), 1.0f);
            dl->AddLine(botL, botR, V2Theme::AccentA(51), 2.0f);

            float cx   = cardX + cardW * 0.5f;
            float curY = cardY + cardPad;

            if (card.tex) {
                float ix = cx - cardIconSz * 0.5f;
                HUDDraw::DrawIconStyled(dl, ImVec2(ix, curY), cardIconSz,
                    reinterpret_cast<ImTextureID>(card.tex),
                    0xFFFFFFFF,
                    IconFxDefault());
            }
            curY += cardIconSz + 6.0f * sc;

            char valBuf[16];
            snprintf(valBuf, sizeof(valBuf), "%.0f", card.value);
            ImGui::SetWindowFontScale(FontDisplay());
            ImVec2 valSz = ImGui::CalcTextSize(valBuf);
            dl->AddText(ImVec2(cx - valSz.x * 0.5f, curY),
                        V2Theme::AccentA(255), valBuf);
            curY += valH + 6.0f * sc;

            std::string lbl = LocalizationManager::Get(card.labelKey);
            ImGui::SetWindowFontScale(FontBody());
            ImVec2 lblSz = ImGui::CalcTextSize(lbl.c_str());
            dl->AddText(ImVec2(cx - lblSz.x * 0.5f, curY),
                        IM_COL32(255, 255, 255, 180), lbl.c_str());
        }
        ImGui::SetWindowFontScale(1.0f);

        auto TierPips = [](const std::string& tierKey) -> int {
            if (tierKey == "Trait.Tier.Master")      return 5;
            if (tierKey == "Trait.Tier.Expert")      return 4;
            if (tierKey == "Trait.Tier.Adept")       return 3;
            if (tierKey == "Trait.Tier.Apprentice")  return 2;
            return 1;
        };
        static constexpr int kPipCount = 5;

        auto TexForKey = [&](const std::string& nameKey) -> ID3D11ShaderResourceView* {
            if (nameKey == "Trait.OneHanded")   return s_CenterSkillTex.oneHanded;
            if (nameKey == "Trait.TwoHanded")   return s_CenterSkillTex.twoHanded;
            if (nameKey == "Trait.Block")       return s_CenterSkillTex.block;
            if (nameKey == "Trait.LightArmor")  return s_CenterSkillTex.lightArmor;
            if (nameKey == "Trait.HeavyArmor")  return s_CenterSkillTex.heavyArmor;
            if (nameKey == "Trait.Smithing")    return s_CenterSkillTex.smithing;
            if (nameKey == "Trait.Destruction") return s_CenterSkillTex.destruction;
            if (nameKey == "Trait.Restoration") return s_CenterSkillTex.restoration;
            if (nameKey == "Trait.Conjuration") return s_CenterSkillTex.conjuration;
            if (nameKey == "Trait.Alteration")  return s_CenterSkillTex.alteration;
            if (nameKey == "Trait.Illusion")    return s_CenterSkillTex.illusion;
            if (nameKey == "Trait.Enchanting")  return s_CenterSkillTex.enchanting;
            if (nameKey == "Trait.Sneak")       return s_CenterSkillTex.sneak;
            if (nameKey == "Trait.Lockpicking") return s_CenterSkillTex.lockpicking;
            if (nameKey == "Trait.Pickpocket")  return s_CenterSkillTex.pickpocket;
            if (nameKey == "Trait.Archery")     return s_CenterSkillTex.archery;
            if (nameKey == "Trait.Alchemy")     return s_CenterSkillTex.alchemy;
            if (nameKey == "Trait.Speech")      return s_CenterSkillTex.speech;
            return nullptr;
        };

        auto traits = TraitResolver::Resolve(stats.skills, kMaxTraits);

        const float colW = size.x / static_cast<float>(kMaxTraits);

        ImGui::SetWindowFontScale(FontBody());
        const float labelH = ImGui::GetTextLineHeight();
        ImGui::SetWindowFontScale(1.0f);

        const float pipSz    = 7.0f * sc;
        const float pipRad   = 2.0f * sc;
        const float pipGapSq = 3.0f * sc;
        const float pipsRowW = static_cast<float>(kPipCount) * pipSz
                               + static_cast<float>(kPipCount - 1) * pipGapSq;

        const float blockH   = iconSz + 4.0f * sc + labelH + 4.0f * sc + pipSz;
        const float resStripH = 22.0f * sc + 18.0f * sc;
        const float blockY   = origin.y + size.y - resStripH - blockH;

        ImGui::SetWindowFontScale(FontBody());

        for (int i = 0; i < static_cast<int>(traits.size()); ++i) {
            const auto& entry = traits[i];
            const float colX  = origin.x + i * colW;
            const float cx    = colX + colW * 0.5f;

            {
                float hovInset = 4.0f * sc;
                ImVec2 htl(colX + hovInset, blockY - hovInset);
                ImVec2 hbr(colX + colW - hovInset, blockY + blockH + hovInset);
                if (ImGui::IsMouseHoveringRect(htl, hbr))
                    dl->AddRectFilled(htl, hbr, kTheme.hoverFill, 4.0f * sc);
            }

            auto* tex = TexForKey(entry.nameKey);
            if (tex) {
                float iconX = cx - iconSz * 0.5f;
                HUDDraw::DrawIconStyled(dl, ImVec2(iconX, blockY), iconSz,
                    reinterpret_cast<ImTextureID>(tex),
                    HUDDraw::PackedToArgb(IM_COL32(255, 255, 255, 178)),
                    IconFxDefault());
            }

            std::string name  = LocalizationManager::Get(entry.labelKey);
            ImVec2 nameSz = ImGui::CalcTextSize(name.c_str());
            float  nameX  = cx - nameSz.x * 0.5f;
            float  nameY  = blockY + iconSz + 4.0f * sc;
            dl->AddText(ImVec2(nameX, nameY), kTheme.textMuted, name.c_str());

            int   filled    = TierPips(entry.tierKey);
            bool  isMaster  = (filled == kPipCount);
            float pipsY     = nameY + labelH + 4.0f * sc;
            float pipsStartX = cx - pipsRowW * 0.5f;

            for (int p = 0; p < kPipCount; ++p) {
                float px = pipsStartX + p * (pipSz + pipGapSq);
                ImVec2 ptl = ImVec2(px, pipsY);
                ImVec2 pbr = ImVec2(px + pipSz, pipsY + pipSz);
                if (p < filled) {
                    ImU32 fillCol = isMaster ? kTheme.accent : kTheme.textMain;
                    dl->AddRectFilled(ptl, pbr, fillCol, pipRad);
                    if (isMaster)
                        dl->AddRect(
                            ImVec2(ptl.x - 1, ptl.y - 1),
                            ImVec2(pbr.x + 1, pbr.y + 1),
                            V2Theme::AccentA(80), pipRad + 1.0f);
                } else {
                    dl->AddRect(ptl, pbr, kTheme.borderSubtle, pipRad);
                }
            }
        }

        ImGui::SetWindowFontScale(1.0f);

        const float resH        = 22.0f * sc;
        const float resPadX     = 28.0f * sc;
        const float resGap      = 16.0f * sc;
        const float resY        = blockY + blockH + 14.0f * sc;

        dl->AddLine(
            ImVec2(origin.x + resPadX, resY),
            ImVec2(origin.x + size.x - resPadX, resY),
            IM_COL32(255, 255, 255, 15));

        ImGui::SetWindowFontScale(FontBody());
        const float resTextY = resY + (resH - ImGui::GetTextLineHeight()) * 0.5f;

        std::string actorName = actor->GetName() ? actor->GetName() : "";
        ImVec2 nameSzR = ImGui::CalcTextSize(actorName.c_str());
        dl->AddText(ImVec2(origin.x + resPadX, resTextY), kTheme.textMain, actorName.c_str());

        float divX = origin.x + resPadX + nameSzR.x + resGap * 0.5f;
        dl->AddLine(
            ImVec2(divX, resY + 3.0f * sc),
            ImVec2(divX, resY + resH - 3.0f * sc),
            IM_COL32(255, 255, 255, 25));

        float curX = divX + resGap * 0.5f;

        auto DrawResItem = [&](const char* labelKey, const std::string& valueStr) {
            std::string lbl = LocalizationManager::Get(labelKey);
            dl->AddText(ImVec2(curX, resTextY), kTheme.textMuted, lbl.c_str());
            curX += ImGui::CalcTextSize(lbl.c_str()).x + 5.0f * sc;
            dl->AddText(ImVec2(curX, resTextY), kTheme.accent, valueStr.c_str());
            curX += ImGui::CalcTextSize(valueStr.c_str()).x + resGap;
        };

        static RE::TESBoundObject* s_GoldObj = nullptr;
        if (!s_GoldObj) {
            if (auto* f = RE::TESForm::LookupByID(0x0000000F))
                s_GoldObj = f->As<RE::TESBoundObject>();
        }
        int displayGold = 0;
        if (s_GoldObj) {
            auto inv = actor->GetInventory([](const RE::TESBoundObject& obj) {
                return obj.GetFormID() == 0x0000000F;
            });
            auto it = inv.find(s_GoldObj);
            if (it != inv.end())
                displayGold = it->second.first;
        }
        float displayCarryW    = actor->GetWeightInContainer();
        float displayCarryWMax = stats.carryWeightMax;

        char goldBuf[16];
        snprintf(goldBuf, sizeof(goldBuf), "%d", displayGold);

        char weightBuf[32];
        snprintf(weightBuf, sizeof(weightBuf), "%.0f/%.0f",
                 displayCarryW, displayCarryWMax);

        DrawResItem("gold",   goldBuf);
        DrawResItem("weight", weightBuf);

        ImGui::SetWindowFontScale(1.0f);

        ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + size.y));
        ImGui::Dummy(ImVec2(size.x, 1.0f));
    }

    static void RenderRightPanel(ImVec2 size) {
        auto subjects = BuildSubjectList();
        if (subjects.empty()) return;
        if (s_SubjectIndex >= static_cast<int>(subjects.size())) s_SubjectIndex = 0;
        RE::Actor* actor = subjects[s_SubjectIndex];
        if (!actor) return;

        const float sc  = s_EffectiveScale;
        ImDrawList* dl  = ImGui::GetWindowDrawList();
        ImVec2 origin   = ImGui::GetCursorScreenPos();
        float  curY     = origin.y;

        auto HoverZoomUV = [&](const char* key, const ImVec2& tl, const ImVec2& br) -> float {
            static std::unordered_map<std::string, float> s_zoom;
            const float kMaxInset = 0.06f;
            const float kSpeed    = 12.0f;
            bool  hovered = ImGui::IsMouseHoveringRect(tl, br);
            float target  = hovered ? 1.0f : 0.0f;
            float& cur    = s_zoom[key];
            float  dt     = ImGui::GetIO().DeltaTime;
            cur += (target - cur) * std::min(1.0f, dt * kSpeed);
            return cur * kMaxInset;
        };

        auto ResolveEquipIcon = [&](RE::TESForm* form, const char* hint) -> ID3D11ShaderResourceView* {
            if (!form) return nullptr;
            if (auto* spell = form->As<RE::SpellItem>()) {
                using ST = RE::MagicSystem::SpellType;
                auto t = spell->GetSpellType();
                if (t != ST::kPower && t != ST::kLesserPower)
                    if (auto* i = SpellIconManager::GetSpellIcon(spell)) return i;
            }
            return TextureManager::GetItemIcon(form, hint);
        };

        static ID3D11ShaderResourceView* s_TexWeapon = nullptr;
        static ID3D11ShaderResourceView* s_TexArmor  = nullptr;
        if (!s_TexWeapon) s_TexWeapon = TextureManager::GetPartyIcon("weapon");
        if (!s_TexArmor)  s_TexArmor  = TextureManager::GetPartyIcon("armor");

        auto DrawBadge = [&](ImVec2 tl, const char* text, ImU32 textCol) {
            ImGui::SetWindowFontScale(FontSmall());
            ImVec2 sz  = ImGui::CalcTextSize(text);
            float  bp  = 3.0f * sc;
            ImVec2 br  = ImVec2(tl.x + sz.x + bp * 2.0f, tl.y + sz.y + bp * 2.0f);
            dl->AddRectFilled(ImVec2(tl.x - bp, tl.y - bp), br,
                IM_COL32(10, 10, 15, 200), 2.0f * sc);
            dl->AddText(tl, textCol, text);
            ImGui::SetWindowFontScale(1.0f);
        };

        const float colGapEquip = 6.0f * sc;
        const float colWEquip   = (size.x - colGapEquip) * 0.5f;
        const float iconW       = Settings::GetSingleton()->psV2IconEquip * sc;
        const float iconH       = iconW * (300.0f / 216.0f);
        const float rowGap      =  6.0f * sc;

        auto DrawEquipRow = [&](float rowY, RE::TESForm* form, const char* hint,
                                const char* subLabel,
                                float dmgOrArmor,
                                ID3D11ShaderResourceView* statTex,
                                ID3D11ShaderResourceView* enchantTex,
                                float enchantDmg,
                                const std::string& enchantType = {},
                                float colX = -1.0f,
                                float colW = -1.0f,
                                bool alignLeft = true) -> float
        {
            if (colX < 0.0f) colX = origin.x;
            if (colW < 0.0f) colW = size.x;

            float iconOffX = (colW - iconW) * 0.5f;
            ImVec2 iTL = ImVec2(colX + iconOffX, rowY);
            ImVec2 iBR = ImVec2(colX + iconOffX + iconW, rowY + iconH);

            static ID3D11ShaderResourceView* s_EquipSlotFrame = nullptr;
            if (!s_EquipSlotFrame) s_EquipSlotFrame = TextureManager::GetFrameTexture("frame-4");
            if (s_EquipSlotFrame) {
                ImVec2 fTL = ImVec2(colX,         rowY);
                ImVec2 fBR = ImVec2(colX + colW,  rowY + iconH);
                dl->AddImage(reinterpret_cast<ImTextureID>(s_EquipSlotFrame), fTL, fBR,
                    ImVec2(0,0), ImVec2(1,1), IM_COL32(110, 110, 115, 255));
            }

            auto* tex = form ? ResolveEquipIcon(form, hint) : nullptr;
            if (tex) {
                char zkey[64];
                snprintf(zkey, sizeof(zkey), "eq_%.0f_%s", colX, subLabel ? subLabel : "");
                float z = HoverZoomUV(zkey, iTL, iBR);
                HUDDraw::DrawIconStyled(dl, iTL, ImVec2(iBR.x - iTL.x, iBR.y - iTL.y),
                    reinterpret_cast<ImTextureID>(tex), 0xFFFFFFFF, IconFxItemArt(),
                    ImVec2(z, z), ImVec2(1.0f - z, 1.0f - z));
            }

            {
                ImGui::SetWindowFontScale(FontSmall());
                const float bp    = 3.0f * sc;
                const float gap   = 2.0f * sc;
                float badgeY = iTL.y + 4.0f * sc;

                auto DrawStatBadge = [&](ID3D11ShaderResourceView* iconTex, const char* text, ImU32, BadgeStyle::BadgeColors bgColors) {
                    ImVec2 tsz    = ImGui::CalcTextSize(text);
                    float  iconSz = std::max(tsz.y, Settings::GetSingleton()->psV2IconBadge * sc);
                    float  badgeW = iconSz + gap + tsz.x + bp * 2.0f;
                    float  badgeH = iconSz + bp * 2.0f;
                    ImVec2 bgTL   = ImVec2(colX + 2.0f * sc, badgeY);
                    ImVec2 bgBR   = ImVec2(bgTL.x + badgeW, bgTL.y + badgeH);
                    dl->AddRectFilled(bgTL, bgBR, bgColors.fill);
                    float iconOffY = (badgeH - iconSz) * 0.5f;
                    if (iconTex)
                        HUDDraw::DrawIconStyled(dl, ImVec2(bgTL.x + bp, bgTL.y + iconOffY), iconSz,
                            reinterpret_cast<ImTextureID>(iconTex),
                            HUDDraw::PackedToArgb(IM_COL32(255, 255, 255, 220)), IconFxDefault());
                    float textOffY = (badgeH - tsz.y) * 0.5f;
                    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                        ImVec2(bgTL.x + bp + iconSz + gap, bgTL.y + textOffY),
                        IM_COL32(255, 255, 255, 220), text);
                    badgeY += badgeH;
                };

                if (dmgOrArmor >= 0.0f) {
                    char buf[16];
                    snprintf(buf, sizeof(buf), "%.0f", dmgOrArmor);
                    DrawStatBadge(statTex, buf, BadgeStyle::TextColor::Neutral(), BadgeStyle::Theme::NeutralGrey());
                }

                if (enchantTex && enchantDmg > 0.0f) {
                    char ebuf[16];
                    auto enchCat = BadgeStyle::ClassifyEnchant(enchantType);
                    switch (enchCat) {
                        case BadgeStyle::EnchantCategory::Resist:
                        case BadgeStyle::EnchantCategory::FortifySkill:
                        case BadgeStyle::EnchantCategory::Regen:
                            snprintf(ebuf, sizeof(ebuf), "%.0f%%", enchantDmg); break;
                        case BadgeStyle::EnchantCategory::DamageFire:
                        case BadgeStyle::EnchantCategory::DamageFrost:
                        case BadgeStyle::EnchantCategory::DamageShock:
                        case BadgeStyle::EnchantCategory::DamagePoison:
                        case BadgeStyle::EnchantCategory::FortifyStats:
                        case BadgeStyle::EnchantCategory::SkillPoints:
                            snprintf(ebuf, sizeof(ebuf), "+%.0f", enchantDmg); break;
                        default:
                            snprintf(ebuf, sizeof(ebuf), "%.0f", enchantDmg); break;
                    }
                    auto enchColors = enchantType.empty()
                        ? BadgeStyle::Theme::NeutralGrey()
                        : BadgeStyle::GetEnchantColors(enchantType);
                    DrawStatBadge(enchantTex, ebuf, BadgeStyle::TextColor::Neutral(), enchColors);
                }

                ImGui::SetWindowFontScale(1.0f);
            }

            float textY = rowY + iconH + 4.0f * sc;

            ImGui::SetWindowFontScale(FontBody());
            static std::string s_emptyLabel;
            if (s_emptyLabel.empty()) s_emptyLabel = LocalizationManager::Get("CharacterSheet.empty");
            const char* nameStr = form ? form->GetName() : s_emptyLabel.c_str();
            ImU32 nameCol = form ? kTheme.textMain : kTheme.textMuted;
            ImVec2 nameSz = ImGui::CalcTextSize(nameStr, nullptr, false, colW);
            float nameX = alignLeft
                ? colX
                : colX + colW - std::min(nameSz.x, colW);
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(nameX, textY), nameCol, nameStr, nullptr, colW);
            textY += nameSz.y + 2.0f * sc;
            ImGui::SetWindowFontScale(1.0f);

            ImGui::SetWindowFontScale(FontSmall());
            char slotLabelBuf[64];
            snprintf(slotLabelBuf, sizeof(slotLabelBuf), "%s", subLabel);
            for (char* p = slotLabelBuf; *p; ++p) *p = static_cast<char>(toupper(*p));
            ImVec2 slotSz = ImGui::CalcTextSize(slotLabelBuf);
            float slotX = alignLeft
                ? colX
                : colX + colW - slotSz.x;
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(slotX, textY), kTheme.textMuted, slotLabelBuf);
            textY += ImGui::GetTextLineHeight() + 2.0f * sc;
            ImGui::SetWindowFontScale(1.0f);

            return textY + rowGap;
        };

        auto DrawSectionTitle = [&](float y, const char* title) -> float {
            ImFont* hf = ImGuiRenderer::GetHeadingFont();
            if (hf) ImGui::PushFont(hf);
            ImGui::SetWindowFontScale(FontHeading());
            ImVec2 tSz = ImGui::CalcTextSize(title);
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(origin.x, y), kTheme.accent, title);
            ImGui::SetWindowFontScale(1.0f);
            if (hf) ImGui::PopFont();
            float sepY = y + tSz.y + 3.0f * sc;
            dl->AddRectFilledMultiColor(
                ImVec2(origin.x,                   sepY),
                ImVec2(origin.x + size.x * 0.5f,   sepY + 1.0f),
                IM_COL32(0,0,0,0), IM_COL32(212,175,55,100), IM_COL32(212,175,55,100), IM_COL32(0,0,0,0));
            dl->AddRectFilledMultiColor(
                ImVec2(origin.x + size.x * 0.5f,   sepY),
                ImVec2(origin.x + size.x,           sepY + 1.0f),
                IM_COL32(212,175,55,100), IM_COL32(0,0,0,0), IM_COL32(0,0,0,0), IM_COL32(212,175,55,100));
            return sepY + 1.0f + 6.0f * sc;
        };

        {
            std::string eqTitle = LocalizationManager::Get("Equipment.SectionEquipped");
            for (char& c : eqTitle) c = static_cast<char>(toupper(c));
            curY = DrawSectionTitle(curY, eqTitle.c_str());
        }

        RE::TESForm* rh = actor->GetEquippedObject(false);
        RE::TESForm* lh = actor->GetEquippedObject(true);
        if (lh && lh == rh) {
            if (auto* rw = rh->As<RE::TESObjectWEAP>()) {
                auto wt = rw->GetWeaponType();
                bool is2H = (wt == RE::WEAPON_TYPE::kTwoHandSword ||
                             wt == RE::WEAPON_TYPE::kTwoHandAxe   ||
                             wt == RE::WEAPON_TYPE::kBow          ||
                             wt == RE::WEAPON_TYPE::kCrossbow);
                if (is2H) {
                    bool leftOccupied = false;
                    if (actor->IsPlayerRef()) {
                        if (auto* p = RE::PlayerCharacter::GetSingleton())
                            leftOccupied = (p->GetEquippedEntryData(true) != nullptr);
                    }
                    if (!leftOccupied) lh = nullptr;
                }
            }
        }

        {
            const float colW   = colWEquip;
            const float rhX    = origin.x;
            const float lhX    = origin.x + colW + colGapEquip;

            auto EnchantTex = [&](const std::string& t) -> ID3D11ShaderResourceView* {
                if (t == "fire_resist"   || t == "resist_fire")   return TextureManager::GetPartyIcon("fire_resist");
                if (t == "frost_resist"  || t == "resist_frost")  return TextureManager::GetPartyIcon("frost_resist");
                if (t == "shock_resist"  || t == "resist_shock")  return TextureManager::GetPartyIcon("shock_resist");
                if (t == "poison_resist" || t == "resist_poison") return TextureManager::GetPartyIcon("poison_resist");
                if (t == "magic_resist"  || t == "resist_magic")  return TextureManager::GetPartyIcon("magic_resist");
                return nullptr;
            };

            float rhDmg      = -1.0f;
            float rhEnchDmg  = 0.0f;
            std::string rhEnchType;
            const char* rhHint = "weapon";
            if (rh && rh->As<RE::TESObjectWEAP>()) {
                if (actor->IsPlayerRef()) {
                    if (auto* p = RE::PlayerCharacter::GetSingleton()) {
                        auto res = EquipmentStatResolver::ResolveHandWeaponStats(p, rh, false);
                        rhDmg     = res.totalDamage >= 0.0f ? res.totalDamage : rhDmg;
                        rhEnchType = res.enchantType;
                        rhEnchDmg  = res.enchantDamage;
                    }
                } else {
                    ActorStats as = GetActorStats(actor);
                    rhDmg = as.weaponDamage >= 0.0f ? as.weaponDamage : rhDmg;
                }
            }

            bool rhIsBow = false;
            if (rh) {
                if (auto* rw = rh->As<RE::TESObjectWEAP>()) {
                    auto wt = rw->GetWeaponType();
                    rhIsBow = (wt == RE::WEAPON_TYPE::kBow || wt == RE::WEAPON_TYPE::kCrossbow);
                }
            }

            RE::TESForm* lhForm    = nullptr;
            float        lhDmg     = -1.0f;
            float        lhEnchDmg = 0.0f;
            const char*  lhHint    = "weapon";
            std::string  lhLabelStr = LocalizationManager::Get("CharacterSheet.slot_left_hand");
            const char*  lhLabel    = lhLabelStr.c_str();
            std::string  lhEnchType;
            static char  s_ammoLabel[32];

            if (rhIsBow) {
                RE::TESAmmo* ammo = nullptr;
                int ammoCount = 0;
                if (actor->IsPlayerRef()) {
                    if (auto* p = RE::PlayerCharacter::GetSingleton()) {
                        ammo = p->GetCurrentAmmo();
                        if (ammo) ammoCount = p->GetItemCount(ammo);
                    }
                } else {
                    if (auto* ic = actor->GetInventoryChanges()) {
                        if (ic->entryList) {
                            for (auto* entry : *ic->entryList) {
                                if (!entry || !entry->object) continue;
                                auto* a = entry->object->As<RE::TESAmmo>();
                                if (!a || !entry->IsWorn()) continue;
                                ammo      = a;
                                ammoCount = entry->countDelta;
                                break;
                            }
                        }
                    }
                }
                {
                    std::string arrowStr = LocalizationManager::Get("Equipment.Arrows");
                    if (ammo && ammoCount > 0)
                        snprintf(s_ammoLabel, sizeof(s_ammoLabel), "%s  x%d", arrowStr.c_str(), ammoCount);
                    else
                        snprintf(s_ammoLabel, sizeof(s_ammoLabel), "%s", arrowStr.c_str());
                }
                bool isCrossbow = rh->As<RE::TESObjectWEAP>()->GetWeaponType() == RE::WEAPON_TYPE::kCrossbow;
                lhForm  = ammo;
                lhHint  = isCrossbow ? "type_bolt" : "type_quiver";
                lhLabel = s_ammoLabel;
                if (ammo) lhDmg = ammo->GetRuntimeData().data.damage;
            } else {
                lhForm = lh;
                if (lh) {
                    if (auto* lw = lh->As<RE::TESObjectWEAP>()) {
                        if (actor->IsPlayerRef()) {
                            if (auto* p = RE::PlayerCharacter::GetSingleton()) {
                                auto res = EquipmentStatResolver::ResolveHandWeaponStats(p, lh, true);
                                lhDmg     = res.totalDamage >= 0.0f ? res.totalDamage : lhDmg;
                                lhEnchType = res.enchantType;
                                lhEnchDmg  = res.enchantDamage;
                            }
                        } else {
                            auto* avo = actor->AsActorValueOwner();
                            if (avo) {
                                float baseDmg = static_cast<float>(lw->GetAttackDamage());
                                float skillLevel = 0.0f;
                                float fortifyMod = 0.0f;
                                auto wt = lw->GetWeaponType();
                                if (wt == RE::WEAPON_TYPE::kOneHandSword || wt == RE::WEAPON_TYPE::kOneHandDagger ||
                                    wt == RE::WEAPON_TYPE::kOneHandAxe   || wt == RE::WEAPON_TYPE::kOneHandMace) {
                                    skillLevel = avo->GetActorValue(RE::ActorValue::kOneHanded);
                                    fortifyMod = avo->GetActorValue(RE::ActorValue::kOneHandedModifier);
                                } else if (wt == RE::WEAPON_TYPE::kTwoHandSword || wt == RE::WEAPON_TYPE::kTwoHandAxe) {
                                    skillLevel = avo->GetActorValue(RE::ActorValue::kTwoHanded);
                                    fortifyMod = avo->GetActorValue(RE::ActorValue::kTwoHandedModifier);
                                }
                                lhDmg = baseDmg * (1.0f + skillLevel / 200.0f) * (1.0f + fortifyMod / 100.0f);
                            }
                        }
                    } else if (auto* shield = lh->As<RE::TESObjectARMO>()) {
                        lhHint = "shield";
                        if (actor->IsPlayerRef()) {
                            if (auto* p = RE::PlayerCharacter::GetSingleton()) {
                                float displayedAR = 0.0f;
                                auto inv = p->GetInventory([](const RE::TESBoundObject& o) { return o.IsArmor(); });
                                for (auto& [item, data] : inv) {
                                    auto& [count, entry] = data;
                                    if (count > 0 && entry && entry->IsWorn() && item == shield) {
                                        displayedAR = p->GetArmorValue(entry.get());
                                        break;
                                    }
                                }
                                lhDmg = displayedAR > 0.0f ? displayedAR : static_cast<float>(shield->GetArmorRating());
                            }
                        } else {
                            lhDmg = static_cast<float>(shield->GetArmorRating());
                        }
                        if (auto* enchantable = shield->As<RE::TESEnchantableForm>()) {
                            if (auto* enchant = enchantable->formEnchanting) {
                                for (auto* effect : enchant->effects) {
                                    if (!effect || !effect->baseEffect) continue;
                                    const float mag = effect->effectItem.magnitude;
                                    if (mag <= 0.0f) continue;
                                    const auto pav = effect->baseEffect->data.primaryAV;
                                    if      (pav == RE::ActorValue::kResistFire)   { lhEnchType = "resist_fire";   lhEnchDmg = mag; break; }
                                    else if (pav == RE::ActorValue::kResistFrost)  { lhEnchType = "resist_frost";  lhEnchDmg = mag; break; }
                                    else if (pav == RE::ActorValue::kResistShock)  { lhEnchType = "resist_shock";  lhEnchDmg = mag; break; }
                                    else if (pav == RE::ActorValue::kPoisonResist) { lhEnchType = "resist_poison"; lhEnchDmg = mag; break; }
                                    else if (pav == RE::ActorValue::kResistMagic)  { lhEnchType = "resist_magic";  lhEnchDmg = mag; break; }
                                }
                            }
                        }
                    }
                }
            }
            ID3D11ShaderResourceView* lhStatTex = s_TexWeapon;
            if (lhForm && lhForm->As<RE::TESObjectARMO>()) lhStatTex = s_TexArmor;

            std::string rhLabelStr = LocalizationManager::Get("CharacterSheet.slot_right_hand");
            float rhBot = DrawEquipRow(curY, rh,     rhHint, rhLabelStr.c_str(), rhDmg,  s_TexWeapon, EnchantTex(rhEnchType), rhEnchDmg, rhEnchType, rhX, colW, true);
            float lhBot = DrawEquipRow(curY, lhForm, lhHint, lhLabel,            lhDmg,  lhStatTex,   EnchantTex(lhEnchType), lhEnchDmg, lhEnchType, lhX, colW, false);
            curY = std::max(rhBot, lhBot);
        }

        curY += 4.0f * sc;
        {
            std::string arTitle = LocalizationManager::Get("Equipment.SectionArmor");
            for (char& c : arTitle) c = static_cast<char>(toupper(c));
            curY = DrawSectionTitle(curY, arTitle.c_str());
        }

        struct ArmorSlot {
            RE::BIPED_MODEL::BipedObjectSlot slot;
            std::string                      label;
            const char*                      hint;
        };
        const ArmorSlot kArmorSlots[] = {
            { RE::BIPED_MODEL::BipedObjectSlot::kHead,    LocalizationManager::Get("CharacterSheet.slot_head"),   "type_helmet"    },
            { RE::BIPED_MODEL::BipedObjectSlot::kBody,    LocalizationManager::Get("CharacterSheet.slot_chest"),  "type_cuirass"   },
            { RE::BIPED_MODEL::BipedObjectSlot::kFeet,    LocalizationManager::Get("CharacterSheet.slot_feet"),   "type_boots"     },
            { RE::BIPED_MODEL::BipedObjectSlot::kHands,   LocalizationManager::Get("CharacterSheet.slot_hands"),  "type_gauntlets" },
            { RE::BIPED_MODEL::BipedObjectSlot::kAmulet,  LocalizationManager::Get("CharacterSheet.slot_amulet"), "type_amulet"    },
            { RE::BIPED_MODEL::BipedObjectSlot::kRing,    LocalizationManager::Get("CharacterSheet.slot_ring"),   "type_ring"      },
        };

        const float armorColGap  = 10.0f * sc;
        const float armorColW    = (size.x - armorColGap * 2.0f) / 3.0f;
        const float armorTextGap = 8.0f * sc;
        const float armorRowGap  = 30.0f * sc;

        const float armorIconSz  = Settings::GetSingleton()->psV2IconArmor * sc;

        auto DrawArmorCell = [&](float cellX, float cellY, RE::TESObjectARMO* arm,
                                 const char* hint, const char* label,
                                 float armorVal,
                                 const WornGearStatResolver::WornGearSlotData& slotData) -> float
        {
            const float frameSz  = armorColW;
            const float frameOffX = 0.0f;
            ImVec2 fTL = ImVec2(cellX + frameOffX, cellY);
            ImVec2 fBR = ImVec2(cellX + frameOffX + frameSz, cellY + frameSz);

            float iconOffX = (frameSz - armorIconSz) * 0.5f;
            float iconOffY = (frameSz - armorIconSz) * 0.5f;
            ImVec2 iTL = ImVec2(cellX + iconOffX, cellY + iconOffY);
            ImVec2 iBR = ImVec2(iTL.x + armorIconSz, iTL.y + armorIconSz);

            bool hasEnchant = slotData.enchantBadgeCount > 0;

            static ID3D11ShaderResourceView* s_ArmorSlotFrame = nullptr;
            if (!s_ArmorSlotFrame) s_ArmorSlotFrame = TextureManager::GetFrameTexture("frame-4");
            if (s_ArmorSlotFrame)
                dl->AddImage(reinterpret_cast<ImTextureID>(s_ArmorSlotFrame), fTL, fBR,
                    ImVec2(0,0), ImVec2(1,1), IM_COL32(110, 110, 115, 255));

            auto* tex = arm ? ResolveEquipIcon(arm, hint) : nullptr;
            if (tex) {
                char zkey[64];
                snprintf(zkey, sizeof(zkey), "ar_%s", label ? label : "");
                float z = HoverZoomUV(zkey, iTL, iBR);
                HUDDraw::DrawIconStyled(dl, iTL, ImVec2(iBR.x - iTL.x, iBR.y - iTL.y),
                    reinterpret_cast<ImTextureID>(tex),
                    HUDDraw::PackedToArgb(arm ? IM_COL32(255,255,255,255) : IM_COL32(255,255,255,60)),
                    IconFxItemArt(), ImVec2(z, z), ImVec2(1.0f - z, 1.0f - z));
            }

            if (hasEnchant)
                dl->AddLine(fTL, ImVec2(fBR.x, fTL.y), BadgeStyle::Theme::ResistPurple().border, 1.5f);

            {
                ImGui::SetWindowFontScale(FontSmall());
                const float bp   = 3.0f * sc;
                const float gap  = 2.0f * sc;
                float badgeY = fTL.y + 2.0f * sc;

                auto DrawStatBadgeCell = [&](ID3D11ShaderResourceView* iconTex, const char* text, ImU32, BadgeStyle::BadgeColors bgColors) {
                    ImVec2 tsz    = ImGui::CalcTextSize(text);
                    float  iconSz = Settings::GetSingleton()->psV2IconBadge * sc;
                    float  badgeW = (iconTex ? iconSz + gap : 0.0f) + tsz.x + bp * 2.0f;
                    float  badgeH = iconSz + bp * 2.0f;
                    ImVec2 bgTL   = ImVec2(cellX + 2.0f * sc, badgeY);
                    ImVec2 bgBR   = ImVec2(bgTL.x + badgeW, bgTL.y + badgeH);
                    dl->AddRectFilled(bgTL, bgBR, bgColors.fill);
                    float textX    = bgTL.x + bp;
                    float iconOffY = (badgeH - iconSz) * 0.5f;
                    float textOffY = (badgeH - tsz.y)  * 0.5f;
                    if (iconTex) {
                        HUDDraw::DrawIconStyled(dl, ImVec2(bgTL.x + bp, bgTL.y + iconOffY), iconSz,
                            reinterpret_cast<ImTextureID>(iconTex),
                            HUDDraw::PackedToArgb(IM_COL32(255, 255, 255, 220)), IconFxDefault());
                        textX += iconSz + gap;
                    }
                    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                        ImVec2(textX, bgTL.y + textOffY), IM_COL32(255, 255, 255, 220), text);
                    badgeY += badgeH;
                };

                if (armorVal >= 0.0f) {
                    char buf[16];
                    snprintf(buf, sizeof(buf), "%.0f", armorVal);
                    DrawStatBadgeCell(s_TexArmor, buf, BadgeStyle::TextColor::Neutral(), BadgeStyle::Theme::NeutralGrey());
                }

                for (std::size_t badgeIndex = 0; badgeIndex < slotData.enchantBadgeCount; ++badgeIndex) {
                    const auto& badge = slotData.enchantBadges[badgeIndex];
                    auto* badgeTex = badge.iconName.empty() ? nullptr : TextureManager::GetPartyIcon(badge.iconName);
                    DrawStatBadgeCell(
                        badgeTex,
                        badge.text.c_str(),
                        BadgeStyle::GetEnchantTextColor(badge.enchantType),
                        BadgeStyle::GetEnchantColors(badge.enchantType));
                }

                ImGui::SetWindowFontScale(1.0f);
            }

            float textY = cellY + frameSz + armorTextGap;
            ImGui::SetWindowFontScale(FontBody());
            const char* nameStr = arm ? arm->GetName() : label;
            ImU32 nameCol = arm ? kTheme.textMain : kTheme.textMuted;
            ImVec2 nameSz = ImGui::CalcTextSize(nameStr, nullptr, false, armorColW);
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(cellX + (armorColW - std::min(nameSz.x, armorColW)) * 0.5f, textY),
                nameCol, nameStr, nullptr, armorColW);
            textY += ImGui::GetTextLineHeight();
            ImGui::SetWindowFontScale(1.0f);

            return textY + armorTextGap;
        };

        float rowStartY = curY;
        float rowMaxY   = curY;
        for (std::size_t i = 0; i < std::size(kArmorSlots); ++i) {
            const auto& s = kArmorSlots[i];
            const auto& slotData = s_WornGearData.slots[i];
            int col = i % 3;
            if (col == 0 && i > 0) {
                curY = rowMaxY + armorRowGap;
                rowStartY = curY;
                rowMaxY   = curY;
            }
            float cellX = origin.x + col * (armorColW + armorColGap);

            float cellBot = DrawArmorCell(
                cellX,
                rowStartY,
                slotData.armor,
                s.hint,
                s.label.c_str(),
                slotData.armorValue,
                slotData);
            rowMaxY = std::max(rowMaxY, cellBot);
            if (rowMaxY > origin.y + size.y) break;
        }
        curY = rowMaxY;

        ImGui::SetCursorScreenPos(ImVec2(origin.x, curY));
        ImGui::Dummy(ImVec2(size.x, 1.0f));
    }

    static void RenderFooter(ImVec2 size) {
        const float sc     = s_EffectiveScale;
        ImDrawList* dl     = ImGui::GetWindowDrawList();
        ImVec2      origin = ImGui::GetCursorScreenPos();

        const float padX = 40.0f * sc;
        dl->AddRectFilledMultiColor(
            ImVec2(origin.x + padX,            origin.y),
            ImVec2(origin.x + size.x - padX,   origin.y + 1.0f),
            V2Theme::AccentA(0),  V2Theme::AccentA(76),
            V2Theme::AccentA(76), V2Theme::AccentA(0));

        auto subjects = BuildSubjectList();

        int   totalFollowers = static_cast<int>(subjects.size()) - 1;
        if (totalFollowers < 0) totalFollowers = 0;

        int   totalGold     = 0;
        float totalCarryW   = 0.0f;
        float totalCarryMax = 0.0f;

        static RE::TESBoundObject* s_FooterGoldObj = nullptr;
        if (!s_FooterGoldObj)
            if (auto* f = RE::TESForm::LookupByID(0x0000000F))
                s_FooterGoldObj = f->As<RE::TESBoundObject>();

        for (auto* actor : subjects) {
            if (!actor) continue;
            if (actor->IsPlayerRef()) {
                if (auto* p = actor->As<RE::PlayerCharacter>()) {
                    if (s_FooterGoldObj) totalGold += p->GetItemCount(s_FooterGoldObj);
                    totalCarryW   += p->GetWeightInContainer();
                    totalCarryMax += p->AsActorValueOwner()->GetActorValue(RE::ActorValue::kCarryWeight);
                }
            } else {
                if (s_FooterGoldObj) {
                    auto inv = actor->GetInventory([](const RE::TESBoundObject& o) {
                        return o.GetFormID() == 0x0000000F;
                    });
                    auto it = inv.find(s_FooterGoldObj);
                    if (it != inv.end()) totalGold += it->second.first;
                }
                totalCarryW   += actor->GetWeightInContainer();
                totalCarryMax += actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kCarryWeight);
            }
        }

        static ID3D11ShaderResourceView* s_TexFollow = nullptr;
        static ID3D11ShaderResourceView* s_TexGold   = nullptr;
        static ID3D11ShaderResourceView* s_TexWeight = nullptr;
        if (!s_TexFollow) s_TexFollow = TextureManager::GetPartyIcon("StateFollow");
        if (!s_TexGold)   s_TexGold   = TextureManager::GetPartyIcon("gold");
        if (!s_TexWeight) s_TexWeight = TextureManager::GetPartyIcon("weight");

        const float iconSz  = Settings::GetSingleton()->psV2IconFooter * sc;
        const float itemGap =  6.0f * sc;
        const float divW    = 14.0f * sc;

        ImGui::SetWindowFontScale(FontBody());
        const float textH   = ImGui::GetTextLineHeight();
        const float centerY = origin.y + size.y * 0.5f;
        const float textY   = centerY - textH * 0.5f;
        const float iconY   = centerY - iconSz * 0.5f;

        struct StatItem {
            ID3D11ShaderResourceView* tex;
            std::string               val;
            std::string               label;
        };

        char followBuf[8];  snprintf(followBuf,  sizeof(followBuf),  "%d",         totalFollowers);
        char goldBuf[16];   snprintf(goldBuf,    sizeof(goldBuf),    "%d",         totalGold);
        char weightBuf[32]; snprintf(weightBuf,  sizeof(weightBuf),  "%.0f / %.0f", totalCarryW, totalCarryMax);

        const StatItem kItems[] = {
            { s_TexFollow, followBuf, LocalizationManager::Get("PartyMembers") },
            { s_TexGold,   goldBuf,   LocalizationManager::Get("gold")         },
            { s_TexWeight, weightBuf, LocalizationManager::Get("weight")       },
        };

        float curX = origin.x + padX;

        for (int i = 0; i < static_cast<int>(std::size(kItems)); ++i) {
            const auto& item = kItems[i];

            if (i > 0) {
                float dvX = curX + divW * 0.5f;
                dl->AddLine(
                    ImVec2(dvX, centerY - 6.0f * sc),
                    ImVec2(dvX, centerY + 6.0f * sc),
                    IM_COL32(255, 255, 255, 25));
                curX += divW;
            }

            if (item.tex) {
                HUDDraw::DrawIconStyled(dl, ImVec2(curX, iconY), iconSz,
                    reinterpret_cast<ImTextureID>(item.tex),
                    HUDDraw::PackedToArgb(IM_COL32(255, 255, 255, 178)), IconFxDefault());
                curX += iconSz + itemGap;
            }

            dl->AddText(ImVec2(curX, textY), kTheme.accent, item.val.c_str());
            curX += ImGui::CalcTextSize(item.val.c_str()).x + itemGap;

            dl->AddText(ImVec2(curX, textY), kTheme.textMuted, item.label.c_str());
            curX += ImGui::CalcTextSize(item.label.c_str()).x + itemGap;
        }

        static std::string s_controlsHint;
        if (s_controlsHint.empty()) s_controlsHint = LocalizationManager::Get("PartySheetV2.ControlsHint");
        const char* hint = s_controlsHint.c_str();
        ImVec2 hintSz = ImGui::CalcTextSize(hint);
        dl->AddText(
            ImVec2(origin.x + size.x - padX - hintSz.x, textY),
            kTheme.textMuted, hint);

        ImGui::SetWindowFontScale(1.0f);
        ImGui::Dummy(ImVec2(size.x, size.y));
    }

    void Render() {
        if (!s_Visible) return;

        if (MenuUtils::ShouldBlockCharacterSheet()) {
            s_Visible = false;
            CloseEffectsModal();
            CloseInventoryModal();
            CloseStatusModal();
            if (!ImGuiRenderer::IsVisible())
                EnablePlayerInput();
            logger::debug("PartySheetV2: Auto-closed (incompatible menu)");
            return;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();

        auto& io = ImGui::GetIO();

        POINT mousePos;
        if (GetCursorPos(&mousePos)) {
            if (const auto renderer = RE::BSGraphics::Renderer::GetSingleton()) {
                HWND hwnd = (HWND)renderer->GetRuntimeData().renderWindows[0].hWnd;
                if (ScreenToClient(hwnd, &mousePos)) {
                    io.AddMousePosEvent(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y));
                }
            }
        }
        io.AddMouseButtonEvent(0, (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
        io.AddMouseButtonEvent(1, (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);
        io.MouseDrawCursor = false;

        ImGuiRenderer::FlushScrollDelta();

        ImGui::NewFrame();

        static bool s_LeftWasDown  = false;
        static bool s_RightWasDown = false;
        const bool leftDown  = (GetAsyncKeyState(VK_LEFT)  & 0x8000) != 0
                            || (GetAsyncKeyState(VK_UP)    & 0x8000) != 0;
        const bool rightDown = (GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0
                            || (GetAsyncKeyState(VK_DOWN)  & 0x8000) != 0;
        if (leftDown  && !s_LeftWasDown)  PrevSubject();
        if (rightDown && !s_RightWasDown) NextSubject();
        s_LeftWasDown  = leftDown;
        s_RightWasDown = rightDown;

        const ImVec2 displaySize = io.DisplaySize;

        // ── Centered scaled canvas ────────────────────────────────────────────
        const float requestedScale = Settings::GetSingleton()->psV2Scale;
        const float fitScale = (std::min)(
            displaySize.x / kBaseCanvasW,
            displaySize.y / kBaseCanvasH);
        s_EffectiveScale      = (std::min)(requestedScale, fitScale);
        const float sc0       = s_EffectiveScale;
        const float canvasW   = kBaseCanvasW * sc0;
        const float canvasH   = kBaseCanvasH * sc0;
        const float canvasX   = (displaySize.x - canvasW) * 0.5f;
        const float canvasY   = (displaySize.y - canvasH) * 0.5f;
        const ImVec2 canvasMin(canvasX, canvasY);
        const ImVec2 canvasMax(canvasX + canvasW, canvasY + canvasH);

        {
            ImDrawList* fullDl = ImGui::GetBackgroundDrawList();
            fullDl->AddRectFilled(ImVec2(0, 0), displaySize, IM_COL32(0, 0, 0, 140));
        }

        ImGui::SetNextWindowPos(canvasMin);
        ImGui::SetNextWindowSize(ImVec2(canvasW, canvasH));
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, kTheme.bgBase);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kPad * sc0, kPad * sc0));

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar        |
            ImGuiWindowFlags_NoResize          |
            ImGuiWindowFlags_NoMove            |
            ImGuiWindowFlags_NoScrollbar       |
            ImGuiWindowFlags_NoCollapse        |
            ImGuiWindowFlags_NoFocusOnAppearing;

        bool open = ImGui::Begin("##PartySheetV2", nullptr, flags);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        if (open) {
            {
                auto subjects = BuildSubjectList();
                RE::Actor* subject = (!subjects.empty() && s_SubjectIndex < (int)subjects.size())
                    ? subjects[s_SubjectIndex] : nullptr;
                ID3D11ShaderResourceView* bgTex = nullptr;
                std::string actorStem = GetActorBgStem(subject);
                if (!actorStem.empty())
                    bgTex = TryLoadBgTexture(actorStem);
                if (!bgTex)
                    bgTex = GetClassBgTexture(GetClassBgName(subject));
                {
                    ImDrawList* bgDl = ImGui::GetBackgroundDrawList();
                    bgDl->AddRectFilled(canvasMin, canvasMax, kTheme.bgBase);
                    if (bgTex) {
                        ImVec2 mp = ImGui::GetIO().MousePos;
                        ImVec2 c  = ImVec2((canvasMin.x + canvasMax.x) * 0.5f,
                                           (canvasMin.y + canvasMax.y) * 0.5f);
                        float halfW = (canvasMax.x - canvasMin.x) * 0.5f;
                        float halfH = (canvasMax.y - canvasMin.y) * 0.5f;

                        bool inside = mp.x >= canvasMin.x && mp.x <= canvasMax.x &&
                                      mp.y >= canvasMin.y && mp.y <= canvasMax.y;
                        float nx = inside ? std::clamp((mp.x - c.x) / halfW, -1.0f, 1.0f) : 0.0f;
                        float ny = inside ? std::clamp((mp.y - c.y) / halfH, -1.0f, 1.0f) : 0.0f;

                        static float s_tx = 0.0f, s_ty = 0.0f;
                        float dt = ImGui::GetIO().DeltaTime;
                        float k  = std::min(1.0f, dt * 4.0f);
                        s_tx += (nx - s_tx) * k;
                        s_ty += (ny - s_ty) * k;

                        const float kShift = halfW * 0.012f;
                        const float over   = kShift + 2.0f;

                        float dx = s_tx * kShift;
                        float dy = s_ty * kShift;
                        ImVec2 tl(canvasMin.x - over + dx, canvasMin.y - over + dy);
                        ImVec2 tr(canvasMax.x + over + dx, canvasMin.y - over + dy);
                        ImVec2 br(canvasMax.x + over + dx, canvasMax.y + over + dy);
                        ImVec2 bl(canvasMin.x - over + dx, canvasMax.y + over + dy);

                        bgDl->PushClipRect(canvasMin, canvasMax, true);
                        bgDl->AddImageQuad(
                            reinterpret_cast<ImTextureID>(bgTex),
                            tl, tr, br, bl,
                            ImVec2(0, 0), ImVec2(1, 0), ImVec2(1, 1), ImVec2(0, 1),
                            IM_COL32(255, 255, 255, 160));
                        bgDl->PopClipRect();
                    }
                }
            }

            const float sc       = sc0;
            const float pad      = kPad      * sc;
            const float gap      = kGap      * sc;
            const float sidebarW = kSidebarW * sc;
            const float leftColW = kLeftColW * sc;
            const float centerW  = kCenterW  * sc;
            const float rightW   = kRightW   * sc;
            const float headerH  = kHeaderH  * sc;
            const float footerH  = kFooterH  * sc;

            const float usableH  = canvasH - pad * 2.0f;
            const float mainH    = usableH - headerH - footerH - gap * 2.0f;

            // ── Row 1: Header (spans left col + center + right) ───────────────
            const float fullColH = usableH;

            ImGui::SetCursorPos(ImVec2(pad, pad));
            ImGui::BeginChild("##psv2_sidebar", ImVec2(sidebarW, fullColH), false,
                ImGuiWindowFlags_NoScrollbar);
            RenderSidebar(ImVec2(sidebarW, fullColH));
            ImGui::EndChild();

            const float headerX = pad + sidebarW + gap;
            const float headerW = leftColW + centerW + rightW + gap * 2.0f;
            ImGui::SetCursorPos(ImVec2(headerX, pad));
            ImGui::BeginChild("##psv2_header", ImVec2(headerW, headerH), false,
                ImGuiWindowFlags_NoScrollbar);
            RenderHeader(ImVec2(headerW, headerH));
            ImGui::EndChild();

            const float mainY = pad + headerH + gap;
            {
                auto subjects = BuildSubjectList();
                if (!subjects.empty()) {
                    if (s_SubjectIndex >= static_cast<int>(subjects.size())) {
                        s_SubjectIndex = 0;
                    }
                    s_WornGearData = WornGearStatResolver::Resolve(subjects[s_SubjectIndex]);
                } else {
                    s_WornGearData = {};
                }
            }

            ImGui::SetCursorPos(ImVec2(headerX, mainY));
            ImGui::BeginChild("##psv2_left", ImVec2(leftColW, mainH), false,
                ImGuiWindowFlags_NoScrollbar);
            RenderLeftColumn(ImVec2(leftColW, mainH));
            ImGui::EndChild();

            ImGui::SetCursorPos(ImVec2(headerX + leftColW + gap, mainY));
            ImGui::BeginChild("##psv2_center", ImVec2(centerW, mainH), false,
                ImGuiWindowFlags_NoScrollbar);
            RenderCenter(ImVec2(centerW, mainH));
            ImGui::EndChild();

            ImGui::SetCursorPos(ImVec2(headerX + leftColW + centerW + gap * 2.0f, mainY));
            ImGui::BeginChild("##psv2_right", ImVec2(rightW, mainH), false,
                ImGuiWindowFlags_NoScrollbar);
            RenderRightPanel(ImVec2(rightW, mainH));
            ImGui::EndChild();

            const float footerY = pad + headerH + mainH + gap * 2.0f;
            ImGui::SetCursorPos(ImVec2(headerX, footerY));
            ImGui::BeginChild("##psv2_footer", ImVec2(headerW, footerH), false,
                ImGuiWindowFlags_NoScrollbar);
            RenderFooter(ImVec2(headerW, footerH));
            ImGui::EndChild();
        }

        ImGui::End();

        {
            static ID3D11ShaderResourceView* s_CornerTL = nullptr;
            static ID3D11ShaderResourceView* s_CornerTR = nullptr;
            static ID3D11ShaderResourceView* s_CornerBL = nullptr;
            static ID3D11ShaderResourceView* s_CornerBR = nullptr;
            if (!s_CornerTL) s_CornerTL = TextureManager::GetFrameTexture("side-tl");
            if (!s_CornerTR) s_CornerTR = TextureManager::GetFrameTexture("side-tr");
            if (!s_CornerBL) s_CornerBL = TextureManager::GetFrameTexture("side-bl");
            if (!s_CornerBR) s_CornerBR = TextureManager::GetFrameTexture("side-br");

            if (s_CornerTL || s_CornerTR || s_CornerBL || s_CornerBR) {
                const float csz = 96.0f * sc0;
                const float L   = canvasMin.x;
                const float T   = canvasMin.y;
                const float R   = canvasMax.x;
                const float B   = canvasMax.y;
                ImDrawList* fdl = ImGui::GetForegroundDrawList();
                const ImU32 tint = IM_COL32(255, 255, 255, 179);

                if (s_CornerTL) fdl->AddImage(reinterpret_cast<ImTextureID>(s_CornerTL),
                    ImVec2(L,       T      ), ImVec2(L + csz, T + csz), ImVec2(0,0), ImVec2(1,1), tint);
                if (s_CornerTR) fdl->AddImage(reinterpret_cast<ImTextureID>(s_CornerTR),
                    ImVec2(R - csz, T      ), ImVec2(R,       T + csz), ImVec2(0,0), ImVec2(1,1), tint);
                if (s_CornerBL) fdl->AddImage(reinterpret_cast<ImTextureID>(s_CornerBL),
                    ImVec2(L,       B - csz), ImVec2(L + csz, B      ), ImVec2(0,0), ImVec2(1,1), tint);
                if (s_CornerBR) fdl->AddImage(reinterpret_cast<ImTextureID>(s_CornerBR),
                    ImVec2(R - csz, B - csz), ImVec2(R,       B      ), ImVec2(0,0), ImVec2(1,1), tint);
            }
        }

        {
            auto modalSubjects = BuildSubjectList();
            int  modalIdx      = (s_SubjectIndex < static_cast<int>(modalSubjects.size()))
                                 ? s_SubjectIndex : 0;
            RE::Actor* modalActor = modalSubjects.empty() ? nullptr : modalSubjects[modalIdx];
            if (IsEffectsModalOpen() && !modalActor)
                logger::warn("PartySheetV2: modal open but subject list empty (subjectCount={}, idx={})",
                    modalSubjects.size(), modalIdx);
            { PerfStats::PerfZone _z("V2:EffectsModal");   RenderEffectsModal(modalActor); }
            { PerfStats::PerfZone _z("V2:InventoryModal"); RenderInventoryModal(modalActor); }
            { PerfStats::PerfZone _z("V2:StatusModal");    RenderStatusModal(modalActor); }
        }

        ImGuiRenderer::DrawSkyrimCursor();
        ImGui::Render();
        ImGuiRenderer::RenderToBoundTarget(ImGui::GetDrawData());
    }

}
