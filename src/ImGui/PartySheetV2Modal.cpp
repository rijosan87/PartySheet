#include "../../includes/ImGui/PartySheetV2Modal.h"
#include "../../includes/ImGui/Renderer.h"
#include "../../includes/Settings.h"
#include "../../includes/TextureManager.h"
#include "../../includes/SpellIconManager.h"
#include "../../includes/EffectCategories.h"
#include "../../includes/Utils/EffectIconRenderer.h"
#include "../../includes/LocalizationManager.h"
#include "../../includes/ImGui/PlayerHUDDisease.h"
#include "../../includes/ImGui/PlayerHUDSurvival.h"
#include "../../includes/Utils/HUDDraw.h"

#include <imgui.h>
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <cfloat>

namespace logger = SKSE::log;

namespace PartySheetV2 {

    static bool s_ModalOpen = false;

    void OpenEffectsModal() {
        s_ModalOpen = true;
        logger::info("EffectsModal: Opened");
    }
    void CloseEffectsModal() {
        if (s_ModalOpen)
            logger::info("EffectsModal: Closed");
        s_ModalOpen = false;
    }
    bool IsEffectsModalOpen() { return s_ModalOpen; }

    // ── Font scale helpers (mirrors PartySheetV2.cpp) ────────────────────────
    static float s_EffectiveScale = 1.0f;
    static float FontDisplay() { auto* s = Settings::GetSingleton(); return s->psV2FontDisplay * s_EffectiveScale; }
    static float FontHeading() { auto* s = Settings::GetSingleton(); return s->psV2FontHeading * s_EffectiveScale; }
    static float FontBody()    { auto* s = Settings::GetSingleton(); return s->psV2FontBody    * s_EffectiveScale; }
    static float FontSmall()   { auto* s = Settings::GetSingleton(); return s->psV2FontSmall   * s_EffectiveScale; }

    static constexpr ImU32 kAccent      = IM_COL32(212, 175,  55, 255);
    static constexpr ImU32 kTextMain    = IM_COL32(240, 240, 240, 255);
    static constexpr ImU32 kTextMuted   = IM_COL32(136, 136, 136, 255);
    static constexpr ImU32 kBorderDim   = IM_COL32( 50,  50,  60, 255);
    static constexpr ImU32 kBuff        = IM_COL32(100, 200, 120, 255);
    static constexpr ImU32 kDebuff      = IM_COL32(220,  80,  80, 255);
    static constexpr ImU32 kOverlayBg   = IM_COL32(  0,   0,   0, 180);

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

    static void DrawIconFit(ImDrawList* dl, ID3D11ShaderResourceView* srv,
                            float boxX, float boxY, float boxSz) {
        if (!srv) return;
        float w = 0.0f, h = 0.0f;
        if (auto* res = [&]() -> ID3D11Resource* { ID3D11Resource* r = nullptr; srv->GetResource(&r); return r; }()) {
            if (auto* tex = [&]() -> ID3D11Texture2D* { ID3D11Texture2D* t = nullptr;
                    res->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&t)); return t; }()) {
                D3D11_TEXTURE2D_DESC d{};
                tex->GetDesc(&d);
                w = static_cast<float>(d.Width);
                h = static_cast<float>(d.Height);
                tex->Release();
            }
            res->Release();
        }
        if (w <= 0.0f || h <= 0.0f) {
            HUDDraw::DrawIconStyled(dl, ImVec2(boxX, boxY), boxSz,
                reinterpret_cast<ImTextureID>(srv), 0xFFFFFFFF, IconFxDefault());
            return;
        }
        float scale = boxSz / (std::max)(w, h);
        float dw = w * scale, dh = h * scale;
        float ox = boxX + (boxSz - dw) * 0.5f;
        float oy = boxY + (boxSz - dh) * 0.5f;
        HUDDraw::DrawIconStyled(dl, ImVec2(ox, oy), ImVec2(dw, dh),
            reinterpret_cast<ImTextureID>(srv), 0xFFFFFFFF, IconFxDefault());
    }

    static float DrawModalSectionTitle(ImDrawList* dl, ImFont* headFont,
                                       float ox, float oy, float colW,
                                       const char* title, float sc) {
        if (headFont) ImGui::PushFont(headFont);
        ImGui::SetWindowFontScale(FontHeading());
        ImVec2 tsz = ImGui::CalcTextSize(title);
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
            ImVec2(ox, oy), kAccent, title);
        ImGui::SetWindowFontScale(1.0f);
        if (headFont) ImGui::PopFont();

        float sepY = oy + tsz.y + 3.0f * sc;
        dl->AddRectFilledMultiColor(
            ImVec2(ox,                   sepY),
            ImVec2(ox + colW * 0.65f,    sepY + 1.0f),
            kAccent, IM_COL32(212,175,55,0), IM_COL32(0,0,0,0), IM_COL32(0,0,0,0));
        return sepY + 7.0f * sc;
    }

    struct EffectEntry {
        std::string source;
        std::string name;
        std::string valStr;
        std::string iconId;
        bool        isHostile     = false;
        float       timeRemaining = -1.0f;
        RE::SpellItem* sourceSpell = nullptr;
        RE::ActorValue primaryAV = RE::ActorValue::kNone;
    };

    static std::string FormatMagnitude(float magnitude, RE::ActorValue primaryAV, bool isHostile) {
        if (magnitude <= 0.0f) return "";
        bool isPct = EffectCategories::IsPercentageMagnitude(primaryAV);
        char buf[32];
        if (isPct)
            snprintf(buf, sizeof(buf), "%s%.0f%%", isHostile ? "-" : "+", magnitude);
        else
            snprintf(buf, sizeof(buf), "%s%.0f", isHostile ? "-" : "+", magnitude);
        return buf;
    }

    static bool IsKye2ArmorSource(RE::MagicItem* spell) {
        if (!spell) return false;
        auto* file = spell->GetFile(0);
        if (!file) return false;
        std::string_view fn = file->GetFilename();
        constexpr std::string_view kKye2Armors = "know_your_enemy_2_armors.esp";
        if (fn.size() != kKye2Armors.size()) return false;
        for (size_t i = 0; i < fn.size(); ++i)
            if (std::tolower(static_cast<unsigned char>(fn[i])) !=
                std::tolower(static_cast<unsigned char>(kKye2Armors[i])))
                return false;
        return true;
    }

    static bool IsStandingStoneSource(RE::SpellItem* sp);

    enum class EffectBucket { Passive, Equipment, Blessing };

    static std::vector<EffectEntry> GetPermanentEffects(RE::Actor* actor, EffectBucket bucket) {
        std::vector<EffectEntry> out;
        if (!actor) return out;
        auto* mt = actor->AsMagicTarget();
        if (!mt) return out;
        auto* list = mt->GetActiveEffectList();
        if (!list) return out;

        std::unordered_set<RE::SpellItem*> raceSpells;
        if (bucket == EffectBucket::Passive) {
            if (auto* race = actor->GetRace()) {
                if (auto* ae = race->actorEffects; ae && ae->spells) {
                    for (std::uint32_t i = 0; i < ae->numSpells; ++i)
                        if (auto* sp = ae->spells[i]) raceSpells.insert(sp);
                }
            }
        }

        for (auto* ae : *list) {
            if (!ae) continue;
            if (ae->flags.any(RE::ActiveEffect::Flag::kDispelled) ||
                ae->flags.any(RE::ActiveEffect::Flag::kInactive))   continue;
            auto* base = ae->GetBaseObject();
            if (!base) continue;
            if (base->data.flags.any(RE::EffectSetting::EffectSettingData::Flag::kHideInUI)) continue;
            const char* nm = base->GetFullName();
            if (!nm || !nm[0]) continue;
            auto src = EffectCategories::GetEffectSource(ae);
            bool match = false;
            switch (bucket) {
                case EffectBucket::Passive:
                    match = (src == EffectCategories::EffectSource::Ability) && ae->duration <= 0.0f;
                    if (match && base->data.primaryAV == RE::ActorValue::kNone &&
                        !(ae->spell && IsStandingStoneSource(ae->spell->As<RE::SpellItem>())))
                        match = false;
                    if (match && ae->spell) {
                        if (auto* sp = ae->spell->As<RE::SpellItem>(); sp && raceSpells.count(sp))
                            match = false;
                    }
                    if (match && IsKye2ArmorSource(ae->spell))
                        match = false;
                    if (match && ae->spell) {
                        if (auto* sp = ae->spell->As<RE::SpellItem>();
                            sp && (PlayerHUDSurvival::IsNeedSpell(sp) ||
                                   PlayerHUDSurvival::IsVitalitySpell(sp)))
                            match = false;
                    }
                    break;
                case EffectBucket::Equipment:
                    match = ((src == EffectCategories::EffectSource::Enchantment) && ae->duration <= 0.0f)
                          || IsKye2ArmorSource(ae->spell);
                    break;
                case EffectBucket::Blessing:
                    match = (src == EffectCategories::EffectSource::Blessing);
                    break;
            }
            if (!match) continue;

            bool isHostile = base->IsHostile() || base->IsDetrimental() ||
                             (ae->magnitude < 0.0f);

            std::string srcName;
            RE::SpellItem* srcSpell = ae->spell ? ae->spell->As<RE::SpellItem>() : nullptr;
            if (ae->spell) {
                const char* sn = ae->spell->GetFullName();
                if (sn && sn[0]) srcName = sn;
            }

            float mag = std::abs(ae->magnitude);
            if (mag <= 0.0f && ae->effect) {
                mag = std::abs(ae->effect->GetMagnitude());
            }
            bool isPct = EffectCategories::IsPercentageMagnitude(base->data.primaryAV);
            if (isPct && mag > 0.0f && mag < 1.0f) {
                mag *= 100.0f;
            }

            EffectEntry entry;
            entry.source      = srcName;
            entry.name        = nm;
            entry.valStr      = FormatMagnitude(mag, base->data.primaryAV, isHostile);
            entry.isHostile   = isHostile;
            entry.sourceSpell = srcSpell;
            entry.primaryAV   = base->data.primaryAV;
            out.push_back(entry);
        }
        return out;
    }

    static std::vector<EffectEntry> GetActive(RE::Actor* actor) {
        auto infos = EffectIconRenderer::GetActiveEffectsInfo(actor, 64, 0, 0);
        std::vector<EffectEntry> out;
        for (auto& e : infos) {
            EffectEntry entry;
            entry.source        = e.sourceName;
            entry.name          = e.name;
            entry.valStr        = FormatMagnitude(e.magnitude, e.primaryAV, e.isHostile);
            entry.iconId        = e.iconId;
            entry.isHostile     = e.isHostile;
            entry.timeRemaining = e.timeRemaining;
            out.push_back(entry);
        }
        return out;
    }

    struct SpellSchool {
        RE::ActorValue          skill = RE::ActorValue::kNone;
        std::vector<std::string> names;
    };

    static std::vector<SpellSchool> GetKnownSpells(RE::Actor* actor) {
        if (!actor) return {};

        std::vector<RE::SpellItem*> spells;

        if (auto* base = actor->GetActorBase()) {
            if (base->actorEffects && base->actorEffects->spells && base->actorEffects->numSpells > 0) {
                for (std::uint32_t i = 0; i < base->actorEffects->numSpells; ++i) {
                    if (auto* sp = base->actorEffects->spells[i])
                        spells.push_back(sp);
                }
            }
        }

        using ST = RE::MagicSystem::SpellType;
        std::vector<RE::SpellItem*> filtered;
        for (auto* s : spells) {
            auto t = s->GetSpellType();
            if (t == ST::kSpell) filtered.push_back(s);
        }

        std::vector<SpellSchool> schools;
        for (auto* s : filtered) {
            const char* nm = s->GetFullName();
            if (!nm || !nm[0]) continue;
            RE::ActorValue skill = s->GetAssociatedSkill();

            auto it = std::find_if(schools.begin(), schools.end(),
                [&](const SpellSchool& sc) { return sc.skill == skill; });
            if (it == schools.end()) {
                schools.push_back({ skill, { nm } });
            } else {
                if (std::find(it->names.begin(), it->names.end(), nm) == it->names.end())
                    it->names.push_back(nm);
            }
        }

        std::sort(schools.begin(), schools.end(),
            [](const SpellSchool& a, const SpellSchool& b) { return a.names.size() > b.names.size(); });

        return schools;
    }

    static ID3D11ShaderResourceView* ResolveSchoolIcon(RE::ActorValue skill) {
        const char* stem = "magic";
        switch (skill) {
            case RE::ActorValue::kAlteration:  stem = "alteration";  break;
            case RE::ActorValue::kConjuration: stem = "conjuration"; break;
            case RE::ActorValue::kDestruction: stem = "destruction"; break;
            case RE::ActorValue::kIllusion:    stem = "illusion";    break;
            case RE::ActorValue::kRestoration: stem = "restoration"; break;
            default: break;
        }
        return TextureManager::GetPartyIcon(stem);
    }

    static ID3D11ShaderResourceView* ResolveMagicIcon(RE::MagicItem* item) {
        if (!item) return nullptr;
        RE::EffectSetting* base = item->GetAVEffect();
        if (!base) {
            if (item->effects.empty() || !item->effects[0]) return nullptr;
            base = item->effects[0]->baseEffect;
        }
        if (!base) return nullptr;

        std::string iconId;
        auto effectType = EffectCategories::GetEffectType(base->GetArchetype());
        if (effectType == EffectCategories::EffectType::Standalone)
            iconId = EffectCategories::GetStandaloneIconId(base->GetArchetype());
        else if (effectType == EffectCategories::EffectType::Modifier)
            iconId = EffectCategories::GetActorValueIconId(base->data.primaryAV);
        else {
            const char* nm = base->GetFullName();
            iconId = EffectCategories::GetScriptEffectIconId(nm ? nm : "", base->data.associatedSkill);
        }
        if (iconId.empty()) return nullptr;
        return TextureManager::GetEffectIcon(iconId);
    }

    static std::string LocalizedAVName(RE::ActorValue av) {
        using AV = RE::ActorValue;
        auto L = [](const char* key, const char* eng) {
            std::string v = LocalizationManager::Get(key);
            return (v.empty() || v == key) ? std::string(eng) : v;
        };
        switch (av) {
            case AV::kHealth:  return L("Health",  "Health");
            case AV::kMagicka: return L("Magicka", "Magicka");
            case AV::kStamina: return L("Stamina", "Stamina");
            case AV::kHealRate:        return L("HealthRegen",     "Health Regen");
            case AV::kMagickaRate:     return L("MagickaRegen",    "Magicka Regen");
            case AV::kStaminaRate:     return L("StaminaRegen",    "Stamina Regen");
            case AV::kHealRateMult:    return L("HealthRegenMult", "Health Regen Mult");
            case AV::kMagickaRateMult: return L("MagickaRegenMult","Magicka Regen Mult");
            case AV::kStaminaRateMult: return L("StaminaRegenMult","Stamina Regen Mult");
            case AV::kResistFire:    return L("Resistances.Fire",    "Fire Resist");
            case AV::kResistFrost:   return L("Resistances.Frost",   "Frost Resist");
            case AV::kResistShock:   return L("Resistances.Shock",   "Shock Resist");
            case AV::kResistMagic:   return L("Resistances.Magic",   "Magic Resist");
            case AV::kPoisonResist:  return L("Resistances.Poison",  "Poison Resist");
            case AV::kResistDisease: return L("Resistances.Disease", "Disease Resist");
            case AV::kOneHanded:
            case AV::kOneHandedModifier:   return L("OneHanded",   "One-Handed");
            case AV::kTwoHanded:
            case AV::kTwoHandedModifier:   return L("TwoHanded",   "Two-Handed");
            case AV::kArchery:
            case AV::kMarksmanModifier:    return L("Archery",     "Archery");
            case AV::kBlock:
            case AV::kBlockModifier:       return L("Block",       "Block");
            case AV::kSmithing:
            case AV::kSmithingModifier:    return L("Smithing",    "Smithing");
            case AV::kHeavyArmor:
            case AV::kHeavyArmorModifier:  return L("HeavyArmor",  "Heavy Armor");
            case AV::kLightArmor:
            case AV::kLightArmorModifier:  return L("LightArmor",  "Light Armor");
            case AV::kPickpocket:
            case AV::kPickpocketModifier:  return L("Pickpocket",  "Pickpocket");
            case AV::kLockpicking:
            case AV::kLockpickingModifier: return L("Lockpicking", "Lockpicking");
            case AV::kSneak:
            case AV::kSneakingModifier:    return L("Sneak",       "Sneak");
            case AV::kAlchemy:
            case AV::kAlchemyModifier:     return L("Alchemy",     "Alchemy");
            case AV::kSpeech:
            case AV::kSpeechcraftModifier: return L("Speech",      "Speech");
            case AV::kAlteration:
            case AV::kAlterationModifier:  return L("Alteration",  "Alteration");
            case AV::kConjuration:
            case AV::kConjurationModifier: return L("Conjuration", "Conjuration");
            case AV::kDestruction:
            case AV::kDestructionModifier: return L("Destruction", "Destruction");
            case AV::kIllusion:
            case AV::kIllusionModifier:    return L("Illusion",    "Illusion");
            case AV::kRestoration:
            case AV::kRestorationModifier: return L("Restoration", "Restoration");
            case AV::kEnchanting:
            case AV::kEnchantingModifier:  return L("Enchanting",  "Enchanting");
            default:
                if (const char* n = EffectCategories::GetActorValueName(av); n && n[0] && std::strcmp(n, "Unknown") != 0)
                    return n;
                return "";
        }
    }

    static std::string RaceMagnitudeLine(RE::MagicItem* item) {
        if (!item) return "";
        RE::EffectSetting* base = item->GetAVEffect();
        RE::Effect* eff = item->effects.empty() ? nullptr : item->effects[0];
        if (!base && eff) base = eff->baseEffect;
        if (!base) return "";

        RE::ActorValue av = base->data.primaryAV;
        if (av == RE::ActorValue::kNone) return "";

        float mag = eff ? eff->GetMagnitude() : 0.0f;
        if (mag <= 0.0f) return "";

        std::string label = LocalizedAVName(av);
        if (label.empty()) return "";

        const bool isPct = EffectCategories::IsPercentageMagnitude(av);
        char buf[96];
        if (isPct) snprintf(buf, sizeof(buf), "%s +%.0f%%", label.c_str(), mag);
        else       snprintf(buf, sizeof(buf), "%s +%.0f",   label.c_str(), mag);
        return buf;
    }

    static ID3D11ShaderResourceView* ResolveBlessingIcon(RE::SpellItem* spell) {
        if (spell) {
            static const std::unordered_map<std::string, const char*> kOverrides = {
            };
            if (const char* ed = spell->GetFormEditorID(); ed && ed[0]) {
                auto it = kOverrides.find(ed);
                if (it != kOverrides.end()) {
                    if (auto* tex = TextureManager::GetPartyIcon(it->second))
                        return tex;
                }
            }
        }
        return TextureManager::GetPartyIcon("blessing");
    }

    struct RaceEntry {
        std::string name;
        std::string typeLabel;
        std::string magLine;
        ID3D11ShaderResourceView* icon = nullptr;
    };

    static std::vector<RaceEntry> GetRaceAbilities(RE::Actor* actor) {
        std::vector<RaceEntry> out;
        if (!actor) return out;
        auto* race = actor->GetRace();
        if (!race) return out;
        auto* ae = race->actorEffects;
        if (!ae || !ae->spells || ae->numSpells == 0) return out;

        using ST = RE::MagicSystem::SpellType;
        for (std::uint32_t i = 0; i < ae->numSpells; ++i) {
            auto* sp = ae->spells[i];
            if (!sp) continue;
            const char* nm = sp->GetFullName();
            if (!nm || !nm[0]) continue;
            const char* label = nullptr;
            std::string typeStr;
            auto spellType = sp->GetSpellType();
            switch (spellType) {
                case ST::kAbility:     typeStr = LocalizationManager::Get("PartySheetV2.SpellTypeAbility");     break;
                case ST::kPower:       typeStr = LocalizationManager::Get("PartySheetV2.SpellTypePower");       break;
                case ST::kLesserPower: typeStr = LocalizationManager::Get("PartySheetV2.SpellTypeLesserPower"); break;
                case ST::kSpell:       typeStr = LocalizationManager::Get("PartySheetV2.SpellTypeSpell");       break;
                default:               break;
            }
            if (typeStr.empty()) continue;

            const bool isPower = (spellType == ST::kPower || spellType == ST::kLesserPower);
            ID3D11ShaderResourceView* icon = isPower
                ? TextureManager::GetPartyIcon("power")
                : ResolveMagicIcon(sp);

            std::string magLine = isPower ? std::string() : RaceMagnitudeLine(sp);

            out.push_back({ nm, typeStr, std::move(magLine), icon });
        }
        return out;
    }

    static bool IsStandingStoneSource(RE::SpellItem* sp) {
        if (!sp) return false;
        switch (sp->GetFormID()) {
            case 0x000E5F4C:
            case 0x000E5F47:
            case 0x000E5F45:
            case 0x000E5F58:
            case 0x000E5F54:
            case 0x000E5F4E:
            case 0x0010E439:
            case 0x00103A94:
            case 0x000E5F51:
            case 0x000E5F5A:
            case 0x000E7328:
            // Ritual/Serpent/Shadow are POWERS in vanilla (activated, never reach the passive
            case 0x000E7329:
            case 0x000E5F61:
            case 0x000E732A:
                return true;
            default:
                break;
        }
        if (auto* dh = RE::TESDataHandler::GetSingleton()) {
            if (auto* steed = dh->LookupForm(0x000E5F5E, "ccQDRSSE001-SurvivalMode.esl"))
                if (sp == steed) return true;
        }
        return false;
    }

    static std::string StoneLineText(const EffectEntry& e, const std::string& stoneTitle) {
        bool echoesTitle = !e.name.empty() &&
            (e.name == stoneTitle || e.name.rfind(stoneTitle, 0) == 0);
        if (echoesTitle && e.primaryAV != RE::ActorValue::kNone) {
            std::string label = LocalizedAVName(e.primaryAV);
            if (!label.empty()) return label;
        }
        return e.name;
    }

    static std::string ResolveStoneDescription(RE::SpellItem* stone) {
        if (!stone) return {};
        for (auto* eff : stone->effects) {
            if (!eff || !eff->baseEffect) continue;
            const char* d = eff->baseEffect->magicItemDescription.c_str();
            if (!d || !d[0]) continue;
            std::string out = d;
            out.erase(std::remove_if(out.begin(), out.end(),
                [](char c){ return c == '<' || c == '>'; }), out.end());
            return out;
        }
        return {};
    }

    void RenderEffectsModal(RE::Actor* actor) {
        if (!s_ModalOpen) return;
        if (!actor) return;

        auto* settings = Settings::GetSingleton();
        auto& io       = ImGui::GetIO();
        const float fitScale = (std::min)(io.DisplaySize.x * 0.95f / 820.0f, io.DisplaySize.y * 0.90f / 620.0f);
        const float sc = (std::min)(settings->psV2Scale, fitScale);
        s_EffectiveScale = sc;

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("##psv2_modal_overlay", nullptr,
            ImGuiWindowFlags_NoTitleBar            | ImGuiWindowFlags_NoResize     |
            ImGuiWindowFlags_NoMove                | ImGuiWindowFlags_NoScrollbar  |
            ImGuiWindowFlags_NoCollapse            | ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav        |
            ImGuiWindowFlags_NoInputs);
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(0, 0), io.DisplaySize, kOverlayBg);
        ImGui::End();
        ImGui::PopStyleVar();

        const float modalW = 820.0f * sc;
        const float modalH = 620.0f * sc;
        ImVec2 modalPos(
            (io.DisplaySize.x - modalW) * 0.5f,
            (io.DisplaySize.y - modalH) * 0.5f);

        ImGui::SetNextWindowPos(modalPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(modalW, modalH), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(1.0f);
        ImGui::SetNextWindowFocus();
        ImGui::PushStyleColor(ImGuiCol_WindowBg,      ImVec4(9/255.f, 9/255.f, 11/255.f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_Border,         ImVec4(212/255.f, 175/255.f, 55/255.f, 0.4f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(20.0f * sc, 16.0f * sc));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

        ImGui::Begin("##psv2_effects_modal", nullptr,
            ImGuiWindowFlags_NoTitleBar  | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove      | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoCollapse  | ImGuiWindowFlags_NoNav);

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);

        ImDrawList* dl    = ImGui::GetWindowDrawList();
        ImVec2      wPos  = ImGui::GetWindowPos();
        ImFont*     headFont = ImGuiRenderer::GetHeadingFont();

        float curY = wPos.y + 16.0f * sc;
        float curX = wPos.x + 20.0f * sc;

        const char* actorName = actor->GetDisplayFullName();
        static std::string s_unknown;
        if (s_unknown.empty()) s_unknown = LocalizationManager::Get("Common.Unknown");
        if (!actorName || !actorName[0]) actorName = s_unknown.c_str();

        if (headFont) ImGui::PushFont(headFont);
        ImGui::SetWindowFontScale(FontDisplay());
        ImVec2 nameSz = ImGui::CalcTextSize(actorName);
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
            ImVec2(curX, curY), kAccent, actorName);
        ImGui::SetWindowFontScale(1.0f);
        if (headFont) ImGui::PopFont();

        const float btnSz  = 20.0f * sc;
        ImVec2 btnTL = ImVec2(wPos.x + modalW - btnSz - 12.0f * sc, wPos.y + 10.0f * sc);
        ImVec2 btnBR = ImVec2(btnTL.x + btnSz, btnTL.y + btnSz);
        bool closeBtnHovered = ImGui::IsMouseHoveringRect(btnTL, btnBR);
        dl->AddRectFilled(btnTL, btnBR, closeBtnHovered ? IM_COL32(80,30,30,200) : IM_COL32(30,30,40,180), 3.0f);
        ImGui::SetWindowFontScale(FontBody());
        ImVec2 xSz = ImGui::CalcTextSize("x");
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
            ImVec2(btnTL.x + (btnSz - xSz.x) * 0.5f, btnTL.y + (btnSz - xSz.y) * 0.5f),
            kTextMuted, "x");
        ImGui::SetWindowFontScale(1.0f);
        if (closeBtnHovered && ImGui::IsMouseClicked(0))
            CloseEffectsModal();

        float sepY = wPos.y + 16.0f * sc + nameSz.y + 6.0f * sc;
        dl->AddRectFilledMultiColor(
            ImVec2(curX,                      sepY),
            ImVec2(curX + (modalW - 40.0f * sc) * 0.85f, sepY + 1.0f),
            kAccent, IM_COL32(212,175,55,0), IM_COL32(0,0,0,0), IM_COL32(0,0,0,0));

        float bodyY = sepY + 12.0f * sc;

        // ── Single-scroll layout ──────────────────────────────────────────────
        const float scrollbarW = 14.0f;
        const float bodyW = modalW - 40.0f * sc - scrollbarW;
        const float bodyH = (std::max)(1.0f, modalH - (bodyY - wPos.y) - 16.0f * sc);

        auto passiveEntries   = GetPermanentEffects(actor, EffectBucket::Passive);
        auto equipmentEntries = GetPermanentEffects(actor, EffectBucket::Equipment);
        auto blessingEntries  = GetPermanentEffects(actor, EffectBucket::Blessing);
        auto activeEntries    = GetActive(actor);
        auto raceEntries      = GetRaceAbilities(actor);
        auto spellSchools     = GetKnownSpells(actor);
        std::vector<PlayerHUDDisease::ActiveDisease> diseaseEntries;
        if (actor->IsPlayerRef())
            diseaseEntries = PlayerHUDDisease::GetActiveDiseases();
        std::vector<PlayerHUDSurvival::NeedStatus> needEntries;
        if (actor->IsPlayerRef())
            needEntries = PlayerHUDSurvival::GetActiveNeeds();
        std::optional<PlayerHUDSurvival::VitalityStatus> vitality;
        if (actor->IsPlayerRef())
            vitality = PlayerHUDSurvival::GetVitality();

        ImGui::SetWindowFontScale(FontBody());
        const float rowH = ImGui::GetTextLineHeight() + 5.0f * sc;
        ImGui::SetWindowFontScale(1.0f);

        ImGui::PushStyleColor(ImGuiCol_ChildBg,               ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,           ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,         ImVec4(212/255.f,175/255.f,55/255.f,0.4f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered,  ImVec4(212/255.f,175/255.f,55/255.f,0.7f));
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 3.0f);

        ImGui::SetCursorScreenPos(ImVec2(curX, bodyY));
        ImGui::BeginChild("##modal_scroll", ImVec2(bodyW + scrollbarW, bodyH), false, ImGuiWindowFlags_NoNav);
        ImDrawList* cdl = ImGui::GetWindowDrawList();

        float sectionGap = 16.0f * sc;

        const float colSrcW  = bodyW * 0.35f;
        const float colValW  = bodyW * 0.20f;
        const float colNameW = bodyW - colSrcW - colValW;

        int tableRowIdx = 0;

        auto DrawEffectRow = [&](const EffectEntry& e) {
            ImVec2 rp = ImGui::GetCursorScreenPos();
            ImGui::SetWindowFontScale(FontBody());

            if (tableRowIdx % 2 == 0)
                cdl->AddRectFilled(ImVec2(rp.x, rp.y), ImVec2(rp.x + bodyW, rp.y + rowH),
                    IM_COL32(255, 255, 255, 6));
            ++tableRowIdx;

            if (!e.source.empty()) {
                std::string src = e.source;
                while (src.size() > 1 && ImGui::CalcTextSize(src.c_str()).x > colSrcW - 6.0f * sc)
                    src.resize(src.size() - 1);
                if (src != e.source) src += "\xe2\x80\xa6";
                cdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(rp.x + 4.0f * sc, rp.y), kTextMuted, src.c_str());
            }

            float divX = rp.x + colSrcW - 6.0f * sc;
            cdl->AddLine(ImVec2(divX, rp.y + 1.0f), ImVec2(divX, rp.y + rowH - 3.0f * sc),
                IM_COL32(80, 80, 90, 120), 1.0f);

            float nameX = rp.x + colSrcW;
            cdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(nameX, rp.y), kTextMain, e.name.c_str());

            if (!e.valStr.empty()) {
                ImU32 valCol = e.isHostile ? kDebuff : kBuff;
                float valW = ImGui::CalcTextSize(e.valStr.c_str()).x;
                float valX = rp.x + bodyW - 8.0f * sc - valW;
                cdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(valX, rp.y), valCol, e.valStr.c_str());
            }

            ImGui::SetWindowFontScale(1.0f);
            ImGui::Dummy(ImVec2(bodyW, rowH));
        };

        bool firstSection = true;
        auto DrawSectionHeader = [&](const char* title) {
            if (!firstSection) {
                ImGui::Dummy(ImVec2(bodyW, sectionGap));
            }
            firstSection = false;
            ImVec2 hp = ImGui::GetCursorScreenPos();
            float contentY = DrawModalSectionTitle(cdl, headFont, hp.x, hp.y, bodyW, title, sc);
            ImGui::SetWindowFontScale(FontHeading());
            float titleH = ImGui::GetTextLineHeight() + 10.0f * sc;
            ImGui::SetWindowFontScale(1.0f);
            ImGui::Dummy(ImVec2(bodyW, titleH));
        };

        static std::string s_none, s_other;
        static std::string s_secRace, s_secPassive, s_secEquipment, s_secBlessings, s_secActive, s_secSpells, s_secDiseases, s_secNeeds, s_secVitality;
        static std::string s_schoolAlt, s_schoolConj, s_schoolDest, s_schoolIllu, s_schoolRest;
        if (s_none.empty()) {
            s_none          = LocalizationManager::Get("Common.None");
            s_other         = LocalizationManager::Get("Common.Other");
            s_secRace       = LocalizationManager::Get("PartySheetV2.SectionRace");
            s_secPassive    = LocalizationManager::Get("PartySheetV2.SectionPassive");
            s_secEquipment  = LocalizationManager::Get("PartySheetV2.SectionEquipment");
            s_secBlessings  = LocalizationManager::Get("PartySheetV2.SectionBlessings");
            s_secActive     = LocalizationManager::Get("PartySheetV2.SectionActive");
            s_secSpells     = LocalizationManager::Get("PartySheetV2.SectionSpells");
            s_secDiseases   = LocalizationManager::Get("header_disease_status");
            s_secNeeds      = LocalizationManager::Get("PartySheetV2.SectionNeeds");
            s_secVitality   = LocalizationManager::Get("PartySheetV2.SectionVitality");
            if (s_secVitality.empty() || s_secVitality == "PartySheetV2.SectionVitality")
                s_secVitality = "Vitality";
            s_schoolAlt     = LocalizationManager::Get("Skills.Alteration");
            s_schoolConj    = LocalizationManager::Get("Skills.Conjuration");
            s_schoolDest    = LocalizationManager::Get("Skills.Destruction");
            s_schoolIllu    = LocalizationManager::Get("Skills.Illusion");
            s_schoolRest    = LocalizationManager::Get("Skills.Restoration");
            for (auto* s : { &s_secRace,&s_secPassive,&s_secEquipment,&s_secBlessings,&s_secActive,&s_secSpells,&s_secDiseases,&s_secNeeds,&s_secVitality })
                for (char& c : *s) c = static_cast<char>(toupper(c));
        }

        DrawSectionHeader(s_secRace.c_str());
        if (raceEntries.empty()) {
            ImVec2 rp = ImGui::GetCursorScreenPos();
            ImGui::SetWindowFontScale(FontSmall());
            cdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(rp.x + 4.0f * sc, rp.y), kTextMuted, s_none.c_str());
            ImGui::SetWindowFontScale(1.0f);
            ImGui::Dummy(ImVec2(bodyW, rowH));
        } else {
            const int   cols     = 2;
            const float colGap   = 10.0f * sc;
            const float rowGap    = 8.0f  * sc;
            const float cardW    = (bodyW - colGap * (cols - 1)) / cols;
            const float iconSz   = 48.0f * sc;
            const float iconPad  = 6.0f  * sc;
            const float textGapX = 10.0f * sc;
            const float lineGap  = 2.0f  * sc;

            ImGui::SetWindowFontScale(FontBody());
            float titleH = ImGui::GetTextLineHeight();
            ImGui::SetWindowFontScale(FontSmall());
            float subH   = ImGui::GetTextLineHeight();
            ImGui::SetWindowFontScale(1.0f);
            bool anyMag = false;
            for (const auto& e : raceEntries) if (!e.magLine.empty()) { anyMag = true; break; }
            const float textStackH = titleH + lineGap + subH + (anyMag ? lineGap + subH : 0.0f);
            const float cardH    = (std::max)(iconSz, textStackH) + 10.0f * sc;

            ImVec2 origin = ImGui::GetCursorScreenPos();
            int n = static_cast<int>(raceEntries.size());
            int gridRows = (n + cols - 1) / cols;

            for (int i = 0; i < n; ++i) {
                const auto& e = raceEntries[i];
                int r = i / cols, c = i % cols;
                float cx = origin.x + c * (cardW + colGap);
                float cy = origin.y + r * (cardH + rowGap);

                cdl->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH),
                    IM_COL32(22, 22, 30, 200), 3.0f);
                cdl->AddRect(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH),
                    kBorderDim, 3.0f, 0, 1.0f);

                float iconX = cx + iconPad;
                float iconY = cy + (cardH - iconSz) * 0.5f;
                if (e.icon)
                    HUDDraw::DrawIconStyled(cdl, ImVec2(iconX, iconY), iconSz,
                        reinterpret_cast<ImTextureID>(e.icon), 0xFFFFFFFF, IconFxDefault());

                float blockH = titleH
                             + (e.typeLabel.empty() ? 0.0f : lineGap + subH)
                             + (e.magLine.empty()   ? 0.0f : lineGap + subH);
                float textX = iconX + iconSz + textGapX;
                float textMaxW = (cx + cardW) - textX - 6.0f * sc;
                float ty = cy + (cardH - blockH) * 0.5f;

                cdl->PushClipRect(ImVec2(textX, cy), ImVec2(textX + textMaxW, cy + cardH), true);

                ImGui::SetWindowFontScale(FontBody());
                cdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(textX, ty), kTextMain, e.name.c_str());
                ty += titleH;

                if (!e.typeLabel.empty()) {
                    ty += lineGap;
                    ImGui::SetWindowFontScale(FontSmall());
                    cdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                        ImVec2(textX, ty), kTextMuted, e.typeLabel.c_str());
                    ty += subH;
                }
                if (!e.magLine.empty()) {
                    ty += lineGap;
                    ImGui::SetWindowFontScale(FontSmall());
                    cdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                        ImVec2(textX, ty), kBuff, e.magLine.c_str());
                }
                cdl->PopClipRect();
                ImGui::SetWindowFontScale(1.0f);
            }

            ImGui::Dummy(ImVec2(bodyW, gridRows * cardH + (gridRows - 1) * rowGap + 6.0f * sc));
        }

        struct StoneLine { std::string text; bool isHostile; };
        struct StoneCard { std::string title; ID3D11ShaderResourceView* icon = nullptr;
                           std::vector<StoneLine> lines;
                           std::string description; };
        std::vector<StoneCard> stoneCards;
        std::vector<EffectEntry> plainPassives;
        for (auto& e : passiveEntries) {
            if (e.sourceSpell && IsStandingStoneSource(e.sourceSpell)) {
                const char* sn = e.sourceSpell->GetFullName();
                std::string title = (sn && sn[0]) ? sn : e.name;
                auto it = std::find_if(stoneCards.begin(), stoneCards.end(),
                    [&](const StoneCard& c) { return c.title == title; });
                if (it == stoneCards.end()) {
                    StoneCard sCard;
                    sCard.title = title;
                    sCard.icon  = TextureManager::GetPartyIcon("stone");
                    sCard.description = ResolveStoneDescription(e.sourceSpell);
                    it = stoneCards.insert(stoneCards.end(), std::move(sCard));
                }
                if (it->description.empty()) {
                    std::string lineTxt = StoneLineText(e, title);
                    if (!e.valStr.empty()) { lineTxt += " "; lineTxt += e.valStr; }
                    bool dup = std::any_of(it->lines.begin(), it->lines.end(),
                        [&](const StoneLine& l) { return l.text == lineTxt; });
                    if (!dup) it->lines.push_back({ std::move(lineTxt), e.isHostile });
                }
            } else {
                plainPassives.push_back(e);
            }
        }

        DrawSectionHeader(s_secPassive.c_str());
        if (!stoneCards.empty()) {
            static ID3D11ShaderResourceView* s_TexStone = nullptr;
            if (!s_TexStone) s_TexStone = TextureManager::GetPartyIcon("magic");
            const int   cols     = 2;
            const float colGap   = 10.0f * sc;
            const float rowGap   = 8.0f  * sc;
            const float cardW    = (bodyW - colGap * (cols - 1)) / cols;
            const float iconSz   = 48.0f * sc;
            const float iconPad  = 6.0f  * sc;
            const float textGapX = 10.0f * sc;
            const float lineGap  = 2.0f  * sc;

            ImGui::SetWindowFontScale(FontBody());
            const float titleH = ImGui::GetTextLineHeight();
            ImGui::SetWindowFontScale(FontSmall());
            const float subH   = ImGui::GetTextLineHeight();
            ImGui::SetWindowFontScale(1.0f);

            const float textMaxW = cardW - (iconPad + iconSz + textGapX) - 6.0f * sc;

            ImGui::SetWindowFontScale(FontSmall());
            const float subFs = ImGui::GetFontSize();
            ImFont* subFont = ImGui::GetFont();
            std::vector<float> bodyH(stoneCards.size(), 0.0f);
            float maxBodyH = 0.0f;
            for (size_t i = 0; i < stoneCards.size(); ++i) {
                const auto& sCard = stoneCards[i];
                float h;
                if (!sCard.description.empty())
                    h = subFont->CalcTextSizeA(subFs, FLT_MAX, textMaxW, sCard.description.c_str()).y;
                else
                    h = (std::max)(size_t(1), sCard.lines.size()) * (lineGap + subH);
                bodyH[i] = h;
                maxBodyH = (std::max)(maxBodyH, h);
            }
            ImGui::SetWindowFontScale(1.0f);
            const float textStackH = titleH + lineGap + maxBodyH;
            const float cardH = (std::max)(iconSz, textStackH) + 10.0f * sc;

            ImVec2 origin = ImGui::GetCursorScreenPos();
            int n = static_cast<int>(stoneCards.size());
            int gridRows = (n + cols - 1) / cols;

            for (int i = 0; i < n; ++i) {
                const auto& sCard = stoneCards[i];
                int r = i / cols, c = i % cols;
                float cx = origin.x + c * (cardW + colGap);
                float cy = origin.y + r * (cardH + rowGap);

                cdl->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH),
                    IM_COL32(22, 22, 30, 200), 3.0f);
                cdl->AddRect(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH),
                    kBorderDim, 3.0f, 0, 1.0f);

                float iconX = cx + iconPad;
                float iconY = cy + (cardH - iconSz) * 0.5f;
                ID3D11ShaderResourceView* tex = sCard.icon ? sCard.icon : s_TexStone;
                if (tex)
                    HUDDraw::DrawIconStyled(cdl, ImVec2(iconX, iconY), iconSz,
                        reinterpret_cast<ImTextureID>(tex), 0xFFFFFFFF, IconFxDefault());

                float blockH = titleH + lineGap + bodyH[i];
                float textX = iconX + iconSz + textGapX;
                float ty = cy + (cardH - blockH) * 0.5f;

                cdl->PushClipRect(ImVec2(textX, cy), ImVec2(textX + textMaxW, cy + cardH), true);
                ImGui::SetWindowFontScale(FontBody());
                cdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(textX, ty), kTextMain, sCard.title.c_str());
                ty += titleH + lineGap;
                ImGui::SetWindowFontScale(FontSmall());
                if (!sCard.description.empty()) {
                    cdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                        ImVec2(textX, ty), kTextMain, sCard.description.c_str(), nullptr, textMaxW);
                } else {
                    for (const auto& ln : sCard.lines) {
                        cdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                            ImVec2(textX, ty), ln.isHostile ? kDebuff : kBuff, ln.text.c_str());
                        ty += lineGap + subH;
                    }
                }
                cdl->PopClipRect();
                ImGui::SetWindowFontScale(1.0f);
            }

            ImGui::Dummy(ImVec2(bodyW, gridRows * cardH + (gridRows - 1) * rowGap + 6.0f * sc));
            if (!plainPassives.empty()) ImGui::Dummy(ImVec2(bodyW, 4.0f * sc));
        }
        if (plainPassives.empty() && stoneCards.empty()) {
            ImVec2 rp = ImGui::GetCursorScreenPos();
            ImGui::SetWindowFontScale(FontSmall());
            cdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(rp.x + 4.0f * sc, rp.y), kTextMuted, s_none.c_str());
            ImGui::SetWindowFontScale(1.0f);
            ImGui::Dummy(ImVec2(bodyW, rowH));
        } else if (!plainPassives.empty()) {
            const int   cols     = 2;
            const float colGap   = 10.0f * sc;
            const float rowGap   = 8.0f  * sc;
            const float cardW    = (bodyW - colGap * (cols - 1)) / cols;
            const float iconSz   = 48.0f * sc;
            const float iconPad  = 6.0f  * sc;
            const float textGapX = 10.0f * sc;
            const float lineGap  = 2.0f  * sc;

            ImGui::SetWindowFontScale(FontBody());
            float titleH = ImGui::GetTextLineHeight();
            ImGui::SetWindowFontScale(FontSmall());
            float subH   = ImGui::GetTextLineHeight();
            ImGui::SetWindowFontScale(1.0f);
            bool anyVal = false;
            for (const auto& e : plainPassives) if (!e.valStr.empty()) { anyVal = true; break; }
            const float textStackH = titleH + (anyVal ? lineGap + subH : 0.0f);
            const float cardH    = (std::max)(iconSz, textStackH) + 10.0f * sc;

            ImVec2 origin = ImGui::GetCursorScreenPos();
            int n = static_cast<int>(plainPassives.size());
            int gridRows = (n + cols - 1) / cols;

            for (int i = 0; i < n; ++i) {
                const auto& e = plainPassives[i];
                int r = i / cols, c = i % cols;
                float cx = origin.x + c * (cardW + colGap);
                float cy = origin.y + r * (cardH + rowGap);

                cdl->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH),
                    IM_COL32(22, 22, 30, 200), 3.0f);
                cdl->AddRect(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH),
                    kBorderDim, 3.0f, 0, 1.0f);

                float iconX = cx + iconPad;
                float iconY = cy + (cardH - iconSz) * 0.5f;
                if (auto* tex = ResolveMagicIcon(e.sourceSpell))
                    HUDDraw::DrawIconStyled(cdl, ImVec2(iconX, iconY), iconSz,
                        reinterpret_cast<ImTextureID>(tex), 0xFFFFFFFF, IconFxDefault());

                float blockH = titleH + (e.valStr.empty() ? 0.0f : lineGap + subH);
                float textX = iconX + iconSz + textGapX;
                float textMaxW = (cx + cardW) - textX - 6.0f * sc;
                float ty = cy + (cardH - blockH) * 0.5f;

                cdl->PushClipRect(ImVec2(textX, cy), ImVec2(textX + textMaxW, cy + cardH), true);
                ImGui::SetWindowFontScale(FontBody());
                cdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(textX, ty), kTextMain, e.name.c_str());
                if (!e.valStr.empty()) {
                    ImGui::SetWindowFontScale(FontSmall());
                    ImU32 valCol = e.isHostile ? kDebuff : kBuff;
                    cdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                        ImVec2(textX, ty + titleH + lineGap), valCol, e.valStr.c_str());
                }
                cdl->PopClipRect();
                ImGui::SetWindowFontScale(1.0f);
            }

            ImGui::Dummy(ImVec2(bodyW, gridRows * cardH + (gridRows - 1) * rowGap + 6.0f * sc));
        }

        DrawSectionHeader(s_secEquipment.c_str());
        static ID3D11ShaderResourceView* s_TexEquipArmor = nullptr;
        if (!s_TexEquipArmor) s_TexEquipArmor = TextureManager::GetPartyIcon("armor");
        struct EffectSeg { std::string text; bool isHostile = false; };
        struct EquipCard {
            ID3D11ShaderResourceView* icon = nullptr;
            std::string title;
            std::vector<EffectSeg> effects;
        };
        auto pushSeg = [](std::vector<EffectSeg>& segs, const std::string& name,
                          const std::string& val, bool isHostile) {
            if (name.empty()) return;
            std::string t = name;
            if (!val.empty()) { t += " "; t += val; }
            segs.push_back({ std::move(t), isHostile });
        };

        std::vector<EquipCard> cards;
        std::unordered_set<std::string> wornEffectNames;

        {
            struct SlotDef { RE::BIPED_MODEL::BipedObjectSlot slot; const char* hint; };
            static const SlotDef kSlots[] = {
                { RE::BIPED_MODEL::BipedObjectSlot::kHead,   "type_helmet"    },
                { RE::BIPED_MODEL::BipedObjectSlot::kBody,   "type_cuirass"   },
                { RE::BIPED_MODEL::BipedObjectSlot::kFeet,   "type_boots"     },
                { RE::BIPED_MODEL::BipedObjectSlot::kHands,  "type_gauntlets" },
                { RE::BIPED_MODEL::BipedObjectSlot::kShield, "shield"         },
                { RE::BIPED_MODEL::BipedObjectSlot::kAmulet, "type_amulet"    },
                { RE::BIPED_MODEL::BipedObjectSlot::kRing,   "type_ring"      },
            };
            for (const auto& sd : kSlots) {
                auto* armor = actor->GetWornArmor(sd.slot);
                if (!armor) continue;
                auto* ench = armor->As<RE::TESEnchantableForm>();
                if (!ench || !ench->formEnchanting) continue;

                EquipCard card;
                const char* an = armor->GetName();
                card.title = (an && an[0]) ? an : "Equipment";
                card.icon  = TextureManager::GetItemIcon(armor, sd.hint);

                for (auto* eff : ench->formEnchanting->effects) {
                    if (!eff || !eff->baseEffect) continue;
                    auto* base = eff->baseEffect;
                    if (base->data.flags.any(RE::EffectSetting::EffectSettingData::Flag::kHideInUI))
                        continue;
                    const char* en = base->GetFullName();
                    if (!en || !en[0]) continue;
                    float mag = std::abs(eff->effectItem.magnitude);
                    bool isHostile = base->IsHostile() || base->IsDetrimental();
                    bool isPct = EffectCategories::IsPercentageMagnitude(base->data.primaryAV);
                    if (isPct && mag > 0.0f && mag < 1.0f) mag *= 100.0f;
                    std::string val = FormatMagnitude(mag, base->data.primaryAV, isHostile);
                    pushSeg(card.effects, en, val, isHostile);
                    wornEffectNames.insert(en);
                }
                cards.push_back(std::move(card));
            }
        }
        size_t wornCardCount = cards.size();

        {
            for (auto& e : equipmentEntries) {
                bool isKye2 = IsKye2ArmorSource(e.sourceSpell);
                std::string title = e.source.empty() ? e.name : e.source;
                if (!isKye2 && wornEffectNames.count(e.name)) continue;

                auto it = std::find_if(cards.begin() + wornCardCount, cards.end(),
                    [&](const EquipCard& c) { return c.title == title; });
                if (it == cards.end()) {
                    EquipCard c;
                    c.title = title;
                    c.icon  = isKye2 ? (s_TexEquipArmor ? s_TexEquipArmor : ResolveMagicIcon(e.sourceSpell))
                                     : ResolveMagicIcon(e.sourceSpell);
                    pushSeg(c.effects, e.name, e.valStr, e.isHostile);
                    cards.push_back(std::move(c));
                } else {
                    pushSeg(it->effects, e.name, e.valStr, e.isHostile);
                }
            }
        }

        if (cards.empty()) {
            ImVec2 rp = ImGui::GetCursorScreenPos();
            ImGui::SetWindowFontScale(FontSmall());
            cdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(rp.x + 4.0f * sc, rp.y), kTextMuted, s_none.c_str());
            ImGui::SetWindowFontScale(1.0f);
            ImGui::Dummy(ImVec2(bodyW, rowH));
        } else {
            const int   cols     = 2;
            const float colGap   = 10.0f * sc;
            const float rowGap   = 8.0f  * sc;
            const float cardW    = (bodyW - colGap * (cols - 1)) / cols;
            const float iconSz   = 48.0f * sc;
            const float iconPad  = 6.0f  * sc;
            const float textGapX = 10.0f * sc;
            const float subGap   = 2.0f  * sc;

            const float textMaxW = cardW - (iconPad + iconSz + textGapX) - 6.0f * sc;

            ImGui::SetWindowFontScale(FontBody());
            const float titleH   = ImGui::GetTextLineHeight();
            ImGui::SetWindowFontScale(1.0f);

            auto drawSegs = [&](ImDrawList* cdl_, ImFont* fnt, float fs,
                                const std::vector<EffectSeg>& segs,
                                float x0, float y0, float wrapW, bool draw) -> float {
                const float lineH = fs;
                float cx = x0, cy = y0;
                bool lineStart = true;
                for (size_t si = 0; si < segs.size(); ++si) {
                    ImU32 col = segs[si].isHostile ? kDebuff : kBuff;
                    std::string piece = segs[si].text;
                    if (si + 1 < segs.size()) piece += ", ";
                    const char* s   = piece.c_str();
                    const char* end = s + piece.size();
                    while (s < end) {
                        const char* ws = s;
                        while (ws < end && *ws != ' ') ++ws;
                        while (ws < end && *ws == ' ') ++ws;
                        float w = fnt->CalcTextSizeA(fs, FLT_MAX, 0.0f, s, ws).x;
                        if (!lineStart && cx + w > x0 + wrapW) { cx = x0; cy += lineH; lineStart = true; }
                        if (draw && cdl_) cdl_->AddText(fnt, fs, ImVec2(cx, cy), col, s, ws);
                        cx += w;
                        lineStart = false;
                        s = ws;
                    }
                }
                return (cy - y0) + lineH;
            };

            float maxSubH = 0.0f;
            std::vector<float> subHeights(cards.size(), 0.0f);
            {
                ImGui::SetWindowFontScale(FontSmall());
                float fs = ImGui::GetFontSize();
                ImFont* fnt = ImGui::GetFont();
                for (size_t i = 0; i < cards.size(); ++i) {
                    if (cards[i].effects.empty()) continue;
                    float h = drawSegs(nullptr, fnt, fs, cards[i].effects,
                                       0.0f, 0.0f, textMaxW, /*draw=*/false);
                    subHeights[i] = h;
                    maxSubH = (std::max)(maxSubH, h);
                }
                ImGui::SetWindowFontScale(1.0f);
            }
            const float textStackH = titleH + (maxSubH > 0.0f ? subGap + maxSubH : 0.0f);
            const float cardH      = (std::max)(iconSz, textStackH) + 10.0f * sc;

            ImVec2 origin = ImGui::GetCursorScreenPos();
            int n = static_cast<int>(cards.size());
            int gridRows = (n + cols - 1) / cols;

            for (int i = 0; i < n; ++i) {
                const auto& ec = cards[i];
                int r = i / cols, c = i % cols;
                float cx = origin.x + c * (cardW + colGap);
                float cy = origin.y + r * (cardH + rowGap);

                cdl->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH),
                    IM_COL32(22, 22, 30, 200), 3.0f);
                cdl->AddRect(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH),
                    kBorderDim, 3.0f, 0, 1.0f);

                float iconX = cx + iconPad;
                float iconY = cy + (cardH - iconSz) * 0.5f;
                ID3D11ShaderResourceView* iconTex = ec.icon ? ec.icon : s_TexEquipArmor;
                DrawIconFit(cdl, iconTex, iconX, iconY, iconSz);

                float thisBlockH = titleH + (subHeights[i] > 0.0f ? subGap + subHeights[i] : 0.0f);
                float tx = iconX + iconSz + textGapX;
                float ty = cy + (cardH - thisBlockH) * 0.5f;

                cdl->PushClipRect(ImVec2(tx, cy), ImVec2(tx + textMaxW, cy + cardH), true);
                ImGui::SetWindowFontScale(FontBody());
                cdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(tx, ty), kTextMain, ec.title.c_str());
                if (!ec.effects.empty()) {
                    ImGui::SetWindowFontScale(FontSmall());
                    drawSegs(cdl, ImGui::GetFont(), ImGui::GetFontSize(), ec.effects,
                             tx, ty + titleH + subGap, textMaxW, /*draw=*/true);
                }
                cdl->PopClipRect();
                ImGui::SetWindowFontScale(1.0f);
            }

            ImGui::Dummy(ImVec2(bodyW, gridRows * cardH + (gridRows - 1) * rowGap + 6.0f * sc));
        }

        DrawSectionHeader(s_secBlessings.c_str());
        if (blessingEntries.empty()) {
            ImVec2 rp = ImGui::GetCursorScreenPos();
            ImGui::SetWindowFontScale(FontSmall());
            cdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(rp.x + 4.0f * sc, rp.y), kTextMuted, s_none.c_str());
            ImGui::SetWindowFontScale(1.0f);
            ImGui::Dummy(ImVec2(bodyW, rowH));
        } else {
            struct BlessingCard {
                RE::SpellItem* spell = nullptr;
                std::string    title;
                std::string    subtitle;
            };
            auto appendEffect = [](std::string& sub, const EffectEntry& e) {
                if (e.name.empty()) return;
                if (!sub.empty()) sub += ", ";
                sub += e.name;
                if (!e.valStr.empty()) { sub += " "; sub += e.valStr; }
            };
            std::vector<BlessingCard> cards;
            for (auto& e : blessingEntries) {
                auto it = std::find_if(cards.begin(), cards.end(), [&](const BlessingCard& c) {
                    return (e.sourceSpell && c.spell == e.sourceSpell) ||
                           (!e.sourceSpell && c.title == (e.source.empty() ? e.name : e.source));
                });
                if (it == cards.end()) {
                    BlessingCard c;
                    c.spell = e.sourceSpell;
                    c.title = e.source.empty() ? e.name : e.source;
                    appendEffect(c.subtitle, e);
                    cards.push_back(std::move(c));
                } else {
                    appendEffect(it->subtitle, e);
                }
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
            int n = static_cast<int>(cards.size());
            int gridRows = (n + cols - 1) / cols;

            for (int i = 0; i < n; ++i) {
                const auto& bc = cards[i];
                int r = i / cols, c = i % cols;
                float cx = origin.x + c * (cardW + colGap);
                float cy = origin.y + r * (cardH + rowGap);

                cdl->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH),
                    IM_COL32(22, 22, 30, 200), 3.0f);
                cdl->AddRect(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH),
                    kBorderDim, 3.0f, 0, 1.0f);

                float iconX = cx + iconPad;
                float iconY = cy + (cardH - iconSz) * 0.5f;
                if (auto* tex = ResolveBlessingIcon(bc.spell))
                    HUDDraw::DrawIconStyled(cdl, ImVec2(iconX, iconY), iconSz,
                        reinterpret_cast<ImTextureID>(tex), 0xFFFFFFFF, IconFxDefault());

                ImGui::SetWindowFontScale(FontBody());
                float titleH = ImGui::GetTextLineHeight();
                ImGui::SetWindowFontScale(FontSmall());
                float subH   = ImGui::GetTextLineHeight();
                ImGui::SetWindowFontScale(1.0f);

                float textBlockH = titleH + (bc.subtitle.empty() ? 0.0f : (subH + 2.0f * sc));
                float textX = iconX + iconSz + textGapX;
                float textY = cy + (cardH - textBlockH) * 0.5f;
                float textMaxW = (cx + cardW) - textX - 6.0f * sc;

                ImGui::SetWindowFontScale(FontBody());
                cdl->PushClipRect(ImVec2(textX, cy), ImVec2(textX + textMaxW, cy + cardH), true);
                cdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(textX, textY), kTextMain, bc.title.c_str());
                if (!bc.subtitle.empty()) {
                    ImGui::SetWindowFontScale(FontSmall());
                    cdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                        ImVec2(textX, textY + titleH + 2.0f * sc), kTextMuted, bc.subtitle.c_str());
                }
                cdl->PopClipRect();
                ImGui::SetWindowFontScale(1.0f);
            }

            ImGui::Dummy(ImVec2(bodyW, gridRows * cardH + (gridRows - 1) * rowGap + 6.0f * sc));
        }

        DrawSectionHeader(s_secActive.c_str());
        if (activeEntries.empty()) {
            ImVec2 rp = ImGui::GetCursorScreenPos();
            ImGui::SetWindowFontScale(FontSmall());
            cdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(rp.x + 4.0f * sc, rp.y), kTextMuted, s_none.c_str());
            ImGui::SetWindowFontScale(1.0f);
            ImGui::Dummy(ImVec2(bodyW, rowH));
        } else {
            const int   cols     = 2;
            const float colGap   = 10.0f * sc;
            const float rowGap   = 8.0f  * sc;
            const float cardW    = (bodyW - colGap * (cols - 1)) / cols;
            const float iconSz   = 48.0f * sc;
            const float iconPad  = 6.0f  * sc;
            const float textGapX = 10.0f * sc;
            const float lineGap  = 2.0f  * sc;

            auto timeStr = [](float t) -> std::string {
                if (t < 0.0f) return "";
                char b[16];
                if (t == 0.0f)        snprintf(b, sizeof(b), "\xe2\x88\x9e");
                else if (t < 60.0f)   snprintf(b, sizeof(b), "%.0fs", t);
                else                  snprintf(b, sizeof(b), "%.0fm", t / 60.0f);
                return b;
            };

            ImGui::SetWindowFontScale(FontBody());
            float titleH = ImGui::GetTextLineHeight();
            ImGui::SetWindowFontScale(FontSmall());
            float subH   = ImGui::GetTextLineHeight();
            ImGui::SetWindowFontScale(1.0f);

            bool anyLine2 = false, anyTime = false;
            for (const auto& e : activeEntries) {
                if (!e.valStr.empty() || !e.source.empty()) anyLine2 = true;
                if (e.timeRemaining >= 0.0f)                anyTime  = true;
            }
            const float textStackH = titleH
                                   + (anyLine2 ? lineGap + subH : 0.0f)
                                   + (anyTime  ? lineGap + subH : 0.0f);
            const float cardH = (std::max)(iconSz, textStackH) + 10.0f * sc;

            ImVec2 origin = ImGui::GetCursorScreenPos();
            int n = static_cast<int>(activeEntries.size());
            int gridRows = (n + cols - 1) / cols;

            for (int i = 0; i < n; ++i) {
                const auto& e = activeEntries[i];
                int r = i / cols, c = i % cols;
                float cx = origin.x + c * (cardW + colGap);
                float cy = origin.y + r * (cardH + rowGap);

                cdl->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH),
                    IM_COL32(22, 22, 30, 200), 3.0f);
                cdl->AddRect(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH),
                    kBorderDim, 3.0f, 0, 1.0f);

                float iconX = cx + iconPad;
                float iconY = cy + (cardH - iconSz) * 0.5f;
                if (auto* tex = TextureManager::GetEffectIcon(e.iconId))
                    DrawIconFit(cdl, tex, iconX, iconY, iconSz);

                bool hasSource = !e.source.empty();
                const std::string& line1 = hasSource ? e.source : e.name;
                std::string line2;
                if (hasSource) {
                    line2 = e.name;
                    if (!e.valStr.empty()) { line2 += "  "; line2 += e.valStr; }
                } else {
                    line2 = e.valStr;
                }
                std::string tstr = timeStr(e.timeRemaining);

                float blockH = titleH
                             + (line2.empty() ? 0.0f : lineGap + subH)
                             + (tstr.empty()  ? 0.0f : lineGap + subH);
                float textX = iconX + iconSz + textGapX;
                float textMaxW = (cx + cardW) - textX - 6.0f * sc;
                float ty = cy + (cardH - blockH) * 0.5f;

                cdl->PushClipRect(ImVec2(textX, cy), ImVec2(textX + textMaxW, cy + cardH), true);
                ImGui::SetWindowFontScale(FontBody());
                cdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(textX, ty), kTextMain, line1.c_str());
                ty += titleH;
                if (!line2.empty()) {
                    ty += lineGap;
                    ImGui::SetWindowFontScale(FontSmall());
                    cdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                        ImVec2(textX, ty), e.isHostile ? kDebuff : kBuff, line2.c_str());
                    ty += subH;
                }
                if (!tstr.empty()) {
                    ty += lineGap;
                    ImGui::SetWindowFontScale(FontSmall());
                    cdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                        ImVec2(textX, ty), kTextMuted, tstr.c_str());
                }
                cdl->PopClipRect();
                ImGui::SetWindowFontScale(1.0f);
            }

            ImGui::Dummy(ImVec2(bodyW, gridRows * cardH + (gridRows - 1) * rowGap + 6.0f * sc));
        }

        if (!needEntries.empty()) {
            DrawSectionHeader(s_secNeeds.c_str());

            ImGui::SetWindowFontScale(FontBody());
            float titleH = ImGui::GetTextLineHeight();
            ImGui::SetWindowFontScale(FontSmall());
            float subH   = ImGui::GetTextLineHeight();
            ImGui::SetWindowFontScale(1.0f);
            const float lineGap = 2.0f * sc;

            const int   cols     = 2;
            const float colGap   = 10.0f * sc;
            const float rowGap   = 8.0f  * sc;
            const float cardW    = (bodyW - colGap * (cols - 1)) / cols;
            const float iconSz   = 48.0f * sc;
            const float iconPad  = 6.0f  * sc;
            const float textGapX = 10.0f * sc;
            const float textStackH = titleH + lineGap + subH;
            const float cardH    = (std::max)(iconSz, textStackH) + 10.0f * sc;

            ImVec2 origin = ImGui::GetCursorScreenPos();
            int n = static_cast<int>(needEntries.size());
            int gridRows = (n + cols - 1) / cols;

            for (int i = 0; i < n; ++i) {
                const auto& nd = needEntries[i];
                int r = i / cols, c = i % cols;
                float cx = origin.x + c * (cardW + colGap);
                float cy = origin.y + r * (cardH + rowGap);

                cdl->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH),
                    IM_COL32(22, 22, 30, 200), 3.0f);
                cdl->AddRect(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH),
                    kBorderDim, 3.0f, 0, 1.0f);

                float iconX = cx + iconPad;
                float iconY = cy + (cardH - iconSz) * 0.5f;
                if (nd.icon)
                    HUDDraw::DrawIconStyled(cdl, ImVec2(iconX, iconY), iconSz,
                        reinterpret_cast<ImTextureID>(nd.icon), 0xFFFFFFFF, IconFxDefault());

                float blockH = titleH + lineGap + subH;
                float textX = iconX + iconSz + textGapX;
                float textMaxW = (cx + cardW) - textX - 6.0f * sc;
                float ty = cy + (cardH - blockH) * 0.5f;

                cdl->PushClipRect(ImVec2(textX, cy), ImVec2(textX + textMaxW, cy + cardH), true);
                ImGui::SetWindowFontScale(FontBody());
                cdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(textX, ty), kTextMain, nd.needName.c_str());
                ty += titleH + lineGap;
                ImGui::SetWindowFontScale(FontSmall());
                ImU32 stageCol = (nd.maxStage > 0 && nd.stage >= nd.maxStage) ? kDebuff
                               : (nd.stage <= 0 ? kBuff : kTextMuted);
                cdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(textX, ty), stageCol, nd.stageName.c_str());
                cdl->PopClipRect();
                ImGui::SetWindowFontScale(1.0f);
            }

            ImGui::Dummy(ImVec2(bodyW, gridRows * cardH + (gridRows - 1) * rowGap + 6.0f * sc));
        }

        if (vitality) {
            DrawSectionHeader(s_secVitality.c_str());

            ImGui::SetWindowFontScale(FontBody());
            float titleH = ImGui::GetTextLineHeight();
            ImGui::SetWindowFontScale(FontSmall());
            float subH   = ImGui::GetTextLineHeight();
            ImGui::SetWindowFontScale(1.0f);
            const float lineGap = 2.0f * sc;

            const float cardW    = bodyW;
            const float iconSz   = 48.0f * sc;
            const float iconPad  = 6.0f  * sc;
            const float textGapX = 10.0f * sc;
            const float textStackH = titleH + lineGap + subH;
            const float cardH    = (std::max)(iconSz, textStackH) + 10.0f * sc;

            ImVec2 origin = ImGui::GetCursorScreenPos();
            float cx = origin.x, cy = origin.y;

            cdl->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH),
                IM_COL32(22, 22, 30, 200), 3.0f);
            cdl->AddRect(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH),
                kBorderDim, 3.0f, 0, 1.0f);

            float iconX = cx + iconPad;
            float iconY = cy + (cardH - iconSz) * 0.5f;
            if (vitality->icon)
                HUDDraw::DrawIconStyled(cdl, ImVec2(iconX, iconY), iconSz,
                    reinterpret_cast<ImTextureID>(vitality->icon), 0xFFFFFFFF, IconFxDefault());

            float blockH = titleH + lineGap + subH;
            float textX = iconX + iconSz + textGapX;
            float textMaxW = (cx + cardW) - textX - 6.0f * sc;
            float ty = cy + (cardH - blockH) * 0.5f;

            cdl->PushClipRect(ImVec2(textX, cy), ImVec2(textX + textMaxW, cy + cardH), true);
            ImGui::SetWindowFontScale(FontBody());
            cdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(textX, ty), kTextMain, vitality->name.c_str());
            ty += titleH + lineGap;
            ImGui::SetWindowFontScale(FontSmall());
            ImU32 tierCol = (vitality->tier <= 0) ? kDebuff
                          : (vitality->tier >  4) ? kBuff
                          :                         kTextMuted;
            cdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(textX, ty), tierCol, vitality->tierName.c_str());
            cdl->PopClipRect();
            ImGui::SetWindowFontScale(1.0f);

            ImGui::Dummy(ImVec2(bodyW, cardH + 6.0f * sc));
        }

        if (!diseaseEntries.empty()) {
            DrawSectionHeader(s_secDiseases.c_str());

            ImGui::SetWindowFontScale(FontBody());
            float titleH = ImGui::GetTextLineHeight();
            ImGui::SetWindowFontScale(FontSmall());
            float subH   = ImGui::GetTextLineHeight();
            ImGui::SetWindowFontScale(1.0f);
            const float lineGap = 2.0f * sc;

            const int   cols     = 2;
            const float colGap   = 10.0f * sc;
            const float rowGap   = 8.0f  * sc;
            const float cardW    = (bodyW - colGap * (cols - 1)) / cols;
            const float iconSz   = 48.0f * sc;
            const float iconPad  = 6.0f  * sc;
            const float textGapX = 10.0f * sc;
            const float textStackH = titleH + lineGap + subH + lineGap + subH;
            const float cardH    = (std::max)(iconSz, textStackH) + 10.0f * sc;

            ImVec2 origin = ImGui::GetCursorScreenPos();
            int n = static_cast<int>(diseaseEntries.size());
            int gridRows = (n + cols - 1) / cols;

            for (int i = 0; i < n; ++i) {
                const auto& d = diseaseEntries[i];
                int r = i / cols, c = i % cols;
                float cx = origin.x + c * (cardW + colGap);
                float cy = origin.y + r * (cardH + rowGap);

                cdl->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH),
                    IM_COL32(22, 22, 30, 200), 3.0f);
                cdl->AddRect(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH),
                    kBorderDim, 3.0f, 0, 1.0f);

                float iconX = cx + iconPad;
                float iconY = cy + (cardH - iconSz) * 0.5f;
                if (d.icon)
                    HUDDraw::DrawIconStyled(cdl, ImVec2(iconX, iconY), iconSz,
                        reinterpret_cast<ImTextureID>(d.icon), 0xFFFFFFFF, IconFxDefault());

                const char* sevKey = "disease_stage_infected";
                if (d.maxStage > 1) {
                    if (d.stage >= 3)      sevKey = "disease_stage_crippling";
                    else if (d.stage == 2) sevKey = "disease_stage_severe";
                }
                std::string sev = LocalizationManager::Get(sevKey);

                float blockH = titleH
                             + (sev.empty()       ? 0.0f : lineGap + subH)
                             + (d.penalty.empty() ? 0.0f : lineGap + subH);
                float textX = iconX + iconSz + textGapX;
                float textMaxW = (cx + cardW) - textX - 6.0f * sc;
                float ty = cy + (cardH - blockH) * 0.5f;

                cdl->PushClipRect(ImVec2(textX, cy), ImVec2(textX + textMaxW, cy + cardH), true);

                ImGui::SetWindowFontScale(FontBody());
                cdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(textX, ty), kTextMain, d.name.c_str());
                ty += titleH;

                if (!sev.empty()) {
                    ty += lineGap;
                    ImGui::SetWindowFontScale(FontSmall());
                    cdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                        ImVec2(textX, ty), kDebuff, sev.c_str());
                    ty += subH;
                }
                if (!d.penalty.empty()) {
                    ty += lineGap;
                    ImGui::SetWindowFontScale(FontSmall());
                    cdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                        ImVec2(textX, ty), kTextMuted, d.penalty.c_str());
                }
                cdl->PopClipRect();
                ImGui::SetWindowFontScale(1.0f);
            }

            ImGui::Dummy(ImVec2(bodyW, gridRows * cardH + (gridRows - 1) * rowGap + 6.0f * sc));
        }

        DrawSectionHeader(s_secSpells.c_str());
        if (spellSchools.empty()) {
            ImVec2 rp = ImGui::GetCursorScreenPos();
            ImGui::SetWindowFontScale(FontSmall());
            cdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(rp.x + 4.0f * sc, rp.y), kTextMuted, s_none.c_str());
            ImGui::SetWindowFontScale(1.0f);
            ImGui::Dummy(ImVec2(bodyW, rowH));
        } else {
            struct SchoolCard {
                RE::ActorValue skill = RE::ActorValue::kNone;
                std::string    title;
                std::string    subtitle;
                int            count = 0;
            };
            std::vector<SchoolCard> cards;
            cards.reserve(spellSchools.size());

            const int   cols     = 2;
            const float colGap   = 10.0f * sc;
            const float rowGap   = 8.0f  * sc;
            const float cardW    = (bodyW - colGap * (cols - 1)) / cols;
            const float iconSz   = 48.0f * sc;
            const float iconPad  = 6.0f  * sc;
            const float textGapX = 10.0f * sc;
            const float subGap   = 2.0f  * sc;
            const float textMaxW = cardW - iconPad - iconSz - textGapX - 6.0f * sc;
            const int   kMaxSubLines = 4;

            for (auto& school : spellSchools) {
                SchoolCard card;
                card.skill = school.skill;
                card.count = static_cast<int>(school.names.size());
                switch (school.skill) {
                    case RE::ActorValue::kAlteration:  card.title = s_schoolAlt;  break;
                    case RE::ActorValue::kConjuration: card.title = s_schoolConj; break;
                    case RE::ActorValue::kDestruction: card.title = s_schoolDest; break;
                    case RE::ActorValue::kIllusion:    card.title = s_schoolIllu; break;
                    case RE::ActorValue::kRestoration: card.title = s_schoolRest; break;
                    default:                           card.title = s_other;       break;
                }

                ImGui::SetWindowFontScale(FontSmall());
                ImFont* fnt = ImGui::GetFont();
                float   fs  = ImGui::GetFontSize();
                float   maxLineH = fnt->CalcTextSizeA(fs, FLT_MAX, -1.0f, "Ay").y;
                float   capH = maxLineH * kMaxSubLines + 0.5f;
                std::string sub;
                int shown = 0;
                for (int i = 0; i < card.count; ++i) {
                    std::string trial = sub;
                    if (!trial.empty()) trial += ", ";
                    trial += school.names[i];
                    if (!sub.empty() &&
                        fnt->CalcTextSizeA(fs, FLT_MAX, textMaxW, trial.c_str()).y > capH)
                        break;
                    sub = std::move(trial);
                    ++shown;
                }
                if (shown < card.count) {
                    char buf[16];
                    snprintf(buf, sizeof(buf), " +%d", card.count - shown);
                    sub += buf;
                }
                card.subtitle = std::move(sub);
                ImGui::SetWindowFontScale(1.0f);

                cards.push_back(std::move(card));
            }

            ImGui::SetWindowFontScale(FontBody());
            const float titleH = ImGui::GetTextLineHeight();
            ImGui::SetWindowFontScale(FontSmall());
            std::vector<float> subHeights(cards.size(), 0.0f);
            float maxSubH = 0.0f;
            {
                ImFont* fnt = ImGui::GetFont();
                float   fs  = ImGui::GetFontSize();
                for (size_t i = 0; i < cards.size(); ++i) {
                    if (cards[i].subtitle.empty()) continue;
                    float h = fnt->CalcTextSizeA(fs, FLT_MAX, textMaxW, cards[i].subtitle.c_str()).y;
                    subHeights[i] = h;
                    maxSubH = (std::max)(maxSubH, h);
                }
            }
            ImGui::SetWindowFontScale(1.0f);
            const float textStackH = titleH + (maxSubH > 0.0f ? subGap + maxSubH : 0.0f);
            const float cardH      = (std::max)(iconSz, textStackH) + 10.0f * sc;

            ImVec2 origin = ImGui::GetCursorScreenPos();
            int n = static_cast<int>(cards.size());
            int gridRows = (n + cols - 1) / cols;

            for (int i = 0; i < n; ++i) {
                const auto& card = cards[i];
                int r = i / cols, c = i % cols;
                float cx = origin.x + c * (cardW + colGap);
                float cy = origin.y + r * (cardH + rowGap);

                cdl->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH),
                    IM_COL32(22, 22, 30, 200), 3.0f);
                cdl->AddRect(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH),
                    kBorderDim, 3.0f, 0, 1.0f);

                float iconX = cx + iconPad;
                float iconY = cy + (cardH - iconSz) * 0.5f;
                if (auto* tex = ResolveSchoolIcon(card.skill))
                    HUDDraw::DrawIconStyled(cdl, ImVec2(iconX, iconY), iconSz,
                        reinterpret_cast<ImTextureID>(tex), 0xFFFFFFFF, IconFxDefault());

                float thisBlockH = titleH + (subHeights[i] > 0.0f ? subGap + subHeights[i] : 0.0f);
                float tx = iconX + iconSz + textGapX;
                float ty = cy + (cardH - thisBlockH) * 0.5f;

                cdl->PushClipRect(ImVec2(tx, cy), ImVec2(cx + cardW - 4.0f * sc, cy + cardH), true);
                ImGui::SetWindowFontScale(FontBody());
                cdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(tx, ty), kAccent, card.title.c_str());
                char cntBuf[16];
                snprintf(cntBuf, sizeof(cntBuf), "%d", card.count);
                float cntW = ImGui::CalcTextSize(cntBuf).x;
                cdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(cx + cardW - 8.0f * sc - cntW, ty), kTextMuted, cntBuf);
                if (!card.subtitle.empty()) {
                    ImGui::SetWindowFontScale(FontSmall());
                    cdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                        ImVec2(tx, ty + titleH + subGap), kTextMain,
                        card.subtitle.c_str(), nullptr, textMaxW);
                }
                cdl->PopClipRect();
                ImGui::SetWindowFontScale(1.0f);
            }

            ImGui::Dummy(ImVec2(bodyW, gridRows * cardH + (gridRows - 1) * rowGap + 6.0f * sc));
        }

        ImGui::Dummy(ImVec2(bodyW, 8.0f * sc));

        ImGui::EndChild();

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);

        ImGui::End();
    }

}
