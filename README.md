# Skyrim Party Sheet - Follower & Player HUD

**The ultimate HUD for your summons and followers in Skyrim with an inspect feature. The all-in-one HUD for Skyrim to track your party, equipment, active effects, collectibles, and more.**


Skyrim Party Sheet gives you a sleek, real-time HUD for your followers and a powerful way to inspect any character in the world. Whether you want to track your companions' health and magicka at a glance or deep-dive into an NPC's hidden stats and resistances, this mod provides all the info you need in a clean, customizable display.

> For detailed configuration and localization instructions see [TECHNICAL.md](TECHNICAL.md).

---

## Key Features

| Feature                   | Description                                                                                                                                                                                                                                                                                                                                                              |
| ------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **NPC Inspection**        | Quickly view detailed stats, resistances, and active effects of any NPC with a hotkey.                                                                                                                                                                                                                                                                                   |
| **Follower HUD**          | A customizable persistent display for your current followers' health, magicka, and stamina, with optional blacklist/whitelist INI filtering.                                                                                                                                                                                                                             |
| **Party Sheet**           | A dedicated menu for an overview of your entire party's equipment, skills, and status.                                                                                                                                                                                                                                                                                   |
| **Party Sheet V2**        | A fullscreen redesign of the Party Sheet with a character-focused view with a left sidebar of diamond portrait avatars, a center column with a class-themed background, vitals, traits, and skills, and a right column for equipment and stats. Includes modal pop-outs for active effects/spells and a read-only inventory list. Toggle between V1 and V2 via settings. |
| **Character Sheet**       | A full-screen right-side panel showing your player's name, XP, resources, equipment slots, combat stats, resistances, skills, and faction/champion/thane status from JSON-configured data.                                                                                                                                                                               |
| **Player HUD**            | A real-time overlay showing your HP, Magicka, Stamina, XP, and Shout cooldown bars, plus a player portrait.                                                                                                                                                                                                                                                              |
| **Active Effects Widget** | A standalone HUD element tracking your active buffs and debuffs in real-time.                                                                                                                                                                                                                                                                                            |
| **Equipment Widget**      | A live overlay displaying your currently equipped weapons, spells, shields, and ammo.                                                                                                                                                                                                                                                                                    |
| **Resistances Widget**    | A standalone widget that tracks your elemental and magic resistances on screen.                                                                                                                                                                                                                                                                                          |
| **Resources Widget**      | Live HUD tracking for your current gold, carry weight, and in-game time.                                                                                                                                                                                                                                                                                                 |
| **Survival Widget**       | Cold, hunger, fatigue and vitality stage cards for Survival Mode. Reads the Creation Club Survival Mode stages and also Last Seed, SunHelm and Starfrost 2.x. Hidden when no supported survival mod is loaded. |
| **Disease Widget**        | Shows contracted diseases as cards with severity pips, a grouped single-card mode and alarm feedback. Works with vanilla diseases, Last Seed and SunHelm Diseases. |
| **Mount Widget**          | Tracks your horses and mounts to display health, stamina, carry weight, and name. Supports Card and Top portrait layouts.                                                                                                                                                                                                                                                |
| **Collectibles Widget**   | Define item collection goals in JSON and track progress with icons and progress bars right on your HUD, with INI overrides for saved toggles.                                                                                                                                                                                                                            |
| **Pinned Items Widget**   | Pin specific items from your JSON config and see live inventory counts with optional goal bars and badges, with INI overrides for saved toggles.                                                                                                                                                                                                                         |
| **Quest Ally Tracking**   | Automatically detects and displays temporary quest allies in the HUD when they engage in combat alongside you.                                                                                                                                                                                                                                                           |
| **Halo Portraits**        | Circular portrait style with a resource ring for health, magicka and stamina on the Follower HUD, Player HUD and Mount widget. |
| **Summon HUD**           | Optional separate, draggable window for summons with countdown borders that deplete with the summon's remaining time. |
| **HUD Settings**          | Adjust colors, positions, sizes, scale (50-200% per widget and for the Settings window), and visibility of all UI elements through an in-game settings menu.                                                                                                                                                                                                                                                                     |
| **Custom Assets**         | Portraits, backgrounds, frames, and icons can all be customized with your own designs.                                                                                                                                                                                                                                                                                   |
| **Localization Support**  | Support for multiple languages, custom fonts (including CJK), and easy-to-create translation files.                                                                                                                                                                                                                                                                      |
| **Gamepad Support**       | All hotkeys support gamepad button bindings for controller users.                                                                                                                                                                                                                                                                                                        |

---

## Requirements & Compatibility

### Requirements

- **SKSE64**: Built and tested for Skyrim AE (v1.6.1170). Also compatible with SE (v1.5.97) and Skyrim 1.7.104 via Address Library.

### Compatibility

- **Incompatible**: [Skyrim NPC Stats Card](https://www.nexusmods.com/skyrimspecialedition/mods/143527). This mod (Skyrim Party Sheet) is the successor to the original NPC Stats Card. While it includes the core features, some performance-heavy elements (like quest listing) have been intentionally removed to ensure the UI remains lightweight and responsive.
- **Nether's Follower Framework (NFF)**: Compatible. All NFF-managed followers are detected and displayed.
- **Other Follower Mods**: Any follower using standard follower factions will work.
- **True Directional Movement (TDM)**: Full integration for target lock feature.
- **SkyUI**: No conflicts. This mod uses ImGui overlays, not Scaleform/SWF.
- **Camera Mods (SmoothCam, Photo Mode, etc.)**: Compatible. Camera state is preserved when using menus.
- **Combat Gameplay Overhaul (CGO)**: Compatible. Character Sheet accurately displays dual-wielding and 2H weapons.
- **Quest Journal Overhaul & CharacterSheet SE**: Compatible. HUD elements will properly auto-hide when their custom menus are active.
- **PNG Icon Injector**: Optional. The Equipment widget can use its icons through the PNG Icon Injector icon style.
- **Survival mods**: Creation Club Survival Mode, Last Seed, SunHelm and Starfrost 2.x drive the Survival and Disease widgets. Current Party Sheet targets Starfrost 2.x only.
- **Skyrim Souls RE / TweenMenu Overhaul**: Full support. UI can open and operate perfectly over unpaused menus.
- **Generally Compatible**: This mod is built with SKSE and ImGui and does not modify any vanilla game records, scripts, or interface files. It should work alongside almost any other mod, including other HUD and party mods.

---

## Build

Requirements:
- Visual Studio 2022 with the "Desktop development with C++" workload
- [vcpkg](https://github.com/microsoft/vcpkg), with the `VCPKG_ROOT` environment variable set
- CMake 3.21 or newer (the one bundled with Visual Studio works)
- CommonLibSSE-NG v7.5.1 source, built together with the plugin (SE + AE + 1.7.104)

Steps:
1. Clone CommonLibSSE-NG into `extern/`:
   ```text
   git clone --depth 1 --branch v7.5.1 https://github.com/alandtse/CommonLibSSE-NG.git extern/CommonLibSSE-NG
   ```
   Alternatively point the `CommonLibSSEPath` environment variable at an existing checkout.
2. Open the project folder in Visual Studio 2022 (File > Open > Folder).
3. Select the `release` (or `debug`) configure preset and build. vcpkg installs the dependencies
   listed in `vcpkg.json` on the first configure.
4. The result is `SkyrimPartySheet.dll`. To copy it to a mod folder after each build, pass
   `-DOUTPUT_FOLDER=<mod folder>` to CMake; the DLL goes to `<mod folder>/SKSE/Plugins`.

## License

GPL-3.0. See [COPYING](COPYING).

---
## Credits

- **[CommonLibSSE-NG](https://github.com/CharmedBaryon/CommonLibSSE-NG)** - SKSE plugin development framework
- **[Dear ImGui](https://github.com/ocornut/imgui)** - Immediate mode GUI library by Omar Cornut
- **[Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)** - Runtime address resolution by meh321
