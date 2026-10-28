#include "../includes/ImGui/InspectCard.h"
#include "../includes/ImGui/InspectCardKYE.h"
#include "../includes/ImGui/Renderer.h"
#include "../includes/Settings.h"
#include "../includes/PlayerStatus.h"
#include "../includes/TextureManager.h"
#include "../includes/BackgroundManager.h"
#include "../includes/LocalizationManager.h"
#include "../includes/ActorStats.h"
#include "../includes/PerfStats.h"
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <chrono>
#include <RE/Skyrim.h>

namespace InspectCard {
    static bool g_Visible = false;
    static RE::Actor* g_TargetActor = nullptr;

    static void DrawBarValueRight(const char* text) {
        ImVec2 barMin = ImGui::GetItemRectMin();
        ImVec2 barMax = ImGui::GetItemRectMax();
        ImVec2 textSize = ImGui::CalcTextSize(text);
        float padX = 6.0f;
        float tx = barMax.x - textSize.x - padX;
        float ty = barMin.y + ((barMax.y - barMin.y) - textSize.y) * 0.5f;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddText(ImVec2(tx + 1.0f, ty + 1.0f), IM_COL32(0, 0, 0, 200), text);
        dl->AddText(ImVec2(tx, ty), IM_COL32(255, 255, 255, 255), text);
    }
    
    static std::chrono::steady_clock::time_point s_LastDrainTime;
    
    void Init() {
        g_Visible = false;
        g_TargetActor = nullptr;
    }
    
    void Toggle() {
        if (g_Visible) {
            Hide();
        } else if (g_TargetActor) {
            Show(g_TargetActor);
        }
    }
    
    void Show(RE::Actor* actor) {
        if (!actor) return;
        g_TargetActor = actor;
        g_Visible = true;
        
        s_LastDrainTime = std::chrono::steady_clock::now();
        
        logger::debug("InspectCard opened for: {}", actor->GetName());
    }
    
    void Hide() {
        g_Visible = false;
        
        
        logger::debug("InspectCard closed");
    }
    
    bool IsVisible() {
        return g_Visible;
    }
    
    RE::Actor* GetTargetActor() {
        return g_TargetActor;
    }
    
    void SetTargetActor(RE::Actor* actor) {
        g_TargetActor = actor;
    }
    
    static ImVec4 hexToImVec4(uint32_t hex, float alpha = 1.0f) {
        float r = ((hex >> 16) & 0xFF) / 255.0f;
        float g = ((hex >> 8) & 0xFF) / 255.0f;
        float b = (hex & 0xFF) / 255.0f;
        return ImVec4(r, g, b, alpha);
    }
    
    static void KyeBadgeColors(InspectCardKYE::BadgeCategory cat, ImU32& bg, ImU32& txt) {
        using C = InspectCardKYE::BadgeCategory;
        txt = IM_COL32(235, 235, 235, 255);
        switch (cat) {
            case C::Archetype:    bg = IM_COL32(78, 70, 60, 220);  break;
            case C::Trait:        bg = IM_COL32(40, 95, 110, 220); break;
            case C::Intelligence: bg = IM_COL32(120, 90, 30, 220); break;
            case C::Armor:        bg = IM_COL32(70, 66, 58, 220);  break;
            case C::Special:      bg = IM_COL32(150, 105, 25, 230);
                                  txt = IM_COL32(255, 245, 220, 255); break;
            default:              bg = IM_COL32(60, 60, 60, 220);  break;
        }
    }

    static void DrawBadgePills(const std::vector<InspectCardKYE::Badge>& badges, float scale, float wrapWidth) {
        if (badges.empty()) return;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImFont* font = ImGui::GetFont();
        float fontSize = ImGui::GetFontSize();
        const float padX = 7.0f * scale;
        const float padY = 3.0f * scale;
        const float gap  = 5.0f * scale;
        const float rounding = 4.0f * scale;
        ImVec2 startPos = ImGui::GetCursorScreenPos();
        float availMax = startPos.x + wrapWidth;
        float x = startPos.x;
        float y = startPos.y;
        float rowH = fontSize + padY * 2.0f;
        float lineBottom = y + rowH;
        float maxRight = startPos.x;

        for (const auto& b : badges) {
            std::string label = LocalizationManager::Get("KnowYourEnemy." + b.text);
            ImVec2 ts = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label.c_str());
            float pillW = ts.x + padX * 2.0f;
            if (x > startPos.x && x + pillW > availMax) {
                x = startPos.x;
                y += rowH + gap;
                lineBottom = y + rowH;
            }
            ImU32 bg, txt;
            KyeBadgeColors(b.category, bg, txt);
            dl->AddRectFilled(ImVec2(x, y), ImVec2(x + pillW, y + rowH), bg, rounding);
            dl->AddText(font, fontSize, ImVec2(x + padX, y + padY), txt, label.c_str());
            x += pillW + gap;
            if (x - gap > maxRight) maxRight = x - gap;
        }
        ImGui::Dummy(ImVec2(maxRight - startPos.x, lineBottom - startPos.y));
    }

    static float GetPlayerAlteration() {
        auto settings = Settings::GetSingleton();
        if (settings->requireAlteration == 0) {
            return 100.0f;
        }
        
        if (auto player = RE::PlayerCharacter::GetSingleton()) {
            if (auto avo = player->AsActorValueOwner()) {
                return avo->GetActorValue(RE::ActorValue::kAlteration);
            }
        }
        return 0.0f;
    }
    
    // Progressive reveal thresholds based on player Alteration skill
    static constexpr float TIER_BASIC = 0.0f;
    static constexpr float TIER_STATS = 15.0f;
    static constexpr float TIER_RESISTS = 35.0f;
    static constexpr float TIER_SOCIAL = 50.0f;
    
    void RenderContent() {
        if (!g_Visible || !g_TargetActor) return;
        
        if (!g_TargetActor->Is3DLoaded()) {
            return;
        }
        
        auto settings = Settings::GetSingleton();
        const float scale = settings->inspectCardScale;
        auto avo = g_TargetActor->AsActorValueOwner();
        auto targetStats = GetActorStats(g_TargetActor);  if (PerfStats::g_Enabled) ++PerfStats::g_Current.icStatQueries;
        float playerAlt = GetPlayerAlteration();
        
        if (settings->requireAlteration == 1) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration<float>(now - s_LastDrainTime).count();
            s_LastDrainTime = now;
            
            if (auto player = RE::PlayerCharacter::GetSingleton()) {
                if (auto pAvo = player->AsActorValueOwner()) {
                    float drainAmount = settings->magickaDrainPerSecond * elapsed;
                    float currentMagicka = pAvo->GetActorValue(RE::ActorValue::kMagicka);
                    if (currentMagicka > 0) {
                        pAvo->DamageActorValue(RE::ActorValue::kMagicka, drainAmount);
                    }
                }
            }
        }
        
        auto& io = ImGui::GetIO();
        
        auto displaySize = io.DisplaySize;
        float posX, posY;
        if (settings->inspectCardPosX < 0) {
            posX = displaySize.x * 0.5f;
        } else {
            posX = static_cast<float>(settings->inspectCardPosX);
        }
        if (settings->inspectCardPosY < 0) {
            posY = displaySize.y * 0.5f;
        } else {
            posY = static_cast<float>(settings->inspectCardPosY);
        }
        ImVec2 pivot = (settings->inspectCardPosX < 0 && settings->inspectCardPosY < 0) 
                       ? ImVec2(0.5f, 0.5f) : ImVec2(0.0f, 0.0f);
        ImGui::SetNextWindowPos(ImVec2(posX, posY), ImGuiCond_Always, pivot);
        
        ID3D11ShaderResourceView* bgTexture = nullptr;
        if (settings->icUseCustomBackground) {
            bgTexture = BackgroundManager::GetBackground(BackgroundManager::Type::Inspectcard, settings->icBackgroundIndex);
        }
        
        if (bgTexture) {
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
        } else {
            ImVec4 bgColor = hexToImVec4(settings->inspectCardBgColor, settings->inspectCardBgOpacity);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, bgColor);
        }
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f * scale);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(17 * scale, 12 * scale));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
        
        ImGui::Begin("##InspectCard", nullptr, 
                     ImGuiWindowFlags_NoTitleBar | 
                     ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_AlwaysAutoResize);
        
        if (bgTexture) {
            ImVec2 winPos = ImGui::GetWindowPos();
            ImVec2 winSize = ImGui::GetWindowSize();
            auto drawList = ImGui::GetWindowDrawList();
            drawList->PushClipRect(winPos, ImVec2(winPos.x + winSize.x, winPos.y + winSize.y));
            drawList->AddImage(
                reinterpret_cast<ImTextureID>(bgTexture),
                winPos,
                ImVec2(winPos.x + winSize.x, winPos.y + winSize.y)
            );
            drawList->PopClipRect();
        }
        
        const char* rawName = g_TargetActor->GetName();
        std::string nameStr = (rawName && strlen(rawName) > 0) ? rawName : LocalizationManager::Get("Unknown");
        const char* name = nameStr.c_str();
        std::uint16_t level = g_TargetActor->GetLevel();
        
        ImVec4 nameCol = hexToImVec4(settings->inspectCardNameColor);
        ImFont* headingFont = ImGuiRenderer::GetHeadingFont();
        ImGui::SetWindowFontScale(settings->inspectCardFontScale * scale);
        if (headingFont) ImGui::PushFont(headingFont);
        ImGui::TextColored(nameCol, "%s", name);
        if (headingFont) ImGui::PopFont();
        ImGui::SetWindowFontScale(scale * settings->icBodyFontScale);

        if (auto race = g_TargetActor->GetRace()) {
            const char* raceName = race->GetFullName();
            if (raceName) {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), LocalizationManager::Get("LevelFull").c_str(), level, raceName);
            } else {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), LocalizationManager::Get("LevelShort").c_str(), level);
            }
        } else {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), LocalizationManager::Get("LevelShort").c_str(), level);
        }
        
        if (playerAlt >= TIER_SOCIAL) {
            auto player = RE::PlayerCharacter::GetSingleton();
            std::string relationship = LocalizationManager::Get("Acquaintance");
            
            if (player && g_TargetActor->IsHostileToActor(player)) {
                relationship = LocalizationManager::Get("Foe");
            } else {
                auto actorBase = g_TargetActor->GetActorBase();
                auto playerBase = player ? player->GetActorBase() : nullptr;
                
                if (actorBase && playerBase) {
                    auto rel = RE::BGSRelationship::GetRelationship(actorBase, playerBase);
                    if (!rel) {
                        rel = RE::BGSRelationship::GetRelationship(playerBase, actorBase);
                    }
                    
                    if (rel) {
                        using Level = RE::BGSRelationship::RELATIONSHIP_LEVEL;
                        switch (rel->level.get()) {
                            case Level::kLover:
                                relationship = LocalizationManager::Get("Lover");
                                break;
                            case Level::kAlly:
                                relationship = LocalizationManager::Get("Ally");
                                break;
                            case Level::kConfidant:
                                relationship = LocalizationManager::Get("Confidant");
                                break;
                            case Level::kFriend:
                                relationship = LocalizationManager::Get("Friend");
                                break;
                            case Level::kRival:
                                relationship = LocalizationManager::Get("Rival");
                                break;
                            case Level::kFoe:
                                relationship = LocalizationManager::Get("Foe");
                                break;
                            case Level::kEnemy:
                                relationship = LocalizationManager::Get("Enemy");
                                break;
                            case Level::kArchnemesis:
                                relationship = LocalizationManager::Get("Archnemesis");
                                break;
                            case Level::kAcquaintance:
                            default:
                                relationship = LocalizationManager::Get("Acquaintance");
                                break;
                        }
                    }
                }
            }
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 0.6f, 1.0f), "%s", relationship.c_str());
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "???");
        }

        float cardContentWidth = (static_cast<float>(settings->inspectCardBarWidth) +
            static_cast<float>(settings->inspectCardIconSize) * scale +
            ImGui::GetStyle().ItemSpacing.x * scale) * scale;

        if (settings->icShowCreatureTypeBadges && playerAlt >= TIER_STATS) {
            ImGui::SetWindowFontScale(scale * settings->icBodyFontScale);
            auto badges = InspectCardKYE::GetBadges(g_TargetActor);
            if (!badges.empty()) {
                ImGui::Spacing();
                DrawBadgePills(badges, scale, cardContentWidth);
            }
        }

        ImGui::SetWindowFontScale(scale);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        // === STAT BARS (with icons) ===
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
        ImGui::SetWindowFontScale(scale * settings->icBodyFontScale);
        if (avo && playerAlt >= TIER_STATS) {
            float iconSize = static_cast<float>(settings->inspectCardIconSize) * scale;
            float barWidth = (static_cast<float>(settings->inspectCardBarWidth) + iconSize + ImGui::GetStyle().ItemSpacing.x * scale) * scale;
            float barHeight = static_cast<float>(settings->inspectCardBarHeight) * scale;
            
            
            auto hpIcon = TextureManager::GetPartyIcon("health");
            float hp = targetStats.health.current;
            float hpMax = targetStats.health.GetMax();
            float hpPct = targetStats.health.GetPercent();
            
            char hpText[32];
            snprintf(hpText, sizeof(hpText), "%.0f / %.0f", hp, hpMax);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, hexToImVec4(settings->inspectCardHealthColor));
            ImGui::ProgressBar(hpPct, ImVec2(barWidth, barHeight), "");
            DrawBarValueRight(hpText);
            ImGui::PopStyleColor();
            
            if (hpIcon) {
                ImVec2 barMin = ImGui::GetItemRectMin();
                float padding = 5.0f * scale;
                float iconY = barMin.y + (barHeight - iconSize) * 0.5f;
                ImVec2 iconPos(barMin.x + padding, iconY);
                ImGui::GetWindowDrawList()->AddImage((ImTextureID)hpIcon, iconPos, ImVec2(iconPos.x + iconSize, iconPos.y + iconSize));
            }

            if (settings->inspectCardBarBorderWidth > 0) {
                auto drawList = ImGui::GetWindowDrawList();
                ImVec2 barMin = ImGui::GetItemRectMin();
                ImVec2 barMax = ImGui::GetItemRectMax();
                ImU32 borderColor = IM_COL32(
                    (settings->inspectCardBarBorderColor >> 16) & 0xFF,
                    (settings->inspectCardBarBorderColor >> 8) & 0xFF,
                    settings->inspectCardBarBorderColor & 0xFF,
                    255);
                drawList->AddRect(barMin, barMax, borderColor, 0.0f, 0, static_cast<float>(settings->inspectCardBarBorderWidth));
            }
            
            float sp = targetStats.stamina.current;
            float spMax = targetStats.stamina.GetMax();
            if (spMax > 0) {
                auto spIcon = TextureManager::GetPartyIcon("stamina");
                float spPct = targetStats.stamina.GetPercent();
                
                char spText[32];
                snprintf(spText, sizeof(spText), "%.0f / %.0f", sp, spMax);
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, hexToImVec4(settings->inspectCardStaminaColor));
                ImGui::ProgressBar(spPct, ImVec2(barWidth, barHeight), "");
                DrawBarValueRight(spText);
                ImGui::PopStyleColor();
                
                if (spIcon) {
                    ImVec2 barMin = ImGui::GetItemRectMin();
                    float padding = 5.0f * scale;
                    float iconY = barMin.y + (barHeight - iconSize) * 0.5f;
                    ImVec2 iconPos(barMin.x + padding, iconY);
                    ImGui::GetWindowDrawList()->AddImage((ImTextureID)spIcon, iconPos, ImVec2(iconPos.x + iconSize, iconPos.y + iconSize));
                }

                if (settings->inspectCardBarBorderWidth > 0) {
                    auto drawList = ImGui::GetWindowDrawList();
                    ImVec2 barMin = ImGui::GetItemRectMin();
                    ImVec2 barMax = ImGui::GetItemRectMax();
                    ImU32 borderColor = IM_COL32(
                        (settings->inspectCardBarBorderColor >> 16) & 0xFF,
                        (settings->inspectCardBarBorderColor >> 8) & 0xFF,
                        settings->inspectCardBarBorderColor & 0xFF,
                        255);
                    drawList->AddRect(barMin, barMax, borderColor, 0.0f, 0, static_cast<float>(settings->inspectCardBarBorderWidth));
                }
            }
            
            float mp = targetStats.magicka.current;
            float mpMax = targetStats.magicka.GetMax();
            if (mpMax > 0) {
                auto mpIcon = TextureManager::GetPartyIcon("magicka");
                float mpPct = targetStats.magicka.GetPercent();
                
                char mpText[32];
                snprintf(mpText, sizeof(mpText), "%.0f / %.0f", mp, mpMax);
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, hexToImVec4(settings->inspectCardMagickaColor));
                ImGui::ProgressBar(mpPct, ImVec2(barWidth, barHeight), "");
                DrawBarValueRight(mpText);
                ImGui::PopStyleColor();
                
                if (mpIcon) {
                    ImVec2 barMin = ImGui::GetItemRectMin();
                    float padding = 5.0f * scale;
                    float iconY = barMin.y + (barHeight - iconSize) * 0.5f;
                    ImVec2 iconPos(barMin.x + padding, iconY);
                    ImGui::GetWindowDrawList()->AddImage((ImTextureID)mpIcon, iconPos, ImVec2(iconPos.x + iconSize, iconPos.y + iconSize));
                }

                if (settings->inspectCardBarBorderWidth > 0) {
                    auto drawList = ImGui::GetWindowDrawList();
                    ImVec2 barMin = ImGui::GetItemRectMin();
                    ImVec2 barMax = ImGui::GetItemRectMax();
                    ImU32 borderColor = IM_COL32(
                        (settings->inspectCardBarBorderColor >> 16) & 0xFF,
                        (settings->inspectCardBarBorderColor >> 8) & 0xFF,
                        settings->inspectCardBarBorderColor & 0xFF,
                        255);
                    drawList->AddRect(barMin, barMax, borderColor, 0.0f, 0, static_cast<float>(settings->inspectCardBarBorderWidth));
                }
            }
            

        } else if (avo) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s: ???", LocalizationManager::Get("Stats.Health").c_str());
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s: ???", LocalizationManager::Get("Stats.Stamina").c_str());
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s: ???", LocalizationManager::Get("Stats.Magicka").c_str());

        }
        ImGui::PopStyleVar();
        ImGui::SetWindowFontScale(scale);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::SetWindowFontScale(scale * settings->icBodyFontScale);
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.55f), LocalizationManager::Get("Resistances.Title").c_str());

        if (avo) {
            float iconSize = 19.0f * scale;
            float armorRating = targetStats.armorRating;
            std::string armorLabel = LocalizationManager::Get("Stats.Armor");
            auto armorIcon = TextureManager::GetPartyIcon("block");

            char armorText[64];
            if (playerAlt >= TIER_STATS)
                snprintf(armorText, sizeof(armorText), "%s: %.0f", armorLabel.c_str(), armorRating);
            else
                snprintf(armorText, sizeof(armorText), "%s: ???", armorLabel.c_str());

            float textWidth = ImGui::CalcTextSize(armorText).x;
            float totalWidth = (armorIcon ? iconSize + ImGui::GetStyle().ItemSpacing.x : 0.0f) + textWidth;
            float windowWidth = ImGui::GetContentRegionAvail().x;

            ImGui::SameLine(windowWidth - totalWidth);

            if (playerAlt >= TIER_STATS) {
                if (armorIcon) {
                    ImGui::Image((ImTextureID)armorIcon, ImVec2(iconSize, iconSize));
                    ImGui::SameLine();
                }
                ImGui::Text("%s: %.0f", armorLabel.c_str(), armorRating);
            } else {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s: ???", armorLabel.c_str());
            }
        }
        ImGui::SetWindowFontScale(scale);
        
        if (avo && playerAlt >= TIER_RESISTS) {
            float resistIconSize = static_cast<float>(settings->inspectCardResistIconSize) * scale;
            auto drawList = ImGui::GetWindowDrawList();
            
            auto DrawResistWithBorder = [&](float x, float y, ID3D11ShaderResourceView* icon, float value, ImU32 color, const char* label) {
                float circleRadius = resistIconSize * 0.7f;
                ImFont* font = ImGui::GetFont();
                // GetFontSize() already includes the current window scale. Start from the
                float fontSize = font->FontSize * scale * settings->icBodyFontScale;

                int r = (color >> 0) & 0xFF;
                int g = (color >> 8) & 0xFF;
                int b = (color >> 16) & 0xFF;

                char valText[32];
                snprintf(valText, sizeof(valText), "%.0f%%", value);

                float gap = 15.0f * scale;
                ImVec2 labelSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label);
                float labelX = x + (resistIconSize - labelSize.x) / 2.0f;

                drawList->AddText(font, fontSize, ImVec2(labelX, y), color, label);

                float labelIconPadding = 8.0f * scale;
                float iconY = y + fontSize + labelIconPadding;
                ImVec2 center(x + resistIconSize / 2.0f, iconY + resistIconSize / 2.0f);

                drawList->AddCircleFilled(center, circleRadius + 4.0f * scale, IM_COL32(r, g, b, 20), 24);
                drawList->AddCircleFilled(center, circleRadius + 2.0f * scale, IM_COL32(r, g, b, 35), 24);

                drawList->AddCircleFilled(center, circleRadius, IM_COL32(20, 25, 45, 230), 24);

                drawList->AddCircle(center, circleRadius, IM_COL32(r, g, b, 200), 24, 2.0f * scale);

                if (icon) drawList->AddImage((ImTextureID)icon, ImVec2(x, iconY), ImVec2(x + resistIconSize, iconY + resistIconSize));

                float valX = x + resistIconSize + gap;
                float valY = iconY + (resistIconSize - fontSize) / 2.0f;
                drawList->AddText(font, fontSize, ImVec2(valX, valY), IM_COL32(220, 220, 220, 255), valText);
            };
            
            ImVec2 cursorPos = ImGui::GetCursorScreenPos();
            float startX = cursorPos.x + 20.0f * scale;
            float startY = cursorPos.y;
            float barWidth = static_cast<float>(settings->inspectCardBarWidth) * scale;
            float colGap = barWidth * 0.55f;
            float rowGap = 70.0f * scale;
            
            struct ResistData {
                const char* iconName;
                const char* labelKey;
                float value;
                ImU32 color;
            };
            ResistData resistData[] = {
                {"fire", "Resistances.Fire", targetStats.resistFire, IM_COL32(255, 100, 50, 255)},
                {"frost", "Resistances.Frost", targetStats.resistFrost, IM_COL32(80, 180, 255, 255)},
                {"magic", "Resistances.Magic", targetStats.resistMagic, IM_COL32(180, 100, 255, 255)},
                {"shock", "Resistances.Shock", targetStats.resistShock, IM_COL32(255, 230, 80, 255)},
                {"disease", "Resistances.Disease", targetStats.resistDisease, IM_COL32(160, 140, 120, 255)},
                {"poison", "Resistances.Poison", targetStats.resistPoison, IM_COL32(100, 220, 100, 255)},
            };
            
            for (int i = 0; i < 6; i++) {
                int col = i % 2;
                int row = i / 2;
                float x = startX + col * colGap;
                float y = startY + row * rowGap;
                
                auto icon = TextureManager::GetPartyIcon(resistData[i].iconName);
                DrawResistWithBorder(x, y, icon, resistData[i].value, resistData[i].color, 
                    LocalizationManager::Get(resistData[i].labelKey).c_str());
            }
            
            ImGui::Dummy(ImVec2(colGap * 2, rowGap * 3));
        } else {
            float resistIconSize = static_cast<float>(settings->inspectCardResistIconSize) * scale;
            auto drawList = ImGui::GetWindowDrawList();
            ImU32 hiddenColor = IM_COL32(128, 128, 128, 255);
            
            auto DrawHiddenResist = [&](float x, float y, ID3D11ShaderResourceView* icon, const char* label) {
                float circleRadius = resistIconSize * 0.7f;
                ImFont* font = ImGui::GetFont();
                float fontSize = font->FontSize * scale * settings->icBodyFontScale;

                char valText[] = "???";
                float gap = 15.0f * scale;
                ImVec2 labelSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label);
                float labelX = x + (resistIconSize - labelSize.x) / 2.0f;

                drawList->AddText(font, fontSize, ImVec2(labelX, y), hiddenColor, label);

                float iconY = y + fontSize + 8.0f * scale;
                ImVec2 center(x + resistIconSize / 2.0f, iconY + resistIconSize / 2.0f);

                drawList->AddCircleFilled(center, circleRadius, IM_COL32(20, 25, 45, 230), 24);
                drawList->AddCircle(center, circleRadius, hiddenColor, 24, 2.0f * scale);

                if (icon) drawList->AddImage((ImTextureID)icon, ImVec2(x, iconY), ImVec2(x + resistIconSize, iconY + resistIconSize));

                float valX = x + resistIconSize + gap;
                float valY = iconY + (resistIconSize - fontSize) / 2.0f;
                drawList->AddText(font, fontSize, ImVec2(valX, valY), hiddenColor, valText);
            };
            
            ImVec2 cursorPos = ImGui::GetCursorScreenPos();
            float startX = cursorPos.x + 20.0f * scale;
            float startY = cursorPos.y;
            float barWidth = static_cast<float>(settings->inspectCardBarWidth) * scale;
            float colGap = barWidth * 0.55f;
            float rowGap = 70.0f * scale;
            
            static const char* hiddenIcons[] = {"fire", "frost", "magic", "shock", "disease", "poison"};
            static const char* hiddenLabels[] = {"Resistances.Fire", "Resistances.Frost", "Resistances.Magic", "Resistances.Shock", "Resistances.Disease", "Resistances.Poison"};
            
            for (int i = 0; i < 6; i++) {
                int col = i % 2;
                int row = i / 2;
                float x = startX + col * colGap;
                float y = startY + row * rowGap;
                
                auto icon = TextureManager::GetPartyIcon(hiddenIcons[i]);
                DrawHiddenResist(x, y, icon, LocalizationManager::Get(hiddenLabels[i]).c_str());
            }
            
            ImGui::Dummy(ImVec2(colGap * 2, rowGap * 3));
        }

        if (settings->icShowKYEResists && playerAlt >= TIER_RESISTS && InspectCardKYE::IsKYE2Loaded()) {
            auto entries = InspectCardKYE::GetResists(g_TargetActor);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::SetWindowFontScale(scale * settings->icBodyFontScale);

            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImFont* font = ImGui::GetFont();
            float fontSize = ImGui::GetFontSize();
            float rowH = fontSize + 8.0f * scale;
            float iconSz = fontSize;
            float iconGap = 8.0f * scale;
            float colGap = 16.0f * scale;
            ImU32 sepCol = IM_COL32(255, 255, 255, 30);
            ImU32 headCol = IM_COL32(220, 220, 220, 200);

            {
                ImVec2 p = ImGui::GetCursorScreenPos();
                if (auto icon = TextureManager::GetPartyIcon("sword"))
                    dl->AddImage((ImTextureID)icon, ImVec2(p.x, p.y), ImVec2(p.x + iconSz, p.y + iconSz));
                dl->AddText(font, fontSize, ImVec2(p.x + iconSz + iconGap, p.y), headCol, LocalizationManager::Get("KnowYourEnemy.Header").c_str());
                ImGui::Dummy(ImVec2(cardContentWidth, rowH));
                ImVec2 lp = ImGui::GetCursorScreenPos();
                dl->AddLine(ImVec2(lp.x, lp.y), ImVec2(lp.x + cardContentWidth, lp.y), sepCol, 1.0f);
                ImGui::Spacing();
            }

            using Tier = InspectCardKYE::ResistTier;
            auto joinTier = [&](Tier t) {
                std::string s;
                for (const auto& e : entries) {
                    if (e.tier != t) continue;
                    if (!s.empty()) s += ", ";
                    s += LocalizationManager::Get("KnowYourEnemy." + e.label);
                }
                if (!s.empty()) s += ".";
                return s;
            };

            std::string lblWeak   = LocalizationManager::Get("KnowYourEnemy.WeakTo");
            std::string lblRes    = LocalizationManager::Get("KnowYourEnemy.ResistantTo");
            std::string lblHighRes = LocalizationManager::Get("KnowYourEnemy.HighlyResistantTo");

            struct Row { const char* icon; const char* label; ImVec4 col; std::string value; };
            Row rows[] = {
                {"weapon",    lblWeak.c_str(),    ImVec4(1.0f, 0.45f, 0.40f, 1.0f), joinTier(Tier::Weak)},
                {"resistant", lblRes.c_str(),     ImVec4(0.55f, 0.80f, 1.0f, 1.0f), joinTier(Tier::Resistant)},
                {"block",     lblHighRes.c_str(), ImVec4(0.50f, 0.90f, 0.65f, 1.0f), joinTier(Tier::HighlyResistant)},
            };

            float labelColW = 0.0f;
            for (auto& r : rows) {
                float w = iconSz + iconGap + font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, r.label).x;
                if (w > labelColW) labelColW = w;
            }
            float valueColX = labelColW + colGap;
            float valueWrap = cardContentWidth - valueColX;
            if (valueWrap < fontSize) valueWrap = fontSize;

            bool anyRow = false;
            for (auto& r : rows) {
                if (r.value.empty()) continue;
                anyRow = true;

                ImVec2 rowStart = ImGui::GetCursorScreenPos();

                float iconY = rowStart.y + (rowH - iconSz) * 0.5f;
                if (auto icon = TextureManager::GetPartyIcon(r.icon))
                    dl->AddImage((ImTextureID)icon, ImVec2(rowStart.x, iconY), ImVec2(rowStart.x + iconSz, iconY + iconSz));
                float labelY = rowStart.y + (rowH - fontSize) * 0.5f;
                dl->AddText(font, fontSize, ImVec2(rowStart.x + iconSz + iconGap, labelY),
                            ImGui::ColorConvertFloat4ToU32(r.col), r.label);

                ImGui::SetCursorScreenPos(ImVec2(rowStart.x + valueColX, labelY));
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + valueWrap);
                ImGui::TextUnformatted(r.value.c_str());
                ImGui::PopTextWrapPos();

                float usedBottom = ImGui::GetItemRectMax().y + (rowH - fontSize) * 0.5f;
                float rowBottom = (rowStart.y + rowH > usedBottom) ? rowStart.y + rowH : usedBottom;
                ImGui::SetCursorScreenPos(ImVec2(rowStart.x, rowBottom));

                dl->AddLine(ImVec2(rowStart.x, rowBottom), ImVec2(rowStart.x + cardContentWidth, rowBottom), sepCol, 1.0f);
                ImGui::Spacing();
            }

            if (!anyRow) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
                ImGui::TextUnformatted("No notable resistances.");
                ImGui::PopStyleColor();
            }

            ImGui::Dummy(ImVec2(0.0f, 10.0f * scale));

            ImGui::SetWindowFontScale(scale);
        }

        ImGui::End();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    }
    
    void Render() {
        if (!g_Visible || !g_TargetActor) return;

        if (!g_TargetActor->Is3DLoaded()) {
            Hide();
            return;
        }

        if (PlayerStatus::GetTransformState() != PlayerStatus::TransformState::None) {
            Hide();
            return;
        }
        
        PerfStats::BeginICFrame();
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();

        auto& io = ImGui::GetIO();
        io.MouseDrawCursor = false;

        ImGui::NewFrame();

        RenderContent();

        ImGui::EndFrame();
        PerfStats::EndICFrame();
        ImGui::Render();
        ImGuiRenderer::RenderToBoundTarget(ImGui::GetDrawData());
    }
}
