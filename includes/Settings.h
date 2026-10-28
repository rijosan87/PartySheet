#pragma once
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <string>

class Settings {
public:
    static Settings* GetSingleton() {
        static Settings singleton;
        return &singleton;
    }

    void Load();
    void Save();

    // ===== Hotkeys & input =====
    std::int32_t  inspectCardHotkey = 21;
    std::int32_t  inspectCardModifier = -1;
    std::int32_t  modifierKey = 0;

    // ===== Inspect Card gameplay gate (INI-only, no menu UI) =====
    std::int32_t  requireAlteration       = 1;
    std::int32_t  requiredAlterationLevel = 25;
    float         magickaDrainPerSecond   = 1.0f;

    // ===== UI feedback =====
    std::int32_t  enableSoundEffects = 1;
    bool          pauseGameInSettings = false;

    // ===== Localization & fonts =====
    std::string   language = "PartySheet_en";
    std::int32_t  fontSize        = 18;
    std::int32_t  headingFontSize = 28;

    std::int32_t  menuPosX     = 280;
    std::int32_t  menuPosY     = 150;
    std::int32_t  menuWidth    = 550;
    std::int32_t  menuHeight   = 710;
    std::int32_t  menuScale    = 100;       // 50-200, HUD Settings UI only
    std::int32_t  advStylePosX = 1280;
    std::int32_t  advStylePosY = 210;

    bool debugEnableCSLogging        = false;
    bool debugEnablePerformanceStats = false;
    bool debugEnableVerboseLogging   = false;

    // -- Hotkeys (PartyUserSettings.ini) ----------
    std::int32_t  hudToggleKey         = 47;
    std::int32_t  hudToggleModifier    = -1;
    std::int32_t  menuSettingsKey      = 0x2D;
    std::int32_t  menuSettingsModifier = -1;
    std::int32_t  partySheetHotkey     = 0x40;
    std::int32_t  partySheetModifier   = -1;
    std::int32_t  characterSheetHotkey = 22;
    std::int32_t  characterSheetModifier = -1;

    // Quick-consume pinned potions (weakest-first per entry). Default keys 4/5/6.
    std::int32_t  potionHealthKey      = -1;
    std::int32_t  potionHealthModifier = -1;
    std::int32_t  potionMagickaKey     = -1;
    std::int32_t  potionMagickaModifier= -1;
    std::int32_t  potionStaminaKey     = -1;
    std::int32_t  potionStaminaModifier= -1;

    // -- Runtime state (not saved) ----------
    bool followerHudVisible = true;
    bool hudOverrideHidden  = false;

    // ===== Behavior =====
    std::int32_t followerHudMode    = 1;
    std::int32_t fadeDelay          = 3;
    bool         hudIdlePortraitStack = true;
    bool         showSummons        = true;
    bool         showFollowerSummons = true;
    bool         hudSeparateSummons = true;
    bool         hudHideManagedSummonEffects = true;
    bool         hudSummonGrowUpward = false;
    bool         hudSummonAnchorRight = false;
    std::int32_t hudSummonPosX = 140;
    std::int32_t hudSummonPosY = 250;
    bool         showFollowers      = true;
    bool         showQuestAllies    = false;
    std::int32_t maxFollowers       = 16;

    // ===== Layout =====
    std::int32_t hudLayout         = 0;
    bool         hudGrowUpward     = false;
    bool         hudAnchorRight    = false;
    std::int32_t maxPerColumnOrRow = 5;
    std::int32_t hudGroupGap       = 0;
    std::int32_t hudPosX           = 20;
    std::int32_t hudPosY           = 250;
    std::int32_t hudScale          = 100;

    bool          showName             = false;
    bool          showLevel            = false;
    bool          levelBadgeOnPortrait = true;
    bool          nameFirstWordOnly    = true;
    bool          nameUppercase        = false;
    bool          levelUppercase       = false;
    std::uint32_t nameColor            = 0xFFFFFF;
    std::uint32_t levelNameColor       = 0xAAAAAA;
    std::uint32_t levelColor           = 0xAAAAAA;
    std::uint32_t hudBadgeBgColor      = 0xB4000000;
    bool          hudBadgeFx           = true;       // drop shadow + top highlight across all portrait badges
    bool          hudLevelBadgeShowBorder = true;    // inner + outer border on level badge
    std::int32_t  hudBadgeRadius        = 16;
    float         nameFontScale        = 0.55f;
    float         hudBadgeFontScale    = 0.55f;
    std::int32_t  hudLevelBadgeOffsetX = 0;
    std::int32_t  hudLevelBadgeOffsetY = 0;
    std::int32_t  textAlignment        = 0;
    bool          textShadowEnabled = false;
    std::int32_t  textShadowOffsetX = 1;
    std::int32_t  textShadowOffsetY = 1;
    std::uint32_t textShadowColor   = 0x000000;
    float         textShadowOpacity = 0.7f;

    bool          showStateBadge        = true;
    bool          hudStateBadgeShowBorder = true;    // inner + outer border on state badge
    float         hudStateBadgeScale    = 0.28f;
    std::uint32_t hudStateBadgeBgColor  = 0xB4000000;
    std::uint32_t hudStateBadgeIconTint = 0xFFFFFF;
    std::int32_t  hudStateBadgeOffsetX  = 0;
    std::int32_t  hudStateBadgeOffsetY  = 0;

    bool          showHealth     = true;
    bool          showMagicka    = false;
    bool          showStamina    = false;
    bool          showTextOnBars = true;
    std::int32_t  hudBarWidth    = 70;
    std::int32_t  hudBarHeight   = 13;
    std::int32_t  barSpacing     = 2;
    bool          compactBars    = true;
    float         compactBarRatio = 0.5f;
    float         barTextFontScale = 0.8f;
    std::int32_t  healthColor    = 0x901515;
    std::int32_t  magickaColor   = 0x1E439F;
    std::int32_t  staminaColor   = 0x1F731F;
    std::int32_t  barTextColor   = 0xFFFFFF;
    bool          barStyleEnabled = true;
    std::int32_t  barStyleIndex   = 1;
    // Procedural bar style (used when barStyleEnabled = false)
    std::int32_t  barVisualStyle  = 0;
    // Segmented style (barVisualStyle=3): cell count derives from stat points, not a fixed count.
    std::int32_t  hudBarSegmentPoints = 100;
    std::int32_t  hudBarSegmentGap    = 2;
    bool          barGlowEnabled  = false;
    // AAA bar passes (HUDDraw::DrawBarStyled): independent layers, compose with any Visual Style
    bool          barTrackEnabled     = true;
    bool          barDepthShading     = true;
    bool          barEdgeLightEnabled = true;
    bool          barTipEnabled       = true;
    bool          barSmoothDrain      = true;
    bool          barDamageGhost      = true;
    float         barCornerRadius = 0.0f;
    std::int32_t  barEndStyle     = 0;
    std::int32_t  barEndStyleLeft = 0;
    std::int32_t  healthGradientColor  = 0x560D0D;
    std::int32_t  magickaGradientColor = 0x122860;
    std::int32_t  staminaGradientColor = 0x124512;
    // Bar borders (procedural style only)
    std::int32_t  barOuterBorderWidth   = 0;
    std::int32_t  barInnerBorderWidth   = 0;
    std::uint32_t barBorderColor        = 0x000000;
    std::uint32_t fhBarInnerBorderColor = 0x000000;
    float         barOuterBorderOpacity = 1.0f;
    float         barInnerBorderOpacity = 1.0f;

    bool          showPortrait              = true;
    std::int32_t  portraitSize              = 72;
    std::int32_t  portraitAlignment         = 1;
    std::int32_t  portraitSpacing           = 4;
    std::int32_t  contentVAlign             = 1;
    std::int32_t  portraitHAlign            = 1;
    std::int32_t  portraitBottomMargin      = 4;
    std::int32_t  portraitShape             = 0;
    float         portraitCornerRadius      = 48.0f;
    bool          hudPortraitFx             = true;
    bool          hudPortraitBacklight      = true;
    std::uint32_t hudPortraitBacklightColor = 0xFFC478;
    // Fill Ribbon mode: portrait covers the whole ribbon as backdrop, bypasses shape.
    bool          hudPortraitFillsRibbon    = false;
    float         hudPortraitZoom           = 1.0f;
    float         hudPortraitFocusU         = 0.5f;
    float         hudPortraitFocusV         = 0.5f;
    // Shape mode zoom/focus: applies when Fill Ribbon is OFF (square/rounded/circle/diamond).
    float         hudPortraitShapeZoom      = 1.0f;
    float         hudPortraitShapeFocusU    = 0.5f;
    float         hudPortraitShapeFocusV    = 0.5f;
    std::int32_t  fhPortraitOuterBorderWidth = 2;
    std::int32_t  fhPortraitInnerBorderWidth = 0;
    std::uint32_t fhPortraitOuterBorderColor = 0x1E1E23;
    std::uint32_t fhPortraitInnerBorderColor = 0xB49650;
    bool          hudPortraitHitShake       = true;  // Shake portrait on hit
    bool          hudPortraitHitFlash       = true;  // Red impact flash on hit

    bool          hudHaloEnable       = false;
    std::int32_t  hudHaloHealthShare  = 70;
    std::int32_t  hudHaloThickness    = 0;
    std::int32_t  hudHaloGap          = 4;
    std::int32_t  hudHaloOffset       = 2;

    // ===== Active Effects =====
    bool          showActiveEffects       = false;
    bool          showEffectIcons         = true;
    bool          showEffectMagnitude     = true;
    bool          showEffectSourceBadge   = false;
    bool          showEffectTimer         = true;
    std::int32_t  maxEffectsShown         = 5;
    std::int32_t  effectIconSize          = 32;
    std::int32_t  effectIconSpacing       = 4;
    std::int32_t  effectIconVertSpacing   = 2;
    std::int32_t  maxEffectIconsPerColumn = 2;
    std::int32_t  effectMinDuration       = 2;
    std::int32_t  effectMaxDuration       = 600;
    std::int32_t  effectIconPlacement     = 0;
    std::int32_t  overlayAnchor           = 0;
    std::int32_t  overlayOffsetX          = 0;
    std::int32_t  overlayOffsetY          = 0;
    std::int32_t  effectTimerStyle        = 2;
    std::int32_t  effectTimerBorderWidth  = 2;
    std::uint32_t effectBuffColor         = 0xFF44FF44;
    std::uint32_t effectDebuffColor       = 0xFFFF4444;
    std::int32_t  effectIconBorderRadius  = 50;
    std::int32_t  effectIconBgPadding     = 2;
    float         effectTextScale         = 0.8f;
    float         effectMagnitudeScale    = 0.7f;
    // Icon Depth: HUDDraw 5-pass stack on effect icons (mirrors Resistances prIcon*).
    bool          hudEffectIconDuotone      = true;
    bool          hudEffectIconShadow       = true;
    bool          hudEffectIconOutline      = true;
    std::int32_t  hudEffectIconOutlineWidth = 1;
    std::uint32_t hudEffectIconOutlineColor = 0x70000000;
    bool          hudEffectIconGlow         = false;
    bool          hudEffectIconGloss        = false;

    bool          showDirectionArrow   = true;
    bool          showDistanceMeters   = true;
    float         hudArrowScale        = 0.28f;
    bool          hudArrowShowBg       = false;
    bool          hudArrowShowBorder   = true;    // border rings on arrow disc and distance pill
    std::uint32_t hudArrowBgColor      = 0xC07B1818; // ARGB pill / disc
    std::uint32_t hudArrowTint         = 0xFFFFFF;
    float         hudDistanceFontScale = 0.5f;
    std::int32_t  hudArrowOffsetX      = -8;
    std::int32_t  hudArrowOffsetY      = 0;
    std::int32_t  hudDistanceOffsetX   = -22;
    std::int32_t  hudDistanceOffsetY   = -50;

    // ===== Ribbon (card background per follower) =====
    std::int32_t  ribbonBgColor          = 0x0D0D1A;
    float         ribbonBgOpacity        = 0.0f;
    float         ribbonCornerRadius     = 6.0f;
    std::int32_t  ribbonBorderWidth      = 0;
    std::int32_t  ribbonBorderColor      = 0x4D4D66;
    std::int32_t  ribbonPaddingTop       = 8;
    std::int32_t  ribbonPaddingRight     = 8;
    std::int32_t  ribbonPaddingBottom    = 8;
    std::int32_t  ribbonPaddingLeft      = 8;
    std::int32_t  ribbonMargin           = -15;
    std::int32_t  ribbonEffectClearance  = 30;

    bool          qhEnabled                = false;
    std::int32_t  qhHotkey                 = 0x23;
    std::int32_t  qhModifier               = -1;
    std::wstring  qhSpellMod               = L"Skyrim.esm";
    std::uint32_t qhSpellFormID            = 0x00012FD2;
    std::int32_t  qhMinHpPercent           = 90;
    std::int32_t  qhMaxRange               = 4000;
    std::int32_t  qhMagickaFloorPercent    = 0;
    bool          qhShowBadgeOnFocus       = true;
    bool          qhHideBadgeIfUnavailable = true;
    std::int32_t  qhBannerOffsetX          = 5;
    std::int32_t  qhBannerOffsetY          = -20;
    float         qhBannerFontScale        = 1.0f;
    bool          qhHealFlash              = true;       // green portrait flash when a follower is healed
    bool          qhBuffFlash              = true;

    // Stable Mount Widget
    bool enableStableMountWidget = true;
    std::int32_t smDisplayMode = 0;
    std::int32_t smLayoutOrientation = 1;
    std::int32_t smPortraitStyle = 2;       // 0=Card, 1=Top Style, 2=Halo
    std::int32_t smHaloHealthShare = 70;    // Health share %, 50-90
    std::int32_t smHaloThickness = 0;       // 0=auto, otherwise pixels, 0-12
    std::int32_t smHaloGap = 4;             // Segment gap in degrees, 0-10
    std::int32_t smHaloOffset = 2;          // Portrait-to-ring offset in pixels, 0-12
    std::int32_t smPosX = 1420;               // Window X position
    std::int32_t smPosY = 840;              // Window Y position
    std::int32_t smScale = 100;              // Whole-widget master scale: 50-200%
    std::int32_t smMountSpacing = 5;
    bool smBarStyleEnabled = true;
    std::int32_t smBarStyleIndex = 1;
    bool smShowCarryWeight = true;
    bool smShowStamina = true;
    bool smShowHealth = true;
    bool smShowName = true;
    std::int32_t smBarSpacing = 2;
    bool smHideInInteriors = true;
    std::int32_t smPortraitBorderRadius = 36;
    std::uint32_t smPortraitBorderColor = 0xFF1E1E1E;
    std::int32_t smPortraitBorderWidth = 2;
    bool smSummonAllowCities = true;
    bool smEnableHorsePicker = false;
    std::int32_t smSummonKey = -1;
    std::int32_t smSummonModifier = -1;
    bool smNameFirstWordOnly = true;
    bool smShowLastRiddenOnly = false;
    std::int32_t smMaxMounts = 0;
    std::int32_t smPortraitSize = 72;
    std::int32_t smBarWidth = 80;
    std::int32_t smBarHeight = 14;
    int tsBarWidth = 4;
    int tsBarHeight = 14;
    std::uint32_t smHealthColor = 0x901515;
    std::uint32_t smStaminaColor = 0x1F731F;
    std::uint32_t smCarryColor = 0xD8C37A;
    std::uint32_t smNameColor = 0xFFFFFF;
    float smNameFontScale = 0.9f;
    float smBarTextFontScale = 0.8f;
    std::uint32_t smRibbonBgColor = 0x000D0D1A;
    float smRibbonCornerRadius = 4.0f;
    std::int32_t smRibbonGap = 4;

    // Collectibles Widget
    bool enableCollectiblesWidget = true;
    std::int32_t ccPosX = 1800;
    std::int32_t ccPosY = 520;
    std::int32_t ccScale = 100;                 // Whole-widget master scale: 50-200%
    float ccLabelScale = 0.9f;
    float ccBarTextScale = 0.9f;
    std::int32_t ccIconSize = 54;
    std::int32_t ccRowSpacing = -6;
    std::int32_t ccPaddingBottom = 8;
    std::uint32_t ccTextColor = 0xFFFFFF;
    std::uint32_t ccGoalMetColor = 0x33B333;
    std::uint32_t ccBgColor = 0x000A0A0A;
    std::int32_t ccCardStyle = 1;
    std::int32_t ccDirection = 0;
    std::int32_t ccBarWidth = 72;
    std::int32_t ccBarHeight = 14;
    std::uint32_t ccBarColor = 0xFF6E25;
    bool ccShowLabel = false;
    bool ccBarStyleEnabled = false;
    std::int32_t ccBarStyleIndex = 5;
    std::uint32_t ccIconBgColor = 0x7B000000;
    std::uint32_t ccIconBorderColor = 0x62000000;
    std::int32_t ccIconBorderWidth = 2;
    std::int32_t ccIconBorderRadius = 18;
    bool          ccIconGlowEnabled = false;
    float         ccIconZoom        = 1.0f;
    float         ccIconFocusU      = 0.5f;
    float         ccIconFocusV      = 0.5f;
    bool ccHideInCombat = true;

    // Pinned Items Widget
    bool enablePinnedWidget = true;
    std::int32_t ptPosX = 1150;                 // Window X position
    std::int32_t ptPosY = 990;
    std::int32_t ptScale = 100;                 // Whole-widget master scale: 50-200%
    float ptLabelScale = 0.9f;
    float ptCountScale = 0.55f;
    std::int32_t ptCountStyle = 1;
    std::uint32_t ptIconBgColor = 0x79000000;
    std::uint32_t ptBadgeBgColor = 0xCF1D1D1D;
    std::int32_t ptIconSize = 48;
    std::int32_t ptRowSpacing = -8;
    std::int32_t ptPaddingBottom = 8;
    std::uint32_t ptTextColor = 0xFFFFFF;
    std::uint32_t ptGoalMetColor = 0x33B333;
    std::int32_t ptCardStyle = 0;
    std::int32_t ptDirection = 1;
    std::int32_t ptItemWidth = 60;
    bool ptShowLabel = false;
    bool ptIconGlowEnabled = false;
    std::int32_t ptIconBorderWidth = 0;
    std::int32_t ptIconBorderRadius = 30;
    std::uint32_t ptIconBorderColor = 0x6BE0E0E0;
    std::int32_t ptBadgeOffsetX = 42;
    std::int32_t ptBadgeOffsetY = 0;
    bool ptHideInCombat = false;
    float ptIconZoom = 1.0f;
    float ptIconFocusU = 0.5f;
    float ptIconFocusV = 0.5f;
    // Quick-consume hotkey badge (only on the 3 potion entries that own a hotkey).
    bool ptShowHotkeyBadge = true;
    float ptHotkeyBadgeScale = 0.9f;
    std::int32_t ptHotkeyBadgeOffsetX = 30;
    std::int32_t ptHotkeyBadgeOffsetY = 60;
    std::uint32_t ptHotkeyBadgeBgColor = 0x001D1D1D;

    // Player HUD - Stats (bars/portrait/XP)
    bool          enablePlayerHud        = true;
    bool          enablePlayerStatusWidget = true;
    std::int32_t  playerHudMode    = 1;
    std::int32_t  phScale          = 100;          // 50–200 %
    std::int32_t  phFadeDelay      = 3;
    bool          phDynamicBars    = true;
    // Portrait normally fades with the bars under Dynamic Bars. Users who want the
    bool          phPortraitAlwaysVisible = true;
    bool          phDamageGhost    = true;
    bool          phScaledBarWidth = false;
    std::int32_t  phPosX           = 20;
    std::int32_t  phPosY           = 950;
    bool          phShowHealth     = true;
    bool          phShowMagicka    = true;
    bool          phShowStamina    = true;
    bool          phShowName       = false;
    bool          phShowPortrait   = true;
    bool          phHaloEnable     = false;       // portrait resource ring, independent of bar visibility
    std::int32_t  phHaloHealthShare = 70;          // health share %, other resources split the remainder
    std::int32_t  phHaloThickness   = 0;           // 0 = auto, otherwise scaled pixels
    std::int32_t  phHaloGap         = 4;           // degrees between segments
    std::int32_t  phHaloOffset      = 2;           // scaled pixels outside the portrait
    std::int32_t  phBarWidth       = 300;
    std::int32_t  phBarHeight      = 16;
    std::int32_t  phMagickaBarHeight = 16;
    std::int32_t  phStaminaBarHeight = 16;
    std::int32_t  phXPBarHeight    = 8;
    std::int32_t  phXPBarWidth     = 0;
    bool          phBarStyleEnabled = false;
    std::int32_t  phBarStyleIndex   = 1;
    std::int32_t  phHealthBarStyleIndex = 0; // 0 = shared texture style
    std::int32_t  phMagickaBarStyleIndex = 0; // 0 = shared texture style
    std::int32_t  phStaminaBarStyleIndex = 0; // 0 = shared texture style
    std::int32_t  phHealthBarWidth = 0; // 0 = shared width; otherwise 150-600 px
    std::int32_t  phMagickaBarWidth = 0; // 0 = shared width; otherwise 150-600 px
    std::int32_t  phStaminaBarWidth = 0; // 0 = shared width; otherwise 150-600 px
    // Code-drawn bar design: active when phBarStyleEnabled (textured Custom Bar Style)
    std::int32_t  phBarVisualStyle  = 0;
    // Segmented style (phBarVisualStyle=3): cell count derives from stat points, not a fixed count.
    std::int32_t  phBarSegmentPoints = 100;
    std::int32_t  phBarSegmentGap    = 3;
    bool          phBarGlow         = true;
    bool          phBarTrack        = true;
    bool          phBarDepthShading = true;
    bool          phBarEdgeLight    = true;
    bool          phBarTip          = true;
    bool          phBarSmoothDrain  = true;
    float         phBarCornerRadius = 3.0f;
    std::int32_t  phBarOuterBorderWidth = 0;
    std::uint32_t phBarOuterBorderColor = 0xEB060608;
    std::int32_t  phBarInnerBorderWidth = 0;
    std::uint32_t phBarInnerBorderColor = 0x37968A74;
    std::int32_t  phBarEndStyle     = 0;
    std::int32_t  phBarEndStyleLeft = 0;
    bool          phShowBarText    = true;
    bool          phShowBarIcons   = true;
    std::uint32_t phBarTextColor   = 0xAEFFFFFF;
    std::uint32_t phBarPulseColor  = 0x70FFFFFF;
    std::int32_t  phHealthColor    = 0xFF901515;
    std::int32_t  phMagickaColor   = 0xFF1E439F;
    std::int32_t  phStaminaColor   = 0xFF1F731F;
    std::int32_t  phPortraitSize   = 96;
    std::int32_t  phPortraitOffsetX = 0;
    std::int32_t  phPortraitOffsetY = 0;
    std::int32_t  phPortraitShape  = 0;
    std::int32_t  phPortraitMode   = 0;
    std::uint32_t phEmblemFillColor = 0xFF2E7D32;
    bool          phLevelBadgeOnPortrait = true;
    bool          phPortraitFrameEnabled = true;
    std::int32_t  phPortraitFrameIndex   = 1;
    std::int32_t  phPortraitIndex        = 0;
    float         phPortraitFrameScale   = 1.4f;
    bool          phShowXP         = true;
    bool          phXPAsBar        = true;
    std::uint32_t phXPColor        = 0xFFD4AF37;

    // Portrait border and spacing
    std::uint32_t phPortraitBorderColor   = 0xFF1E1E1E;
    std::int32_t  phPortraitBorderWidth   = 4;
    float         phPortraitBorderRadius  = 8.0f;
    bool          phPortraitHitShake      = true;       // Shake portrait on hit
    bool          phPortraitHitFlash      = true;       // Red impact flash on hit
    std::int32_t  phPortraitColumnSpacing = 20;
    std::int32_t  phBarSpacing            = 6;
    std::int32_t  phBarTextOffsetY        = 0;

    std::int32_t  phHpAnnotationOffsetX = 0;
    std::int32_t  phHpAnnotationOffsetY = 0;
    std::int32_t  phMpAnnotationOffsetX = 0;
    std::int32_t  phMpAnnotationOffsetY = 0;
    std::int32_t  phSpAnnotationOffsetX = 0;
    std::int32_t  phSpAnnotationOffsetY = 0;
    std::int32_t  phXPAnnotationOffsetX = 0;
    std::int32_t  phXPAnnotationOffsetY = 0;

    std::int32_t  phHpOffsetX = 0;
    std::int32_t  phHpOffsetY = 0;
    std::int32_t  phMpOffsetX = 0;
    std::int32_t  phMpOffsetY = 0;
    std::int32_t  phSpOffsetX = 0;
    std::int32_t  phSpOffsetY = 0;
    std::int32_t  phXPBarOffsetX = 0;
    std::int32_t  phXPBarOffsetY = 0;

    // Animated portrait sprite sheet (player-sprites.png in Portraits folder).
    bool          phPortraitAnimEnabled = true;
    float         phPortraitAnimFps     = 6.0f;
    std::int32_t  phPortraitAnimCols    = 8;
    std::int32_t  phPortraitAnimRows    = 8;
    std::int32_t  phPortraitAnimFrames  = 64;

    // Shared level/XP badge appearance; offsets stay independent of the portrait.
    std::uint32_t phBadgeBgColor   = 0xB4000000;
    std::uint32_t phBadgeFontColor = 0xFFDCDCDC;
    float         phBadgeFontScale = 0.75f;
    std::uint32_t phBadgeBorderColor = 0x50968A74; // ARGB; dark outer edge follows this opacity
    float         phBadgeRadius = -1.0f;          // -1 = automatic pill; 0 = square
    float         phBadgePaddingX = 4.0f;
    float         phBadgePaddingY = 2.5f;
    std::int32_t  phLevelBadgeOffsetX = 0;
    std::int32_t  phLevelBadgeOffsetY = 0;
    std::int32_t  phXPBadgeOffsetX = 0;
    std::int32_t  phXPBadgeOffsetY = 0;

    // Text font scales and colors
    float         phNameFontScale     = 0.65f;
    float         phBarTextFontScale  = 0.75f;
    std::uint32_t phNameColor         = 0xFFFFFFFF;

    bool          phShowRegenPip      = true;
    float         phRegenFontScale    = 0.70f;
    bool          phShowFortify       = true;
    float         phFortifyFontScale  = 0.80f;
    bool          phFortifyRight      = true;

    // ============================================================
    // Player HUD - Survival Widget
    // ============================================================
    bool          enablePlayerSurvivalWidget = true;
    std::int32_t  phSurvivalPosX        = 470;
    std::int32_t  phSurvivalPosY        = 25;
    std::int32_t  phSurvivalScale       = 100;         // Whole-widget master scale: 50-200%
    std::int32_t  phSurvivalIconSize    = 36;
    float         phSurvivalLabelScale  = 0.70f;
    float         phSurvivalStageScale  = 0.80f;
    std::uint32_t phSurvivalLabelColor  = 0xFF8C9BAF;
    std::int32_t  phSurvivalCardWidth   = 70;
    std::int32_t  phSurvivalCardSpacing = 6;
    std::int32_t  phSurvivalBgCornerRadius = 6;
    bool          phSurvivalShowBorder = false;
    bool          phSurvivalBgTexture = true;
    bool          phSurvivalBgDuotone = true;
    float         phSurvivalBorderWidth = 1.2f;
    std::uint32_t phSurvivalBorderColor = 0xEB060608;
    std::uint32_t phSurvivalBgColor     = 0x00000000;
    std::int32_t  phSurvivalHideBelowStage = 0;
    std::uint32_t phSurvivalColdColor   = 0xFF78C8FF;
    std::uint32_t phSurvivalHungerColor = 0xFFFF8C32;
    std::uint32_t phSurvivalFatigueColor= 0xFFB4A0D2;
    std::uint32_t phSurvivalThirstColor = 0xFF4FC8E0;
    std::uint32_t phSurvivalAlarmColor  = 0xFFDC2828;
    bool          phSurvivalAlarmPulse  = true;
    std::int32_t  phSurvivalStyle          = 0;
    std::int32_t  phSurvivalDirection     = 0;
    std::int32_t  phSurvivalIndicatorStyle = 0;          // 0=Pips, 1=ProgressBar, 2=None, 3=Arc
    bool          phSurvivalIconFill = true;
    std::int32_t  phSurvivalPipShape = 0;               // 0=Circle, 1=Diamond
    std::int32_t  phSurvivalArcSweep = 100;             // degrees; 360 draws a full ring
    std::int32_t  phSurvivalArcRotation = 90;           // midpoint angle; 90 is below the icon
    float         phSurvivalArcRadius = 24.0f;
    float         phSurvivalArcThickness = 2.0f;
    bool          phSurvivalShowPercentage = false;
    float         phSurvivalPercentageScale = 0.80f;
    bool          phSurvivalShowLabel      = false;
    bool          phSurvivalShowStage      = false;
    // -1 preserves each layout's existing padding; offsets never change card anchors.
    std::int32_t  phSurvivalInnerPaddingX = -1;
    std::int32_t  phSurvivalInnerPaddingY = -1;
    std::int32_t  phSurvivalIconOffsetX = 0;
    std::int32_t  phSurvivalIconOffsetY = 0;
    std::int32_t  phSurvivalLabelOffsetX = 0;
    std::int32_t  phSurvivalLabelOffsetY = 0;
    std::int32_t  phSurvivalStageOffsetX = 0;
    std::int32_t  phSurvivalStageOffsetY = 0;
    std::int32_t  phSurvivalIndicatorOffsetX = 0;
    std::int32_t  phSurvivalIndicatorOffsetY = 0;
    std::int32_t  phSurvivalPercentageOffsetX = 0;
    std::int32_t  phSurvivalPercentageOffsetY = 0;
    // ===== Indicator depth =====
    bool          phSurvivalIndicatorShadow = true;
    bool          phSurvivalIndicatorDuotone = true;
    bool          phSurvivalIndicatorGlow = true;
    bool          phSurvivalIndicatorHighlight = true;
    // ===== Text depth ===== (HUDDraw::DrawTextStyled on the need/stage labels; mirrors Disease phDiseaseText*)
    bool          phSurvivalTextShadow  = true;
    bool          phSurvivalTextOutline = false;
    std::int32_t  phSurvivalTextOutlineOpacity = 160;
    bool          phSurvivalTextDuotone = true;
    // ===== Icon depth ===== (HUDDraw::DrawIconStyled, mirrors Resistances prIcon*; all off = plain image)
    bool          phSurvivalIconDuotone     = true;
    bool          phSurvivalIconShadow      = true;
    bool          phSurvivalIconOutline     = true;
    std::int32_t  phSurvivalIconOutlineWidth = 1;
    std::uint32_t phSurvivalIconOutlineColor = 0xC80A080C;
    bool          phSurvivalIconGlow        = false;
    bool          phSurvivalIconGloss       = false;

    // ============================================================
    // Player HUD - Disease Widget
    // ============================================================
    bool          enablePlayerDiseaseWidget = true;
    std::int32_t  phDiseasePosX        = 1300;
    std::int32_t  phDiseasePosY        = 30;
    std::int32_t  phDiseaseScale       = 100;         // Whole-widget master scale: 50-200%
    std::int32_t  phDiseaseIconSize    = 36;
    std::int32_t  phDiseaseCardWidth   = 65;
    std::int32_t  phDiseaseCardSpacing = 6;
    std::int32_t  phDiseaseBgCornerRadius = 6;
    bool          phDiseaseShowBorder  = true;
    std::int32_t  phDiseaseStyle       = 0;
    bool          phDiseaseGrouped     = false;
    bool          phDiseaseIdleCollapse = true;
    bool          phDiseaseHideInCombat = true;
    std::int32_t  phDiseaseDirection   = 0;
    float         phDiseaseLabelScale  = 0.75f;
    float         phDiseaseStageScale  = 0.70f;
    bool          phDiseaseShowLabel   = true;
    bool          phDiseaseShowStage   = true;
    bool          phDiseaseShowPips    = true;
    bool          phDiseaseShowMagnitude = false;     // primary disease effect text, shared with V2
    std::uint32_t phDiseaseColor       = 0xFFC8B45A;
    std::uint32_t phDiseaseAlarmColor  = 0xFFDC2828;
    std::uint32_t phDiseaseBgColor     = 0x00000000;
    // ===== BG depth ===== (mirrors Survival's phSurvivalBg* set; ShowBorder is taken by Alarm Pulse)
    bool          phDiseaseBgBorder    = false;       // single card stroke in BgBorderColor
    std::uint32_t phDiseaseBgBorderColor = 0x66D2C8B4; // ARGB card stroke
    bool          phDiseaseBgTexture   = true;        // Frames\ChipGrain.png over the card fill
    bool          phDiseaseBgDuotone   = true;        // lit-top / dark-bottom gradient from Card BG; off = flat
    bool          phDiseaseIconBorder  = true;
    std::uint32_t phDiseaseIconBgColor = 0xC80C0E12;
    std::uint32_t phDiseaseLabelColor  = 0xFF8C9BAF;
    // ===== Text depth ===== (HUDDraw::DrawTextStyled: name + severity word; turn shadow
    bool          phDiseaseTextShadow  = true;
    bool          phDiseaseTextOutline = false;
    std::int32_t  phDiseaseTextOutlineOpacity = 160;
    bool          phDiseaseTextDuotone = true;
    // ===== Icon depth ===== (HUDDraw::DrawIconStyled, mirrors Resistances prIcon*; all off = plain image)
    bool          phDiseaseIconDuotone     = true;
    bool          phDiseaseIconShadow      = true;
    bool          phDiseaseIconOutline     = true;
    std::int32_t  phDiseaseIconOutlineWidth = 1;
    std::uint32_t phDiseaseIconOutlineColor = 0xC80A080C;
    bool          phDiseaseIconGlow        = false;
    bool          phDiseaseIconGloss       = false;

    // ============================================================
    // Player HUD - Equipment Widget
    // ============================================================
    bool          enablePlayerEquipWidget = true;
    std::int32_t  pePosX           = 1550;
    std::int32_t  pePosY           = 890;
    std::int32_t  peScale          = 100;          // Whole-widget master scale: 50-200%
    std::int32_t  peIconSize       = 80;
    bool          peShowIcons      = true;
    bool          peShowNames      = true;
    std::uint32_t peNameColor      = 0xFFFFFFFF;
    std::uint32_t peNameBgColor    = 0x00000000;
    std::int32_t  peNameAlignment  = 1;
    std::int32_t  peNameMaxChars   = 20;
    float         peNameFontScale  = 0.75f;
    float         peBadgeFontScale = 0.75f;
    std::uint32_t peBadgeBgColor   = 0xDB212121;
    std::int32_t  peBadgeOffsetX   = -4;
    std::int32_t  peBadgeOffsetY   = -4;
    std::int32_t  peChargeStyle    = 1;
    std::int32_t  peChargeBarWidth = 8;
    float         peChargeBarHeight = 0.20f;
    bool          peShowDamage     = true;

    // Layout
    std::int32_t  peLayout         = 0;
    std::int32_t  peSlotSpacing    = 50;
    bool          peShowSlotBadge  = true;
    bool          peShowAmmoCount  = true;
    bool          peShowDurability = true;
    float         peDurabilityScale = 0.6f;
    std::int32_t  peSlotOrder0     = 0;
    std::int32_t  peSlotOrder1     = 1;
    std::int32_t  peSlotOrder2     = 2;
    // Per-slot size scale (multiplies peIconSize). Keyed to slot TYPE index:
    float         peSlotScale0     = 1.0f;
    float         peSlotScale1     = 1.0f;
    float         peSlotScale2     = 1.0f;
    std::int32_t  peSlotOffsetX1   = 0;
    std::int32_t  peSlotOffsetY1   = 0;
    std::int32_t  peSlotOffsetX2   = 0;
    std::int32_t  peSlotOffsetY2   = 0;

    bool          peIconBgEnabled     = true;
    std::int32_t  peIconBgShape       = 0;
    std::uint32_t peIconBgColor       = 0x99000000;
    std::uint32_t peIconBorderColor   = 0xFF3E3D32;
    std::int32_t  peIconBorderWidth   = 0;
    std::int32_t  peIconBorderRadius  = 15;
    std::int32_t  peIconBgPadding     = 8;

    // Slot backplate fill style: only applies to the solid (non-textured) backplate,
    std::int32_t  peIconBgStyle       = 0;
    std::int32_t  peIconBgFadeMode    = 0;
    std::int32_t  peIconBgFadeAnchor  = 25;
    std::int32_t  peIconBgFadeLength  = 75;

    // Aura (weapon poison/enchant tint)
    bool          peShowAura        = true;
    std::uint32_t peAuraPoisonColor = 0xFF6EDC5A;
    std::uint32_t peAuraFireColor   = 0xFFFF822D;
    std::uint32_t peAuraFrostColor  = 0xFF5AC8F5;
    std::uint32_t peAuraShockColor  = 0xFFFFD23C;
    std::uint32_t peAuraOtherColor  = 0xFF96AAFF;

    bool          peUseIconBgTexture  = true;
    std::int32_t  peIconBgIndex       = 10;
    float         peIconBgTexOpacity  = 0.7f;

    // Spell / shout icon depth
    bool          peSpellIconDuotone  = true;
    bool          peSpellIconShadow   = true;
    bool          peSpellIconOutline  = true;
    std::int32_t  peSpellIconOutlineWidth = 1;

    // Icon source. PNGII art comes from PNG Icon Injector's API; without it, item art.
    std::int32_t  peIconStyle          = 0;
    float         peGlyphScale         = 0.62f;
    std::uint32_t peGlyphColor         = 0xFFE8E4DA;   // ARGB glyph tint
    bool          peGlyphTintByElement = true;         // poison/enchant element color overrides peGlyphColor

    // Zoom / crop
    float         peIconZoom       = 1.25f;
    float         peIconFocusU     = 0.5f;
    float         peIconFocusV     = 0.5f;
    float         peWeaponRotation = 0.45f;

    // ============================================================
    // Player HUD - Resistances Widget
    // ============================================================
    bool          enablePlayerResistWidget = true;
    std::int32_t  prScale          = 100;          // 50–200 %
    std::int32_t  prPosX           = 135;
    std::int32_t  prPosY           = 900;
    std::int32_t  prIconSize       = 22;
    float         prFontScale      = 0.80f;
    std::uint32_t prArmorColor     = 0xFFB4B4B4;
    std::uint32_t prDmgResistColor = 0xFFCCC8C0;
    std::uint32_t prFireColor      = 0xFFCC4400;
    std::uint32_t prFrostColor     = 0xFF4488CC;
    std::uint32_t prShockColor     = 0xFFCCCC00;
    std::uint32_t prMagicColor     = 0xFF8844CC;
    std::uint32_t prPoisonColor    = 0xFF44AA44;
    std::uint32_t prDiseaseColor   = 0xFFA0A0A0;
    // Movement speed (SpeedMult AV %, 100 = normal). Lives in this strip alongside Armor/Phys
    bool          prShowSpeed      = true;
    std::uint32_t prSpeedColor     = 0xFF7FD4A0;
    std::int32_t  prLayout         = 1;
    bool          prShowValue      = true;
    bool          prIconFill       = false;
    // Radial layout: entries ride a circle around the window center, each with a value ring.
    std::int32_t  prRadialRadius     = 70;
    std::int32_t  prRadialStartAngle = -90;
    bool          prRadialShowRing   = true;
    std::int32_t  prSpacing        = 14;
    std::uint32_t prRibbonColor    = 0x00000000;
    std::int32_t  prRibbonBgStyle  = 0;
    std::int32_t  prRibbonFadeMode = 0;
    std::int32_t  prRibbonFadeAnchor = 25;
    std::int32_t  prRibbonFadeLength = 75;
    bool          prBgTintEnabled  = true;
    std::int32_t  prBgTintOpacity  = 0;
    std::int32_t  prCornerRadius   = 3;

    // ===== Layered chip style (prBgStyle=1) =====
    std::int32_t  prBgStyle              = 1;
    std::uint32_t prChipTopColor         = 0xEB2E2C32;
    std::uint32_t prChipBottomColor      = 0xFF0F0E11;
    bool          prChipShadow           = true;
    bool          prChipHighlight        = true;
    bool          prChipAccent           = true;
    std::uint32_t prChipInnerBorderColor = 0x37968A74;
    std::uint32_t prChipOuterBorderColor = 0xEB060608;
    bool          prChipNeutralText      = true;
    bool          prChipGrain            = true;
    std::int32_t  prChipGrainOpacity     = 40;

    // ===== Icon depth =====
    bool          prIconDuotone = true;
    bool          prIconShadow  = true;
    bool          prIconOutline = true;
    std::int32_t  prIconOutlineWidth = 1;
    std::uint32_t prIconOutlineColor = 0xC80A080C;
    bool          prIconGlow    = false;
    bool          prIconGloss   = false;

    // ===== Value text =====
    bool          prTextShadow         = true;
    bool          prTextOutline        = false;
    std::int32_t  prTextOutlineOpacity = 160;
    bool          prTextDuotone        = true;

    // ============================================================
    // Player HUD - Resources Widget
    // ============================================================
    bool          enablePlayerResourcesWidget = true;
    std::int32_t  presPosX         = 1565;
    std::int32_t  presPosY         = 1040;
    std::int32_t  presScale        = 100;           // Whole-widget master scale: 50-200%
    float         presFontScale    = 0.95f;
    bool          presShowGold        = true;
    bool          presShowCarryWeight = true;
    bool          presShowTime        = true;
    bool          pres24HourTime      = false;
    bool          presShowWeather     = true;
    std::uint32_t presTextColor    = 0xFFFFFFFF;
    std::uint32_t presGoldColor    = 0xFFFFFFFF;
    std::uint32_t presCarryColor   = 0xFFFFFFFF;
    std::uint32_t presTimeColor    = 0xFFFFFFFF;
    std::uint32_t presWeatherColor = 0xFFFFFFFF;
    std::int32_t  presIconSize     = 24;
    std::int32_t  presLayout       = 1;
    std::int32_t  presSpacing      = 24;
    // Resources window background (manual AddRectFilled, drawn behind items). Alpha=0 → off.
    std::uint32_t presBgColor        = 0x00000000;
    std::int32_t  presBgStyle        = 0;
    std::int32_t  presFadeMode       = 0;
    std::int32_t  presFadeAnchor     = 25;
    std::int32_t  presFadeLength     = 75;
    std::int32_t  presBgPadding      = 7;
    bool          presLocSeparate     = true;
    std::int32_t  presLocPosX         = 1926;
    std::int32_t  presLocPosY         = 0;
    std::uint32_t presLocBgColor      = 0x70000000;
    std::int32_t  presLocBgStyle      = 1;
    std::int32_t  presLocFadeMode     = 1;
    std::int32_t  presLocFadeAnchor   = 25;
    std::int32_t  presLocFadeLength   = 75;
    std::int32_t  presLocBgPadding    = 7;
    std::int32_t  presLocMaxChars     = 25;
    bool          presLocAlignRight   = true;
    bool          presLocIconDiamond            = true;
    std::uint32_t presLocIconDiamondColor       = 0xE6252530;
    std::int32_t  presLocIconDiamondBorderWidth = 2;
    std::uint32_t presLocIconDiamondBorderColor = 0xFF464646;

    // ============================================================
    // Player HUD - Effects Widget
    // ============================================================
    bool          enablePlayerEffectWidget   = true;
    bool          phShowEffectText           = false;
    std::int32_t  phEffectPosX              = 1880;
    std::int32_t  phEffectPosY              = 90;
    std::int32_t  phEffectScale             = 100;    // Whole-widget master scale: 50-200%
    std::int32_t  phEffectIconSize          = 48;
    std::int32_t  phEffectMaxIcons          = 16;
    std::int32_t  phEffectMinDuration       = 2;
    std::int32_t  phEffectMaxDuration       = 600;
    std::int32_t  phEffectVertSpacing       = 40;
    std::int32_t  phEffectHorizSpacing      = 20;
    std::int32_t  phEffectMaxPerColumn      = 6;
    std::int32_t  phEffectColumnDirection   = 0;
    std::int32_t  phEffectAnchor            = 1;
    bool          phShowEffectTimer         = true;
    std::int32_t  phEffectTimerStyle        = 0;
    std::int32_t  phEffectTimerBorderWidth  = 2;
    std::uint32_t phEffectBuffColor        = 0xFF44FF44;
    std::uint32_t phEffectDebuffColor      = 0xFFFF4444;
    bool          phShowEffectMagnitude     = true;
    bool          phEffectShowSourceBadge  = true;
    float         phEffectSourceBadgeScale  = 0.45f;
    std::uint32_t phEffectSourceBadgeTint   = 0xFFFFD080;
    std::int32_t  phEffectSourceBadgeOffsetX = 0;
    std::int32_t  phEffectSourceBadgeOffsetY = 7;
    float         phEffectMagnitudeScale    = 0.8f;
    std::int32_t  phEffectMagnitudeOffsetX  = 0;
    std::int32_t  phEffectMagnitudeOffsetY  = 0;
    bool          phShowEffectTimeText      = true;
    float         phEffectTimeTextScale     = 0.9f;
    std::uint32_t phEffectTimeTextColor     = 0xFFFFFFFF;
    bool          phEffectIconBgEnabled     = true;
    std::uint32_t phEffectIconBgColor       = 0x80000000;
    bool          phEffectIconGlowEnabled   = false;
    std::uint32_t phEffectIconBorderColor   = 0xFFFFFFFF;
    std::int32_t  phEffectIconBorderWidth   = 0;
    std::int32_t  phEffectIconBorderRadius  = 50;
    std::int32_t  phEffectIconBgPadding    = 4;
    float         phEffectIconZoom         = 1.0f;
    float         phEffectIconFocusU       = 0.5f;
    float         phEffectIconFocusV       = 0.5f;
    // Icon Depth: HUDDraw 5-pass stack on player effect icons (mirrors Resistances prIcon*).
    bool          phEffectDepthDuotone      = false;
    bool          phEffectDepthShadow       = false;
    bool          phEffectDepthOutline      = false;
    std::int32_t  phEffectDepthOutlineWidth = 2;
    std::uint32_t phEffectDepthOutlineColor = 0xC80A080C;
    bool          phEffectDepthGlow         = false;
    bool          phEffectDepthGloss        = false;

    // HUD Visibility System
    bool  hvEnabled              = true;
    bool  hvHideVanillaBars      = true;
    float hvFadeOutDelay         = 3.0f;
    float hvFadeDuration         = 0.8f;
    bool  hvTriggerPlayerEquip     = true;
    bool  hvTriggerPlayerEffects   = true;
    bool  hvTriggerPlayerResist    = true;
    bool  hvShowResistCombatOnly   = false;
    bool  hvTriggerPlayerResources = true;
    bool  hvTriggerPlayerLocation  = true;
    bool  hvTriggerCollectibles    = true;
    bool  hvTriggerPinnedItems     = true;
    bool  hvTriggerStableMount     = true;

    // Portrait & bar border colors (RGB)
    std::uint32_t psPortraitOuterBorderColor = 0x1E1E23;
    std::uint32_t psPortraitInnerBorderColor = 0xB49650;
    std::uint32_t psBarOuterBorderColor      = 0x141419;
    std::uint32_t psBarInnerBorderColor      = 0x642828;

    bool psUseFullscreenMode = true;
    float psV2Scale = 1.0f;
    float psV2FontDisplay = 1.00f;
    float psV2FontHeading = 0.70f;
    float psV2FontBody    = 0.85f;
    float psV2FontSmall   = 0.8f;
    float psV2IconSkill   = 28.0f;
    float psV2IconStat    = 24.0f;
    float psV2IconCard    = 48.0f;
    float psV2IconTrait   = 36.0f;
    float psV2IconBadge   = 20.0f;
    float psV2IconFooter  = 28.0f;
    float psV2IconEquip   = 144.0f;
    float psV2IconArmor   = 112.0f;
    // ===== Icon Design ===== (HUDDraw::DrawIconStyled, mirrors FollowerHUD hudEffectIcon*)
    bool          psV2IconDuotone     = true;
    bool          psV2IconShadow      = true;
    bool          psV2IconOutline     = true;
    std::int32_t  psV2IconOutlineWidth = 1;
    std::uint32_t psV2IconOutlineColor = 0x70000000;
    std::int32_t partySheetPosX = -1;
    std::int32_t partySheetPosY = -1;
    std::int32_t partySheetWidth = 1365;
    std::int32_t partySheetHeight = 900;
    std::int32_t partySheetBgColor = 0x0D0D1A;
    float partySheetBgOpacity = 0.90f;
    std::int32_t partyCardBgColor = 0x262833;
    float partyCardBgOpacity = 0.90f;
    std::int32_t psIconBgColor = 0x282832;
    float psIconBgOpacity = 0.78f;
    std::uint32_t psLabelColor = 0x5E5E5A;

    // Party Sheet element scaling (for 4K support)
    bool psUseCustomBackground = true;
    bool pcUseCustomBackground = true;
    std::int32_t psPortraitSize = 80;
    std::int32_t psBarHeight = 20;
    std::int32_t psBarWidth = 180;
    std::int32_t psStatIconSize = 30;
    std::int32_t psStatIconBgSize = 32;
    std::int32_t psEquipIconSize = 24;
    std::int32_t psResistIconSize = 20;
    std::int32_t psCardsPerPage = 4;
    std::int32_t psBottomPadding = 80;

    // Custom background selection indices (1-based, 0 = disabled)
    std::int32_t psBackgroundIndex = 1;
    std::int32_t pcBackgroundIndex = 1;

    // Bar Colors (stored as 0xRRGGBB)
    std::int32_t partySheetHealthColor = 0x901515;
    std::int32_t partySheetMagickaColor = 0x1E439F;
    std::int32_t partySheetStaminaColor = 0x1F731F;
    std::int32_t partySheetNameColor = 0xFFE5B3;
    float partySheetNameFontScale = 0.75f;
    float psEquipTextScale = 0.85f;
    float psLabelFontScale = 1.0f;
    float psBodyFontScale = 1.0f;

    // Inspect Card                  (colors: 6-hex RGB, 8-hex ARGB)
    std::int32_t inspectCardPosX = 1170;
    std::int32_t inspectCardPosY = 280;
    std::int32_t inspectCardBgColor = 0x0D0D1A;
    float inspectCardBgOpacity = 0.95f;
    bool icUseCustomBackground = true;
    std::int32_t icBackgroundIndex = 1;
    float inspectCardFontScale = 1.0f;
    float inspectCardScale = 1.0f;
    float icBodyFontScale = 1.0f;

    // Name Color
    std::uint32_t inspectCardNameColor = 0xFFE5B3;

    // Bar Settings
    std::int32_t inspectCardBarWidth = 300;
    std::int32_t inspectCardBarHeight = 20;
    std::int32_t inspectCardBarBorderWidth = 0;
    std::uint32_t inspectCardBarBorderColor = 0x000000;

    // Icon Settings
    std::int32_t inspectCardIconSize = 16;
    std::int32_t inspectCardResistIconSize = 28;

    // Bar Colors (stored as 0xRRGGBB)
    std::int32_t inspectCardHealthColor = 0x901515;
    std::int32_t inspectCardMagickaColor = 0x1E439F;
    std::int32_t inspectCardStaminaColor = 0x1F731F;

    // Know Your Enemy 2 integration (see InspectCardKYE.cpp)
    bool icShowCreatureTypeBadges = true;
    bool icShowKYEResists = true;

    // Character Sheet               (colors: 6-hex RGB, 8-hex ARGB)
    bool enableCharacterSheet = true;
    bool csTextShadowEnabled = true;
    float csOffsetX = 0.0f;
    float csOffsetY = 0.0f;
    // Crop/Hide settings for when Inventory/Magic menu is open
    bool csHideNameOnMenus = false;
    bool csHideEquipmentOnMenus = true;
    bool csHideSkillsOnMenus = false;
    bool csHideStatusOnMenus = true;
    bool csUseCustomBackground = true;
    std::int32_t csBackgroundIndex = 1;
    std::uint32_t csBackgroundColor = 0x1A1A2E;
    float csBackgroundOpacity = 0.85f;
    float csPadding = 10.0f;
    float csCarouselInterval = 4.0f;
    bool csEnableCameraControl = false;
    float csCameraZoom = 20.0f;
    float csScale = 1.0f;
    float csMenuScale = 1.0f;

    // Row 1: Name & bars
    std::uint32_t csNameColor = 0xE6DCB4;
    float csNameFontScale = 1.0f;
    std::uint32_t csRaceColor = 0x969696;
    std::uint32_t csXpBarColor = 0xCDB46D;
    std::uint32_t csShoutBarColor = 0x8586A5;

    // Row 2: Stat bars
    std::uint32_t csHealthColor = 0x901515;
    std::uint32_t csStaminaColor = 0x1F731F;
    std::uint32_t csMagickaColor = 0x1E439F;
    std::uint32_t csBarTextColor = 0xDCDCDC;
    float csBarTextScale = 1.0f;
    std::uint32_t csRegenColor = 0xB4B4B4;

    // Row 3: Slot customization
    float csDiamondSize = 52.0f;
    std::uint32_t csDiamondBorderColor = 0xC800C8FF;
    std::uint32_t csDiamondFillColor = 0xE614192D;
    std::uint32_t csDiamondEmptyColor = 0xB4505064;
    bool csGlowEnabled = true;
    std::uint32_t csGlowColor = 0x00C8FF;
    std::uint32_t csSlotLabelColor = 0xDCC8A0;
    float csSlotFontScale = 0.85f;
    bool csShowBadges = true;
    float csBadgeTextScale = 0.85f;

    // Row 4: Stats & Resistances
    std::uint32_t csStatLabelColor = 0xB4AFA0;
    std::int32_t csStatIconSize = 24;
    bool csStatGlowEnabled = true;
    std::uint32_t csStatGlowColor = 0xFF5050;
    std::uint32_t csStatBgColor = 0xE60F1428;
    std::int32_t csResistIconSize = 28;
    bool csResistGlowEnabled = true;
    std::uint32_t csResistGlowColor = 0x9664FF;
    std::uint32_t csResistBgColor = 0xE60F1428;
    float csStatFontScale = 1.0f;

    // Row 5: Skills
    std::uint32_t csSkillTextColor = 0xB4AFA0;
    bool csSkillGlowEnabled = true;
    std::uint32_t csSkillGlowColor = 0x3278FF;
    std::uint32_t csSkillBgColor = 0xE60F1428;
    float csSkillFontScale = 1.0f;

    bool csStatusGlowEnabled = true;
    std::uint32_t csStatusGlowColor = 0xB4A064;
    std::uint32_t csStatusBgColor = 0xC8000000;
    float csStatusFontScale = 1.0f;

private:
    Settings() = default;
    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;
};
