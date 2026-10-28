# Skyrim Party Sheet - Configuration & Mechanics

This document covers configuration, file paths, visual styling, custom portraits, icons, and localization.

## Index

- [Inspect Card Mechanics](#inspect-card-mechanics)
- [Creating & Sharing Visual Presets](#creating--sharing-visual-presets)
- [Follower HUD](#follower-hud)
- [Hotkey Configuration](#hotkey-configuration)
- [Localization & Custom Fonts](#localization--custom-fonts)
- [Party Sheet](#party-sheet)
- [Party Sheet V2](#party-sheet-v2)
- [Character Sheet](#character-sheet)
- [Shared Visual Assets](#shared-visual-assets)
- [Player HUD](#player-hud)
- [Stable Mount Widget](#stable-mount-widget)
- [Collectibles & Pinned Items Widgets](#collectibles--pinned-items-widgets)

## Inspect Card Mechanics

The Inspect Card uses your character's **Alteration skill** to progressively reveal an NPC's hidden information. This creates an immersive, RPG-style experience where magical knowledge unlocks deeper insights.

### Progressive Reveal Tiers

| Alteration Level | Information Revealed |
|------------------|---------------------|
| 0+ | Name, Level, Race (always visible) |
| 15+ | Health, Stamina, Magicka, Armor Rating |
| 35+ | All Resistances (Fire, Frost, Shock, Magic, Poison, Disease) |
| 50+ | Relationship status (Friend, Foe, Acquaintance) |

Information you haven't unlocked yet appears as `???`.

### Magicka Drain

While the Inspect Card is open, your magicka drains continuously. This represents the magical focus required to sense another creature's essence.

### INI Configuration

Edit `Data/SKSE/Plugins/PartyUserSettings.ini` under the `[Behavior]` section:

```ini
[Behavior]
RequireAlteration=1
RequiredAlterationLevel=25
MagickaDrainPerSecond=1.0
```

| Setting | Description | Default |
|---------|-------------|---------|
| `RequireAlteration` | `1` = Enable skill requirement + magicka drain, `0` = Disable (show everything) | `1` |
| `RequiredAlterationLevel` | Minimum Alteration skill to use Inspect Card | `25` |
| `MagickaDrainPerSecond` | Magicka drained per second while card is open | `1.0` |

> **Tip:** Set `RequireAlteration=0` if you prefer instant access to all information without skill requirements or magicka cost.

---

## Creating & Sharing Visual Presets

Party Sheet uses two separate INI files, making it easy to share visual themes without overwriting user settings:

| File | Purpose | Share? |
|------|---------|--------|
| `PartyUserSettings.ini` | Hotkeys, behavior, language | ❌ Don't share |
| `PartySheet.ini` | Colors, sizes, scaling | ✅ Share this! |

### Creating a Preset

1. Customize your Party Sheet using the in-game settings menu
2. Copy `Data/SKSE/Plugins/PartySheet.ini` to your mod folder
3. Share it! Users can drop it into their `Data/SKSE/Plugins/` folder

### Preset Contents

`PartySheet.ini` contains:
- **FollowerHUD** - Colors, bar sizes, portrait settings, opacity, borders
- **PartySheet** - Window size, background color, bar colors, icon sizes, scaling
- **InspectCard** - Position, colors, sizes

### For Users

To use a preset:
1. Download the preset's `PartySheet.ini`
2. Place in `Data/SKSE/Plugins/`
3. Your hotkeys, language, and behavior settings stay unchanged!

---

## Follower HUD

Follower HUD tracks followers, summons, and quest allies. It filters actors through `party_blacklist.ini` and `party_whitelist.ini` before rendering, and whitelisted followers can also come from supported follower factions/frameworks such as CC Pets, Spider friends, and standard Follow/Escort/Travel/Accompany packages.

### Modular Filter Files

Quest and follower mods can ship their own filter files without touching the base INIs. Any file matching the following patterns in `Data\SKSE\Plugins\` is automatically loaded and merged:

| Pattern | Purpose |
|---------|---------|
| `*_party_allies.ini` | Additional quest allies to track |
| `*_party_blacklist.ini` | Additional actors to exclude |
| `*_party_whitelist.ini` | Additional actors to include |

For example, a mod named `MyFollowerMod` can ship `MyFollowerMod_party_whitelist.ini` and its followers will appear in the HUD without any user configuration.

### Quest Ally Tracking (Beta)

Quest Ally Tracking automatically detects temporary allies (like Hadvar, Delphine, or Serana during quests) and displays them in the Follower HUD. This feature can be enabled or disabled in the settings menu.

#### Adding Custom Allies

Edit `Data/SKSE/Plugins/party_allies.ini` and add NPC names (one per line):
```
Delphine
Hadvar
Ralof
My Custom Follower
```

> **Note:** The INI is a whitelist filter, not a "show everyone on this list" command. NPCs must still be using an allied AI package (Follow, Escort, Accompany, etc.) to appear.

---

### What It Shows

- Portraits, names, levels, races, and relationship/class info
- Health, magicka, and stamina bars
- Blacklist/whitelist filtering before render
- Separate handling for followers, summons, and quest allies

---

### Quick Heal (Beta)

Quick Heal lets you cast a heal spell on the ally with the lowest HP by pressing a single hotkey. A heal badge appears on that follower's portrait and shifts automatically as HP changes between casts.

The spell allowlist is driven by `Data/SKSE/Plugins/PartySheet_HealSpells.json`. Any compatible restoration spell can be added to the file - the default entry is Heal Other.

| Setting | Description |
|---------|-------------|
| Hotkey | Configurable in-game (default `H`) |
| Target selection | Lowest-HP ally within range and below the minimum HP threshold |
| Magicka floor | Will not cast if your Magicka is below the configured minimum |

---

## Hotkey Configuration

All hotkeys can be customized **in-game** via the Settings Menu (press `X` by default).

### Default Hotkeys

| Function | Default Key |
|----------|-------------|
| Inspect Card | `Y` |
| Party Sheet | `F6` |
| Character Sheet | `U` |
| Settings Menu | `X` |
| HUD Toggle | `V` (toggles all HUD widgets: Follower HUD, Player HUD, Equipment, Effects, Resistances, Resources, Survival, Stable Mount, Collectibles, Pinned Items) |

### Keycodes

Hotkeys use DirectInput keycodes. See [`text-preview/Hotkeys-preview.md`](text-preview/Hotkeys-preview.md) for the current UI mapping.

> **Note:** Key mappings may change as the settings UI evolves, so treat the code and preview doc as the source of truth.

---

## Localization & Custom Fonts

Party Sheet supports multiple languages through translation files and custom fonts.

### Configuration

Edit `Data/SKSE/Plugins/PartySheet.ini`:

```ini
[Localization]
Language=PartySheet_en
FontSize=18
```

| Setting | Description | Default |
|---------|-------------|---------|
| `Language` | Translation file to load (without extension) | `PartySheet_en` |
| `FontSize` | Font size in pixels (12-48) | `18` |

---

### Built-in Language Support

The default font covers Latin, Polish, and Cyrillic glyphs well enough for the standard European translation set.

### Glyph Suffixes

These suffixes trigger special glyph ranges:

| Suffix | Glyphs |
|--------|--------|
| `_zh` | Chinese |
| `_ja` | Japanese |
| `_ko` | Korean |
| `_vi` | Vietnamese |
| `_th` | Thai |
| `_el` | Greek |

> **Note:** These suffixes are the important ones for glyph coverage. Other languages can still use the normal translation pipeline and the default font.

> **Tip:** `_en` is just another valid translation code. You can still pair it with a custom font if you want a non-default English look.

---

```
[Relationships]
Friend=Friend
Foe=Foe
Acquaintance=Acquaintance
Ally=Ally
```

> **Note:** Keep `%d` and `%s` placeholders intact.
> **Note:** The translation file name should match the language code, e.g. `PartySheet_zh.txt`.

---

### Extended Language Support

Set `Language=PartySheet_vi`, `PartySheet_zh`, `PartySheet_ja`, `PartySheet_ko`, `PartySheet_th`, or `PartySheet_el` to get the matching glyph range automatically.

---

### Custom Fonts

For any language, you can ship a matching `.ttf` in `Data/SKSE/Plugins/fonts/`.
The font file should use the same base name as the translation file, e.g. `PartySheet_zh.ttf` for `PartySheet_zh.txt`.

**File Structure:**
```
Data/SKSE/Plugins/
├── fonts/
│   └── PartySheet_zh.ttf     ← Custom font for Chinese
├── translations/
│   └── PartySheet_zh.txt     ← Chinese translation
└── PartySheet.ini
```

**How It Works:**
1. Set `Language=PartySheet_zh` in INI
2. The mod checks for `fonts/PartySheet_zh.ttf`
3. If found, it loads that font with the matching glyph range
4. If not found, it falls back to Segoe UI or the ImGui default font

**Font Requirements:**
- Must be a TrueType Font (.ttf)
- Must include all characters used in your translation
- Recommended: Noto Sans CJK for Chinese/Japanese/Korean

---

### Creating a Translation

**Example: Creating a German translation (`PartySheet_de.txt`)**

1. Navigate to `Data/SKSE/Plugins/translations/`
2. Copy `PartySheet_en.txt` and rename it to `PartySheet_de.txt`
3. Open `PartySheet_de.txt` in a text editor
4. Translate the values (text after the `=` sign), keeping the keys unchanged:
   ```ini
   # Before (English)
   Health=Health
   
   # After (German)
   Health=Gesundheit
   ```
5. Keep all `%d` (number) and `%s` (text) placeholders in your translations
6. **Save as UTF-8 with BOM encoding**
7. If the default font doesn't support your language's characters, add a custom font: `fonts/PartySheet_de.ttf`
8. Edit `PartySheet.ini` and set `Language=PartySheet_de`
9. Restart the game

> ⚠️ **Important:** Translation files must be saved as **UTF-8 with BOM**. If you see English instead of your translation, check the file encoding. Incorrectly encoded files (e.g., ANSI/GBK) will fall back to English automatically.

---

### Custom Font for Any Language

You can also ship a custom font for existing languages:

```
fonts/PartySheet_en.ttf     ← Custom English font (e.g., Roboto)
```

This replaces the default Segoe UI for English users.

---

**Note:** Language changes require a game restart to take effect.

---

### Portraits

Party Sheet supports custom portraits for followers, summons, and creatures.


### Portrait Location

```
Data/Interface/PartySheet/Portraits/
```

### Supported Format

- **JPG preferred, PNG supported**
- Recommended size: **128x128** to **256x256** pixels

### Fallbacks

Portrait lookups use a fallback chain, and each base name is tried as `.jpg` first, then `.png`:

#### For Followers (NPCs)

| Priority | Filename | Example |
|----------|----------|---------|
| 1 | Actor's display name | `Lydia.jpg` / `Lydia.png` |
| 2 | Plugin-FormID (ESL-safe) | `Pigletpig.esp-0x801.jpg` / `.png` |
| 3 | Actor's FormID (8 hex digits) | `00091918.jpg` / `.png` |
| 4 | `female <class>` | `female barbarian.jpg` / `.png` |
| 5 | `<class>` | `barbarian.jpg` / `.png` |
| 6 | `female <race>` | `female nord.jpg` / `.png` |
| 7 | `<race>` | `nord.jpg` / `.png` |
| 8 | Default fallback | `default.jpg` / `default.png` |

#### For Summons & Creatures

| Priority | Filename | Example |
|----------|----------|---------|
| 1 | Actor's display name | `Flame Atronach.jpg` / `.png` |
| 2 | Plugin-FormID (ESL-safe) | `Skyrim.esm-0x10EE43.jpg` / `.png` |
| 3 | Actor's FormID | `0010EE43.jpg` / `.png` |
| 4 | `summon female <race>` | `summon female frost atronach.jpg` / `.png` |
| 5 | `summon <race>` | `summon frost atronach.jpg` / `.png` |
| 6 | Generic summon | `summon.jpg` / `.png` |
| 7 | Default fallback | `default.jpg` / `default.png` |

> **Tip:** Race names are lowercase. Check the game's race names in xEdit if unsure.

> **Tip:** Plugin-FormID format is ideal for portrait packs and ESL mods. It is also language-independent because it uses the plugin name instead of translated display names or the runtime load order index.
> **Note:** Specific summon files like `summon frost atronach.png` are fine for normal English setups, but `summon.png` is the universal fallback.

---

### Examples

```
Data/Interface/PartySheet/Portraits/
├── Lydia.png                          ← Named follower
├── Jenassa.jpg                        ← Named follower
├── Piglet.esp-0x827.png               ← Plugin-FormID (ESL-safe)
├── 00091918.jpg                       ← FormID-based fallback
├── summon frost atronach.png          ← All Frost Atronachs
├── summon female frost atronach.jpg   ← Female Frost Atronach only
├── summon dremora.png                 ← Dremora summons
├── summon.jpg                         ← Generic summon fallback
└── default.png                        ← Ultimate fallback
```

---

### Finding Plugin-FormID (Recommended for ESL Mods and Non-English Users)

The Plugin-FormID format is stable across load orders and ideal for portrait packs.

1. Open the mod's ESP/ESM in xEdit
2. Navigate to the NPC record (under `Non-Player Character (Actor)`)
3. Note the **plugin filename** (e.g., `Piglet.esp`)
4. Note the **local FormID** shown in the FormID column (the last 3-6 hex digits)
5. Combine them with either extension: `Piglet.esp-0x827.jpg` or `Piglet.esp-0x827.png`

---

**Note:** Portraits are cached.

---

## Party Sheet

### Folders

- `Data/Interface/PartySheet/Party/` - Party Sheet icons
- `Data/Interface/PartySheet/Portraits/` - actor portraits
- `Data/Interface/PartySheet/Backgrounds/` - Party Sheet window background and Party member card frames

> **Note:** `Items/` and `Spells/` are used by the Character Sheet, not the Party Sheet.

### Shows

- Current party members from the Follower HUD ally list
- Portrait, name, level, race, relationship, and class
- Health, stamina, and magicka bars
- Equipment rows for weapon, shield, armor, gauntlets, boots, amulet, and ring
- Weapon damage, armor rating, movement speed, resistances, and skills
- Pagination for multiple party members

---

## Party Sheet V2

Party Sheet V2 is a fullscreen redesign of the Party Sheet, activated when `psUseFullscreenMode = 1` in `PartySheet.ini`. It replaces the paginated card layout with a single-subject character view.

### Layout

The window is divided into three columns plus a header and footer:

| Region | Width | Content |
|--------|-------|---------|
| **Sidebar** | ~110 px | Scrollable avatar strip - diamond portraits of the player and all allies; click or use gamepad LB/RB to switch subject |
| **Center column** | ~550 px | Class-themed background image, large portrait, name (hero font), trait line, vitals bars (HP/MP/SP), skills |
| **Right column** | ~460 px | Equipped gear (weapon, shield, armor slots) with stat badges, combat stats, and resistances |
| **Header** | full width | Subject name + condensed vitals |
| **Footer** | full width | Party aggregate stats + navigation controls |

### Class Backgrounds

V2 automatically picks a background image from `Data\Interface\PartySheet\Backgrounds\` based on the subject's race and NPC class. Supported stems:

| Filename stem | Shown for |
|---------------|-----------|
| `class-warrior.png/jpg` | Warrior, Barbarian, Guard, Blade, Scout, Bandit, etc. |
| `class-mage.png/jpg` | Sorcerer, Conjurer, Priest, Spellsword, Fire/Frost/Shock Mage, etc. |
| `class-stealth.png/jpg` | Assassin, Thief, Nightblade, Rogue, Jailor |
| `class-ranged.png/jpg` | Ranger, Fletcher, Bandit Archer, Forsworn Missile |
| `class-undead.png/jpg` | Draugr, Skeleton, Lich, Ghost, Draugr/Dragon Priest classes |
| `class-daedra.png/jpg` | Atronach races, Dremora |
| `class-vampire.png/jpg` | Vampire races |
| `class-werewolf.png/jpg` | Werewolf races |
| `class-creatures.png/jpg` | Dragon, Dwarven, Falmer, Hagraven, Spider, Giant, Horse, Mammoth, Bear, Wolf |
| `class-common.png/jpg` | Fallback for anything not matched above |
| `class-default.png/jpg` | Final fallback if the matched stem file is missing |

### Per-NPC Background Override

You can assign a custom background to a specific NPC by placing an image in `Data\Interface\PartySheet\Backgrounds\` named after the NPC's **Plugin-FormID**. This takes priority over the class-based lookup.

| Priority | Filename | Example |
|----------|----------|---------|
| 1 | Plugin-FormID | `Skyrim.esm-0x91918.png` |
| 2 | Class stem (see table above) | `class-warrior.png` |
| 3 | Common fallback | `class-common.png` |
| 4 | Final fallback | `class-default.png` |

The Plugin-FormID format is the same convention used for portraits: `PluginName.esp-0xFormID.png` or `.jpg`. This is ideal for named followers or modded NPCs where you want a unique background regardless of their class.

> **Tip:** Open the NPC record in xEdit to find the plugin filename and local FormID. Combine them as `Piglet.esp-0x801.png`.

### Modals

Two pop-out modals are available from the center column:

| Modal | Content | Availability |
|-------|---------|-------------|
| **Effects & Spells** | Active magical effects list + known spells for the current subject | All subjects |
| **Inventory** | Read-only inventory list for the current subject | All subjects |
| **Status** | Champions, Thane titles, and Faction standings - mirrors the Character Sheet's Row 6 data | Player only |

### Font Scale Settings (`psV2Font*`)

Four tiers are user-adjustable via settings:

| Setting | Tier | Used for |
|---------|------|---------|
| `psV2FontDisplay` | Display | Hero name (heading font atlas) |
| `psV2FontHeading` | Heading | Section titles (heading font atlas) |
| `psV2FontBody` | Body | Labels, skill values, equipment names |
| `psV2FontSmall` | Small | Fine print, timers, badges |

### Toggling V1 vs V2

Set `psUseFullscreenMode` in `PartySheet.ini` (or via the in-game Settings → Party Sheet section). Both implementations share the same hotkey.

---

## Character Sheet

### Folders

- `Data/Interface/PartySheet/Items/` - item icons for weapons and armor
- `Data/Interface/PartySheet/Spells/` - spell icons and `SpellIcons.json`
- `Data/Interface/PartySheet/Status/` - faction, champion, and thane icons
- `Data/Interface/PartySheet/Backgrounds/` - Character Sheet background

### Shows

- Player name, race, level, XP, shout recovery, and core stats
- Equipped weapons, armor, spells, ammo, and slot badges
- Armor, damage, speed, resistances, and skill rows
- Faction, champion, and thane status carousel

### Data Files

- `Data/SKSE/Plugins/PartySheet_Status.json` - faction/champion/thane tracking
- `Data/Interface/PartySheet/Spells/SpellIcons.json` - spell icon mapping

### Notes

- `Items/` has a strong category-specific fallback chain
- `Spells/` has a strong category-specific fallback chain
- `Status/` uses the `key` field from `PartySheet_Status.json`

## Shared Visual Assets

These folders are shared across multiple feature sections, but the lookup rules are still feature-specific.

### Backgrounds and Bar Textures

You can easily customize the appearance of the Party Sheet, Party Cards, Inspect Cards, and Character Sheet by adding your own background images.

### Installation Location

Backgrounds and portrait frames go in:
```
Data/Interface/PartySheet/Backgrounds/
```

Custom bar style textures go in:
```
Data/Interface/PartySheet/Bars/
```

### Supported File Types
- `.png` (Recommended)
- `.jpg` / `.jpeg`
- `.dds` (Best performance)
- `.bmp`

### Naming Convention - Backgrounds & Frames
The mod recognizes images based on their filename prefix.

| UI Element | Filename Prefix | Orientation | Example |
| :--- | :--- | :--- | :--- |
| **Party Window** | `PartySheet-` | Landscape (16:9) | `PartySheet-Wood.png` |
| **Party Member Card** | `PartyCard-` | Vertical | `PartyCard-Dark.png` |
| **Inspect Card** | `InspectCard-` | Vertical | `InspectCard-Paper.dds` |
| **Character Sheet** | `CharacterSheet-` | Vertical | `CharacterSheet-Viking.jpg` |
| **Player HUD Portrait Frame** | `Frame-` | Square | `Frame-01.png` |

> **Note:** The part after the hyphen `-` can be anything (numbers, names, etc.).
> **Note:** `Frame-` textures are used by the Player HUD portrait frame overlay.

### Naming Convention - Bar Style Textures

Bar style textures let you replace the flat/gradient fill bars with your own images across the Follower HUD, Player HUD, Stable Mount Widget, Collectibles, and Pinned Items widgets.

Place matching pairs in the `Bars/` folder:

| Filename Pattern | Purpose |
|-----------------|---------|
| `BarStyle_1.png` | Bar background texture for style slot 1 |
| `BarStyleFill_1.png` | Bar fill texture for style slot 1 |
| `BarStyle_2.png` | Bar background texture for style slot 2 |
| `BarStyleFill_2.png` | Bar fill texture for style slot 2 |

The number can be any index. Each widget has its own **bar style index** setting that picks which slot to use. The background and fill textures are drawn layered (bg first, fill on top clipped to the current value).

### Resolution & Dimensions

#### Party Window (`PartySheet-`)
- **Recommended:** 1920x1080 (or your screen resolution).

#### Party Member Card (`PartyCard-`)
- **Dynamic Size:** Changes based on settings.
- **How to find your size:** Check the **"Card Size: [Width]x[Height]"** tooltip in Mod Settings.

#### Inspect Card (`InspectCard-`)
- **Recommended:** Approx. 500x800 (Vertical).

#### Character Sheet (`CharacterSheet-`)
- **Recommended:** Screen Height (e.g., 1080px height).

#### Player HUD Portrait Frame (`Frame-`)
- **Recommended:** Square, same size as your portrait setting (e.g., 128x128).

### Usage In-Game
1. Install images into the appropriate folder (`Backgrounds/` or `Bars/`).
2. Open **Party Sheet Settings**.
3. Enable **"Use Custom"** for the relevant section.
4. Click the **Image Button** to select from the visual grid!

---

## Player HUD

The Player HUD is a real-time overlay for the player character, split into independent sub-widgets that can each be positioned and toggled separately. Its portrait frame and bar styling use the same shared managers, but the status layout is unique to the player overlay.

### Sub-Widgets

| Widget | Description |
|--------|-------------|
| **Status** | HP, Magicka, Stamina, XP, and Shout cooldown bars + player portrait + frame overlay |
| **Effects** | Active effect icons with optional timer, magnitude, and category filtering |
| **Equipment** | Equipped item slot icons (weapons, armor, ammo, shout) with stat badges and strong category-specific fallbacks |
| **Resistances** | Resist icons (Fire, Frost, Shock, Magic, Poison, Disease) with current values |
| **Resources** | Gold, Carry Weight, current in-game time, and weather. Shows current location name instead of weather when indoors. |
| **Survival** | Cold, Hunger, Fatigue and Vitality stage cards. Backends: the Survival Mode CC addon (`ccqdrsse001-survivalmode.esl`), Last Seed, SunHelm and Starfrost 2.x. Hidden automatically if none is present. |
| **Disease** | Contracted-disease cards with severity pips; vanilla, Last Seed and SunHelm Diseases. Optional grouped single-card mode. |

### Bar Penalties

The Status widget's HP, Magicka, and Stamina bars can display penalty segments that visually reduce the effective bar length. These overlay on top of the normal fill to show how much of a stat is being eaten by a persistent condition:

| Penalty | Bar affected | Source |
|---------|-------------|--------|
| Survival hunger / cold / fatigue | HP, Stamina, Magicka | Survival Mode stage effects |
| Vampirism | HP, Magicka, Stamina | Vampire lord / feeding stage |
| Disease / curse | Any bar | Active disease or curse effects |

Penalty segments are drawn as a darkened overlay on the right side of the bar fill, so the remaining usable portion is immediately visible. When a penalty is active the bar's denominator shows the **unpenalized max** (e.g. `195 / 220`) so the true ceiling is always visible.

### Dynamic Bar Behaviors

| Option | Description |
|--------|-------------|
| **Dynamic Bars** | HP / Magicka / Stamina bars fade out when full and idle out of combat. They snap back instantly on weapon draw, combat entry, damage, regen, casting, or sprint. Off by default. |
| **Scale Bar Width to Stat** | Each bar's width scales proportionally with its stat's max value. 100 points equals the configured Bar Width; below 100 shrinks it, above 100 grows it (capped at 3x Bar Width). Bars scale independently. Off by default. |
| **Empty Bar Glow** | Magicka and Stamina bars pulse with a brightened tint of their bar color when fully depleted, making it visually clear when a resource is exhausted. |

### Transform State Handling

The Player HUD adapts when the player enters a transform state. Werewolf, Vampire Lord, and Werebear forms each show the correct portrait and hide bars that are irrelevant for that form. Werebear support requires the Dragonborn DLC (`DLC2WerebearBeastRace`).

### Configuration

All Player HUD settings use the `ph*` prefix in `PartySheet.ini`. Each sub-widget has its own enable toggle, position, scale, color, and icon size settings, all accessible via the in-game Settings Menu under **Player HUD**.

---

## Stable Mount Widget

The Stable Mount Widget tracks your horses and rideable mounts and displays their status on a persistent HUD overlay.

### Features

- Shows **Health**, **Stamina**, and **Carry Weight** bars per mount
- Displays **mount name** with optional first-word-only shortening
- Supports **Card** (portrait left/top, bars alongside) and **Top** (portrait top, side bars) layouts
- **Low-health pulse border**: slow pulse below 80%, fast pulse below 25%
- **HorseRegistry**: horses are persisted across sessions to `PartyHorses.ini` so your mounts are remembered after fast travel or cell changes

### Configuration

Settings use the `sm*` prefix in `PartySheet.ini`.

---

## Collectibles & Pinned Items Widgets

Both widgets are driven by a shared JSON config file and display live inventory counts on your HUD.

### Config File

**Path:** `Data/SKSE/Plugins/PartySheet_Widgets.json`

```json
{
  "collectibles": [
    {
      "id": "dragonclaws",
      "label": "Dragon Claws",
      "goal": 10,
      "enabled": true,
      "items": [
        { "mod": "Skyrim.esm", "formId": "0x3ADA3" }
      ]
    }
  ],
  "pinned": [
    {
      "id": "lockpicks",
      "label": "Lockpicks",
      "goal": 50,
      "enabled": true,
      "items": [
        { "mod": "Skyrim.esm", "formId": "0xA" }
      ]
    }
  ]
}
```

Each entry supports multiple `items[]` (all counts are summed), an optional `quest` completion check (Collectibles only), and a label override via localization key `Widgets.<id>`.

### Collectibles & Pinned Icons

These icons are for the Collectibles and Pinned Items widgets only.

Place a `.png` named after the entry `id` in:
```
Data/Interface/PartySheet/Widgets/<id>.png
```

### Collectibles Widget

Tracks item collections with an optional quest completion state. When the quest is completed the entry turns gold. Settings use the `cc*` prefix.

### Pinned Items Widget

Tracks specific items with a count/goal display. Per-item enabled state is written immediately to `PartySheet.ini → [PinnedItems] → <id>_enabled` when toggled - no Save required. Settings use the `pt*` prefix.

---

## Debug & Developer Options

A `[Debug]` section in `PartyUserSettings.ini` enables opt-in logging and performance instrumentation. These are off by default and intended for troubleshooting or mod development.

```ini
[Debug]
EnableCSLogging = 1
EnableVerboseLogging = 1
EnablePerformanceStats = 1
```

| Setting | Description |
|---------|-------------|
| `EnableCSLogging` | Enables verbose Character Sheet logging to `PartySheet.log` |
| `EnableVerboseLogging` | Switches the log to debug level so every `logger::debug` line appears |
| `EnablePerformanceStats` | Enables per-frame render cost counters across all UI surfaces and widgets (visible in log) |

> **Note:** The runtime log is written to `C:\Users\<name>\Documents\My Games\Skyrim Special Edition\SKSE\PartySheet.log`. Default log level shows `info` and above; trace-level entries require debug builds.
