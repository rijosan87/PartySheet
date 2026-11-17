#include "../../includes/ImGui/PartySheetV2.h"
#include "../../includes/ImGui/PartySheetV2Modal.h"
#include "../../includes/ImGui/PartySheetV2Tooltip.h"
#include "../../includes/ImGui/PartySheetV2InventoryModal.h"
#include "../../includes/ImGui/PartySheetV2StatusModal.h"
#include "../../includes/ImGui/PartySheetV2Camera.h"
#include "../../includes/ImGui/Renderer.h"
#include "../../includes/ImGui/CharacterSheet.h"
#include "../../includes/ImGui/FollowerHUD.h"
#include "../../includes/Settings.h"
#include "../../includes/MenuUtils.h"
#include "../../includes/TextureManager.h"
#include "../../includes/SpellIconManager.h"
#include "../../includes/ActorStats.h"
#include "../../includes/Utils/EquipmentStatResolver.h"
#include "../../includes/Utils/SpellDamageResolver.h"
#include "../../includes/Utils/WornGearStatResolver.h"
#include "../../includes/BadgeStyle.h"
#include "../../includes/LocalizationManager.h"
#include "../../includes/Utils/TraitResolver.h"
#include "../../includes/PerfStats.h"
#include "../../includes/Utils/HUDDraw.h"
#include "../../includes/Utils/SkillRowTint.h"
#include "../../includes/Utils/EffectAttribution.h"
#include "../../includes/Utils/GearSwitcher.h"
#include "../../includes/UIAnimation.h"

#include <imgui.h>
#include <cstring>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <Windows.h>
#include <string>
#include <algorithm>
#include <array>
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
    static constexpr float kHeaderH    = 146.0f;
    static constexpr float kFooterH    =  60.0f;

    static constexpr float kBaseCanvasW = kPad * 2.0f + kSidebarW + kLeftColW + kCenterW + kRightW + kGap * 3.0f;
    static constexpr float kBaseMainH   = 714.0f;
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

    static bool s_PausedByUs = false;
    static void ApplyPause(bool want) {
        auto* main = RE::Main::GetSingleton();
        if (!main) return;
        if (want && !s_PausedByUs && !main->freezeTime) {
            main->freezeTime = true;
            s_PausedByUs = true;
        } else if (!want && s_PausedByUs) {
            main->freezeTime = false;
            s_PausedByUs = false;
        }
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
            MenuUtils::DisablePlayerInput();
            PartySheetV2Camera::Enter();
        } else if (!s_Visible && wasVisible) {
            ApplyPause(false);
            PartySheetV2Camera::Exit();
            CloseEffectsModal();
            CloseInventoryModal();
            CloseStatusModal();
            if (!ImGuiRenderer::IsVisible())
                MenuUtils::EnablePlayerInput();
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
                ActorStats stats = GetActorStats(actor);
                float hp = stats.health.GetPercent();
                if (hp < 0.8f) {
                    bool crit = hp < 0.25f;
                    float speed = crit ? 5.0f : 2.5f;
                    float alpha = UIAnimation::Pulse(speed, 0.08f, 0.43f);
                    ImU32 pulseCol = IM_COL32(220, 20, 20, static_cast<int>(alpha * 255));
                    dl->AddQuadFilled(top, right, bottom, left, pulseCol);
                    if (crit)
                        DrawDiamondBorder(dl, cx, cy, half, IM_COL32(255, 60, 60, 180), 1.5f * sc);
                }
            }

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

            ImGui::SetCursorScreenPos(ImVec2(cx - half, cy - half));
            ImGui::InvisibleButton(("##psv2_avatar_" + std::to_string(i)).c_str(),
                ImVec2(half * 2.0f, half * 2.0f));
            if (ImGui::IsItemClicked())
                s_SubjectIndex = i;

            curY += half * 2.0f + gapY;
        }

        dl->PopClipRect();

        ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + size.y));
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
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

        const char* name = actor->GetDisplayFullName();
        static std::string s_unknownName;
        if (s_unknownName.empty()) s_unknownName = LocalizationManager::Get("Common.Unknown");
        if (!name || name[0] == '\0') name = s_unknownName.c_str();
        char nameBuf[128];
        snprintf(nameBuf, sizeof(nameBuf), "%s", name);
        for (char* p = nameBuf; *p; ++p) *p = static_cast<char>(toupper(*p));

        ImGui::PushFont(nullptr, ImGuiRenderer::GetHeadingFontSize());
        ImGui::SetWindowFontScale(FontDisplay());
        ImVec2 nameSz = ImGui::CalcTextSize(nameBuf);
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
            ImVec2(origin.x, rowY), kTheme.textMain, nameBuf);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopFont();

        {
            constexpr float kFadeIn = 0.2f, kHold = 3.0f, kFadeOut = 0.6f;
            const auto now = std::chrono::steady_clock::now();
            float msgY = origin.y;
            ImGui::SetWindowFontScale(FontBody());
            for (const auto& m : GearSwitcher::RecentMessages()) {
                const float age = std::chrono::duration<float>(now - m.at).count();
                const float alpha = UIAnimation::AgeFade(age, kFadeIn, kHold, kFadeOut);
                const ImVec2 textSz = ImGui::CalcTextSize(m.text.c_str());
                const ImVec2 pos(origin.x + (size.x - textSz.x) * 0.5f, msgY);
                dl->AddText(ImVec2(pos.x + 1.0f, pos.y + 1.0f), IM_COL32(0, 0, 0, static_cast<int>(200 * alpha)), m.text.c_str());
                dl->AddText(pos, IM_COL32(255, 255, 255, static_cast<int>(255 * alpha)), m.text.c_str());
                const float slot = UIAnimation::SmoothStep(alpha);
                msgY += (textSz.y + 2.0f * sc) * slot;
            }
            ImGui::SetWindowFontScale(1.0f);
        }

        float lineB_Y = rowY + nameSz.y + 4.0f * sc;
        float penX    = origin.x;

        {
            auto Upper = [](std::string s) { for (char& c : s) c = static_cast<char>(toupper(c)); return s; };
            std::string raceWord, midWord, midTitle;
            bool midMuted = false;
            if (auto* race = actor->GetRace(); race && race->GetFullName()) raceWord = Upper(race->GetFullName());
            if (isPlayer) {
                auto* stone = EffectAttribution::GetStandingStone(actor);
                if (stone) midWord = Upper(LocalizationManager::Get("PartySheetV2.StoneLabel"));
                else { midWord = Upper(LocalizationManager::Get("PartySheetV2.NoStone")); midMuted = true; }
                midTitle = Upper(LocalizationManager::Get("PartySheetV2.StandingStone"));
            } else if (auto* base = actor->GetActorBase()) {
                if (auto* npc = base->As<RE::TESNPC>(); npc && npc->npcClass && npc->npcClass->GetFullName()[0] != '\0')
                    midWord = Upper(npc->npcClass->GetFullName());
            }
            if (midTitle.empty()) midTitle = midWord;
            const bool blessed = HasBlessings(actor);
            const std::string blessWord = blessed ? Upper(LocalizationManager::Get("PartySheetV2.SectionBlessings")) : std::string();
            std::string levelWord = Upper(LocalizationManager::Get("Common.Level") + " " + std::to_string(actor->GetLevel()));
            const std::string sep = "  |  ";

            ImGui::PushFont(nullptr, ImGuiRenderer::GetHeadingFontSize());
            ImGui::SetWindowFontScale(FontHeading());
            const bool modalOpen = IsEffectsModalOpen() || IsInventoryModalOpen() || IsStatusModalOpen();
            const float lineH = ImGui::GetTextLineHeight();

            const float iconSz  = lineH * 0.9f;
            const float iconGap = 4.0f * sc;
            auto DrawWord = [&](const std::string& w, ImU32 col, bool hoverable,
                                ID3D11ShaderResourceView* icon = nullptr) -> bool {
                const float startX = penX;
                if (icon) penX += iconSz + iconGap;
                float ww = ImGui::CalcTextSize(w.c_str()).x;
                bool hovered = false;
                if (hoverable) {
                    ImVec2 rMin(startX - 3.0f * sc, lineB_Y - 1.0f * sc);
                    ImVec2 rMax(penX + ww + 3.0f * sc, lineB_Y + lineH + 2.0f * sc);
                    hovered = !modalOpen && ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(rMin, rMax);
                    if (hovered) {
                        dl->AddRectFilled(rMin, rMax, IM_COL32(29, 33, 41, 220), 2.0f * sc);
                        dl->AddRect(rMin, rMax, IM_COL32(212, 175, 55, 85), 2.0f * sc, 0, 1.0f);
                    }
                    float uy = lineB_Y + lineH;
                    for (float x = penX; x < penX + ww; x += 4.0f * sc)
                        dl->AddLine(ImVec2(x, uy), ImVec2((std::min)(x + 1.5f * sc, penX + ww), uy),
                            IM_COL32(136, 136, 136, 200), 1.0f);
                }
                if (icon) {
                    const ImU32 tint = (col & IM_COL32_A_MASK) == IM_COL32_A_MASK ? 0xFFFFFFFF : IM_COL32(255, 255, 255, 140);
                    dl->AddImage(reinterpret_cast<ImTextureID>(icon),
                        ImVec2(startX, lineB_Y + (lineH - iconSz) * 0.5f),
                        ImVec2(startX + iconSz, lineB_Y + (lineH + iconSz) * 0.5f), ImVec2(0, 0), ImVec2(1, 1), tint);
                }
                dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(penX, lineB_Y), col, w.c_str());
                penX += ww;
                return hovered;
            };
            auto DrawSep = [&]() {
                dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(penX, lineB_Y), kTheme.textMuted, sep.c_str());
                penX += ImGui::CalcTextSize(sep.c_str()).x;
            };

            static ID3D11ShaderResourceView* s_TexStoneIco = nullptr;
            static ID3D11ShaderResourceView* s_TexBlessIco = nullptr;
            if (!s_TexStoneIco) s_TexStoneIco = TextureManager::GetPartyIcon("standing_stone");
            if (!s_TexBlessIco) s_TexBlessIco = TextureManager::GetPartyIcon("blessing");

            bool raceHover = false, midHover = false, blessHover = false, levelHover = false;

            enum class LvState { Player, Mirror, Max, Fixed } lvState = LvState::Fixed;
            float xpCur = 0.0f, xpNeed = 0.0f;
            if (auto* pc = RE::PlayerCharacter::GetSingleton())
                if (auto* sk = pc->GetInfoRuntimeData().skills; sk && sk->data) {
                    xpCur = sk->data->xp; xpNeed = sk->data->levelThreshold;
                }
            if (isPlayer) lvState = LvState::Player;
            else if (auto* nb = actor->GetActorBase(); nb && nb->HasPCLevelMult()) {
                const auto maxLv = nb->actorData.calcLevelMax;
                lvState = (maxLv > 0 && actor->GetLevel() >= maxLv) ? LvState::Max : LvState::Mirror;
            }
            const float xpFrac = (xpNeed > 0.0f) ? std::clamp(xpCur / xpNeed, 0.0f, 1.0f) : 0.0f;

            {
                const float lvX0 = penX;
                DrawWord(levelWord, kTheme.textMuted, false);
                const float lvX1 = penX;
                if (lvState != LvState::Fixed) {
                    const float barY = lineB_Y + lineH + 1.0f * sc;
                    const float barH = 3.0f * sc;
                    const float fill = (lvState == LvState::Max) ? 1.0f : xpFrac;
                    const ImU32 fillCol = (lvState == LvState::Player) ? IM_COL32(212, 175, 55, 255) : IM_COL32(170, 140, 50, 230);
                    dl->AddRectFilled(ImVec2(lvX0, barY), ImVec2(lvX1, barY + barH), IM_COL32(40, 44, 52, 220), 1.0f * sc);
                    if (fill > 0.0f)
                        dl->AddRectFilled(ImVec2(lvX0, barY), ImVec2(lvX0 + (lvX1 - lvX0) * fill, barY + barH), fillCol, 1.0f * sc);
                }
                ImVec2 rMin(lvX0 - 3.0f * sc, lineB_Y - 1.0f * sc);
                ImVec2 rMax(lvX1 + 3.0f * sc, lineB_Y + lineH + 5.0f * sc);
                levelHover = !modalOpen && ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(rMin, rMax);
                if (levelHover)
                    dl->AddRect(rMin, rMax, IM_COL32(212, 175, 55, 85), 2.0f * sc, 0, 1.0f);
            }
            DrawSep();
            if (!raceWord.empty()) { raceHover = DrawWord(raceWord, kTheme.textMuted, true); if (!midWord.empty() || blessed) DrawSep(); }
            if (!midWord.empty()) {
                ImU32 midCol = midMuted ? IM_COL32(136, 136, 136, 140) : kTheme.textMuted;
                midHover = DrawWord(midWord, midCol, true, isPlayer ? s_TexStoneIco : nullptr);
                if (blessed) DrawSep();
            }
            if (blessed) blessHover = DrawWord(blessWord, kTheme.textMuted, true, s_TexBlessIco);
            penX += 16.0f * sc;
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopFont();

            if (levelHover) {
                Tip::TipRow r;
                static ID3D11ShaderResourceView* s_TexXpIco = nullptr;
                if (!s_TexXpIco) s_TexXpIco = TextureManager::GetPartyIcon("experience");
                r.icon = s_TexXpIco;
                r.name = LocalizationManager::Get("PartySheetV2.Experience");
                char xpBuf[96];
                snprintf(xpBuf, sizeof(xpBuf), "%d / %d  (%d%%)",
                    static_cast<int>(xpCur), static_cast<int>(xpNeed), static_cast<int>(xpFrac * 100.0f + 0.5f));
                switch (lvState) {
                    case LvState::Player: r.lines.push_back({ xpBuf, kTheme.textMain }); break;
                    case LvState::Mirror:
                        r.lines.push_back({ LocalizationManager::Get("PartySheetV2.LevelsWithYou"), kTheme.textMuted });
                        r.lines.push_back({ xpBuf, kTheme.textMain });
                        break;
                    case LvState::Max:   r.lines.push_back({ LocalizationManager::Get("PartySheetV2.MaxLevel"), kTheme.textMain }); break;
                    case LvState::Fixed: r.lines.push_back({ LocalizationManager::Get("PartySheetV2.FixedLevel"), kTheme.textMuted }); break;
                }
                Tip::DrawCompactTooltip(levelWord, { r }, {}, sc);
            }
            if (raceHover) DrawRaceTooltip(actor, sc);
            if (midHover)   DrawPassiveTooltip(actor, midTitle, sc, isPlayer);
            if (blessHover) DrawBlessingsTooltip(actor, blessWord, sc);
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

            if (auto curse = GetCurse(actor); curse.kind != CurseInfo::Kind::None) {
                const bool vamp = curse.kind == CurseInfo::Kind::Vampire;
                std::string cs = LocalizationManager::Get(vamp ? "PartySheetV2.Vampire" : "PartySheetV2.Werewolf");
                for (char& c : cs) c = static_cast<char>(toupper(c));
                const std::string title = cs;
                if (vamp && curse.stage > 0) {
                    std::string st = LocalizationManager::Get("PartySheetV2.Stage");
                    for (char& c : st) c = static_cast<char>(toupper(c));
                    cs += " \xC2\xB7 " + st + " " + std::to_string(curse.stage);
                }
                ImVec2 sz = ImGui::CalcTextSize(cs.c_str());
                ImVec2 tMin(penX, tagY);
                ImVec2 tMax(penX + sz.x + tpad * 2.0f, tagY + sz.y + tpadH * 2.0f);
                const bool modalOpen = IsEffectsModalOpen() || IsInventoryModalOpen() || IsStatusModalOpen();
                const bool hovered = !modalOpen && ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(tMin, tMax);
                if (hovered) dl->AddRectFilled(tMin, tMax, IM_COL32(60, 20, 20, 200), 2.0f * sc);
                DrawTag(cs.c_str(), IM_COL32(200, 60, 60, 220), IM_COL32(230, 90, 90, 255));
                if (hovered) {
                    ImGui::SetWindowFontScale(1.0f);
                    DrawCurseTooltip(actor, title, sc);
                    ImGui::SetWindowFontScale(FontSmall());
                }
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

        ImGui::PushFont(nullptr, ImGuiRenderer::GetHeadingFontSize());
        ImGui::SetWindowFontScale(FontHeading());
        float lineBH = ImGui::GetTextLineHeight();
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopFont();
        const float sepY = lineB_Y + lineBH + 10.0f * sc;
        HUDDraw::DrawFadeRule(dl, origin.x, origin.x + size.x, sepY, IM_COL32(212,175,55,100));

        const float barsY    = sepY + 14.0f * sc;
        const float barH     = 5.0f  * sc;
        const float labelGap = 6.0f  * sc;
        ActorStats stats     = GetActorStats(actor);

        struct VitalDef { std::string label; VitalReadout v; ImU32 color; RE::ActorValue av; RE::ActorValue rateAV; };
        VitalDef vitals[3] = {
            { LocalizationManager::Get("Stats.Health"),  GetVital(actor, stats, RE::ActorValue::kHealth),  kTheme.hp, RE::ActorValue::kHealth,  RE::ActorValue::kHealRate },
            { LocalizationManager::Get("Stats.Magicka"), GetVital(actor, stats, RE::ActorValue::kMagicka), kTheme.mp, RE::ActorValue::kMagicka, RE::ActorValue::kMagickaRate },
            { LocalizationManager::Get("Stats.Stamina"), GetVital(actor, stats, RE::ActorValue::kStamina), kTheme.sp, RE::ActorValue::kStamina, RE::ActorValue::kStaminaRate },
        };
        int hoveredRegen = -1;
        static constexpr ImU32 kPenaltyRed = IM_COL32(220, 80, 80, 255);
        const bool vitalModalOpen = IsEffectsModalOpen() || IsInventoryModalOpen() || IsStatusModalOpen();
        int hoveredVital = -1;

        const float vGap     = 24.0f * sc;
        const float leftColW = size.x - kCenterW * sc - kRightW * sc - 2.0f * kGap * sc;
        const float barsW    = std::max(leftColW, 200.0f * sc);
        const float vitalW   = (barsW - 2.0f * vGap) / 3.0f;

        for (int v = 0; v < 3; ++v) {
            const auto& vt = vitals[v];
            float vx = origin.x + v * (vitalW + vGap);

            ImGui::SetWindowFontScale(FontBody());
            ImVec2 labelSz = ImGui::CalcTextSize(vt.label.c_str());

            const bool  reduced = vt.v.bonus <= -0.5f;
            const float lost    = vt.v.lost > 0.5f ? vt.v.lost : 0.0f;
            char curBuf[16], maxBuf[16];
            snprintf(curBuf, sizeof(curBuf), "%.0f / ", vt.v.current);
            snprintf(maxBuf, sizeof(maxBuf), "%.0f", vt.v.FullMax());
            const float curW = ImGui::CalcTextSize(curBuf).x;
            ImVec2 valSz = ImGui::CalcTextSize(maxBuf);
            valSz.x += curW;

            if (HasStatPassives(actor, vt.av)) {
                float barBottom = barsY + labelSz.y + labelGap + barH;
                ImVec2 hMin(vx - 3.0f * sc, barsY - 1.0f * sc);
                ImVec2 hMax(vx + vitalW + 3.0f * sc, barBottom + 2.0f * sc);
                bool hovered = !vitalModalOpen && ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(hMin, hMax);
                if (hovered) {
                    dl->AddRectFilled(hMin, ImVec2(vx + labelSz.x + 3.0f * sc, barsY + labelSz.y + 2.0f * sc),
                        IM_COL32(29, 33, 41, 220), 2.0f * sc);
                    dl->AddRect(hMin, ImVec2(vx + labelSz.x + 3.0f * sc, barsY + labelSz.y + 2.0f * sc),
                        IM_COL32(212, 175, 55, 85), 2.0f * sc, 0, 1.0f);
                    hoveredVital = v;
                }
                float uy = barsY + labelSz.y;
                for (float x = vx; x < vx + labelSz.x; x += 4.0f * sc)
                    dl->AddLine(ImVec2(x, uy), ImVec2((std::min)(x + 1.5f * sc, vx + labelSz.x), uy),
                        IM_COL32(136, 136, 136, 200), 1.0f);
            }
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(vx, barsY), kTheme.textMuted, vt.label.c_str());
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(vx + vitalW - valSz.x, barsY), kTheme.textMain, curBuf);
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(vx + vitalW - valSz.x + curW, barsY), reduced ? kPenaltyRed : kTheme.textMain, maxBuf);
            ImGui::SetWindowFontScale(1.0f);

            float barY = barsY + labelSz.y + labelGap;
            ImVec2 bMin(vx, barY);
            ImVec2 bMax(vx + vitalW, barY + barH);
            dl->AddRectFilled(bMin, bMax, IM_COL32(0, 0, 0, 130), 2.0f * sc);
            dl->AddRect(bMin, bMax, IM_COL32(255, 255, 255, 15), 2.0f * sc, 0, 1.0f);

            const float full    = vt.v.max + lost;
            const float usableW = (full > 0.0f) ? vitalW * (vt.v.max / full) : vitalW;
            if (lost > 0.0f && usableW < vitalW) {
                dl->AddRectFilled(ImVec2(vx + usableW, barY), bMax, IM_COL32(220, 80, 80, 110), 2.0f * sc);
                HUDDraw::DrawRuleV(dl, vx + usableW, barY - 1.0f * sc, barY + barH + 1.0f * sc, kPenaltyRed, 1.5f * sc);
            }
            float pct = (vt.v.max > 0.0f) ? std::clamp(vt.v.current / vt.v.max, 0.0f, 1.0f) : 0.0f;
            if (pct > 0.0f) {
                ImVec2 fMax(vx + usableW * pct, barY + barH);
                dl->AddRectFilled(bMin, fMax, vt.color, 2.0f * sc);
                dl->AddRectFilledMultiColor(
                    bMin, ImVec2(fMax.x, barY + barH * 0.5f),
                    IM_COL32(255,255,255,40), IM_COL32(255,255,255,40),
                    IM_COL32(0,0,0,0), IM_COL32(0,0,0,0));
            }

            {
                const float perSec = vt.v.regenPerSec;
                char rBuf[24];
                snprintf(rBuf, sizeof(rBuf), "%+.1f/s", perSec);
                ImGui::SetWindowFontScale(FontSmall());
                const ImVec2 rSz = ImGui::CalcTextSize(rBuf);
                const ImVec2 rPos(vx + vitalW - rSz.x, barY + barH + 3.0f * sc);
                if (HasStatPassives(actor, vt.rateAV)) {
                    ImVec2 hMin(rPos.x - 3.0f * sc, rPos.y - 1.0f * sc);
                    ImVec2 hMax(rPos.x + rSz.x + 3.0f * sc, rPos.y + rSz.y + 2.0f * sc);
                    if (!vitalModalOpen && ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(hMin, hMax)) {
                        dl->AddRectFilled(hMin, hMax, IM_COL32(29, 33, 41, 220), 2.0f * sc);
                        dl->AddRect(hMin, hMax, IM_COL32(212, 175, 55, 85), 2.0f * sc, 0, 1.0f);
                        hoveredRegen = v;
                    }
                    float uy = rPos.y + rSz.y;
                    for (float x = rPos.x; x < rPos.x + rSz.x; x += 4.0f * sc)
                        dl->AddLine(ImVec2(x, uy), ImVec2((std::min)(x + 1.5f * sc, rPos.x + rSz.x), uy),
                            IM_COL32(136, 136, 136, 200), 1.0f);
                }
                dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), rPos, kTheme.textMuted, rBuf);
                ImGui::SetWindowFontScale(1.0f);
            }
        }
        if (hoveredVital >= 0)
            DrawStatTooltip(actor, vitals[hoveredVital].label, vitals[hoveredVital].av, sc);
        if (hoveredRegen >= 0)
            DrawStatTooltip(actor, EffectCategories::LocalizedAVName(vitals[hoveredRegen].rateAV), vitals[hoveredRegen].rateAV, sc);

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

        ImGui::PushFont(nullptr, ImGuiRenderer::GetHeadingFontSize());
        ImGui::SetWindowFontScale(FontHeading());
        std::string skillsTitle = LocalizationManager::Get("Skills.SectionTitle");
        for (char& c : skillsTitle) c = static_cast<char>(toupper(c));
        const char* titleText = skillsTitle.c_str();
        ImVec2 titleSz = ImGui::CalcTextSize(titleText);
        AddTextShadowed(dl, ImGui::GetFont(), ImGui::GetFontSize(),
            ImVec2(origin.x, origin.y), kTheme.accent, titleText);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopFont();

        const float sepY  = origin.y + titleSz.y + 4.0f * sc;
        const float lineW = size.x;
        HUDDraw::DrawFadeRule(dl, origin.x, origin.x + lineW, sepY, IM_COL32(212,175,55,100));

        struct SkillEntry { std::string name; float value; ID3D11ShaderResourceView* tex; RE::ActorValue av; };
        const SkillEntry skills[18] = {
            { LocalizationManager::Get("Skills.OneHanded"),  sk.oneHanded.value,  s_SkillTex.oneHanded, RE::ActorValue::kOneHanded },
            { LocalizationManager::Get("Skills.TwoHanded"),  sk.twoHanded.value,  s_SkillTex.twoHanded, RE::ActorValue::kTwoHanded },
            { LocalizationManager::Get("Skills.Block"),      sk.block.value,       s_SkillTex.block, RE::ActorValue::kBlock },
            { LocalizationManager::Get("Skills.HeavyArmor"), sk.heavyArmor.value,  s_SkillTex.heavyArmor, RE::ActorValue::kHeavyArmor },
            { LocalizationManager::Get("Skills.LightArmor"), sk.lightArmor.value,  s_SkillTex.lightArmor, RE::ActorValue::kLightArmor },
            { LocalizationManager::Get("Skills.Smithing"),   sk.smithing.value,    s_SkillTex.smithing, RE::ActorValue::kSmithing },
            { LocalizationManager::Get("Skills.Sneak"),      sk.sneak.value,       s_SkillTex.sneak, RE::ActorValue::kSneak },
            { LocalizationManager::Get("Skills.Archery"),    sk.archery.value,     s_SkillTex.archery, RE::ActorValue::kArchery },
            { LocalizationManager::Get("Skills.Speech"),     sk.speech.value,      s_SkillTex.speech, RE::ActorValue::kSpeech },
            { LocalizationManager::Get("Skills.Alchemy"),    sk.alchemy.value,     s_SkillTex.alchemy, RE::ActorValue::kAlchemy },
            { LocalizationManager::Get("Skills.Lockpicking"),sk.lockpicking.value, s_SkillTex.lockpicking, RE::ActorValue::kLockpicking },
            { LocalizationManager::Get("Skills.Pickpocket"), sk.pickpocket.value,  s_SkillTex.pickpocket, RE::ActorValue::kPickpocket },
            { LocalizationManager::Get("Skills.Conjuration"),sk.conjuration.value, s_SkillTex.conjuration, RE::ActorValue::kConjuration },
            { LocalizationManager::Get("Skills.Illusion"),   sk.illusion.value,    s_SkillTex.illusion, RE::ActorValue::kIllusion },
            { LocalizationManager::Get("Skills.Alteration"), sk.alteration.value,  s_SkillTex.alteration, RE::ActorValue::kAlteration },
            { LocalizationManager::Get("Skills.Destruction"),sk.destruction.value, s_SkillTex.destruction, RE::ActorValue::kDestruction },
            { LocalizationManager::Get("Skills.Restoration"),sk.restoration.value, s_SkillTex.restoration, RE::ActorValue::kRestoration },
            { LocalizationManager::Get("Skills.Enchanting"), sk.enchanting.value,  s_SkillTex.enchanting, RE::ActorValue::kEnchanting },
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

            auto tint = SkillRowTint::Get(actor, s.av, s_WornGearData);
            if (tint.on)
                dl->AddRectFilled(rMin, rMax, tint.bg);

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
            const ImU32 labelCol = tint.on ? tint.name  : kTheme.textMuted;
            const ImU32 valCol   = tint.on ? tint.value : kTheme.textMain;
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

            if (!IsEffectsModalOpen() && !IsInventoryModalOpen() && !IsStatusModalOpen() &&
                ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(rMin, rMax))
                DrawStatTooltip(actor, s.name, s.av, sc);
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
        static ID3D11ShaderResourceView* s_TexMoveSpeed  = nullptr;
        if (!s_TexArmor)     s_TexArmor     = TextureManager::GetPartyIcon("armor");
        if (!s_TexDmgRes)    s_TexDmgRes    = TextureManager::GetPartyIcon("damage_resist");
        if (!s_TexMagic)     s_TexMagic     = TextureManager::GetPartyIcon("magic_resist");
        if (!s_TexFire)      s_TexFire      = TextureManager::GetPartyIcon("fire_resist");
        if (!s_TexFrost)     s_TexFrost     = TextureManager::GetPartyIcon("frost_resist");
        if (!s_TexShock)     s_TexShock     = TextureManager::GetPartyIcon("shock_resist");
        if (!s_TexPoison)    s_TexPoison    = TextureManager::GetPartyIcon("poison_resist");
        if (!s_TexDisease)   s_TexDisease   = TextureManager::GetPartyIcon("disease_resist");
        if (!s_TexMoveSpeed) s_TexMoveSpeed = TextureManager::GetPartyIcon("movespeed");

        struct ResistTile {
            std::string               label;
            float                     value;
            float                     rangeMax;
            const char*               fmt;
            ID3D11ShaderResourceView* tex;
            uint8_t                   resistBit = 0;
            RE::ActorValue            av = RE::ActorValue::kNone;
        };
        const ResistTile tiles[] = {
            { LocalizationManager::Get("Stats.Armor"),         stats.armorRating,     500.0f, "%.0f",   s_TexArmor,   0, RE::ActorValue::kDamageResist },
            { LocalizationManager::Get("Stats.DamageResist"),  stats.damageResistPct,  80.0f, "%.0f%%", s_TexDmgRes,  0 },
            { LocalizationManager::Get("Stats.Magic"),         stats.resistMagic,      85.0f, "%.0f%%", s_TexMagic,   WornGearStatResolver::kResistMagic,  RE::ActorValue::kResistMagic },
            { LocalizationManager::Get("Resistances.Fire"),    stats.resistFire,      100.0f, "%.0f%%", s_TexFire,    WornGearStatResolver::kResistFire,   RE::ActorValue::kResistFire },
            { LocalizationManager::Get("Resistances.Frost"),   stats.resistFrost,     100.0f, "%.0f%%", s_TexFrost,   WornGearStatResolver::kResistFrost,  RE::ActorValue::kResistFrost },
            { LocalizationManager::Get("Resistances.Shock"),   stats.resistShock,     100.0f, "%.0f%%", s_TexShock,   WornGearStatResolver::kResistShock,  RE::ActorValue::kResistShock },
            { LocalizationManager::Get("Resistances.Poison"),  stats.resistPoison,    100.0f, "%.0f%%", s_TexPoison,  WornGearStatResolver::kResistPoison, RE::ActorValue::kPoisonResist },
            { LocalizationManager::Get("Resistances.Disease"), stats.resistDisease,   100.0f, "%.0f%%", s_TexDisease, WornGearStatResolver::kResistDisease, RE::ActorValue::kResistDisease },
            { LocalizationManager::Get("Stats.Move"),          stats.speedMult,       150.0f, "%.0f%%", s_TexMoveSpeed },
        };

        {
            ImVec2 tp = ImGui::GetCursorScreenPos();
            ImGui::PushFont(nullptr, ImGuiRenderer::GetHeadingFontSize());
            ImGui::SetWindowFontScale(FontHeading());
            std::string defTitle = LocalizationManager::Get("Stats.Defense");
            for (char& c : defTitle) c = static_cast<char>(toupper(c));
            const ImVec2 dSz = ImGui::CalcTextSize(defTitle.c_str());
            AddTextShadowed(dl, ImGui::GetFont(), ImGui::GetFontSize(), tp, kTheme.accent, defTitle.c_str());
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopFont();
            const float dSepY = tp.y + dSz.y + 4.0f * sc;
            HUDDraw::DrawFadeRule(dl, tp.x, tp.x + size.x, dSepY, IM_COL32(212,175,55,100));
            ImGui::Dummy(ImVec2(size.x, dSepY - tp.y + 8.0f * sc));
        }
        ImVec2 dcOrigin = ImGui::GetCursorScreenPos();
        const float tGapX   = 8.0f * sc;
        const float tGapY   = 4.0f * sc;
        const float tColW   = (size.x - tGapX) * 0.5f;
        const float tIconSz = Settings::GetSingleton()->psV2IconStat * sc;
        const float tPadX   = 5.0f * sc;
        const float tValW   = 44.0f * sc;
        ImGui::SetWindowFontScale(FontBody());
        const float tTextH  = ImGui::GetTextLineHeight();
        ImGui::SetWindowFontScale(1.0f);
        const float tRowH   = std::max(tIconSz, tTextH) + 6.0f * sc;

        constexpr int kPips = 6;
        const float pipR   = 3.5f * sc;
        const float pipGap = 2.0f * sc;
        const float pipsW  = kPips * pipR * 2.0f + (kPips - 1) * pipGap;
        const ImU32 pipFill   = IM_COL32(212, 175,  55, 200);
        const ImU32 pipBorder = IM_COL32(255, 255, 255, 25);
        const ImU32 pipEmpty  = IM_COL32(255, 255, 255, 15);

        const bool defModalOpen = IsEffectsModalOpen() || IsInventoryModalOpen() || IsStatusModalOpen();
        const ResistTile* hoveredTile = nullptr;
        constexpr int kTileCount = static_cast<int>(std::size(tiles));
        for (int i = 0; i < kTileCount; ++i) {
            const auto& t = tiles[i];
            const ImVec2 tMin(dcOrigin.x + (i % 2) * (tColW + tGapX), dcOrigin.y + (i / 2) * (tRowH + tGapY));
            const ImVec2 tMax(tMin.x + tColW, tMin.y + tRowH);

            const bool hovered = ImGui::IsMouseHoveringRect(tMin, tMax);
            if (hovered) dl->AddRectFilled(tMin, tMax, kTheme.hoverFill);
            const bool fromGear = t.resistBit && (s_WornGearData.resistHighlightMask & t.resistBit);
            const bool debuffed = t.av != RE::ActorValue::kNone && EffectAttribution::HasStatDebuff(actor, t.av);
            const bool potion   = !debuffed && !fromGear && t.av != RE::ActorValue::kNone && EffectAttribution::HasPotionBuff(actor, t.av);
            if (debuffed)      dl->AddRectFilled(tMin, tMax, IM_COL32(100, 38, 38, 200));
            else if (fromGear) dl->AddRectFilled(tMin, tMax, BadgeStyle::Theme::ResistPurple().fill);
            else if (potion)   dl->AddRectFilled(tMin, tMax, IM_COL32(35, 85, 45, 210));

            const float iconX = tMin.x + tPadX;
            const float textY = tMin.y + (tRowH - tTextH) * 0.5f;
            if (t.tex)
                HUDDraw::DrawIconStyled(dl, ImVec2(iconX, tMin.y + (tRowH - tIconSz) * 0.5f), tIconSz,
                    reinterpret_cast<ImTextureID>(t.tex), HUDDraw::PackedToArgb(IM_COL32(255,255,255,200)), IconFxDefault());

            ImGui::SetWindowFontScale(FontBody());
            AddTextShadowed(dl, ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(iconX + tIconSz + tPadX, textY), (fromGear || potion || debuffed) ? IM_COL32(255,255,255,255) : kTheme.textMuted, t.label.c_str());

            const float valueRight = tMax.x - tPadX;
            const float pipsLeft   = valueRight - tValW - tPadX - pipsW;
            const int   filled     = static_cast<int>(std::round(std::clamp(t.value / t.rangeMax, 0.0f, 1.0f) * kPips));
            for (int p = 0; p < kPips; ++p) {
                const ImVec2 c(pipsLeft + pipR + p * (pipR * 2.0f + pipGap), tMin.y + tRowH * 0.5f);
                dl->AddCircleFilled(c, pipR, p < filled ? pipFill : pipEmpty, 0);
                dl->AddCircle(c, pipR, pipBorder, 0, 1.0f);
        }

            char valBuf[16];
            snprintf(valBuf, sizeof(valBuf), t.fmt, t.value);
            const ImVec2 valSz = ImGui::CalcTextSize(valBuf);
            const ImU32 valCol = t.value < -0.5f ? IM_COL32(220, 80, 80, 255) : (fromGear ? IM_COL32(255,255,255,255) : kTheme.textMain);
            AddTextShadowed(dl, ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(valueRight - valSz.x, textY), valCol, valBuf);
            ImGui::SetWindowFontScale(1.0f);

            if (hovered && t.av != RE::ActorValue::kNone && !defModalOpen && ImGui::IsWindowHovered())
                hoveredTile = &t;
        }
        if (hoveredTile)
            DrawStatTooltip(actor, hoveredTile->label, hoveredTile->av, sc);

        const int   tileRows = (kTileCount + 1) / 2;
        const float dcUsedH  = tileRows * tRowH + (tileRows - 1) * tGapY;
        ImGui::Dummy(ImVec2(size.x, dcUsedH));

        bool anyModalOpen = IsEffectsModalOpen() || IsInventoryModalOpen() || IsStatusModalOpen();

        ImGui::SetWindowFontScale(FontSmall());
        float tabH = ImGui::GetTextLineHeight() + 2.0f * 5.0f * sc;
        ImGui::SetWindowFontScale(1.0f);

        auto DrawTab = [&](const char* id, ImVec2 size, const char* label,
                            ID3D11ShaderResourceView* icon) -> bool {
            ImGui::InvisibleButton(id, size);
            bool hovered = ImGui::IsItemHovered() && !anyModalOpen;
            bool active  = ImGui::IsItemActive()  && !anyModalOpen;
            bool clicked = ImGui::IsItemClicked() && !anyModalOpen;

            const float hoverFrac = UIAnimation::Follow(id, hovered ? 1.0f : 0.0f, 8.0f, 8.0f);

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
                HUDDraw::DrawRuleH(dl, bMin.x + 2.0f * sc, bMax.x - 2.0f * sc, bMax.y - 1.0f, line);
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
        if (DrawTab("##psv2_fx_btn", ImVec2(TabWidth(fxBtnLabel.c_str()), tabH), fxBtnLabel.c_str(), s_TexFxTab))
            OpenEffectsModal();
        ImGui::EndDisabled();

        ImGui::SameLine(0.0f, 6.0f * sc);

        ImGui::BeginDisabled(anyModalOpen);
        if (DrawTab("##psv2_inv_btn", ImVec2(TabWidth(invBtnLabel.c_str()), tabH), invBtnLabel.c_str(), s_TexInvTab))
            OpenInventoryModal();
        ImGui::EndDisabled();

        if (actor && actor->IsPlayerRef()) {
            ImGui::SameLine(0.0f, 6.0f * sc);
            ImGui::BeginDisabled(anyModalOpen);
            if (DrawTab("##psv2_status_btn", ImVec2(TabWidth(statusBtnLabel.c_str()), tabH), statusBtnLabel.c_str(), s_TexStatusTab))
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

        constexpr int   kMaxTraits  = 4;
        const float     iconSz      = Settings::GetSingleton()->psV2IconTrait * sc;

        ImGui::SetWindowFontScale(FontSmall());
        const float traitLabelH = ImGui::GetTextLineHeight();
        ImGui::SetWindowFontScale(1.0f);
        const float pipSzPre    = 7.0f * sc;
        const float traitBlockH = iconSz + 4.0f * sc + traitLabelH + 4.0f * sc + pipSzPre;
        [[maybe_unused]] const float portraitBottom = origin.y + size.y - traitBlockH - 64.0f * sc;

#if 0
        if (auto* portrait = TextureManager::GetPortrait(actor)) {
            float texW = 1.0f, texH = 1.0f;
            ID3D11Resource* res = nullptr;
            portrait->GetResource(&res);
            if (res) {
                ID3D11Texture2D* tex2d = nullptr;
                if (SUCCEEDED(res->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&tex2d))) && tex2d) {
                    D3D11_TEXTURE2D_DESC desc{};
                    tex2d->GetDesc(&desc);
                    texW = static_cast<float>(desc.Width);
                    texH = static_cast<float>(desc.Height);
                    tex2d->Release();
                }
                res->Release();
            }
            const float areaTop = origin.y + 8.0f * sc;
            const float areaH   = portraitBottom - 8.0f * sc - areaTop;
            const float areaW   = size.x - 16.0f * sc * 2.0f;
            if (areaH > 0.0f && areaW > 0.0f) {
                const float fit = (std::min)(areaW / texW, areaH / texH);
                const float w = texW * fit, h = texH * fit;
                const ImVec2 pMin(origin.x + (size.x - w) * 0.5f, portraitBottom - 8.0f * sc - h);
                dl->AddImage(reinterpret_cast<ImTextureID>(portrait), pMin, ImVec2(pMin.x + w, pMin.y + h));
            }
        }
#endif
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

        {
            struct StatCard { const char* iconName; const char* labelKey; float value; };
            const StatCard cards[2] = {
                {"weapon", "Stats.Damage", stats.totalDamage > 0.0f ? stats.totalDamage : stats.unarmedDamage},
                {"armor",  "Stats.Armor",  stats.armorRating},
            };
            ImFont* font = ImGui::GetFont();
            const float labelSize = ImGui::GetFontSize() * FontSmall();
            const float valueSize = ImGui::GetFontSize() * FontHeading() * 1.4f;
            const float cardIcon  = labelSize + valueSize;
            const float pad   = 8.0f * sc;
            const float cardW = size.x * 0.5f;
            const float cardY = blockY - cardIcon - pad * 2.0f - 12.0f * sc;
            for (int i = 0; i < 2; ++i) {
                const auto& card = cards[i];
                ImVec2 iconMin(origin.x + i * cardW + pad, cardY + pad);
                if (auto icon = TextureManager::GetPartyIcon(card.iconName))
                    dl->AddImage((ImTextureID)icon, iconMin, ImVec2(iconMin.x + cardIcon, iconMin.y + cardIcon));
                std::string label = LocalizationManager::Get(card.labelKey);
                for (auto& c : label) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
                char valueText[32];
                snprintf(valueText, sizeof(valueText), "%.0f", (std::max)(card.value, 0.0f));
                const float textX = iconMin.x + cardIcon + pad;
                dl->AddText(font, labelSize, ImVec2(textX, iconMin.y), IM_COL32(200, 200, 200, 200), label.c_str());
                dl->AddText(font, valueSize, ImVec2(textX + 1.0f, iconMin.y + labelSize + 1.0f), IM_COL32(0, 0, 0, 180), valueText);
                dl->AddText(font, valueSize, ImVec2(textX, iconMin.y + labelSize), IM_COL32(240, 240, 240, 255), valueText);
            }
        }

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

            HUDDraw::PipStyle ps;
            ps.shape     = 2;
            ps.radius    = pipSz * 0.5f;
            ps.gap       = pipGapSq;
            ps.rounding  = pipRad;
            ps.litColor  = HUDDraw::PackedToArgb(isMaster ? kTheme.accent : kTheme.textMain);
            ps.emptyColor = 0;
            ps.emptyEdge = HUDDraw::PackedToArgb(kTheme.borderSubtle);
            ps.litEdge   = isMaster ? HUDDraw::PackedToArgb(V2Theme::AccentA(80)) : 0;
            ps.duotone = ps.glow = ps.socket = ps.specular = ps.contour = false;
            HUDDraw::DrawPipRow(dl, ImVec2(pipsStartX, pipsY + pipSz * 0.5f), filled, kPipCount, ps);
        }

        ImGui::SetWindowFontScale(1.0f);

        const float resH        = 22.0f * sc;
        const float resPadX     = 28.0f * sc;
        const float resGap      = 16.0f * sc;
        const float resY        = blockY + blockH + 14.0f * sc;

        HUDDraw::DrawRuleH(dl, origin.x + resPadX, origin.x + size.x - resPadX, resY, IM_COL32(255, 255, 255, 15));

        ImGui::SetWindowFontScale(FontBody());
        const float resTextY = resY + (resH - ImGui::GetTextLineHeight()) * 0.5f;

        std::string actorName = actor->GetName() ? actor->GetName() : "";
        ImVec2 nameSzR = ImGui::CalcTextSize(actorName.c_str());
        dl->AddText(ImVec2(origin.x + resPadX, resTextY), kTheme.textMain, actorName.c_str());

        float divX = origin.x + resPadX + nameSzR.x + resGap * 0.5f;
        HUDDraw::DrawRuleV(dl, divX, resY + 3.0f * sc, resY + resH - 3.0f * sc, IM_COL32(255, 255, 255, 25));

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

    struct ItemTooltipCopy {
        const char*          name = nullptr;
        RE::EnchantmentItem* enchant = nullptr;
        bool                 isCopy = false;
    };

    static void DrawItemTooltip(RE::Actor* actor, RE::TESForm* form, const char* slotLabel,
                                ID3D11ShaderResourceView* icon, bool bodySlot, float sc,
                                const ItemTooltipCopy& copy = {}) {
        using namespace Tip;
        if (!actor || !form) return;
        using AV = RE::ActorValue;
        using EffFlag = RE::EffectSetting::EffectSettingData::Flag;

        TipRow item;
        item.icon = icon;
        item.name = copy.name ? copy.name : (form->GetName() ? form->GetName() : "");
        auto* spell = form->As<RE::SpellItem>();
        auto* weap  = form->As<RE::TESObjectWEAP>();
        if (auto* arm = form->As<RE::TESObjectARMO>()) {
            if (arm->IsHeavyArmor())      item.type = EffectCategories::LocalizedAVName(AV::kHeavyArmor);
            else if (arm->IsLightArmor()) item.type = EffectCategories::LocalizedAVName(AV::kLightArmor);
        } else if (weap) {
            item.type = EffectCategories::LocalizedAVName(weap->weaponData.skill.get());
        } else if (spell) {
            item.type = EffectCategories::LocalizedAVName(spell->GetAssociatedSkill());
        }

        auto AddLine = [](TipRow& row, const RE::EffectSetting* base, float mag, float dur, ImU32 col) {
            if (!base || base->data.flags.any(EffFlag::kHideInUI)) return;
            std::string t = DescribeEffect(base, DisplayMagnitude(base, mag), dur, col == kDebuff);
            if (t.empty()) return;
            if (std::none_of(row.lines.begin(), row.lines.end(), [&](const TipLine& l) { return l.text == t; }))
                row.lines.push_back({ std::move(t), col });
        };
        auto IsBad = [](const RE::EffectSetting* b, float mag) {
            return EffectCategories::IsDebuff(b, mag);
        };

        const auto& records = EffectAttribution::Get(actor);

        if (spell) {
            for (auto* eff : spell->effects)
                if (eff && eff->baseEffect)
                    AddLine(item, eff->baseEffect, eff->effectItem.magnitude,
                            static_cast<float>(eff->effectItem.duration), kTextMuted);
        } else {
            bool fromLive = false;
            if (!weap && !copy.isCopy) {
                for (const auto& rec : records) {
                    if (rec.item != form) continue;
                    AddLine(item, rec.mgef, rec.rawMagnitude, 0.0f, rec.hostile ? kDebuff : kBuff);
                    fromLive = true;
                }
            }
            if (!fromLive) {
                RE::EnchantmentItem* ench = copy.enchant;
                if (!copy.isCopy) {
                auto inv = actor->GetInventory([form](const RE::TESBoundObject& o) { return &o == form; });
                for (auto& [obj, data] : inv) {
                    auto& entry = data.second;
                    if (entry && entry->IsWorn()) { ench = entry->GetEnchantment(); break; }
                    }
                }
                if (!ench)
                    if (auto* ef = form->As<RE::TESEnchantableForm>()) ench = ef->formEnchanting;
                if (ench)
                    for (auto* eff : ench->effects)
                        if (eff && eff->baseEffect)
                            AddLine(item, eff->baseEffect, eff->effectItem.magnitude,
                                    static_cast<float>(eff->effectItem.duration),
                                    (!weap && IsBad(eff->baseEffect, eff->effectItem.magnitude)) ? kDebuff : kBuff);
            }
        }

        if (copy.isCopy) {
            if (auto* magic = form->As<RE::MagicItem>(); magic && !spell)
                for (auto* eff : magic->effects)
                    if (eff && eff->baseEffect)
                        AddLine(item, eff->baseEffect, eff->effectItem.magnitude,
                                static_cast<float>(eff->effectItem.duration),
                                IsBad(eff->baseEffect, eff->effectItem.magnitude) ? kDebuff : kBuff);
        }

        std::vector<TipRow> rows;
        rows.push_back(std::move(item));

        if (bodySlot) {
            static ID3D11ShaderResourceView* s_TexArmorIco = nullptr;
            if (!s_TexArmorIco) s_TexArmorIco = TextureManager::GetPartyIcon("armor");
            for (const auto& rec : records) {
                if (!rec.kye2Armor) continue;
                const char* sn = rec.sourceName;
                if (!sn[0]) continue;
                auto it = std::find_if(rows.begin() + 1, rows.end(), [&](const TipRow& r) { return r.name == sn; });
                if (it == rows.end()) {
                    Tip::TipRow r;
                    r.icon = s_TexArmorIco;
                    r.name = sn;
                    r.tone = 1;
                    rows.push_back(std::move(r));
                    it = rows.end() - 1;
                }
                AddLine(*it, rec.mgef, rec.rawMagnitude, 0.0f, rec.hostile ? kDebuff : kBuff);
            }
            rows.erase(std::remove_if(rows.begin() + 1, rows.end(),
                [](const TipRow& r) { return r.lines.empty(); }), rows.end());
        }

        if (!copy.isCopy && rows.size() == 1 && rows[0].lines.empty()) return;
        std::string title = slotLabel ? slotLabel : "";
        for (char& c : title) c = static_cast<char>(toupper(c));
        DrawCompactTooltip(title, rows, {}, sc);
    }

    void DrawInventoryItemTooltip(RE::Actor* actor, RE::TESForm* form, const char* name,
                                  RE::EnchantmentItem* enchant, const char* typeLabel, void* icon, float sc) {
        ItemTooltipCopy copy;
        copy.name = name;
        copy.enchant = enchant;
        copy.isCopy = true;
        DrawItemTooltip(actor, form, typeLabel, static_cast<ID3D11ShaderResourceView*>(icon), false, sc, copy);
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
            const float kMaxInset = 0.06f;
            const float kSpeed    = 12.0f;
            const bool  hovered   = ImGui::IsMouseHoveringRect(tl, br);
            const float cur = UIAnimation::Follow(std::string("psv2_zoom_") + key, hovered ? 1.0f : 0.0f, kSpeed, kSpeed);
            return cur * kMaxInset;
        };

        struct RolledStat { float shown; ImU32 tint; float sinceOpen; };
        auto RollStat = [&](const char* slotKey, float value, bool settled) -> RolledStat {
            if (!settled) return { value, IM_COL32(255, 255, 255, 220), 0.0f };
            char key[96];
            snprintf(key, sizeof(key), "psv2_stat_%08X_%s", actor ? actor->GetFormID() : 0u, slotKey);
            constexpr float kRollSpeed = 10.0f;
            RolledStat r;
            r.shown     = UIAnimation::Follow(key, value, kRollSpeed, kRollSpeed);
            r.sinceOpen = UIAnimation::SinceShown(key, value);
            r.tint      = IM_COL32(255, 255, 255, 220);
            if (std::fabs(r.shown - value) >= 0.5f)
                r.tint = r.shown < value ? IM_COL32(125, 220, 122, 255) : IM_COL32(232, 103, 106, 255);
            return r;
        };
        auto DrawChangeChip = [&](ImVec2 edge, float sinceOpen) {
            if (std::fabs(sinceOpen) < 0.5f) return;
            char chipBuf[16];
            snprintf(chipBuf, sizeof(chipBuf), "%+.0f", sinceOpen);
            const bool   up  = sinceOpen > 0.0f;
            const ImVec2 csz = ImGui::CalcTextSize(chipBuf);
            const float  cp  = 3.0f * sc;
            const ImVec2 cTL(edge.x + 4.0f * sc, edge.y - csz.y * 0.5f - cp * 0.5f);
            const ImVec2 cBR(cTL.x + csz.x + cp * 2.0f, cTL.y + csz.y + cp);
            dl->AddRectFilled(cTL, cBR, up ? IM_COL32(125, 220, 122, 45) : IM_COL32(232, 103, 106, 45), 8.0f * sc);
            dl->AddText(ImVec2(cTL.x + cp, cTL.y + cp * 0.5f),
                up ? IM_COL32(125, 220, 122, 255) : IM_COL32(232, 103, 106, 255), chipBuf);
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

        const bool anyModalOpen = IsEffectsModalOpen() || IsInventoryModalOpen() || IsStatusModalOpen();

        auto OpenCardOnClick = [&](GearCatalog::Card card, ImVec2 tl, ImVec2 br) {
            if (!ImGui::IsWindowHovered() || !ImGui::IsMouseHoveringRect(tl, br) || !ImGui::IsMouseClicked(0)) return;
            const auto gear = GearCatalog::Get(actor);
            if (gear && !gear->cards[static_cast<std::size_t>(card)].interactive) return;
            OpenInventoryModal(card);
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

        struct EnchBadge { ID3D11ShaderResourceView* tex; float value; std::string type; };

        auto DrawEquipRow = [&](float rowY, RE::TESForm* form, const char* hint,
                                const char* subLabel,
                                float dmgOrArmor,
                                ID3D11ShaderResourceView* statTex,
                                const std::vector<EnchBadge>& enchBadges,
                                float colX = -1.0f,
                                float colW = -1.0f,
                                bool alignLeft = true,
                                const std::vector<std::string>& chips = {},
                                float dmgSeconds = 0.0f,
                                bool statSettled = true) -> float
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

                auto DrawStatBadge = [&](ID3D11ShaderResourceView* iconTex, const char* text, ImU32, BadgeStyle::BadgeColors bgColors,
                                         ImU32 textTint = IM_COL32(255, 255, 255, 220)) -> ImVec2 {
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
                        textTint, text);
                    badgeY += badgeH;
                    return ImVec2(bgBR.x, bgTL.y + badgeH * 0.5f);
                };

                if (dmgOrArmor >= 0.0f) {
                    char slotKey[48];
                    snprintf(slotKey, sizeof(slotKey), "hand_%d_%s", alignLeft ? 0 : 1, subLabel ? subLabel : "");
                    const RolledStat stat = RollStat(slotKey, dmgOrArmor, statSettled);
                    char buf[24];
                    if (dmgSeconds > 1.0f) snprintf(buf, sizeof(buf), "%.0f / %.0fs", stat.shown, dmgSeconds);
                    else                   snprintf(buf, sizeof(buf), "%.0f", stat.shown);
                    const ImVec2 edge = DrawStatBadge(statTex, buf, BadgeStyle::TextColor::Neutral(), BadgeStyle::Theme::NeutralGrey(), stat.tint);
                    DrawChangeChip(edge, stat.sinceOpen);
                }

                for (const auto& [enchantTex, enchantDmg, enchantType] : enchBadges) {
                    if (!enchantTex || enchantDmg <= 0.0f) continue;
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

            if (!chips.empty()) {
                const float cp = 4.0f * sc, cg = 4.0f * sc;
                const float chipH = ImGui::GetTextLineHeight() + 2.0f * sc;
                float cx = colX;
                textY += 2.0f * sc;
                for (const auto& chip : chips) {
                    const ImVec2 tsz = ImGui::CalcTextSize(chip.c_str());
                    const float  w   = tsz.x + cp * 2.0f;
                    if (cx > colX && cx + w > colX + colW) { cx = colX; textY += chipH + cg; }
                    dl->AddRectFilled(ImVec2(cx, textY), ImVec2(cx + w, textY + chipH), IM_COL32(255, 255, 255, 14), 2.0f * sc);
                    dl->AddRect(ImVec2(cx, textY), ImVec2(cx + w, textY + chipH), IM_COL32(255, 255, 255, 30), 2.0f * sc);
                    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(cx + cp, textY + 1.0f * sc), kTheme.textMain, chip.c_str());
                    cx += w + cg;
                }
                textY += chipH + 2.0f * sc;
            }
            ImGui::SetWindowFontScale(1.0f);

            if (form && !anyModalOpen && ImGui::IsWindowHovered()) {
                if (ImGui::IsMouseHoveringRect(ImVec2(colX, rowY), ImVec2(colX + colW, textY))) {
                    if (auto* spellForm = form->As<RE::SpellItem>()) {
                        const auto sd = SpellDamageResolver::ResolveSpellDamage(actor, spellForm);
                        if (sd.HasDamage()) Tip::DrawSpellTooltip(subLabel, spellForm, sd, sc);
                        else if (auto d = Tip::SpellDescriptions(spellForm); !d.empty()) {
                            std::vector<Tip::SumLine> school;
                            if (const auto av = spellForm->GetAssociatedSkill(); av != RE::ActorValue::kNone)
                                school.push_back({ EffectCategories::LocalizedAVName(av), {}, {}, Tip::kTextMuted });
                            Tip::DrawSumTooltip(subLabel, school, sc, d);
                        }
                    } else {
                        const auto wd = form->As<RE::TESObjectWEAP>()
                            ? EquipmentStatResolver::ResolveWeaponDamage(actor, form, !alignLeft)
                            : EquipmentStatResolver::WeaponDamage{};
                        if (wd.valid) {
                            Tip::DrawWeaponTooltip(subLabel, wd, sc);
                        } else {
                DrawItemTooltip(actor, form, subLabel, tex, false, sc);
                        }
                    }
                }
            }

            return textY + rowGap;
        };

        auto DrawSectionTitle = [&](float y, const char* title) -> float {
            ImGui::PushFont(nullptr, ImGuiRenderer::GetHeadingFontSize());
            ImGui::SetWindowFontScale(FontHeading());
            ImVec2 tSz = ImGui::CalcTextSize(title);
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(origin.x, y), kTheme.accent, title);
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopFont();
            float sepY = y + tSz.y + 3.0f * sc;
            HUDDraw::DrawFadeRule(dl, origin.x, origin.x + size.x, sepY, IM_COL32(212,175,55,100));
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
                if (t == "stamina")                               return TextureManager::GetPartyIcon("stamina");
                if (t == "magicka")                               return TextureManager::GetPartyIcon("magicka");
                return nullptr;
            };

            auto WeaponEnchBadges = [&](const EquipmentStatResolver::WeaponDamage& wd) {
                std::vector<EnchBadge> out;
                for (const auto& e : wd.enchants) out.push_back({ EnchantTex(e.type), e.magnitude, e.type });
                return out;
            };

            float rhDmg      = -1.0f;
            std::vector<EnchBadge> rhEnch;
            std::string rhStatType;
            float       rhDmgSeconds = 0.0f;
            const char* rhHint = "weapon";
            if (rh && rh->As<RE::TESObjectWEAP>()) {
                const auto wd = EquipmentStatResolver::ResolveWeaponDamage(actor, rh, false);
                if (wd.valid) rhDmg = wd.physical;
                rhEnch = WeaponEnchBadges(wd);
            } else if (rh && rh->As<RE::SpellItem>()) {
                const auto sd = SpellDamageResolver::ResolveSpellDamage(actor, rh);
                const auto* p = sd.HasDamage() ? sd.Primary() : sd.Drain();
                if (p) {
                    rhDmg = sd.HasDamage() ? sd.Headline() : p->Total(sd.isConcentration);
                    rhStatType = p->type;
                    if (!sd.isConcentration) rhDmgSeconds = p->duration;
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
            bool         lhDmgSettled = true;
            float        lhEnchDmg = 0.0f;
            const char*  lhHint    = "weapon";
            std::string  lhLabelStr = LocalizationManager::Get("CharacterSheet.slot_left_hand");
            const char*  lhLabel    = lhLabelStr.c_str();
            std::string  lhEnchType;
            std::vector<EnchBadge> lhEnch;
            std::string  lhStatType;
            float        lhDmgSeconds = 0.0f;
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
                        const auto wd = EquipmentStatResolver::ResolveWeaponDamage(actor, lw, true);
                        if (wd.valid) lhDmg = wd.physical;
                        lhEnch = WeaponEnchBadges(wd);
                    } else if (lh->As<RE::SpellItem>()) {
                        const auto sd = SpellDamageResolver::ResolveSpellDamage(actor, lh);
                        const auto* p = sd.HasDamage() ? sd.Primary() : sd.Drain();
                        if (p) {
                            lhDmg = sd.HasDamage() ? sd.Headline() : p->Total(sd.isConcentration);
                            lhStatType = p->type;
                            if (!sd.isConcentration) lhDmgSeconds = p->duration;
                        }
                    } else if (auto* shield = lh->As<RE::TESObjectARMO>()) {
                        lhHint = "shield";
                        lhDmg = WornGearStatResolver::ShownArmorValue(actor, shield, &lhDmgSettled);
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
            if (!lhEnchType.empty()) lhEnch.push_back({ EnchantTex(lhEnchType), lhEnchDmg, lhEnchType });
            ID3D11ShaderResourceView* lhStatTex = s_TexWeapon;
            if (lhForm && lhForm->As<RE::TESObjectARMO>()) lhStatTex = s_TexArmor;
            if (auto* t = lhStatType.empty() ? nullptr : EnchantTex(lhStatType)) lhStatTex = t;
            ID3D11ShaderResourceView* rhStatTex = s_TexWeapon;
            if (auto* t = rhStatType.empty() ? nullptr : EnchantTex(rhStatType)) rhStatTex = t;

            auto WeaponChips = [](RE::TESForm* form) {
                std::vector<std::string> chips;
                auto* w = form ? form->As<RE::TESObjectWEAP>() : nullptr;
                if (!w) return chips;
                char buf[48];
                snprintf(buf, sizeof(buf), "%s %.2f", LocalizationManager::Get("Stats.Speed").c_str(), w->GetSpeed());
                chips.emplace_back(buf);
                if (w->IsMelee()) {
                    snprintf(buf, sizeof(buf), "%s %.2f", LocalizationManager::Get("Stats.Reach").c_str(), w->GetReach());
                    chips.emplace_back(buf);
                }
                snprintf(buf, sizeof(buf), "%s %.2f", LocalizationManager::Get("Stats.Stagger").c_str(), w->GetStagger());
                chips.emplace_back(buf);
                return chips;
            };

            std::string rhLabelStr = LocalizationManager::Get("CharacterSheet.slot_right_hand");
            float rhBot = DrawEquipRow(curY, rh,     rhHint, rhLabelStr.c_str(), rhDmg,  rhStatTex,   rhEnch, rhX, colW, true,
                                       WeaponChips(rh), rhDmgSeconds);
            float lhBot = DrawEquipRow(curY, lhForm, lhHint, lhLabel,            lhDmg,  lhStatTex,   lhEnch, lhX, colW, false,
                                       WeaponChips(lhForm), lhDmgSeconds, lhDmgSettled);
            OpenCardOnClick(GearCatalog::Card::RightHand, ImVec2(rhX, curY), ImVec2(rhX + colW, rhBot));
            OpenCardOnClick(GearCatalog::Card::LeftHand,  ImVec2(lhX, curY), ImVec2(lhX + colW, lhBot));
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

                auto DrawStatBadgeCell = [&](ID3D11ShaderResourceView* iconTex, const char* text, ImU32, BadgeStyle::BadgeColors bgColors,
                                             ImU32 textTint = IM_COL32(255, 255, 255, 220)) -> ImVec2 {
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
                        ImVec2(textX, bgTL.y + textOffY), textTint, text);
                    badgeY += badgeH;
                    return ImVec2(bgBR.x, bgTL.y + badgeH * 0.5f);
                };

                if (armorVal >= 0.0f) {
                    char slotKey[48];
                    snprintf(slotKey, sizeof(slotKey), "armor_%s", label ? label : "");
                    const RolledStat stat = RollStat(slotKey, armorVal, slotData.armorValueSettled);
                    char buf[16];
                    snprintf(buf, sizeof(buf), "%.0f", stat.shown);
                    const ImVec2 edge = DrawStatBadgeCell(s_TexArmor, buf, BadgeStyle::TextColor::Neutral(), BadgeStyle::Theme::NeutralGrey(), stat.tint);
                    DrawChangeChip(edge, stat.sinceOpen);
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

            if (arm && !anyModalOpen && ImGui::IsWindowHovered() &&
                ImGui::IsMouseHoveringRect(fTL, ImVec2(fBR.x, textY)))
                DrawItemTooltip(actor, arm, label, tex, std::strcmp(hint, "type_cuirass") == 0, sc);

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
            OpenCardOnClick(static_cast<GearCatalog::Card>(2 + i), ImVec2(cellX, rowStartY), ImVec2(cellX + armorColW, cellBot));
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
        HUDDraw::DrawFadeRule(dl, origin.x + padX, origin.x + size.x - padX, origin.y, V2Theme::AccentA(76));

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
                HUDDraw::DrawRuleV(dl, dvX, centerY - 6.0f * sc, centerY + 6.0f * sc, IM_COL32(255, 255, 255, 25));
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
        const float hintX = origin.x + size.x - padX - hintSz.x;
        dl->AddText(ImVec2(hintX, textY), kTheme.textMuted, hint);

        {
            auto* set = Settings::GetSingleton();
            static std::string s_pauseLabel;
            if (s_pauseLabel.empty()) s_pauseLabel = LocalizationManager::Get("PartySheetV2.PauseGame");
            const float trackH = textH * 0.85f, trackW = trackH * 1.9f;
            const ImVec2 lblSz = ImGui::CalcTextSize(s_pauseLabel.c_str());
            const float  gapX  = 8.0f * sc;
            const float  x1    = hintX - divW * 2.0f;
            const float  trackX = x1 - trackW;
            const float  lblX   = trackX - gapX - lblSz.x;
            const ImVec2 tMin(trackX, centerY - trackH * 0.5f), tMax(x1, centerY + trackH * 0.5f);

            ImGui::SetCursorScreenPos(ImVec2(lblX, centerY - textH * 0.5f));
            if (ImGui::InvisibleButton("##psv2_pause", ImVec2(x1 - lblX, textH)))
                set->psV2PauseGame = !set->psV2PauseGame;
            const bool hov = ImGui::IsItemHovered();
            const bool on  = set->psV2PauseGame;

            dl->AddText(ImVec2(lblX, textY), hov || on ? kTheme.accent : kTheme.textMuted, s_pauseLabel.c_str());
            dl->AddRectFilled(tMin, tMax, on ? V2Theme::AccentA(150) : IM_COL32(255, 255, 255, 30), trackH * 0.5f);
            dl->AddRect(tMin, tMax, V2Theme::AccentA(hov ? 200 : 90), trackH * 0.5f);
            const float r  = trackH * 0.5f - 2.0f * sc;
            const float kx = on ? tMax.x - trackH * 0.5f : tMin.x + trackH * 0.5f;
            dl->AddCircleFilled(ImVec2(kx, centerY), r, on ? IM_COL32(255, 255, 255, 235) : IM_COL32(255, 255, 255, 120));
        }

        ImGui::SetWindowFontScale(1.0f);
        ImGui::SetCursorScreenPos(origin);
        ImGui::Dummy(ImVec2(size.x, size.y));
    }

    static ImU32 LiveBackingColor(RE::Actor* actor) {
        auto* set = Settings::GetSingleton();
        if (!set->psV2LiveClassTint) return kTheme.bgBase;
        static constexpr const char* kLiveClassKeys[11] = {
            "class-player", "class-warrior", "class-ranged", "class-stealth", "class-mage", "class-vampire",
            "class-werewolf", "class-undead", "class-daedra", "class-creatures", "class-common" };
        const std::string_view name = GetClassBgName(actor);
        int idx = 10;
        for (int i = 0; i < 11; ++i)
            if (name == kLiveClassKeys[i]) { idx = i; break; }
        const std::uint32_t argb = set->psV2LiveClassColors[idx];
        return IM_COL32((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF, 255);
    }

    static void DrawLiveOpening(ImDrawList* dl, ImVec2 canvasMin, ImVec2 canvasMax,
                                ImVec2 displaySize, float centerX, ImU32 base) {
        const auto* set = Settings::GetSingleton();
        const float canvasW = canvasMax.x - canvasMin.x, canvasH = canvasMax.y - canvasMin.y;
        const float rx    = canvasW * set->psV2LiveOpenW * 0.005f;
        const float ry    = canvasH * set->psV2LiveOpenH * 0.005f;
        const float cy    = (canvasMin.y + canvasMax.y) * 0.5f;
        const float soft  = set->psV2LiveSoftness * 0.01f;
        const float fadeH = canvasH * set->psV2LiveBottomFade * 0.01f;

        const ImU32 dim = IM_COL32(0, 0, 0, 140);
        dl->AddRectFilled(ImVec2(0, 0), ImVec2(displaySize.x, canvasMin.y), dim);
        dl->AddRectFilled(ImVec2(0, canvasMax.y), displaySize, dim);
        dl->AddRectFilled(ImVec2(0, canvasMin.y), ImVec2(canvasMin.x, canvasMax.y), dim);
        dl->AddRectFilled(ImVec2(canvasMax.x, canvasMin.y), ImVec2(displaySize.x, canvasMax.y), dim);

        constexpr int   kSeg   = 72;
        constexpr int   kInner = 6;
        constexpr int   kFade  = 8;
        constexpr int   kRings = kInner + kFade + 1;
        constexpr float kFar   = 20.0f;
        auto smooth = [](float t) { t = std::clamp(t, 0.0f, 1.0f); return t * t * (3.0f - 2.0f * t); };
        auto bottomA = [&](float y) {
            return fadeH > 0.0f ? smooth((y - (canvasMax.y - fadeH)) / fadeH) : 0.0f;
        };
        const ImU32 rgb   = base & ~IM_COL32_A_MASK;
        const float baseA = static_cast<float>((base >> IM_COL32_A_SHIFT) & 0xFF);
        auto col = [&](float a) {
            return rgb | (static_cast<ImU32>(baseA * std::clamp(a, 0.0f, 1.0f) + 0.5f) << IM_COL32_A_SHIFT);
        };
        auto ringScale = [&](int r) {
            if (r < kInner) return (1.0f - soft) * static_cast<float>(r + 1) / kInner;
            if (r < kInner + kFade) return (1.0f - soft) + soft * static_cast<float>(r - kInner + 1) / kFade;
            return kFar;
        };
        auto ringA = [&](int r) {
            if (r < kInner) return 0.0f;
            if (r < kInner + kFade) return smooth(static_cast<float>(r - kInner + 1) / kFade);
            return 1.0f;
        };

        dl->PushClipRect(canvasMin, canvasMax, true);

        if (set->psV2LiveDim > 0) {
            const ImU32 tint = IM_COL32(0, 0, 0, static_cast<int>(set->psV2LiveDim * 2.55f));
            std::array<ImVec2, kSeg> pts;
            for (int i = 0; i < kSeg; ++i) {
                const float ang = 6.2831853f * i / kSeg;
                pts[i] = ImVec2(centerX + std::cos(ang) * rx, cy + std::sin(ang) * ry);
            }
            dl->AddConvexPolyFilled(pts.data(), kSeg, tint);
        }

        const ImVec2 uv = ImGui::GetFontTexUvWhitePixel();
        const int vtxCount = 1 + kSeg * kRings;
        const int idxCount = kSeg * 3 + kSeg * (kRings - 1) * 6;
        dl->PrimReserve(idxCount, vtxCount);
        const ImDrawIdx v0 = static_cast<ImDrawIdx>(dl->_VtxCurrentIdx);
        dl->PrimWriteVtx(ImVec2(centerX, cy), uv, col(bottomA(cy)));
        for (int r = 0; r < kRings; ++r) {
            const float f = ringScale(r), a = ringA(r);
            for (int i = 0; i < kSeg; ++i) {
                const float ang = 6.2831853f * i / kSeg;
                const ImVec2 p(centerX + std::cos(ang) * rx * f, cy + std::sin(ang) * ry * f);
                dl->PrimWriteVtx(p, uv, col((std::max)(a, bottomA(p.y))));
            }
        }
        auto vi = [&](int r, int i) { return static_cast<ImDrawIdx>(v0 + 1 + r * kSeg + (i % kSeg)); };
        for (int i = 0; i < kSeg; ++i) {
            dl->PrimWriteIdx(v0); dl->PrimWriteIdx(vi(0, i)); dl->PrimWriteIdx(vi(0, i + 1));
        }
        for (int r = 0; r + 1 < kRings; ++r)
            for (int i = 0; i < kSeg; ++i) {
                dl->PrimWriteIdx(vi(r, i));     dl->PrimWriteIdx(vi(r + 1, i));     dl->PrimWriteIdx(vi(r + 1, i + 1));
                dl->PrimWriteIdx(vi(r, i));     dl->PrimWriteIdx(vi(r + 1, i + 1)); dl->PrimWriteIdx(vi(r, i + 1));
            }

        dl->PopClipRect();
    }

    void Render() {
        if (!s_Visible) return;

        if (MenuUtils::ShouldBlockCharacterSheet()) {
            s_Visible = false;
            ApplyPause(false);
            PartySheetV2Camera::Exit();
            CloseEffectsModal();
            CloseInventoryModal();
            CloseStatusModal();
            if (!ImGuiRenderer::IsVisible())
                MenuUtils::EnablePlayerInput();
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
                ApplyPause(Settings::GetSingleton()->psV2PauseGame);
                PartySheetV2Camera::RetryPending();
                const bool live = PartySheetV2Camera::ShowSubject(subject);
                if (live) PartySheetV2Camera::Nudge(ImGui::GetIO().DeltaTime);
                if (live) {
                    const float hx0 = canvasMin.x + (kPad + kSidebarW + kGap + kLeftColW + kGap) * sc0;
                    DrawLiveOpening(ImGui::GetBackgroundDrawList(), canvasMin, canvasMax, displaySize,
                                    hx0 + kCenterW * sc0 * 0.5f, LiveBackingColor(subject));
                } else {
                    ImDrawList* bgDl = ImGui::GetBackgroundDrawList();
                    bgDl->AddRectFilled(ImVec2(0, 0), displaySize, IM_COL32(0, 0, 0, 140));
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

                        const float tiltX = UIAnimation::Follow("psv2_parallax_x", nx, 4.0f, 4.0f);
                        const float tiltY = UIAnimation::Follow("psv2_parallax_y", ny, 4.0f, 4.0f);

                        const float kShift = halfW * 0.012f;
                        const float over   = kShift + 2.0f;

                        float dx = tiltX * kShift;
                        float dy = tiltY * kShift;
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
