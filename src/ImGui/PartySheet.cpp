#include "../includes/ImGui/PartySheet.h"
#include "../includes/ImGui/FollowerHUD.h"
#include "../includes/ImGui/Renderer.h"
#include "../includes/ImGui/CharacterSheet.h"
#include "../includes/Settings.h"
#include "../includes/TextureManager.h"
#include "../includes/BackgroundManager.h"
#include "../includes/LocalizationManager.h"
#include "../includes/ActorStats.h"
#include "../includes/MenuUtils.h"
#include "../includes/PerfStats.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <RE/Skyrim.h>
#include <vector>
#include <string>

namespace PartySheet {
    static bool g_Visible = false;
    static int g_CurrentPage = 0;
    static constexpr int CARDS_PER_PAGE = 4;
    
    static void DisablePlayerInput() {
        if (auto pc = RE::PlayerControls::GetSingleton()) {
            if (pc->lookHandler) pc->lookHandler->SetInputEventHandlingEnabled(false);
            if (pc->movementHandler) pc->movementHandler->SetInputEventHandlingEnabled(false);
            if (pc->attackBlockHandler) pc->attackBlockHandler->SetInputEventHandlingEnabled(false);
        }
    }
    
    static void EnablePlayerInput() {
        if (auto pc = RE::PlayerControls::GetSingleton()) {
            if (pc->lookHandler) pc->lookHandler->SetInputEventHandlingEnabled(true);
            if (pc->movementHandler) pc->movementHandler->SetInputEventHandlingEnabled(true);
            if (pc->attackBlockHandler) pc->attackBlockHandler->SetInputEventHandlingEnabled(true);
        }
    }
    
    void Init() {
        g_Visible = false;
    }
    
    void Toggle() {
        auto ui = RE::UI::GetSingleton();
        if (ui && !g_Visible) {
            if (ImGuiRenderer::IsVisible()) {
                ImGuiRenderer::Toggle();
                logger::debug("PartySheet: Auto-closed Settings to open PartySheet");
            }
            
            if (Settings::GetSingleton()->enableCharacterSheet && CharacterSheet::GetSingleton()->IsVisible()) {
                CharacterSheet::GetSingleton()->Toggle();
                logger::debug("PartySheet: Auto-closed CharacterSheet to open PartySheet");
            }
            
            if (MenuUtils::ShouldBlockCharacterSheet()) {
                return;
            }
        }
        
        bool wasVisible = g_Visible;
        g_Visible = !g_Visible;
        
        if (g_Visible && !wasVisible) {
            DisablePlayerInput();
            logger::debug("PartySheet: Opened - player input disabled");
        }
        else if (!g_Visible && wasVisible) {
            if (!ImGuiRenderer::IsVisible()) {
                EnablePlayerInput();
                logger::debug("PartySheet: Closed - player input enabled");
            } else {
                logger::debug("PartySheet: Closed but Settings open - input stays disabled");
            }
        }
        
        logger::debug("PartySheet toggled: {}", g_Visible ? "visible" : "hidden");
    }
    
    void Show() {
        if (g_Visible) {
            return;
        }
        
        if (ImGuiRenderer::IsVisible()) {
            ImGuiRenderer::Toggle();
            logger::debug("PartySheet: Auto-closed Settings to open PartySheet");
        }
        
        g_Visible = true;
        DisablePlayerInput();
        logger::debug("PartySheet: Force-opened via Show()");
    }
    
    bool IsVisible() {
        return g_Visible;
    }
    
    void PrevPage() {
        if (g_CurrentPage > 0) {
            g_CurrentPage--;
        }
    }
    
    void NextPage() {
        g_CurrentPage++;
    }
    
    static ImVec4 hexToImVec4(uint32_t hex, float alpha = 1.0f) {
        float r = ((hex >> 16) & 0xFF) / 255.0f;
        float g = ((hex >> 8) & 0xFF) / 255.0f;
        float b = (hex & 0xFF) / 255.0f;
        return ImVec4(r, g, b, alpha);
    }
    
    static ImU32 hexToU32(uint32_t hex, uint8_t alpha = 255) {
        uint8_t r = (hex >> 16) & 0xFF;
        uint8_t g = (hex >> 8) & 0xFF;
        uint8_t b = hex & 0xFF;
        return IM_COL32(r, g, b, alpha);
    }
    
    static std::string GetRelationship(RE::Actor* actor) {
        if (!actor) return LocalizationManager::Get("Unknown");
        
        auto player = RE::PlayerCharacter::GetSingleton();
        if (!player) return LocalizationManager::Get("Unknown");
        
        if (actor->IsHostileToActor(player)) {
            return LocalizationManager::Get("Foe");
        }
        
        auto actorBase = actor->GetActorBase();
        auto playerBase = player->GetActorBase();
        
        if (actorBase && playerBase) {
            auto relationship = RE::BGSRelationship::GetRelationship(actorBase, playerBase);
            if (!relationship) {
                relationship = RE::BGSRelationship::GetRelationship(playerBase, actorBase);
            }
            
            if (relationship) {
                using Level = RE::BGSRelationship::RELATIONSHIP_LEVEL;
                switch (relationship->level.get()) {
                    case Level::kLover:
                        return LocalizationManager::Get("Lover");
                    case Level::kAlly:
                        return LocalizationManager::Get("Ally");
                    case Level::kConfidant:
                        return LocalizationManager::Get("Confidant");
                    case Level::kFriend:
                        return LocalizationManager::Get("Friend");
                    case Level::kRival:
                        return LocalizationManager::Get("Rival");
                    case Level::kFoe:
                        return LocalizationManager::Get("Foe");
                    case Level::kEnemy:
                        return LocalizationManager::Get("Enemy");
                    case Level::kArchnemesis:
                        return LocalizationManager::Get("Archnemesis");
                    case Level::kAcquaintance:
                    default:
                        return LocalizationManager::Get("Acquaintance");
                }
            }
        }
        
        return LocalizationManager::Get("Acquaintance");
    }
    
    static void TextTruncated(const char* text, float maxWidth, ImVec4 color = ImVec4(1,1,1,1)) {
        std::string str(text);
        float textWidth = ImGui::CalcTextSize(text).x;
        
        if (textWidth <= maxWidth) {
            ImGui::TextColored(color, "%s", text);
        } else {
            std::string truncated = str;
            while (truncated.length() > 3 && ImGui::CalcTextSize((truncated + "...").c_str()).x > maxWidth) {
                truncated.pop_back();
            }
            truncated += "...";
            ImGui::TextColored(color, "%s", truncated.c_str());
        }
        
        if (ImGui::IsItemHovered() && textWidth > maxWidth) {
            ImGui::SetTooltip("%s", text);
        }
    }
    
    static void DrawVerticalSectionLabel(const char* text, float startScreenY, float endScreenY, ImU32 color, float fontScale = 1.0f) {
        auto*  drawList  = ImGui::GetWindowDrawList();
        auto*  font      = ImGui::GetFont();
        float  fontSize  = ImGui::GetFontSize() * 0.72f * fontScale;
        float  labelX    = ImGui::GetWindowPos().x + 5.0f;
        int    len       = (int)strlen(text);
        float  charH     = fontSize + 2.0f;
        float  totalTxtH = (float)len * charH;
        float  startY    = startScreenY + ((endScreenY - startScreenY) - totalTxtH) * 0.5f;
        if (startY < startScreenY) startY = startScreenY;
        for (int i = 0; i < len; i++) {
            char ch[2] = {text[i], '\0'};
            drawList->AddText(font, fontSize, ImVec2(labelX, startY + (float)i * charH), color, ch);
        }
    }

    struct EquipSlot {
        std::string iconName;
        std::string itemName;
        std::string tooltipText;
        float stat;
        bool hasItem;
    };
    

    static std::string GetSpellSchoolIcon(RE::SpellItem* spell) {
        if (!spell) return "magic";
        
        auto* effect = spell->GetAVEffect();
        if (!effect) return "magic";
        
        auto school = effect->GetMagickSkill();
        switch (school) {
            case RE::ActorValue::kDestruction:  return "destruction";
            case RE::ActorValue::kConjuration:  return "conjuration";
            case RE::ActorValue::kRestoration:  return "restoration";
            case RE::ActorValue::kIllusion:     return "illusion";
            case RE::ActorValue::kAlteration:   return "alteration";
            default:                            return "magic";
        }
    }
    
    static std::vector<EquipSlot> GetEquippedItems(RE::Actor* actor) {
        std::vector<EquipSlot> items;
        if (!actor) return items;
        
        bool rightHandIs2H = false;
        
        {
            EquipSlot slot = {"weapon", "None", "", 0.0f, false};
            auto rightHand = actor->GetEquippedObject(false);
            if (rightHand) {
                slot.itemName = rightHand->GetName();
                slot.hasItem = true;
                
                if (auto spell = rightHand->As<RE::SpellItem>()) {
                    slot.iconName = GetSpellSchoolIcon(spell);
                } else if (auto weapon = rightHand->As<RE::TESObjectWEAP>()) {
                    slot.stat = weapon->GetAttackDamage();
                    
                    auto weaponType = weapon->GetWeaponType();
                    switch (weaponType) {
                        case RE::WEAPON_TYPE::kTwoHandSword:
                        case RE::WEAPON_TYPE::kTwoHandAxe:
                            slot.iconName = "greatsword";
                            rightHandIs2H = true;
                            break;
                        case RE::WEAPON_TYPE::kBow:
                            slot.iconName = "bow";
                            rightHandIs2H = true;
                            break;
                        case RE::WEAPON_TYPE::kCrossbow:
                            slot.iconName = "crossbow";
                            rightHandIs2H = true;
                            break;
                        case RE::WEAPON_TYPE::kStaff:
                            slot.iconName = "staff";
                            rightHandIs2H = true;
                            break;
                        case RE::WEAPON_TYPE::kOneHandSword:
                        case RE::WEAPON_TYPE::kOneHandAxe:
                        case RE::WEAPON_TYPE::kOneHandMace:
                        default:
                            slot.iconName = "weapon";
                            break;
                        case RE::WEAPON_TYPE::kOneHandDagger:
                            slot.iconName = "dagger";
                            break;
                    }
                }
            }
            items.push_back(slot);
        }
        
        {
            EquipSlot slot = {"empty", "—", "", 0.0f, false};  // Default empty
            
            if (rightHandIs2H) {
                auto rightHand = actor->GetEquippedObject(false);
                if (rightHand) {
                    if (auto weapon = rightHand->As<RE::TESObjectWEAP>()) {
                        auto weaponType = weapon->GetWeaponType();
                        if (weaponType == RE::WEAPON_TYPE::kBow || weaponType == RE::WEAPON_TYPE::kCrossbow) {
                            slot.iconName = "quiver";
                            
                            RE::TESAmmo* ammo     = nullptr;
                            int32_t      ammoCount = 0;
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
                            if (ammo) {
                                std::string arrowsLabel = LocalizationManager::Get("Equipment.Arrows");
                                
                                char countBuf[64];
                                snprintf(countBuf, sizeof(countBuf), 
                                         LocalizationManager::Get("Equipment.ArrowsCount").c_str(), ammoCount);
                                slot.itemName = countBuf;
                                
                                slot.tooltipText = ammo->GetName();
                                slot.hasItem = true;
                            } else {
                                slot.itemName = LocalizationManager::Get("Equipment.Arrows");
                                slot.tooltipText = LocalizationManager::Get("Equipment.Arrows");
                            }
                        } else {
                            slot.iconName = "empty";
                        }
                    }
                }
            } else {
                // 1H weapon or unarmed - check left hand
                auto leftHand = actor->GetEquippedObject(true);
                if (leftHand) {
                    slot.itemName = leftHand->GetName();
                    slot.hasItem = true;
                    
                    if (auto spell = leftHand->As<RE::SpellItem>()) {
                        slot.iconName = GetSpellSchoolIcon(spell);
                    } else if (auto armor = leftHand->As<RE::TESObjectARMO>()) {
                        slot.stat = armor->GetArmorRating();
                        slot.iconName = "shield";
                    } else if (auto weapon = leftHand->As<RE::TESObjectWEAP>()) {
                        slot.stat = weapon->GetAttackDamage();
                        slot.iconName = "weapon";
                    }
                } else {
                    slot.iconName = "empty";
                    slot.itemName = "—";
                }
            }
            items.push_back(slot);
        }
        
        static const std::pair<RE::BIPED_MODEL::BipedObjectSlot, const char*> armorSlots[] = {
            {RE::BIPED_MODEL::BipedObjectSlot::kHead, "helmet"},
            {RE::BIPED_MODEL::BipedObjectSlot::kBody, "armor"},
            {RE::BIPED_MODEL::BipedObjectSlot::kHands, "gauntlets"},
            {RE::BIPED_MODEL::BipedObjectSlot::kFeet, "boots"},
            {RE::BIPED_MODEL::BipedObjectSlot::kAmulet, "amulet"},
            {RE::BIPED_MODEL::BipedObjectSlot::kRing, "ring"},
        };
        
        for (const auto& [bipedSlot, iconName] : armorSlots) {
            EquipSlot slot = {iconName, "None", "", 0.0f, false};
            
            auto armor = actor->GetWornArmor(bipedSlot);
            if (!armor && bipedSlot == RE::BIPED_MODEL::BipedObjectSlot::kHead) {
                armor = actor->GetWornArmor(RE::BIPED_MODEL::BipedObjectSlot::kHair);
                if (!armor)
                    armor = actor->GetWornArmor(RE::BIPED_MODEL::BipedObjectSlot::kCirclet);
            }
            if (armor) {
                slot.itemName = armor->GetName();
                slot.stat = armor->GetArmorRating();
                slot.hasItem = true;
            }
            items.push_back(slot);
        }
        
        return items;
    }
    
    void Render() {
        if (!g_Visible) return;
        
        if (MenuUtils::ShouldBlockCharacterSheet()) {
            g_Visible = false;
            if (!ImGuiRenderer::IsVisible()) {
                EnablePlayerInput();
            }
            logger::debug("PartySheet: Auto-closed (incompatible menu or Photo Mode)");
            return;
        }
        
        auto settings = Settings::GetSingleton();
        PerfStats::BeginPSFrame();

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
        
        ImGui::NewFrame();
        
        auto allies = FollowerHUD::GetAllies();
        
        auto displaySize = io.DisplaySize;
        if (settings->partySheetPosX >= 0 && settings->partySheetPosY >= 0) {
            ImGui::SetNextWindowPos(ImVec2(static_cast<float>(settings->partySheetPosX), 
                                           static_cast<float>(settings->partySheetPosY)), ImGuiCond_Appearing);
        } else {
            ImGui::SetNextWindowPos(ImVec2(displaySize.x * 0.5f, displaySize.y * 0.5f), 
                                    ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        }
        ImGui::SetNextWindowSize(ImVec2(static_cast<float>(settings->partySheetWidth), 
                                        static_cast<float>(settings->partySheetHeight)), ImGuiCond_Appearing);
        
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
        
        char windowTitle[128];
        snprintf(windowTitle, sizeof(windowTitle), "%s (%zu)",
                 LocalizationManager::Get("PartyMembers").c_str(),
                 static_cast<size_t>(allies.size()));
        
        ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, ImVec4(0, 0, 0, 0));
        
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 0));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));

        ImGui::Begin(windowTitle, &g_Visible, ImGuiWindowFlags_NoCollapse);
        
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        
        ImGui::PopStyleColor();
        
        ImGui::PopStyleColor(3);

        ImGui::PopStyleColor();
        
        // Define Padding Constants (Used for both Content and Title Alignment)
        constexpr float PAD_TOP = 15.0f;
        constexpr float PAD_LEFT = 10.0f;
        
        ImVec2 windowPos = ImGui::GetWindowPos();
        ImVec2 windowSize = ImGui::GetWindowSize();
        {
            
            ImGui::GetWindowDrawList()->PushClipRect(windowPos, ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y), false);
            
            bool backgroundDrawn = false;
            
            if (settings->psUseCustomBackground) {
                ID3D11ShaderResourceView* windowBgTexture = BackgroundManager::GetBackground(BackgroundManager::Type::Partysheet, settings->psBackgroundIndex);
                if (windowBgTexture) {
                    ImGui::GetWindowDrawList()->AddImage(
                        reinterpret_cast<ImTextureID>(windowBgTexture),
                        windowPos,
                        ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y)
                    );
                    backgroundDrawn = true;
                }
            }
            
            if (!backgroundDrawn) {
                ImU32 bgColor = hexToU32(settings->partySheetBgColor, static_cast<uint8_t>(settings->partySheetBgOpacity * 255));
                ImGui::GetWindowDrawList()->AddRectFilled(
                    windowPos, 
                    ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y), 
                    bgColor
                );
            }
            
            // Redraw Title Bar Text (as background covers it)
            const float titlePadTop = ImGui::GetStyle().FramePadding.y;
            const float titlePadLeft = ImGui::GetStyle().WindowPadding.x + PAD_LEFT;
            
            ImGui::GetWindowDrawList()->AddText(
                ImGui::GetFont(), 
                ImGui::GetFontSize(), 
                ImVec2(windowPos.x + titlePadLeft + 10.0f, windowPos.y + titlePadTop + 15.0f), 
                ImGui::GetColorU32(ImGuiCol_Text), 
                windowTitle
            );

            ImGui::GetWindowDrawList()->PopClipRect();
        }

        {
            constexpr float btnSize = 18.0f;
            ImVec2 btnMin = { windowPos.x + windowSize.x - btnSize - 6.0f, windowPos.y + 6.0f };
            ImVec2 btnMax = { btnMin.x + btnSize, btnMin.y + btnSize };

            ImGui::SetCursorScreenPos(btnMin);
            bool closeClicked = ImGui::InvisibleButton("##psclose", ImVec2(btnSize, btnSize));
            bool closeHovered = ImGui::IsItemHovered();

            if (closeClicked) g_Visible = false;

            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->PushClipRect(windowPos, ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y), false);

            ImU32 bgCol = closeHovered ? IM_COL32(200, 55, 55, 255) : IM_COL32(120, 35, 35, 255);
            ImU32 borderCol = closeHovered ? IM_COL32(220, 80, 80, 255) : IM_COL32(160, 60, 60, 180);
            dl->AddRectFilled(btnMin, btnMax, bgCol, 4.0f);
            dl->AddRect(btnMin, btnMax, borderCol, 4.0f, 0, 1.0f);
            constexpr float p = 5.0f;
            ImU32 lineCol = IM_COL32(220, 220, 220, closeHovered ? 255 : 200);
            dl->AddLine({ btnMin.x + p, btnMin.y + p }, { btnMax.x - p, btnMax.y - p }, lineCol, 1.5f);
            dl->AddLine({ btnMax.x - p, btnMin.y + p }, { btnMin.x + p, btnMax.y - p }, lineCol, 1.5f);

            dl->PopClipRect();
        }

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + PAD_TOP);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + PAD_LEFT);
        
        if (allies.empty()) {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), LocalizationManager::Get("NoParty").c_str());
        } else {
            
            float portraitSize = static_cast<float>(settings->psPortraitSize);
            float barHeight = static_cast<float>(settings->psBarHeight);
            float iconSize = static_cast<float>(settings->psStatIconSize);
            float iconBgSize = static_cast<float>(settings->psStatIconBgSize);
            float padding = 26.0f;
            
            float fontScale = static_cast<float>(settings->fontSize) / 18.0f;
            float labelHeight = 16.0f * fontScale;
            
            float barWidth = static_cast<float>(settings->psBarWidth);
            float cardWidth = padding * 2 + portraitSize + 10.0f + barWidth;
            
            float bottomPad = static_cast<float>(settings->psBottomPadding);
            float cardHeight = padding + 12.0f + bottomPad
                + portraitSize + 8.0f
                + barHeight * 3 + 12.0f
                + labelHeight + (iconSize + 6.0f) * 4 + 8.0f
                + labelHeight + iconBgSize * 2 + 12.0f
                + labelHeight + (iconSize + 6.0f) * 2 + 8.0f
                + labelHeight + (iconBgSize + 8.0f) * 3;
            
            int cardsPerRow = settings->psCardsPerPage;
            
            int totalPages = (static_cast<int>(allies.size()) + settings->psCardsPerPage - 1) / settings->psCardsPerPage;
            if (g_CurrentPage >= totalPages) g_CurrentPage = totalPages - 1;
            if (g_CurrentPage < 0) g_CurrentPage = 0;
            
            int startIdx = g_CurrentPage * settings->psCardsPerPage;
            int endIdx = std::min(startIdx + settings->psCardsPerPage, static_cast<int>(allies.size()));
            
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + PAD_LEFT);

            for (int i = startIdx; i < endIdx; i++) {
                auto actor = allies[i];
                
                if (!actor || !FollowerHUD::IsActorValidForRender(actor)) {
                    continue;
                }
                
                auto avo = actor->AsActorValueOwner();
                
                if ((i - startIdx) > 0 && (i - startIdx) % cardsPerRow == 0) {
                }
                if ((i - startIdx) % cardsPerRow != 0) {
                    ImGui::SameLine();
                } else if (i != startIdx) {
                    ImGui::SetCursorPosX(ImGui::GetStyle().WindowPadding.x + PAD_LEFT);
                }
                
                ImGui::BeginGroup();
                
                char cardId[32];
                snprintf(cardId, sizeof(cardId), "##Card%zu", i);
                
                ID3D11ShaderResourceView* frameTexture = nullptr;
                if (settings->pcUseCustomBackground) {
                    frameTexture = BackgroundManager::GetBackground(BackgroundManager::Type::Partycard, settings->pcBackgroundIndex);
                }
                
                if (frameTexture) {
                    // Custom frame found and enabled - stretch to fit dynamic card size
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
                    ImGui::BeginChild(cardId, ImVec2(cardWidth, cardHeight), false, ImGuiWindowFlags_NoScrollbar);
                    
                    ImVec2 childPos = ImGui::GetWindowPos();
                    ImGui::GetWindowDrawList()->AddImage(
                        reinterpret_cast<ImTextureID>(frameTexture),
                        childPos,
                        ImVec2(childPos.x + cardWidth, childPos.y + cardHeight)
                    );
                } else {
                    float cr = ((settings->partyCardBgColor >> 16) & 0xFF) / 255.0f;
                    float cg = ((settings->partyCardBgColor >> 8) & 0xFF) / 255.0f;
                    float cb = (settings->partyCardBgColor & 0xFF) / 255.0f;
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(cr, cg, cb, settings->partyCardBgOpacity));
                    ImGui::BeginChild(cardId, ImVec2(cardWidth, cardHeight), false, ImGuiWindowFlags_NoScrollbar);
                }
                
                float padding = 26.0f;
                float contentWidth = cardWidth - (padding * 2);
                ImGui::SetCursorPos(ImVec2(padding, padding));
                ImGui::BeginGroup();
                
                auto portrait = TextureManager::GetPortrait(actor);
                float psPortrait = static_cast<float>(settings->psPortraitSize);
                if (portrait) {
                    ImVec2 portraitPos = ImGui::GetCursorScreenPos();
                    ImVec2 portraitSize = ImVec2(psPortrait, psPortrait);
                    
                    ImGui::Image(reinterpret_cast<ImTextureID>(portrait), portraitSize);
                    
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    ImVec2 pMin = portraitPos;
                    ImVec2 pMax = ImVec2(portraitPos.x + portraitSize.x, portraitPos.y + portraitSize.y);
                    
                    drawList->AddRect(pMin, pMax, hexToU32(settings->psPortraitOuterBorderColor, 255), 0.0f, 0, 2.0f);
                    drawList->AddRect(
                        ImVec2(pMin.x + 2, pMin.y + 2), 
                        ImVec2(pMax.x - 2, pMax.y - 2), 
                        hexToU32(settings->psPortraitInnerBorderColor, 200), 0.0f, 0, 1.0f);
                    
                    ImGui::SameLine();
                }
                
                const char* actorName = actor->GetDisplayFullName();
                if (!actorName || strlen(actorName) == 0) {
                     actorName = actor->GetName();
                }
                std::string nameStr = (actorName && strlen(actorName) > 0) ? actorName : LocalizationManager::Get("Unknown");
                std::uint16_t level = actor->GetLevel();
                
                ImGui::BeginGroup();
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));
                
                float nr = ((settings->partySheetNameColor >> 16) & 0xFF) / 255.0f;
                float ng = ((settings->partySheetNameColor >> 8) & 0xFF) / 255.0f;
                float nb = (settings->partySheetNameColor & 0xFF) / 255.0f;
                
                ImGui::SetWindowFontScale(settings->partySheetNameFontScale);
                ImFont* headingFont = ImGuiRenderer::GetHeadingFont();
                if (headingFont) ImGui::PushFont(headingFont);
                TextTruncated(nameStr.c_str(), cardWidth - 90, ImVec4(nr, ng, nb, 1.0f));
                if (headingFont) ImGui::PopFont();
                ImGui::SetWindowFontScale(1.0f);

                ImGui::SetWindowFontScale(settings->psBodyFontScale);
                if (auto race = actor->GetRace()) {
                    const char* raceName = race->GetFullName();
                    if (raceName) {
                        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), LocalizationManager::Get("LevelFull").c_str(), level, raceName);
                    } else {
                        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), LocalizationManager::Get("LevelShort").c_str(), level);
                    }
                } else {
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), LocalizationManager::Get("LevelShort").c_str(), level);
                }
                
                std::string relationship = GetRelationship(actor);
                ImGui::TextColored(ImVec4(0.6f, 0.8f, 0.6f, 1.0f), "%s", relationship.c_str());
                
                if (auto npc = actor->GetActorBase()) {
                    if (auto npcClass = npc->npcClass) {
                        const char* className = npcClass->GetFullName();
                        if (className && strlen(className) > 0) {
                            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.9f, 1.0f), "%s", className);
                        }
                    }
                }
                
                ImGui::SetWindowFontScale(1.0f);
                ImGui::PopStyleVar();
                ImGui::EndGroup();

                auto actorStats = GetActorStats(actor); if (PerfStats::g_Enabled) ++PerfStats::g_Current.psActorStatQueries;
                if (avo) {
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    float barHeight = static_cast<float>(settings->psBarHeight);
                    float halfWidth = (contentWidth - 4.0f) / 2.0f;
                    
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));
                    ImGui::SetWindowFontScale(settings->psBodyFontScale);

                    float hp = actorStats.health.current;
                    float hpMax = actorStats.health.GetMax();
                    float hpPct = actorStats.health.GetPercent();
                    
                    char hpText[32];
                    snprintf(hpText, sizeof(hpText), "%.0f / %.0f", hp, hpMax);
                    
                    ImVec2 hpPos = ImGui::GetCursorScreenPos();
                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, hexToImVec4(settings->partySheetHealthColor));
                    ImGui::ProgressBar(hpPct, ImVec2(contentWidth, barHeight), hpText);
                    ImGui::PopStyleColor();
                    drawList->AddRect(hpPos, ImVec2(hpPos.x + contentWidth, hpPos.y + barHeight), hexToU32(settings->psBarOuterBorderColor, 255), 0.0f, 0, 2.0f);
                    drawList->AddRect(ImVec2(hpPos.x + 1, hpPos.y + 1), ImVec2(hpPos.x + contentWidth - 1, hpPos.y + barHeight - 1), hexToU32(settings->psBarInnerBorderColor, 150), 0.0f, 0, 1.0f);
                    
                    float sp = actorStats.stamina.current;
                    float spMax = actorStats.stamina.GetMax();
                    float mp = actorStats.magicka.current;
                    float mpMax = actorStats.magicka.GetMax();
                    
                    bool hasStamina = spMax > 0;
                    bool hasMagicka = mpMax > 0;
                    
                    if (hasStamina && hasMagicka) {
                        float spPct = sp / spMax;
                        char spText[32];
                        snprintf(spText, sizeof(spText), "%.0f", sp);
                        
                        ImVec2 spPos = ImGui::GetCursorScreenPos();
                        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, hexToImVec4(settings->partySheetStaminaColor));
                        ImGui::ProgressBar(spPct, ImVec2(halfWidth, barHeight), spText);
                        ImGui::PopStyleColor();
                        drawList->AddRect(spPos, ImVec2(spPos.x + halfWidth, spPos.y + barHeight), hexToU32(settings->psBarOuterBorderColor, 255), 0.0f, 0, 2.0f);
                        drawList->AddRect(ImVec2(spPos.x + 1, spPos.y + 1), ImVec2(spPos.x + halfWidth - 1, spPos.y + barHeight - 1), hexToU32(settings->psBarInnerBorderColor, 150), 0.0f, 0, 1.0f);
                        
                        ImGui::SameLine(0, 4.0f);
                        
                        float mpPct = mp / mpMax;
                        char mpText[32];
                        snprintf(mpText, sizeof(mpText), "%.0f", mp);
                        
                        ImVec2 mpPos = ImGui::GetCursorScreenPos();
                        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, hexToImVec4(settings->partySheetMagickaColor));
                        ImGui::ProgressBar(mpPct, ImVec2(halfWidth, barHeight), mpText);
                        ImGui::PopStyleColor();
                        drawList->AddRect(mpPos, ImVec2(mpPos.x + halfWidth, mpPos.y + barHeight), hexToU32(settings->psBarOuterBorderColor, 255), 0.0f, 0, 2.0f);
                        drawList->AddRect(ImVec2(mpPos.x + 1, mpPos.y + 1), ImVec2(mpPos.x + halfWidth - 1, mpPos.y + barHeight - 1), hexToU32(settings->psBarInnerBorderColor, 150), 0.0f, 0, 1.0f);
                    } else if (hasStamina) {
                        float spPct = sp / spMax;
                        char spText[32];
                        snprintf(spText, sizeof(spText), "%.0f / %.0f", sp, spMax);
                        
                        ImVec2 spPos = ImGui::GetCursorScreenPos();
                        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, hexToImVec4(settings->partySheetStaminaColor));
                        ImGui::ProgressBar(spPct, ImVec2(contentWidth, barHeight), spText);
                        ImGui::PopStyleColor();
                        drawList->AddRect(spPos, ImVec2(spPos.x + contentWidth, spPos.y + barHeight), hexToU32(settings->psBarOuterBorderColor, 255), 0.0f, 0, 2.0f);
                        drawList->AddRect(ImVec2(spPos.x + 1, spPos.y + 1), ImVec2(spPos.x + contentWidth - 1, spPos.y + barHeight - 1), hexToU32(settings->psBarInnerBorderColor, 150), 0.0f, 0, 1.0f);
                    } else if (hasMagicka) {
                        float mpPct = mp / mpMax;
                        char mpText[32];
                        snprintf(mpText, sizeof(mpText), "%.0f / %.0f", mp, mpMax);
                        
                        ImVec2 mpPos = ImGui::GetCursorScreenPos();
                        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, hexToImVec4(settings->partySheetMagickaColor));
                        ImGui::ProgressBar(mpPct, ImVec2(contentWidth, barHeight), mpText);
                        ImGui::PopStyleColor();
                        drawList->AddRect(mpPos, ImVec2(mpPos.x + contentWidth, mpPos.y + barHeight), hexToU32(settings->psBarOuterBorderColor, 255), 0.0f, 0, 2.0f);
                        drawList->AddRect(ImVec2(mpPos.x + 1, mpPos.y + 1), ImVec2(mpPos.x + contentWidth - 1, mpPos.y + barHeight - 1), hexToU32(settings->psBarInnerBorderColor, 150), 0.0f, 0, 1.0f);
                    } else {
                        ImGui::Dummy(ImVec2(contentWidth, barHeight));
                    }
                    
                    ImGui::SetWindowFontScale(1.0f);
                    ImGui::PopStyleVar();
                }

                ImGui::Spacing();

                std::string equipLabel = LocalizationManager::Get("Equipment.Title");
                float equipSectionStartY = ImGui::GetCursorScreenPos().y;
                
                auto equipment = GetEquippedItems(actor);
                float iconSize = static_cast<float>(settings->psStatIconSize);
                float slotWidth = (contentWidth - 16.0f) / 2.0f;
                float textWidth = slotWidth - iconSize - 8.0f;
                
                int slotCount = 0;
                for (const auto& slot : equipment) {
                    if (slotCount >= 8) break;
                    
                    int col = slotCount % 2;
                    if (col == 0) {
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8.0f);
                    } else {
                        // Second column - position at fixed offset
                        ImGui::SameLine(slotWidth + 24.0f);
                    }
                    
                    ImGui::BeginGroup();
                    
                    ImVec2 pos = ImGui::GetCursorScreenPos();
                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    
                    // Background box is larger than icon (adds padding)
                    float boxSize = iconSize + 4.0f;
                    float iconPadding = (boxSize - iconSize) / 2.0f;
                    
                    uint8_t iconAlpha = static_cast<uint8_t>(settings->psIconBgOpacity * 255);
                    dl->AddRectFilled(pos, ImVec2(pos.x + boxSize, pos.y + boxSize), 
                                     hexToU32(settings->psIconBgColor, iconAlpha), 4.0f);
                    dl->AddRect(pos, ImVec2(pos.x + boxSize, pos.y + boxSize), 
                               hexToU32(settings->psIconBgColor, 255), 4.0f);
                    
                    auto icon = TextureManager::GetPartyIcon(slot.iconName);
                    if (icon) {
                        ImGui::SetCursorScreenPos(ImVec2(pos.x + iconPadding, pos.y + iconPadding));
                        ImGui::Image(reinterpret_cast<ImTextureID>(icon), ImVec2(iconSize, iconSize));
                        ImGui::SetCursorScreenPos(ImVec2(pos.x + boxSize, pos.y));
                        ImGui::Dummy(ImVec2(0, boxSize));
                    } else {
                        ImGui::Dummy(ImVec2(boxSize, boxSize));
                    }
                    
                    ImGui::SameLine(0, 4.0f);
                    
                    ImGui::SetWindowFontScale(settings->psEquipTextScale);
                    float lineHeight = ImGui::GetTextLineHeight();
                    float maxTextHeight = lineHeight * 2.0f;
                    
                    float textVerticalOffset = (boxSize - maxTextHeight) / 2.0f;
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + textVerticalOffset);
                    
                    ImGui::BeginGroup();
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
                    if (slot.hasItem && !slot.itemName.empty()) {
                        ImVec2 textSize = ImGui::CalcTextSize(slot.itemName.c_str(), nullptr, false, textWidth);
                        float actualTextHeight = std::min(textSize.y, maxTextHeight);
                        float innerOffset = (maxTextHeight - actualTextHeight) / 2.0f;
                        
                        ImGui::BeginChild(("##eq" + std::to_string(slotCount)).c_str(), 
                                         ImVec2(textWidth, maxTextHeight), false, 
                                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + innerOffset);
                        ImGui::PushTextWrapPos(textWidth);
                        ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "%s", slot.itemName.c_str());
                        ImGui::PopTextWrapPos();
                        ImGui::EndChild();
                        
                        if (ImGui::IsItemHovered()) {
                            const std::string& tooltip = slot.tooltipText.empty() ? slot.itemName : slot.tooltipText;
                            ImGui::SetTooltip("%s", tooltip.c_str());
                        }
                    } else {
                        std::string emptyText = LocalizationManager::Get("Common.Empty");
                        ImVec2 emptySize = ImGui::CalcTextSize(emptyText.c_str());
                        float emptyInnerOffset = (maxTextHeight - emptySize.y) / 2.0f;
                        
                       ImGui::BeginChild(("##eq" + std::to_string(slotCount)).c_str(), 
                                         ImVec2(textWidth, maxTextHeight), false, 
                                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + emptyInnerOffset);
                        ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "%s", emptyText.c_str());
                        ImGui::EndChild();
                    }
                    ImGui::PopStyleColor();
                    ImGui::SetWindowFontScale(1.0f);
                    ImGui::EndGroup();

                    ImGui::EndGroup();

                    slotCount++;
                }
                {
                    float equipSectionEndY = ImGui::GetCursorScreenPos().y;
                    ImU32 lblCol = IM_COL32((settings->psLabelColor >> 16) & 0xFF,
                                           (settings->psLabelColor >> 8)  & 0xFF,
                                            settings->psLabelColor        & 0xFF, 180);
                    DrawVerticalSectionLabel(equipLabel.c_str(), equipSectionStartY, equipSectionEndY, lblCol, settings->psLabelFontScale);
                }
                
                ImGui::Spacing();
                
                if (avo) {
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    float statIconSize = static_cast<float>(settings->psStatIconSize);
                    float resistIconSize = statIconSize;
                    
                    // ========== GROUP 1: AR, Damage, Speed ==========
                    ImGui::Dummy(ImVec2(0, 0.0f));
                    float armorRating = actorStats.armorRating;
                    float moveSpeed = actorStats.speedMult;
                    
                    float weaponDamage = actorStats.weaponDamage;
                    float weaponSpeed = actorStats.weaponSpeed;
                    float weaponReach = actorStats.weaponReach;
                    bool isMeleeWeapon = false;
                    std::string weaponName = LocalizationManager::Get("Equipment.Unarmed");
                    
                    // Get weapon name and melee flag from equipped item
                    auto rightHand = actor->GetEquippedObject(false);
                    if (rightHand) {
                        if (auto weapon = rightHand->As<RE::TESObjectWEAP>()) {
                            weaponName = weapon->GetName() ? weapon->GetName() : "Weapon";
                            isMeleeWeapon = weapon->IsMelee();
                        }
                    }
                    
                    if (weaponDamage < 0.1f) {
                        weaponDamage = actorStats.unarmedDamage;
                    }
                    
                    // Calculate column widths for even spacing (same as resistances/skills)
                    float statColWidth = contentWidth / 3.0f;
                    float statItemWidth = statIconSize + ImGui::CalcTextSize("9999").x + 4;
                    
                    float lblR = ((settings->psLabelColor >> 16) & 0xFF) / 255.0f;
                    float lblG = ((settings->psLabelColor >> 8) & 0xFF) / 255.0f;
                    float lblB = (settings->psLabelColor & 0xFF) / 255.0f;
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(lblR, lblG, lblB, 1.0f));
                    ImGui::SetWindowFontScale(settings->psLabelFontScale);
                    
                    std::string armorLabel = LocalizationManager::Get("Stats.Armor");
                    float armorLabelWidth = ImGui::CalcTextSize(armorLabel.c_str()).x;
                    float armorLabelX = padding + (statColWidth - armorLabelWidth) * 0.5f;
                    ImGui::SetCursorPosX(armorLabelX);
                    ImGui::Text("%s", armorLabel.c_str());
                    
                    std::string damageLabel = LocalizationManager::Get("Stats.Damage");
                    float damageLabelWidth = ImGui::CalcTextSize(damageLabel.c_str()).x;
                    float damageLabelX = padding + statColWidth + (statColWidth - damageLabelWidth) * 0.5f;
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(damageLabelX);
                    ImGui::Text("%s", damageLabel.c_str());
                    
                    std::string moveLabel = LocalizationManager::Get("Stats.Move");
                    float moveLabelWidth = ImGui::CalcTextSize(moveLabel.c_str()).x;
                    float moveLabelX = padding + statColWidth * 2 + (statColWidth - moveLabelWidth) * 0.5f;
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(moveLabelX);
                    ImGui::Text("%s", moveLabel.c_str());
                    
                    ImGui::SetWindowFontScale(1.0f);
                    ImGui::PopStyleColor();

                    ImGui::SetWindowFontScale(settings->psBodyFontScale);
                    auto arIcon = TextureManager::GetPartyIcon("armor");
                    auto dmgIcon = TextureManager::GetPartyIcon("weapon");
                    auto spdIcon = TextureManager::GetPartyIcon("movespeed");
                    
                    float armorItemX = padding + (statColWidth - statItemWidth) * 0.5f;
                    ImGui::SetCursorPosX(armorItemX);
                    // Draw background box for armor icon (26px box for 22px icon)
                    float statBoxOffset = 4.0f;
                    uint8_t statIconAlpha = static_cast<uint8_t>(settings->psIconBgOpacity * 255);
                    ImVec2 arPos = ImGui::GetCursorScreenPos();
                    drawList->AddRectFilled(ImVec2(arPos.x - statBoxOffset, arPos.y - statBoxOffset), 
                                           ImVec2(arPos.x + statIconSize + statBoxOffset, arPos.y + statIconSize + statBoxOffset), 
                                           hexToU32(settings->psIconBgColor, statIconAlpha), 4.0f);
                    drawList->AddRect(ImVec2(arPos.x - statBoxOffset, arPos.y - statBoxOffset), 
                                     ImVec2(arPos.x + statIconSize + statBoxOffset, arPos.y + statIconSize + statBoxOffset), 
                                     hexToU32(settings->psIconBgColor, 255), 4.0f);
                    if (arIcon) {
                        ImGui::Image(reinterpret_cast<ImTextureID>(arIcon), ImVec2(statIconSize, statIconSize));
                        ImGui::SameLine();
                    } else {
                        ImGui::Dummy(ImVec2(statIconSize, statIconSize));
                        ImGui::SameLine();
                    }
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (statIconSize - ImGui::GetFontSize()) / 2.0f);
                    ImGui::TextColored(ImVec4(0.8f, 0.7f, 0.5f, 1.0f), "%.0f", armorRating);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s: %.0f", LocalizationManager::Get("Stats.Armor").c_str(), armorRating);
                    
                    float damageItemX = padding + statColWidth + (statColWidth - statItemWidth) * 0.5f;
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(damageItemX);
                    ImVec2 dmgPos = ImGui::GetCursorScreenPos();
                    drawList->AddRectFilled(ImVec2(dmgPos.x - statBoxOffset, dmgPos.y - statBoxOffset), 
                                           ImVec2(dmgPos.x + statIconSize + statBoxOffset, dmgPos.y + statIconSize + statBoxOffset), 
                                           hexToU32(settings->psIconBgColor, statIconAlpha), 4.0f);
                    drawList->AddRect(ImVec2(dmgPos.x - statBoxOffset, dmgPos.y - statBoxOffset), 
                                     ImVec2(dmgPos.x + statIconSize + statBoxOffset, dmgPos.y + statIconSize + statBoxOffset), 
                                     hexToU32(settings->psIconBgColor, 255), 4.0f);
                    if (dmgIcon) {
                        ImGui::Image(reinterpret_cast<ImTextureID>(dmgIcon), ImVec2(statIconSize, statIconSize));
                        ImGui::SameLine();
                    } else {
                        ImGui::Dummy(ImVec2(statIconSize, statIconSize));
                        ImGui::SameLine();
                    }
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (statIconSize - ImGui::GetFontSize()) / 2.0f);
                    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.7f, 1.0f), "%.0f", weaponDamage);
                    if (ImGui::IsItemHovered()) {
                        if (isMeleeWeapon) {
                            ImGui::SetTooltip("%s\nDamage: %.0f | Speed: %.2f | Reach: %.2f", 
                                             weaponName.c_str(), weaponDamage, weaponSpeed, weaponReach);
                        } else if (weaponDamage > 0) {
                            ImGui::SetTooltip("%s\n%s: %.0f | %s: %.2f", 
                                             weaponName.c_str(), LocalizationManager::Get("Stats.Damage").c_str(), weaponDamage, LocalizationManager::Get("Stats.Speed").c_str(), weaponSpeed);
                        } else {
                            ImGui::SetTooltip(LocalizationManager::Get("Equipment.Unarmed").c_str());
                        }
                    }
                    
                    float moveItemX = padding + statColWidth * 2 + (statColWidth - statItemWidth) * 0.5f;
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(moveItemX);
                    ImVec2 spdPos = ImGui::GetCursorScreenPos();
                    drawList->AddRectFilled(ImVec2(spdPos.x - statBoxOffset, spdPos.y - statBoxOffset), 
                                           ImVec2(spdPos.x + statIconSize + statBoxOffset, spdPos.y + statIconSize + statBoxOffset), 
                                           hexToU32(settings->psIconBgColor, statIconAlpha), 4.0f);
                    drawList->AddRect(ImVec2(spdPos.x - statBoxOffset, spdPos.y - statBoxOffset), 
                                     ImVec2(spdPos.x + statIconSize + statBoxOffset, spdPos.y + statIconSize + statBoxOffset), 
                                     hexToU32(settings->psIconBgColor, 255), 4.0f);
                    if (spdIcon) {
                        ImGui::Image(reinterpret_cast<ImTextureID>(spdIcon), ImVec2(statIconSize, statIconSize));
                        ImGui::SameLine();
                    } else {
                        ImGui::Dummy(ImVec2(statIconSize, statIconSize));
                        ImGui::SameLine();
                    }
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (statIconSize - ImGui::GetFontSize()) / 2.0f);
                    ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.9f, 1.0f), "%.0f", moveSpeed);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s: %.0f", LocalizationManager::Get("Stats.Move").c_str(), moveSpeed);
                    ImGui::SetWindowFontScale(1.0f);

                    ImGui::Spacing();

                    struct ResistInfo {
                        const char* iconName;
                        const char* label;
                        const char* nameKey;
                        float value;
                        ImVec4 color;
                    };
                    ResistInfo resists[] = {
                        {"fire", "Fr", "Resistances.Fire", actorStats.resistFire, ImVec4(1.0f, 0.4f, 0.2f, 1.0f)},
                        {"frost", "Fs", "Resistances.Frost", actorStats.resistFrost, ImVec4(0.4f, 0.7f, 1.0f, 1.0f)},
                        {"shock", "Sh", "Resistances.Shock", actorStats.resistShock, ImVec4(1.0f, 1.0f, 0.3f, 1.0f)},
                        {"magic", "Mg", "Resistances.Magic", actorStats.resistMagic, ImVec4(0.8f, 0.4f, 1.0f, 1.0f)},
                        {"poison", "Ps", "Resistances.Poison", actorStats.resistPoison, ImVec4(0.5f, 0.9f, 0.3f, 1.0f)},
                        {"disease", "Ds", "Resistances.Disease", actorStats.resistDisease, ImVec4(0.7f, 0.5f, 0.3f, 1.0f)},
                    };
                    
                    ImGui::Dummy(ImVec2(0, 4.0f));
                    std::string resistLabel = LocalizationManager::Get("Resistances.Title");
                    float resistSectionStartY = ImGui::GetCursorScreenPos().y;
                    
                    // 2 rows x 3 columns, centered like attack/armor/move
                    float resistColWidth = contentWidth / 3.0f;
                    float resistItemWidth = resistIconSize + ImGui::CalcTextSize("100%").x + 8;
                    
                    ImGui::SetWindowFontScale(settings->psBodyFontScale);
                    int count = 0;
                    for (const auto& res : resists) {
                        float val = res.value;
                        
                        int col = count % 3;
                        float colStartX = padding + (resistColWidth * col);
                        float centeredX = colStartX + (resistColWidth - resistItemWidth) * 0.5f;
                        
                        if (col == 0) {
                            ImGui::SetCursorPosX(centeredX);
                        } else {
                            ImGui::SameLine();
                            ImGui::SetCursorPosX(centeredX);
                        }
                        
                        ImGui::BeginGroup();
                        auto icon = TextureManager::GetPartyIcon(res.iconName);
                        if (icon) {
                            ImGui::Image(reinterpret_cast<ImTextureID>(icon), ImVec2(resistIconSize, resistIconSize));
                            ImGui::SameLine(0, 4.0f);
                        } else {
                            ImGui::Dummy(ImVec2(resistIconSize, resistIconSize));
                            ImGui::SameLine(0, 4.0f);
                        }
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (resistIconSize - ImGui::GetFontSize()) / 2.0f);
                        ImGui::TextColored(res.color, "%.0f%%", val);
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("%s: %.0f%%", LocalizationManager::Get(res.nameKey).c_str(), val);
                        }
                        ImGui::EndGroup();
                        
                        count++;
                    }
                    ImGui::SetWindowFontScale(1.0f);
                    {
                        float resistSectionEndY = ImGui::GetCursorScreenPos().y;
                        ImU32 lblCol = IM_COL32((settings->psLabelColor >> 16) & 0xFF,
                                               (settings->psLabelColor >> 8)  & 0xFF,
                                                settings->psLabelColor        & 0xFF, 180);
                        DrawVerticalSectionLabel(resistLabel.c_str(), resistSectionStartY, resistSectionEndY, lblCol, settings->psLabelFontScale);
                    }
                    
                    ImGui::Spacing();
                    
                    // ========== GROUP 3: Top 3 Skills per Category ==========
                    ImGui::Dummy(ImVec2(0, 2.0f));
                    std::string skillsLabel = LocalizationManager::Get("Skills.Title");
                    float skillsSectionStartY = ImGui::GetCursorScreenPos().y;
                    struct SkillInfo {
                        const char* iconName;
                        const char* label;
                        const char* nameKey;
                        RE::ActorValue av;
                        int category;
                    };
                    static const SkillInfo skills[] = {
                        {"onehanded", "1H", "Skills.OneHanded", RE::ActorValue::kOneHanded, 0},
                        {"twohanded", "2H", "Skills.TwoHanded", RE::ActorValue::kTwoHanded, 0},
                        {"archery", "Arc", "Skills.Archery", RE::ActorValue::kArchery, 0},
                        {"block", "Blk", "Skills.Block", RE::ActorValue::kBlock, 0},
                        {"heavyarmor", "Hvy", "Skills.HeavyArmor", RE::ActorValue::kHeavyArmor, 0},
                        {"lightarmor", "Lgt", "Skills.LightArmor", RE::ActorValue::kLightArmor, 0},
                        {"smithing", "Smth", "Skills.Smithing", RE::ActorValue::kSmithing, 0},
                        {"destruction", "Des", "Skills.Destruction", RE::ActorValue::kDestruction, 1},
                        {"restoration", "Res", "Skills.Restoration", RE::ActorValue::kRestoration, 1},
                        {"conjuration", "Con", "Skills.Conjuration", RE::ActorValue::kConjuration, 1},
                        {"alteration", "Alt", "Skills.Alteration", RE::ActorValue::kAlteration, 1},
                        {"illusion", "Ill", "Skills.Illusion", RE::ActorValue::kIllusion, 1},
                        {"enchanting", "Enc", "Skills.Enchanting", RE::ActorValue::kEnchanting, 1},
                        {"sneak", "Snk", "Skills.Sneak", RE::ActorValue::kSneak, 2},
                        {"alchemy", "Alc", "Skills.Alchemy", RE::ActorValue::kAlchemy, 2},
                        {"lockpicking", "Lck", "Skills.Lockpicking", RE::ActorValue::kLockpicking, 2},
                        {"pickpocket", "Pck", "Skills.Pickpocket", RE::ActorValue::kPickpocket, 2},
                        {"speech", "Spc", "Skills.Speech", RE::ActorValue::kSpeech, 2},
                    };
                    
                    struct SkillVal {
                        const char* iconName;
                        const char* label;
                        const char* nameKey;
                        float val;
                        int category;
                    };
                    std::vector<SkillVal> skillVals;
                    for (const auto& sk : skills) {
                        skillVals.push_back({sk.iconName, sk.label, sk.nameKey, actorStats.skills.GetSkillValue(sk.av), sk.category});
                    }
                    
                    // Show all 18 skills flat, 3 per row, sequential array order
                    float skillColWidth = contentWidth / 3.0f;
                    float skillIconSize = static_cast<float>(settings->psStatIconSize);
                    float skillItemWidth = skillIconSize + ImGui::CalcTextSize("100").x + 8;

                    ImGui::SetWindowFontScale(settings->psBodyFontScale);
                    int globalIdx = 0;
                    for (const auto& sv : skillVals) {
                        int col = globalIdx % 3;

                        float colStartX = padding + (skillColWidth * col);
                        float centeredX = colStartX + (skillColWidth - skillItemWidth) * 0.5f;

                        if (col == 0) {
                            ImGui::SetCursorPosX(centeredX);
                        } else {
                            ImGui::SameLine();
                            ImGui::SetCursorPosX(centeredX);
                        }

                        ImGui::BeginGroup();
                        float skillBoxOffset = 4.0f;
                        uint8_t skillIconAlpha = static_cast<uint8_t>(settings->psIconBgOpacity * 255);
                        ImVec2 skPos = ImGui::GetCursorScreenPos();
                        ImDrawList* skDl = ImGui::GetWindowDrawList();
                        skDl->AddRectFilled(ImVec2(skPos.x - skillBoxOffset, skPos.y - skillBoxOffset),
                                           ImVec2(skPos.x + skillIconSize + skillBoxOffset, skPos.y + skillIconSize + skillBoxOffset),
                                           hexToU32(settings->psIconBgColor, skillIconAlpha), 4.0f);
                        skDl->AddRect(ImVec2(skPos.x - skillBoxOffset, skPos.y - skillBoxOffset),
                                     ImVec2(skPos.x + skillIconSize + skillBoxOffset, skPos.y + skillIconSize + skillBoxOffset),
                                     hexToU32(settings->psIconBgColor, 255), 4.0f);
                        auto icon = TextureManager::GetPartyIcon(sv.iconName);
                        if (icon) {
                            ImGui::Image(reinterpret_cast<ImTextureID>(icon), ImVec2(skillIconSize, skillIconSize));
                            ImGui::SameLine(0, 8.0f);
                        } else {
                            ImGui::Dummy(ImVec2(skillIconSize, skillIconSize));
                            ImGui::SameLine(0, 8.0f);
                        }
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (skillIconSize - ImGui::GetFontSize()) / 2.0f);
                        ImGui::TextColored(ImVec4(0.7f, 0.8f, 0.9f, 1.0f), "%.0f", sv.val);
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("%s: %.0f", LocalizationManager::Get(sv.nameKey).c_str(), sv.val);
                        }
                        ImGui::EndGroup();

                        if (col == 2) {
                            ImGui::Dummy(ImVec2(0, 2.0f));
                        }

                        globalIdx++;
                    }
                    ImGui::SetWindowFontScale(1.0f);
                    {
                        float skillsSectionEndY = ImGui::GetCursorScreenPos().y;
                        ImU32 lblCol = IM_COL32((settings->psLabelColor >> 16) & 0xFF,
                                               (settings->psLabelColor >> 8)  & 0xFF,
                                                settings->psLabelColor        & 0xFF, 180);
                        DrawVerticalSectionLabel(skillsLabel.c_str(), skillsSectionStartY, skillsSectionEndY, lblCol, settings->psLabelFontScale);
                    }
                }
                
                ImGui::EndGroup();
                ImGui::EndChild();
                ImGui::PopStyleColor();
                ImGui::EndGroup();
            }
            
            if (totalPages > 1) {
                ImGui::Spacing();
                ImGui::Spacing();
                
                float buttonWidth = 80.0f;
                float spacing = 20.0f;
                float totalWidth = buttonWidth * 2 + spacing + 100.0f;
                float indent = (ImGui::GetContentRegionAvail().x - totalWidth) * 0.5f;
                if (indent > 0) ImGui::Indent(indent);
                
                if (g_CurrentPage > 0) {
                    if (ImGui::Button("< Prev", ImVec2(buttonWidth, 0))) {
                        g_CurrentPage--;
                    }
                } else {
                    ImGui::BeginDisabled();
                    ImGui::Button("< Prev", ImVec2(buttonWidth, 0));
                    ImGui::EndDisabled();
                }
                
                ImGui::SameLine();
                ImGui::Text(" Page %d / %d ", g_CurrentPage + 1, totalPages);
                ImGui::SameLine();
                
                if (g_CurrentPage < totalPages - 1) {
                    if (ImGui::Button("Next >", ImVec2(buttonWidth, 0))) {
                        g_CurrentPage++;
                    }
                } else {
                    ImGui::BeginDisabled();
                    ImGui::Button("Next >", ImVec2(buttonWidth, 0));
                    ImGui::EndDisabled();
                }
                
                if (indent > 0) ImGui::Unindent(indent);
            }
        }
        
        ImVec2 currentPos = ImGui::GetWindowPos();
        ImVec2 currentSize = ImGui::GetWindowSize();
        
        bool changed = false;
        
        if (settings->partySheetPosX != static_cast<std::int32_t>(currentPos.x) || 
            settings->partySheetPosY != static_cast<std::int32_t>(currentPos.y)) {
            settings->partySheetPosX = static_cast<std::int32_t>(currentPos.x);
            settings->partySheetPosY = static_cast<std::int32_t>(currentPos.y);
            changed = true;
        }
        
        if (settings->partySheetWidth != static_cast<std::int32_t>(currentSize.x) || 
            settings->partySheetHeight != static_cast<std::int32_t>(currentSize.y)) {
            settings->partySheetWidth = static_cast<std::int32_t>(currentSize.x);
            settings->partySheetHeight = static_cast<std::int32_t>(currentSize.y);
            changed = true;
        }
        
        if (changed) {
        }
        
        static int lastSavedX = -1;
        static int lastSavedY = -1;
        static int lastSavedW = -1;
        static int lastSavedH = -1;
        
        if (lastSavedX == -1) {
            lastSavedX = settings->partySheetPosX;
            lastSavedY = settings->partySheetPosY;
            lastSavedW = settings->partySheetWidth;
            lastSavedH = settings->partySheetHeight;
        }
        
        if (ImGui::IsMouseReleased(0)) {
            if (settings->partySheetPosX != lastSavedX || 
                settings->partySheetPosY != lastSavedY || 
                settings->partySheetWidth != lastSavedW || 
                settings->partySheetHeight != lastSavedH) {
                
                settings->Save();
                
                lastSavedX = settings->partySheetPosX;
                lastSavedY = settings->partySheetPosY;
                lastSavedW = settings->partySheetWidth;
                lastSavedH = settings->partySheetHeight;
            }
        }
        
        ImGui::End();
        
        if (!g_Visible) {
            if (auto main = RE::Main::GetSingleton()) {
                main->freezeTime = false;
            }
            if (!ImGuiRenderer::IsVisible()) {
                EnablePlayerInput();
            }
            ImGui::GetIO().MouseDrawCursor = false;
        }
        
        ImGui::EndFrame();
        PerfStats::EndPSFrame();
        ImGuiRenderer::DrawSkyrimCursor();
        ImGui::Render();
        ImGuiRenderer::RenderToBoundTarget(ImGui::GetDrawData());
    }
}

