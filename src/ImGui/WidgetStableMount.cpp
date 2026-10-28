#include "../../includes/ImGui/WidgetStableMount.h"
#include "../../includes/PerfStats.h"
#include "../../includes/ImGui/RendererSections.h"
#include "../../includes/Utils/ColorUtils.h"
#include "../../includes/StableMountPresets.h"

#include "../../includes/MenuUtils.h"
#include "../../includes/Settings.h"
#include "../../includes/TextureManager.h"
#include "../../includes/BackgroundManager.h"
#include "../../includes/ImGui/Renderer.h"
#include "../../includes/HorseRegistry.h"
#include "../../includes/Utils/BarRenderer.h"
#include "../../includes/Utils/HUDDraw.h"
#include "../../includes/HUDVisibility.h"

#include <imgui_internal.h>
#include <Windows.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <unordered_map>
#include <vector>

namespace WidgetStableMount {
    namespace {
        std::vector<RE::ActorHandle> g_CachedMounts;
        RE::FormID g_LastRiddenFormID = 0;
        bool g_ForceStableMountPos = true;

        // ── Master scale helpers ────────────────────────────────────────────────
        static float MasterScale()
        {
            const auto* settings = Settings::GetSingleton();
            return settings
                ? std::clamp(static_cast<float>(settings->smScale) / 100.0f, 0.5f, 2.0f)
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

        std::unordered_map<RE::FormID, float> g_PrevHealth;
        std::unordered_map<RE::FormID, float> g_PrevStamina;
        bool g_IHCompatCheckComplete = false;
        bool g_IsImmersiveHorsesLoaded = false;
        bool g_IsImmersiveCompatActive = false;
        RE::TESFaction* g_IHDismissedFaction = nullptr;
        bool g_IHDismissedFactionLookupDone = false;

        bool IsActorValid(RE::Actor* actor)
        {
            if (!actor) {
                return false;
            }

            RE::FormID formID = actor->GetFormID();
            if (formID == 0) {
                return false;
            }

            auto* lookedUp = RE::TESForm::LookupByID<RE::Actor>(formID);
            return lookedUp != nullptr && lookedUp == actor;
        }

        bool IsHorse(RE::Actor* actor)
        {
            auto* base = actor->GetActorBase();
            if (!base) return false;
            auto* race = base->GetRace();
            if (!race) return false;
            const char* eid = race->GetFormEditorID();
            return eid && strcmp(eid, "HorseRace") == 0;
        }

        bool IsPlayerOwned(RE::Actor* actor)
        {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) return false;
            auto* owner = actor->extraList.GetOwner();
            if (!owner) return false;
            if (owner == player->GetActorBase()) return true;
            if (auto* fac = owner->As<RE::TESFaction>())
                return player->IsInFaction(fac);
            return false;
        }

        bool IsStolen(RE::Actor* actor)
        {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) return false;
            auto* owner = actor->extraList.GetOwner();
            return owner && owner == player->GetActorBase();
        }

        bool IsPlayerFollowerHorse(RE::Actor* actor)
        {
            if (actor->IsPlayerTeammate()) return true;
            constexpr RE::FormID kCurrentFollowerFaction = 0x0005A1A4;
            auto* fac = RE::TESForm::LookupByID<RE::TESFaction>(kCurrentFollowerFaction);
            return fac && actor->IsInFaction(fac);
        }

        bool IsIHDismissed(RE::Actor* actor)
        {
            if (!g_IsImmersiveCompatActive) return false;
            if (!g_IHDismissedFactionLookupDone) {
                g_IHDismissedFactionLookupDone = true;
                g_IHDismissedFaction = RE::TESForm::LookupByEditorID<RE::TESFaction>("DismissedFollowerFaction");
            }
            return g_IHDismissedFaction && actor->IsInFaction(g_IHDismissedFaction);
        }

        void RefreshImmersiveHorseCompatState(bool forceLog = false)
        {
            auto* dataHandler = RE::TESDataHandler::GetSingleton();
            if (!dataHandler) return;

            g_IsImmersiveHorsesLoaded =
                (dataHandler->LookupModByName("Immersive Horses.esp") != nullptr) ||
                (dataHandler->LookupModByName("ImmersiveHorses.esp") != nullptr);
            g_IHCompatCheckComplete = true;

            if (forceLog || g_IsImmersiveHorsesLoaded != g_IsImmersiveCompatActive) {
                logger::info("StableMount: ImmersiveHorsesLoaded={}", g_IsImmersiveHorsesLoaded);
            }

            g_IsImmersiveCompatActive = g_IsImmersiveHorsesLoaded;
        }

        std::vector<RE::NiPointer<RE::Actor>> ResolveMounts()
        {
            if (!g_IHCompatCheckComplete) {
                RefreshImmersiveHorseCompatState();
            }

            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                g_CachedMounts.clear();
                return {};
            }

            std::vector<RE::NiPointer<RE::Actor>> result;
            std::vector<RE::FormID> seen;

            auto tryAdd = [&](RE::NiPointer<RE::Actor> actor) {
                if (!actor) return;
                RE::FormID fid = actor->GetFormID();
                if (fid == 0) return;
                if (std::find(seen.begin(), seen.end(), fid) != seen.end()) return;
                seen.push_back(fid);
                result.push_back(std::move(actor));
            };

            RE::NiPointer<RE::Actor> active;
            if (player->GetMount(active) && active) {
                g_LastRiddenFormID = active->GetFormID();
                tryAdd(active);
            }

            for (auto& handle : g_CachedMounts) {
                auto cached = handle.get();
                if (cached && IsActorValid(cached.get()) && cached->Is3DLoaded() && !cached->IsDead(true) && (IsPlayerOwned(cached.get()) || IsPlayerFollowerHorse(cached.get())) && !IsIHDismissed(cached.get())) {
                    tryAdd(cached);
                }
            }

            if (auto* pl = RE::ProcessLists::GetSingleton()) {
                for (auto& handle : pl->highActorHandles) {
                    auto candidate = handle.get();
                    if (!candidate || candidate.get() == player) continue;
                    if (candidate->IsDead(true)) continue;
                    if (!IsHorse(candidate.get())) continue;
                    if (!IsPlayerOwned(candidate.get()) && !IsPlayerFollowerHorse(candidate.get())) continue;
                    if (IsIHDismissed(candidate.get())) continue;
                    tryAdd(candidate);
                }
            }

            for (auto& record : PartyHorses::HorseRegistry::GetSingleton()->GetAll()) {
                if (!record.actorPtr || !record.is3DLoaded) continue;
                auto* horse = record.actorPtr;
                if (horse->IsDead(true)) continue;
                if (!IsPlayerOwned(horse) && !IsPlayerFollowerHorse(horse)) continue;
                if (IsIHDismissed(horse)) continue;
                tryAdd(horse->GetHandle().get());
            }

            g_CachedMounts.clear();
            for (auto& m : result) {
                if (m) {
                    g_CachedMounts.push_back(m->GetHandle());
                    if (!IsStolen(m.get()) && IsPlayerOwned(m.get()))
                        PartyHorses::HorseRegistry::GetSingleton()->Register(m.get());
                }
            }

            return result;
        }

        std::vector<RE::NiPointer<RE::Actor>> FilterMounts(std::vector<RE::NiPointer<RE::Actor>> mounts)
        {
            auto* settings = Settings::GetSingleton();
            if (!settings) return mounts;

            if (settings->smShowLastRiddenOnly && g_LastRiddenFormID != 0) {
                auto it = std::find_if(mounts.begin(), mounts.end(), [](const RE::NiPointer<RE::Actor>& m) {
                    return m && m->GetFormID() == g_LastRiddenFormID;
                });
                if (it != mounts.end()) {
                    mounts = { *it };
                } else {
                    mounts.clear();
                }
            }

            if (settings->smMaxMounts > 0 && static_cast<int>(mounts.size()) > settings->smMaxMounts) {
                mounts.resize(static_cast<size_t>(settings->smMaxMounts));
            }

            return mounts;
        }

        void RenderStableMountWindow(bool allowDrag, const std::vector<RE::NiPointer<RE::Actor>>& mounts)
        {
            auto* settings = Settings::GetSingleton();
            if (!settings || !settings->enableStableMountWidget) {
                return;
            }

            if (!allowDrag && MenuUtils::ShouldBlockOverlay()) {
                return;
            }

            if (!allowDrag && settings->smHideInInteriors) {
                auto* player = RE::PlayerCharacter::GetSingleton();
                if (player) {
                    auto* cell = player->GetParentCell();
                    if (cell && cell->IsInteriorCell()) {
                        return;
                    }
                    auto* worldspace = player->GetWorldspace();
                    constexpr RE::FormID kTamriel = 0x0000003C;
                    if (worldspace && worldspace->GetFormID() != kTamriel) {
                        return;
                    }
                }
            }

            auto registry = PartyHorses::HorseRegistry::GetSingleton();
            auto allRecords = registry->GetAll();

            if (mounts.empty()) {
                return;
            }

            ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
                                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground;
            if (!allowDrag) {
                flags |= ImGuiWindowFlags_NoMove;
            }

            const ImVec2 targetPos(static_cast<float>(settings->smPosX), static_cast<float>(settings->smPosY));
            if (!allowDrag || g_ForceStableMountPos) {
                ImGui::SetNextWindowPos(targetPos, ImGuiCond_Always);
                g_ForceStableMountPos = false;
            } else {
                ImGui::SetNextWindowPos(targetPos, ImGuiCond_FirstUseEver);
            }

            if (!ImGui::Begin("##StableMountWidget", nullptr, flags)) {
                ImGui::End();
                return;
            }

            const float portraitSize = ScalePx(static_cast<float>(settings->smPortraitSize));
            const float barHeight    = ScalePx(static_cast<float>(settings->smBarHeight));
            const float barWidth     = ScalePx(static_cast<float>(settings->smBarWidth));

            for (int i = 0; i < static_cast<int>(mounts.size()); ++i) {
                const auto& mount = mounts[i];
                if (!mount) continue;

                ImDrawList* ribbonDrawList = ImGui::GetWindowDrawList();
                const bool hasRibbon = ((settings->smRibbonBgColor >> 24) & 0xFF) > 0;

                if (settings->smLayoutOrientation == 1 && i > 0) {
                    ImGui::SameLine(0.0f, 0.0f);
                    float hGap = ScalePx(static_cast<float>(settings->smMountSpacing)) + (hasRibbon ? ScalePx(static_cast<float>(settings->smRibbonGap)) : 0.0f);
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + hGap);
                }

                ImGui::PushID(i);
                if (hasRibbon) {
                    ribbonDrawList->ChannelsSplit(2);
                    ribbonDrawList->ChannelsSetCurrent(1);
                }

                ImGui::BeginGroup();

                auto* avo = mount->AsActorValueOwner();
                if (!avo) {
                    ImGui::EndGroup();
                    ImGui::PopID();
                    continue;
                }

                float hpCurrent = avo->GetActorValue(RE::ActorValue::kHealth);
                float hpBase = avo->GetBaseActorValue(RE::ActorValue::kHealth);
                float hpPerm = mount->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kHealth);
                float hpTemp = mount->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth);
                float hpMax = hpBase + hpPerm + hpTemp;
                if (hpMax < hpCurrent) hpMax = hpCurrent;
                if (hpMax < 1.0f) hpMax = 1.0f;
                float hpPct = (std::clamp)(hpCurrent / hpMax, 0.0f, 1.0f);

                float stCurrent = avo->GetActorValue(RE::ActorValue::kStamina);
                float stBase = avo->GetBaseActorValue(RE::ActorValue::kStamina);
                float stPerm = mount->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kStamina);
                float stTemp = mount->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kStamina);
                float stMax = stBase + stPerm + stTemp;
                if (stMax < stCurrent) stMax = stCurrent;
                if (stMax < 1.0f) stMax = 1.0f;
                float stPct = (std::clamp)(stCurrent / stMax, 0.0f, 1.0f);
                bool canShowStamina = settings->smShowStamina && stMax > 0.0f;

                float carryCurrent = mount->GetWeightInContainer();
                if (carryCurrent <= 0.0f) carryCurrent = avo->GetActorValue(RE::ActorValue::kInventoryWeight);
                float carryBase = avo->GetBaseActorValue(RE::ActorValue::kCarryWeight);
                float carryPerm = mount->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kCarryWeight);
                float carryTemp = mount->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kCarryWeight);
                float carryMax = carryBase + carryPerm + carryTemp;
                if (carryMax <= 0.0f) carryMax = avo->GetActorValue(RE::ActorValue::kCarryWeight);
                if (carryMax < carryCurrent) carryMax = carryCurrent;
                bool canShowCarry = settings->smShowCarryWeight && carryMax > 0.0f;
                float carryPct = canShowCarry ? (std::clamp)(carryCurrent / carryMax, 0.0f, 1.0f) : 0.0f;

                if (settings->smPortraitStyle == 2) { // Halo
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    ImFont* font = ImGui::GetFont();
                    const float nameFontSize = ImGui::GetFontSize() * settings->smNameFontScale;
                    std::string mountName;
                    ImVec2 nameSize(0, 0);
                    if (settings->smShowName) {
                        const char* name = mount->GetDisplayFullName();
                        if (!name || !name[0]) name = mount->GetName();
                        mountName = name && name[0] ? name : "Mount";
                        if (settings->smNameFirstWordOnly)
                            mountName = mountName.substr(0, mountName.find(' '));
                        nameSize = font->CalcTextSizeA(nameFontSize, FLT_MAX, 0.0f, mountName.c_str());
                    }

                    const float thickness = settings->smHaloThickness > 0
                        ? ScalePx(static_cast<float>(settings->smHaloThickness))
                        : (std::max)(ScalePx(3.0f), portraitSize * 0.055f);
                    const float radius = portraitSize * 0.5f + thickness * 0.5f + ScalePx(static_cast<float>(settings->smHaloOffset));
                    // Reserve the ring and its shadow so adjacent mounts and ribbons don't overlap.
                    const float ringSize = (radius + thickness * 0.5f + ScalePx(4.0f)) * 2.0f;
                    const float width = (std::max)(ringSize, nameSize.x);
                    const float nameGap = mountName.empty() ? 0.0f : ScalePx(4.0f);
                    const ImVec2 origin = ImGui::GetCursorScreenPos();
                    ImGui::Dummy(ImVec2(width, ringSize + nameGap + nameSize.y));
                    const ImVec2 center(origin.x + width * 0.5f, origin.y + ringSize * 0.5f);
                    const ImVec2 pMin(center.x - portraitSize * 0.5f, center.y - portraitSize * 0.5f);
                    const ImVec2 pMax(center.x + portraitSize * 0.5f, center.y + portraitSize * 0.5f);
                    if (auto* portrait = TextureManager::GetPortrait(mount.get()))
                        drawList->AddImageRounded(reinterpret_cast<ImTextureID>(portrait), pMin, pMax,
                            ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, portraitSize * 0.5f);
                    else
                        drawList->AddCircleFilled(center, portraitSize * 0.5f, IM_COL32(30, 30, 30, 255));
                    if (hpPct < 0.8f) {
                        const float speed = hpPct < 0.25f ? 5.0f : 2.5f;
                        const float alpha = (sinf(static_cast<float>(ImGui::GetTime()) * speed) * 0.5f + 0.5f) * 0.4f + 0.1f;
                        drawList->AddCircleFilled(center, portraitSize * 0.5f, IM_COL32(255, 0, 0, static_cast<int>(alpha * 255)));
                    }
                    const float borderWidth = ScalePx(static_cast<float>(settings->smPortraitBorderWidth));
                    if (borderWidth > 0.0f)
                        drawList->AddCircle(center, portraitSize * 0.5f,
                            ImGui::ColorConvertFloat4ToU32(ColorUtils::ArgbToVec4(settings->smPortraitBorderColor)), 0, borderWidth);

                    // Match the follower Halo: health on top, stamina clockwise, carry below left.
                    const float configuredHealthShare = settings->smHaloHealthShare / 100.0f;
                    const float otherShare = (1.0f - configuredHealthShare) * 0.5f;
                    float healthShare = settings->smShowHealth ? configuredHealthShare : 0.0f;
                    float staminaShare = canShowStamina ? otherShare : 0.0f;
                    float carryShare = canShowCarry ? otherShare : 0.0f;
                    const float totalShare = healthShare + staminaShare + carryShare;
                    if (totalShare > 0.0f) {
                        const HUDDraw::RingSegment segments[] = {
                            {hpPct, healthShare / totalShare, 0xFF000000u | (settings->smHealthColor & 0xFFFFFFu)},
                            {stPct, staminaShare / totalShare, 0xFF000000u | (settings->smStaminaColor & 0xFFFFFFu)},
                            {carryPct, carryShare / totalShare, 0xFF000000u | (settings->smCarryColor & 0xFFFFFFu)}
                        };
                        HUDDraw::ResourceRingStyle ring;
                        ring.radius = radius;
                        ring.thickness = thickness;
                        ring.gapDeg = static_cast<float>(settings->smHaloGap);
                        ring.segments = segments;
                        ring.segmentCount = IM_ARRAYSIZE(segments);
                        HUDDraw::DrawResourceRing(drawList, center, ring);
                    }
                    if (!mountName.empty()) {
                        HUDDraw::TextFx textFx;
                        textFx.font = font;
                        textFx.fontSize = nameFontSize;
                        HUDDraw::DrawTextStyled(drawList,
                            ImVec2(origin.x + (width - nameSize.x) * 0.5f, origin.y + ringSize + nameGap),
                            ImGui::ColorConvertFloat4ToU32(ColorUtils::RgbToVec4(settings->smNameColor)),
                            mountName.c_str(), textFx);
                    }
                } else if (settings->smPortraitStyle == 1) { // Top Style
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    
                    float boxSize = portraitSize;

                    std::string mountNameStr;
                    ImVec2 nameSize(0, 0);
                    if (settings->smShowName) {
                        const char* mountNameRaw = mount->GetDisplayFullName();
                        if (!mountNameRaw || mountNameRaw[0] == '\0') mountNameRaw = mount->GetName();
                        if (!mountNameRaw || mountNameRaw[0] == '\0') mountNameRaw = "Mount";
                        
                        mountNameStr = mountNameRaw;
                        if (settings->smNameFirstWordOnly) {
                            size_t spacePos = mountNameStr.find(' ');
                            if (spacePos != std::string::npos) {
                                mountNameStr = mountNameStr.substr(0, spacePos);
                            }
                        }
                        
                        ImGui::PushFont(ImGui::GetFont());
                        nameSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize() * settings->smNameFontScale, FLT_MAX, 0.0f, mountNameStr.c_str());
                        ImGui::PopFont();
                    }
                    
                    char carryText[64] = "";
                    ImVec2 carrySize(0, 0);
                    if (settings->smShowCarryWeight) {
                        snprintf(carryText, sizeof(carryText), "%.0f / %.0f", carryCurrent, carryMax);
                        ImGui::PushFont(ImGui::GetFont());
                        carrySize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize() * settings->smBarTextFontScale, FLT_MAX, 0.0f, carryText);
                        ImGui::PopFont();
                    }
                    
                    // Box Width
                    float tsBarW = ScalePx(static_cast<float>(settings->tsBarWidth));
                    float reqWidth = boxSize + (tsBarW * 2.0f) + ScalePx(4.0f); // Portrait + 2 sidebars + padding
                    if (nameSize.x > reqWidth) reqWidth = nameSize.x + ScalePx(8.0f);
                    if (carrySize.x > reqWidth) reqWidth = carrySize.x + ScalePx(8.0f);
                    
                    float textHeight = 0.0f;
                    if (settings->smShowName || settings->smShowCarryWeight) {
                         textHeight = ScalePx(static_cast<float>(settings->tsBarHeight));
                    }
                    float textMarginTop = textHeight > 0 ? ScalePx(4.0f) : 0.0f; // Gap between portrait and text block
                    
                    ImVec2 contentP = ImGui::GetCursorScreenPos();
                    ImGui::Dummy(ImVec2(reqWidth, boxSize + textMarginTop + textHeight));
                    
                    ImVec2 p = contentP;
                    p.x += (reqWidth - boxSize) * 0.5f;

                    // Background & Portrait
                    float radius = ScalePx(static_cast<float>(settings->smPortraitBorderRadius));
                    float borderW = static_cast<float>(ScalePxInt0(settings->smPortraitBorderWidth));
                    
                    bool mountLowHealth = hpPct < 0.8f;
                    ImU32 combatBorderColor = 0;
                    if (mountLowHealth) {
                        bool isCritical = hpPct < 0.25f;
                        float t = static_cast<float>(ImGui::GetTime());
                        float pulseSpeed = isCritical ? 5.0f : 2.5f;
                        float alpha = (sinf(t * pulseSpeed) * 0.5f + 0.5f) * 0.4f + 0.1f;
                        combatBorderColor = IM_COL32(255, 0, 0, static_cast<int>(alpha * 255));
                    }

                    drawList->AddRectFilled(p, ImVec2(p.x + boxSize, p.y + boxSize), IM_COL32(20, 20, 20, 255), radius);
                    if (auto* portrait = TextureManager::GetPortrait(mount.get())) {
                        drawList->AddImageRounded(reinterpret_cast<ImTextureID>(portrait), p, ImVec2(p.x + boxSize, p.y + boxSize), ImVec2(0,0), ImVec2(1,1), IM_COL32_WHITE, radius);
                    }
                    if (mountLowHealth) {
                        drawList->AddRectFilled(p, ImVec2(p.x + boxSize, p.y + boxSize), combatBorderColor, radius);
                    }
                    if (borderW > 0.0f) {
                        ImU32 bCol = mountLowHealth ? combatBorderColor :
                            IM_COL32((settings->smPortraitBorderColor >> 16) & 0xFF, (settings->smPortraitBorderColor >> 8) & 0xFF, settings->smPortraitBorderColor & 0xFF, 255);
                        drawList->AddRect(p, ImVec2(p.x + boxSize, p.y + boxSize), bCol, radius, 0, borderW);
                    }

                    ImVec2 textBgTopLeft = ImVec2(contentP.x, p.y + boxSize + textMarginTop);

                    if (settings->smShowName) {
                        ImU32 safeNameCol = IM_COL32((settings->smNameColor >> 16) & 0xFF, (settings->smNameColor >> 8) & 0xFF, settings->smNameColor & 0xFF, 255);
                        float nameYArgs = textBgTopLeft.y + (textHeight * 0.5f) - (nameSize.y * (settings->smShowCarryWeight ? 1.0f : 0.5f));
                        drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize() * settings->smNameFontScale, ImVec2(contentP.x + (reqWidth - nameSize.x) * 0.5f, nameYArgs), safeNameCol, mountNameStr.c_str());
                    }

                    if (settings->smShowCarryWeight) {
                        drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize() * settings->smBarTextFontScale, ImVec2(contentP.x + (reqWidth - carrySize.x) * 0.5f, textBgTopLeft.y + (textHeight * 0.5f) - (settings->smShowName ? 0.0f : (carrySize.y * 0.5f))), IM_COL32(200, 200, 200, 255), carryText);
                    }

                    if (settings->smShowStamina) {
                        ImU32 safeStCol = IM_COL32((settings->smStaminaColor >> 16) & 0xFF, (settings->smStaminaColor >> 8) & 0xFF, settings->smStaminaColor & 0xFF, 255);
                        float thickness = tsBarW;
                        ImVec2 bgMin = ImVec2(p.x - thickness - ScalePx(2.0f), p.y);
                        ImVec2 pMax  = ImVec2(p.x - ScalePx(2.0f), p.y + boxSize);

                        BarRenderer::Style stBar;
                        stBar.width         = thickness;
                        stBar.height        = boxSize;
                        stBar.fillFraction  = stPct;
                        stBar.fillColor     = safeStCol;
                        stBar.verticalFill  = true;
                        if (settings->smBarStyleEnabled) {
                            stBar.bgTex   = BackgroundManager::GetBackground(BackgroundManager::Type::BarStyle, settings->smBarStyleIndex);
                            stBar.fillTex = BackgroundManager::GetBarFillTexture(settings->smBarStyleIndex);
                        }

                        if (!BarRenderer::Draw(drawList, bgMin, stBar)) {
                            float stHeight = boxSize * stPct;
                            ImVec2 pMin = ImVec2(bgMin.x, pMax.y - stHeight);
                            drawList->AddRectFilled(pMin, pMax, safeStCol, ScalePx(2.0f));
                        }
                    }

                    if (settings->smShowHealth) {
                        ImU32 safeHpCol = IM_COL32((settings->smHealthColor >> 16) & 0xFF, (settings->smHealthColor >> 8) & 0xFF, settings->smHealthColor & 0xFF, 255);
                        float thickness = tsBarW;
                        ImVec2 bgMin = ImVec2(p.x + boxSize + ScalePx(2.0f), p.y);
                        ImVec2 pMax  = ImVec2(p.x + boxSize + thickness + ScalePx(2.0f), p.y + boxSize);

                        BarRenderer::Style hpBar;
                        hpBar.width        = thickness;
                        hpBar.height       = boxSize;
                        hpBar.fillFraction = hpPct;
                        hpBar.fillColor    = safeHpCol;
                        hpBar.verticalFill = true;
                        if (settings->smBarStyleEnabled) {
                            hpBar.bgTex   = BackgroundManager::GetBackground(BackgroundManager::Type::BarStyle, settings->smBarStyleIndex);
                            hpBar.fillTex = BackgroundManager::GetBarFillTexture(settings->smBarStyleIndex);
                        }

                        if (!BarRenderer::Draw(drawList, bgMin, hpBar)) {
                            float hpHeight = boxSize * hpPct;
                            ImVec2 pMin = ImVec2(bgMin.x, pMax.y - hpHeight);
                            drawList->AddRectFilled(pMin, pMax, safeHpCol, ScalePx(2.0f));
                        }
                    }
                    
                } else {
                    float actualPortraitSize = portraitSize;

                    bool mountLowHealth = hpPct < 0.8f;
                    ImU32 combatBorderColor = 0;
                    if (mountLowHealth) {
                        bool isCritical = hpPct < 0.25f;
                        float t = static_cast<float>(ImGui::GetTime());
                        float pulseSpeed = isCritical ? 5.0f : 2.5f;
                        float alpha = (sinf(t * pulseSpeed) * 0.5f + 0.5f) * 0.4f + 0.1f;
                        combatBorderColor = IM_COL32(255, 0, 0, static_cast<int>(alpha * 255));
                    }

                    if (auto* portrait = TextureManager::GetPortrait(mount.get())) {
                        ImDrawList* drawList = ImGui::GetWindowDrawList();
                        ImVec2 p = ImGui::GetCursorScreenPos();
                        float radius = ScalePx(static_cast<float>(settings->smPortraitBorderRadius));
                        float borderW = static_cast<float>(ScalePxInt0(settings->smPortraitBorderWidth));
                        ImU32 bCol = mountLowHealth ? combatBorderColor :
                            IM_COL32((settings->smPortraitBorderColor >> 16) & 0xFF, (settings->smPortraitBorderColor >> 8) & 0xFF, settings->smPortraitBorderColor & 0xFF, (settings->smPortraitBorderColor >> 24) & 0xFF);

                        if (settings->smLayoutOrientation == 0) {
                            drawList->AddImageRounded(reinterpret_cast<ImTextureID>(portrait), p, ImVec2(p.x + actualPortraitSize, p.y + actualPortraitSize), ImVec2(0,0), ImVec2(1,1), IM_COL32_WHITE, radius);
                            if (mountLowHealth) drawList->AddRectFilled(p, ImVec2(p.x + actualPortraitSize, p.y + actualPortraitSize), combatBorderColor, radius);
                            if (borderW > 0.0f) drawList->AddRect(p, ImVec2(p.x + actualPortraitSize, p.y + actualPortraitSize), bCol, radius, 0, borderW);
                            ImGui::Dummy(ImVec2(actualPortraitSize, actualPortraitSize));
                            ImGui::SameLine();
                        } else {
                            float indent = (barWidth - actualPortraitSize) * 0.5f;
                            if (indent > 0) p.x += indent;
                            drawList->AddImageRounded(reinterpret_cast<ImTextureID>(portrait), p, ImVec2(p.x + actualPortraitSize, p.y + actualPortraitSize), ImVec2(0,0), ImVec2(1,1), IM_COL32_WHITE, radius);
                            if (mountLowHealth) drawList->AddRectFilled(p, ImVec2(p.x + actualPortraitSize, p.y + actualPortraitSize), combatBorderColor, radius);
                            if (borderW > 0.0f) drawList->AddRect(p, ImVec2(p.x + actualPortraitSize, p.y + actualPortraitSize), bCol, radius, 0, borderW);
                            ImGui::Dummy(ImVec2(barWidth, actualPortraitSize));
                        }
                    }

                    ImGui::BeginGroup();
                    
                    if (settings->smShowName) {
                        const char* mountNameRaw = mount->GetDisplayFullName();
                        if (!mountNameRaw || mountNameRaw[0] == '\0') mountNameRaw = mount->GetName();
                        if (!mountNameRaw || mountNameRaw[0] == '\0') mountNameRaw = "Mount";
                        
                        std::string mountNameStr = mountNameRaw;
                        if (settings->smNameFirstWordOnly) {
                            size_t spacePos = mountNameStr.find(' ');
                            if (spacePos != std::string::npos) {
                                mountNameStr = mountNameStr.substr(0, spacePos);
                            }
                        }
                        
                        ImGui::PushStyleColor(ImGuiCol_Text, ColorUtils::RgbToVec4(settings->smNameColor));
                        ImGui::SetWindowFontScale(settings->smNameFontScale);
                        if (settings->smLayoutOrientation == 1) {
                            float textWidth = ImGui::CalcTextSize(mountNameStr.c_str()).x;
                            float indent = (barWidth - textWidth) * 0.5f;
                            if (indent > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
                        }
                        ImGui::TextUnformatted(mountNameStr.c_str());
                        
                        ImGui::SetWindowFontScale(1.0f);
                        ImGui::PopStyleColor();
                    }

                    ImGui::SetWindowFontScale(settings->smBarTextFontScale);
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, ScalePx(static_cast<float>(settings->smBarSpacing))));

                    ImU32 smTextColor = IM_COL32(
                        (settings->barTextColor >> 16) & 0xFF,
                        (settings->barTextColor >>  8) & 0xFF,
                         settings->barTextColor        & 0xFF, 255);

                    auto drawCardBar = [&](float fraction, ImU32 color, const char* text) {
                        ImVec2 pos = ImGui::GetCursorScreenPos();
                        ImVec2 size = ImVec2(barWidth, barHeight);
                        ImDrawList* dl = ImGui::GetWindowDrawList();

                        BarRenderer::Style bar;
                        bar.width         = barWidth;
                        bar.height        = barHeight;
                        bar.fillFraction  = fraction;
                        bar.fillColor     = color;
                        bar.text          = text;
                        bar.textColor     = smTextColor;
                        bar.textFontScale = settings->smBarTextFontScale;
                        if (settings->smBarStyleEnabled) {
                            bar.bgTex   = BackgroundManager::GetBackground(BackgroundManager::Type::BarStyle, settings->smBarStyleIndex);
                            bar.fillTex = BackgroundManager::GetBarFillTexture(settings->smBarStyleIndex);
                        }

                        if (BarRenderer::Draw(dl, pos, bar)) {
                            ImGui::ItemSize(size);
                            ImGui::ItemAdd(ImRect(pos, ImVec2(pos.x + barWidth, pos.y + barHeight)), ImGui::GetID(text));
                        } else {
                            ImVec4 c = ImGui::ColorConvertU32ToFloat4(color);
                            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, c);
                            ImGui::ProgressBar(fraction, size, text);
                            ImGui::PopStyleColor();
                        }
                    };

                    if (settings->smShowHealth) {
                        char hpText[64];
                        snprintf(hpText, sizeof(hpText), "%.0f / %.0f", hpCurrent, hpMax);
                        ImU32 hpColor = IM_COL32((settings->smHealthColor >> 16) & 0xFF, (settings->smHealthColor >> 8) & 0xFF, settings->smHealthColor & 0xFF, 255);
                        drawCardBar(hpPct, hpColor, hpText);
                    }

                    if (canShowStamina) {
                        char stText[64];
                        snprintf(stText, sizeof(stText), "%.0f / %.0f", stCurrent, stMax);
                        ImU32 stColor = IM_COL32((settings->smStaminaColor >> 16) & 0xFF, (settings->smStaminaColor >> 8) & 0xFF, settings->smStaminaColor & 0xFF, 255);
                        drawCardBar(stPct, stColor, stText);
                    }

                    if (canShowCarry) {
                        char carryText[64];
                        snprintf(carryText, sizeof(carryText), "%.0f / %.0f", carryCurrent, carryMax);
                        ImU32 carryColor = IM_COL32((settings->smCarryColor >> 16) & 0xFF, (settings->smCarryColor >> 8) & 0xFF, settings->smCarryColor & 0xFF, 255);
                        drawCardBar(carryPct, carryColor, carryText);
                    }
                    
                    ImGui::PopStyleVar();
                    ImGui::SetWindowFontScale(1.0f);
                    
                    ImGui::EndGroup();
                }
                ImGui::EndGroup();

                if (hasRibbon) {
                    ImVec2 rMin = ImGui::GetItemRectMin();
                    ImVec2 rMax = ImGui::GetItemRectMax();
                    const float pad = ScalePx(4.0f);
                    rMin.x -= pad; rMin.y -= pad;
                    rMax.x += pad; rMax.y += pad;
                    ribbonDrawList->ChannelsSetCurrent(0);
                    ImU32 ribbonCol = IM_COL32(
                        (settings->smRibbonBgColor >> 16) & 0xFF,
                        (settings->smRibbonBgColor >> 8) & 0xFF,
                        settings->smRibbonBgColor & 0xFF,
                        (settings->smRibbonBgColor >> 24) & 0xFF);
                    ribbonDrawList->AddRectFilled(rMin, rMax, ribbonCol, ScalePx(settings->smRibbonCornerRadius));
                    ribbonDrawList->ChannelsMerge();
                }

                if (settings->smLayoutOrientation == 0 && i < static_cast<int>(mounts.size()) - 1) {
                    float vGap = ScalePx(static_cast<float>(settings->smMountSpacing)) + (hasRibbon ? ScalePx(static_cast<float>(settings->smRibbonGap)) : 0.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + vGap);
                }

                ImGui::PopID();
            }

            if (allowDrag) {
                ImVec2 pos = ImGui::GetWindowPos();
                settings->smPosX = static_cast<int>(pos.x);
                settings->smPosY = static_cast<int>(pos.y);
            }

            ImGui::End();
        }
    }

    void Init()
    {
        PartyHorses::HorseRegistry::GetSingleton()->Load();
        PartyHorses::HorseRegistry::GetSingleton()->Purge();
        g_CachedMounts.clear();
        g_LastRiddenFormID = 0;
        g_ForceStableMountPos = true;
        g_IHCompatCheckComplete = false;
        g_IsImmersiveHorsesLoaded = false;
        g_IsImmersiveCompatActive = false;
        g_IHDismissedFaction = nullptr;
        g_IHDismissedFactionLookupDone = false;
        RefreshImmersiveHorseCompatState(true);
    }

    bool IsImmersiveHorsesActive()
    {
        if (!g_IHCompatCheckComplete) RefreshImmersiveHorseCompatState();
        return g_IsImmersiveCompatActive;
    }

    void RenderSettingsPanel(Settings* settings, const ImVec2& displaySize)
    {
        if (!settings) {
            return;
        }

        ImGui::PushStyleColor(ImGuiCol_Header,        IM_COL32(26, 42, 64, 255));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(36, 58, 84, 255));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  IM_COL32(46, 74, 104, 255));
        bool widgetsOpen = ImGui::CollapsingHeader("Widgets - Mount, Pinned, Collectibles");
        ImGui::PopStyleColor(3);
        if (widgetsOpen) {
            ImGui::Indent(10);
            ImGui::PushStyleColor(ImGuiCol_Header,        IM_COL32(18, 28, 46, 255));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(26, 42, 64, 255));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive,  IM_COL32(36, 58, 84, 255));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
            bool smOpen = ImGui::CollapsingHeader("Stable Mount");
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar();
            if (smOpen) {
                ImGui::Indent(10);
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::Checkbox("Enable Stable Mount", &settings->enableStableMountWidget);
                ImGui::Checkbox("Use HUD Visibility Trigger##smHVTrigger", &settings->hvTriggerStableMount);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Widget is always visible while mounted.\nWhen enabled, also fades in briefly when your horse takes damage or loses stamina while stabled.");
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::TextDisabled("Presets");
                ImGui::Separator();
                {
                    static int presetIndex = 0;
                    if (presetIndex < 0 || presetIndex >= (int)StableMountPresets::kPresets.size())
                        presetIndex = 0;
                    const char* presetNames[] = {
                        StableMountPresets::kPresets[0].name,
                        StableMountPresets::kPresets[1].name,
                        StableMountPresets::kPresets[2].name,
                        StableMountPresets::kPresets[3].name
                    };
                    ImGui::SetNextItemWidth(300.0f);
                    if (ImGui::Combo("##smPreset", &presetIndex, presetNames, (int)StableMountPresets::kPresets.size())) {
                        StableMountPresets::ApplyPreset(*settings, (std::size_t)presetIndex);
                        g_ForceStableMountPos = true;
                        logger::debug("[StableMount] Preset applied: {}, portrait style={}", presetNames[presetIndex], settings->smPortraitStyle);
                    }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Applies immediately for preview.\nClick Save Settings to persist.");
                }
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::TextDisabled("Layout");
                if (ImGui::BeginTable("SMLayout", 2, ImGuiTableFlags_SizingFixedFit)) {
                    ImGui::TableSetupColumn("Label",  ImGuiTableColumnFlags_WidthFixed, 160.0f);
                    ImGui::TableSetupColumn("Slider", ImGuiTableColumnFlags_WidthStretch);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Text("Layout Orientation");
                    ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1);
                    if (ImGui::Combo("##smLO", &settings->smLayoutOrientation, "Vertical\0Horizontal\0")) g_ForceStableMountPos = true;

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Text("Portrait Style");
                    ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1);
                    if (ImGui::Combo("##smPSy", &settings->smPortraitStyle, "Card\0Top Style\0Halo\0")) g_ForceStableMountPos = true;

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Text("X Position");
                    ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1);
                    if (ImGui::SliderInt("##smX", &settings->smPosX, 0, static_cast<int>(displaySize.x))) g_ForceStableMountPos = true;

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Text("Y Position");
                    ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1);
                    if (ImGui::SliderInt("##smY", &settings->smPosY, 0, static_cast<int>(displaySize.y))) g_ForceStableMountPos = true;

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("Tip: Drag the widget while HUD Settings is open.");

                    ImGui::EndTable();
                }
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::TextDisabled("Behaviour");
                if (ImGui::BeginTable("SMBehaviour", 2, ImGuiTableFlags_SizingStretchSame)) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Text("Max Horses");
                    ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1);
                    ImGui::SliderInt("##smMaxM", &settings->smMaxMounts, 0, 10);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Cap the number of mounts displayed.\n0 = no limit.");

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Checkbox("Show Last Ridden Only##smSLR", &settings->smShowLastRiddenOnly);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Show only the horse you most recently mounted.\nClears on new game / reload.");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Checkbox("Allow Summon in Cities##smSAC", &settings->smSummonAllowCities);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Checkbox("Hide in Cities / Dungeons##smHID", &settings->smHideInInteriors);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Hides the widget in dungeons/caves (interior cells)\nand walled cities (Whiterun, Markarth, Solitude, etc.)");

                    ImGui::EndTable();
                }
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::TextDisabled("Visibility & Features");
                if (ImGui::BeginTable("VisFeatures", 2, ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Checkbox("Show Health", &settings->smShowHealth);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Checkbox("Show Name", &settings->smShowName);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Checkbox("Show Stamina", &settings->smShowStamina);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Checkbox("Show Carry Weight", &settings->smShowCarryWeight);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Checkbox("First Name Only", &settings->smNameFirstWordOnly);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Display only the first word of the mount's name.");

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Checkbox("Custom Bar Style", &settings->smBarStyleEnabled);
                    ImGui::TableSetColumnIndex(1);
                    if (settings->smBarStyleEnabled) {
                        ImGuiRenderer::RenderBackgroundPicker("SMBarStylePicker", settings->smBarStyleIndex, BackgroundManager::Type::BarStyle);
                    }

                    ImGui::EndTable();
                }
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::TextDisabled("Dimensions & Position");
                if (ImGui::BeginTable("DimPos", 2, ImGuiTableFlags_SizingFixedFit)) {
                    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 160.0f);
                    ImGui::TableSetupColumn("Slider", ImGuiTableColumnFlags_WidthStretch);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Text("Name Size");
                    ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1);
                    ImGui::SliderFloat("##smNFS", &settings->smNameFontScale, 0.5f, 1.5f, "%.2f");

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Text("Bar Text Size");
                    ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1);
                    ImGui::SliderFloat("##smBTFS", &settings->smBarTextFontScale, 0.5f, 1.5f, "%.2f");

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Text("Portrait Size");
                    ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1); ImGui::SliderInt("##smPS", &settings->smPortraitSize, 32, 160);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Text("Portrait Corner Radius");
                    ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1);
                    if (ImGui::SliderInt("##smPBR", &settings->smPortraitBorderRadius, 0, settings->smPortraitSize / 2)) g_ForceStableMountPos = true;

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Text("Portrait Border Width");
                    ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1);
                    if (ImGui::SliderInt("##smPBW", &settings->smPortraitBorderWidth, 0, 10)) g_ForceStableMountPos = true;

                    if (settings->smPortraitStyle == 1) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0); ImGui::Text("Text Box Height");
                        ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1);
                        if (ImGui::SliderInt("##tsBH", &settings->tsBarHeight, 8, 48)) g_ForceStableMountPos = true;
                        
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0); ImGui::Text("Bar Width");
                        ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1);
                        if (ImGui::SliderInt("##tsBW", &settings->tsBarWidth, 2, 16)) g_ForceStableMountPos = true;
                    } else if (settings->smPortraitStyle == 0) { // Card Style
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0); ImGui::Text("Bar Width");
                        ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1); ImGui::SliderInt("##smBW", &settings->smBarWidth, 80, 400);

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0); ImGui::Text("Bar Height");
                        ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1); ImGui::SliderInt("##smBH", &settings->smBarHeight, 8, 28);
                        
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0); ImGui::Text("Bar Spacing");
                        ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1); ImGui::SliderInt("##smBS", &settings->smBarSpacing, 0, 10);
                    }

                    ImGui::EndTable();
                }
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (settings->smPortraitStyle == 2) {
                    ImGui::TextDisabled("Halo");
                    ImGui::Indent(20);
                    ImGui::SliderInt("Health Share##smHaloHealthShare", &settings->smHaloHealthShare, 50, 90, "%d%%");
                    ImGui::SliderInt("Thickness##smHaloThickness", &settings->smHaloThickness, 0, 12,
                        settings->smHaloThickness == 0 ? "Auto" : "%d px");
                    ImGui::SliderInt("Segment Gap##smHaloGap", &settings->smHaloGap, 0, 10, "%d deg");
                    ImGui::SliderInt("Ring Offset##smHaloOffset", &settings->smHaloOffset, 0, 12, "%d px");
                    ImGui::Unindent(20);
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                }

                ImGui::TextDisabled("Colors");
                if (ImGui::BeginTable("ColorsTable", 5, ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    ImVec4 healthCol = ColorUtils::RgbToVec4(settings->smHealthColor);
                    if (ImGui::ColorEdit3("Health", &healthCol.x, ImGuiColorEditFlags_NoInputs)) {
                        settings->smHealthColor = (static_cast<std::uint32_t>(healthCol.x*255.0f)<<16) | (static_cast<std::uint32_t>(healthCol.y*255.0f)<<8) | static_cast<std::uint32_t>(healthCol.z*255.0f);
                    }

                    ImGui::TableSetColumnIndex(1);
                    ImVec4 staminaCol = ColorUtils::RgbToVec4(settings->smStaminaColor);
                    if (ImGui::ColorEdit3("Stamina", &staminaCol.x, ImGuiColorEditFlags_NoInputs)) {
                        settings->smStaminaColor = (static_cast<std::uint32_t>(staminaCol.x*255.0f)<<16) | (static_cast<std::uint32_t>(staminaCol.y*255.0f)<<8) | static_cast<std::uint32_t>(staminaCol.z*255.0f);
                    }

                    ImGui::TableSetColumnIndex(2);
                    ImVec4 carryCol = ColorUtils::RgbToVec4(settings->smCarryColor);
                    if (ImGui::ColorEdit3("Carry", &carryCol.x, ImGuiColorEditFlags_NoInputs)) {
                        settings->smCarryColor = (static_cast<std::uint32_t>(carryCol.x*255.0f)<<16) | (static_cast<std::uint32_t>(carryCol.y*255.0f)<<8) | static_cast<std::uint32_t>(carryCol.z*255.0f);
                    }
                    
                    ImGui::TableSetColumnIndex(3);
                    ImVec4 nameCol = ColorUtils::RgbToVec4(settings->smNameColor);
                    if (ImGui::ColorEdit3("Name", &nameCol.x, ImGuiColorEditFlags_NoInputs)) {
                        settings->smNameColor = (static_cast<std::uint32_t>(nameCol.x*255.0f)<<16) | (static_cast<std::uint32_t>(nameCol.y*255.0f)<<8) | static_cast<std::uint32_t>(nameCol.z*255.0f);
                    }

                    ImGui::TableSetColumnIndex(4);
                    ImVec4 borderCol = ColorUtils::ArgbToVec4(settings->smPortraitBorderColor);
                    if (ImGui::ColorEdit4("Border", &borderCol.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar)) {
                        settings->smPortraitBorderColor = ColorUtils::Vec4ToArgb(borderCol);
                    }
                    
                    ImGui::EndTable();
                }
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::TextDisabled("Ribbon");
                if (ImGui::BeginTable("RibbonTable", 2, ImGuiTableFlags_SizingFixedFit)) {
                    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                    ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthStretch);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Text("BG Color");
                    ImGui::TableSetColumnIndex(1);
                    ImVec4 ribbonCol = ColorUtils::ArgbToVec4(settings->smRibbonBgColor);
                    if (ImGui::ColorEdit4("##smRibbonCol", &ribbonCol.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar)) {
                        settings->smRibbonBgColor = ColorUtils::Vec4ToArgb(ribbonCol);
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Alpha = 0 hides the ribbon");

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Text("Corner Radius");
                    ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1);
                    ImGui::SliderFloat("##smRibbonCR", &settings->smRibbonCornerRadius, 0.0f, 20.0f, "%.1f");

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Text("Ribbon Gap");
                    ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1);
                    ImGui::SliderInt("##smRibbonGap", &settings->smRibbonGap, 0, 40);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Extra spacing between ribbons");

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Text("Item Spacing");
                    ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1);
                    if (ImGui::SliderInt("##smMS", &settings->smMountSpacing, -50, 50)) g_ForceStableMountPos = true;
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Gap between mount cards");

                    ImGui::EndTable();
                }
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::TextDisabled("Summoning");

                ImGui::Checkbox("Enable Summon Picker Hotkey##smEnableHorsePicker", &settings->smEnableHorsePicker);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Press the Summon Horses hotkey (set under Hotkeys) to open a list\nand summon one chosen horse. Separate from Summon All Horses below.");

                if (ImGui::BeginTable("SummonTable", 2, ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);

                    bool blockedByLocation = false;
                    bool isInterior = false;
                    auto* player = RE::PlayerCharacter::GetSingleton();
                    if (player) {
                        auto* cell = player->GetParentCell();
                        if (cell && cell->IsInteriorCell()) {
                            blockedByLocation = true;
                            isInterior = true;
                        } else if (!settings->smSummonAllowCities) {
                            auto* worldspace = player->GetWorldspace();
                            constexpr RE::FormID kTamriel = 0x0000003C;
                            if (worldspace && worldspace->GetFormID() != kTamriel) {
                                blockedByLocation = true;
                            }
                        }
                    }

                    if (blockedByLocation) ImGui::BeginDisabled();
                    if (ImGui::Button("Summon All Horses", ImVec2(-1, 0))) {
                        PartyHorses::HorseRegistry::GetSingleton()->SummonAllFromRegistry();
                    }
                    if (blockedByLocation) {
                        ImGui::EndDisabled();
                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                            if (isInterior) {
                                ImGui::SetTooltip("Summoning horses inside interiors is not allowed.");
                            } else {
                                ImGui::SetTooltip("Summon is disabled in cities.\nEnable 'Allow Summon in Cities' in Behaviour.");
                            }
                        }
                    } else {
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("Teleports all registered horses to your location.");
                    }

                    ImGui::EndTable();
                }
                ImGui::Unindent(10);
            }
            ImGuiRenderer::SettingsUI::RenderCollectiblesSection(settings);
            ImGuiRenderer::SettingsUI::RenderPinnedSection(settings);
            ImGui::Unindent(10);
        }
    }

    void ForceReposition()
    {
        g_ForceStableMountPos = true;
    }

    void RenderPreview()
    {
        auto mounts = FilterMounts(ResolveMounts());
        RenderStableMountWindow(true, mounts);
    }

    void RenderHUD()
    {
        auto* settings = Settings::GetSingleton();
        if (!settings || !settings->enableStableMountWidget) {
            return;
        }
        if (MenuUtils::ShouldBlockOverlay()) {
            return;
        }
        auto mounts = FilterMounts(ResolveMounts());
        if (PerfStats::g_Enabled) ++PerfStats::g_Current.stableMountResolves;

        if (mounts.empty()) {
            g_PrevHealth.clear();
            g_PrevStamina.clear();
            return;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        RE::NiPointer<RE::Actor> activeMount;
        const bool isMounted = player && player->GetMount(activeMount) && activeMount;

        if (settings->hvEnabled && settings->hvTriggerStableMount) {
            for (auto& ni : mounts) {
                auto* mount = ni.get();
                if (!mount) continue;
                RE::FormID id = mount->GetFormID();
                auto* avo = mount->AsActorValueOwner();
                if (!avo) continue;
                float hp  = avo->GetActorValue(RE::ActorValue::kHealth);
                float sta = avo->GetActorValue(RE::ActorValue::kStamina);
                auto hpIt  = g_PrevHealth.find(id);
                auto staIt = g_PrevStamina.find(id);
                bool hpDrop  = hpIt  != g_PrevHealth.end()  && hp  < hpIt->second;
                bool staDrop = staIt != g_PrevStamina.end() && sta < staIt->second;
                if (hpDrop || staDrop)
                    HUDVisibility::Trigger(HUDVisibility::WidgetID::StableMount);
                g_PrevHealth[id]  = hp;
                g_PrevStamina[id] = sta;
            }

            static bool s_WasMounted = false;
            if (isMounted)
                HUDVisibility::Trigger(HUDVisibility::WidgetID::StableMount);
            else if (s_WasMounted)
                HUDVisibility::Trigger(HUDVisibility::WidgetID::StableMount);
            s_WasMounted = isMounted;

            if (!HUDVisibility::IsVisible(HUDVisibility::WidgetID::StableMount))
                return;
        } else {
            g_PrevHealth.clear();
            g_PrevStamina.clear();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, HUDVisibility::GetAlpha(HUDVisibility::WidgetID::StableMount));

        RenderStableMountWindow(false, mounts);

        ImGui::PopStyleVar();

        ImGui::EndFrame();
        ImGui::Render();
        HUDVisibility::ApplyAlphaToDrawData(HUDVisibility::GetAlpha(HUDVisibility::WidgetID::StableMount));
        ImGuiRenderer::RenderToBoundTarget(ImGui::GetDrawData());
    }
}
