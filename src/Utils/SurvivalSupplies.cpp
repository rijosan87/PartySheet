#include "../../includes/Utils/SurvivalSupplies.h"
#include "../../includes/Utils/GearCatalog.h"

#include <RE/Skyrim.h>

#include <cstring>
#include <algorithm>
#include <unordered_map>
#include <vector>

namespace SurvivalSupplies {

    namespace {

        struct Fix {
            int  hunger = 0, thirst = 0, cold = 0;
            bool dirtyWater = false;
        };

        constexpr RE::FormID kFirewood = 0x06F993;
        constexpr RE::FormID kCampfireFuel[] = { 0x02E68E, 0x02564C };
        constexpr RE::FormID kRndBedding[] = { 0x031144, 0x0177CD };
        constexpr RE::FormID kSunHelmWaterskin[] = { 0x4DE9AE, 0x4DE9AF, 0x4DE9B0 };

        constexpr RE::FormID kLastSeedDirtyWater[] = {
            0x01088E, 0x010890, 0x010892, 0x010894, 0x2FCDA9, 0x506652, 0x506656, 0x533F8F };
        constexpr RE::FormID kLastSeedCleanWater[] = {
            0x01088F, 0x010891, 0x010893, 0x1B8C0C, 0x506653, 0x506659 };

        constexpr RE::FormID kRndMeal[]  = { 0x003375, 0x01114D, 0x003E4D, 0x004919, 0x004918 };
        constexpr int        kRndMealSize[] = { 1, 1, 1, 2, 3 };
        constexpr RE::FormID kRndDrink[] = { 0x00FB9F, 0x003374, 0x006437, 0x01114F, 0x011150 };
        constexpr RE::FormID kRndDirty[] = { 0x07168F, 0x06442A };
        constexpr RE::FormID kRndNotFood[] = {
            0x003381, 0x07B838, 0x0053E4, 0x07A811, 0x0053EB, 0x07B839, 0x047F91, 0x07B83A, 0x003380, 0x0803AD, 0x0803AE };

        RE::TESForm* Lookup(RE::FormID id, const char* plugin)
        {
            auto* dh = RE::TESDataHandler::GetSingleton();
            return (dh && dh->LookupModByName(plugin)) ? dh->LookupForm(id, plugin) : nullptr;
        }

        struct Markers {
            RE::EffectSetting* hunger[4] = {};
            RE::EffectSetting* cold[2]   = {};
            RE::BGSKeyword* shLight = nullptr, *shMedium = nullptr, *shHeavy = nullptr, *shSoup = nullptr;
            RE::BGSKeyword* shDrink = nullptr, *shHot = nullptr, *shIgnore = nullptr;
            RE::BGSKeyword* lsHunger[4] = {};
            RE::BGSKeyword* lsThirst = nullptr, *lsNotFood = nullptr;
            std::vector<RE::TESForm*> lsDirty, lsClean;
            std::vector<RE::TESForm*> rndMeal, rndDrink, rndDirty, rndNotFood;
        };

        const Markers& GetMarkers()
        {
            static Markers m;
            static bool done = false;
            if (done) return m;
            done = true;
            auto mgef = [](RE::FormID id) {
                auto* f = Lookup(id, "Update.esm");
                return f ? f->As<RE::EffectSetting>() : nullptr;
            };
            auto kw = [](RE::FormID id, const char* plugin) {
                auto* f = Lookup(id, plugin);
                return f ? f->As<RE::BGSKeyword>() : nullptr;
            };
            for (int i = 0; i < 4; ++i) m.hunger[i] = mgef(0x2EE1 + i);
            m.cold[0] = mgef(0x2EE5);
            m.cold[1] = mgef(0x2EE6);
            if (Lookup(0xABE495, "SunHelmSurvival.esp")) {
                m.shLight  = kw(0xF3DD6D, "Update.esm");
                m.shMedium = kw(0xF3DD6E, "Update.esm");
                m.shHeavy  = kw(0xF3DD6F, "Update.esm");
                m.shSoup   = kw(0xF3DD74, "Update.esm");
                m.shDrink  = kw(0xF3DD72, "Update.esm");
                m.shHot    = kw(0xA17387, "SunHelmSurvival.esp");
                m.shIgnore = kw(0xABE495, "SunHelmSurvival.esp");
            }
            if (Lookup(0x515962, "LastSeed.esp")) {
                for (int i = 0; i < 4; ++i) m.lsHunger[i] = kw(0x515963 + i, "LastSeed.esp");
                m.lsThirst  = kw(0x515967, "LastSeed.esp");
                m.lsNotFood = kw(0x51596F, "LastSeed.esp");
                for (auto id : kLastSeedDirtyWater) if (auto* f = Lookup(id, "LastSeed.esp")) m.lsDirty.push_back(f);
                for (auto id : kLastSeedCleanWater) if (auto* f = Lookup(id, "LastSeed.esp")) m.lsClean.push_back(f);
            }
            auto addRnd = [](std::vector<RE::TESForm*>& out, const auto& ids) {
                for (auto id : ids) out.push_back(Lookup(id, "RealisticNeedsandDiseases.esp"));
            };
            addRnd(m.rndMeal, kRndMeal);
            addRnd(m.rndDrink, kRndDrink);
            addRnd(m.rndDirty, kRndDirty);
            addRnd(m.rndNotFood, kRndNotFood);
            return m;
        }

        Fix Classify(RE::FormID baseID)
        {
            static std::unordered_map<RE::FormID, Fix> cache;
            if (auto it = cache.find(baseID); it != cache.end()) return it->second;

            Fix fix;
            auto* item = RE::TESForm::LookupByID<RE::AlchemyItem>(baseID);
            if (item && !item->IsPoison()) {
                const auto& m = GetMarkers();
                bool rndDirty = false, rndNotFood = false;
                for (auto* eff : item->effects) {
                    auto* base = eff ? eff->baseEffect : nullptr;
                    if (!base) continue;
                    for (int i = 0; i < 4; ++i)
                        if (base == m.hunger[i]) fix.hunger = (std::max)(fix.hunger, i + 1);
                    if (base == m.cold[0] || base == m.cold[1]) fix.cold = (std::max)(fix.cold, 1);
                    const auto baseIn = [base](const std::vector<RE::TESForm*>& list) {
                        return std::find(list.begin(), list.end(), base) != list.end();
                    };
                    for (std::size_t i = 0; i < m.rndMeal.size(); ++i)
                        if (base == m.rndMeal[i]) fix.hunger = (std::max)(fix.hunger, kRndMealSize[i]);
                    if (baseIn(m.rndDrink)) fix.thirst = 1;
                    rndDirty |= baseIn(m.rndDirty);
                    rndNotFood |= baseIn(m.rndNotFood);
                }
                auto has = [item](RE::BGSKeyword* k) { return k && item->HasKeyword(k); };
                if (!has(m.shIgnore)) {
                    if (has(m.shLight))  fix.hunger = (std::max)(fix.hunger, 1);
                    if (has(m.shMedium)) fix.hunger = (std::max)(fix.hunger, 2);
                    if (has(m.shHeavy) || has(m.shSoup)) fix.hunger = (std::max)(fix.hunger, 3);
                    if (has(m.shDrink) || has(m.shSoup)) fix.thirst = 1;
                    if (has(m.shHot) || has(m.shSoup)) fix.cold = (std::max)(fix.cold, 1);
                }
                if (!has(m.lsNotFood)) {
                    for (int i = 0; i < 4; ++i)
                        if (has(m.lsHunger[i])) fix.hunger = (std::max)(fix.hunger, i + 1);
                    if (has(m.lsThirst)) fix.thirst = 1;
                }
                const auto isIn = [item](const std::vector<RE::TESForm*>& list) {
                    return std::find(list.begin(), list.end(), item) != list.end();
                };
                if (isIn(m.lsClean)) fix.thirst = 1;
                if (isIn(m.lsDirty)) { fix = Fix{}; fix.dirtyWater = true; }
                if (rndDirty) { fix = Fix{}; fix.dirtyWater = true; }
                if (item->IsFood() && !rndNotFood && !fix.hunger && !fix.thirst && !fix.dirtyWater) fix.hunger = 1;
            }
            cache[baseID] = fix;
            return fix;
        }

        struct TentKeywords { RE::BGSKeyword* item = nullptr, *warm = nullptr; };
        const TentKeywords& GetTentKeywords()
        {
            static TentKeywords k;
            static bool done = false;
            if (done) return k;
            done = true;
            auto kw = [](RE::FormID id) {
                auto* f = Lookup(id, "Campfire.esm");
                return f ? f->As<RE::BGSKeyword>() : nullptr;
            };
            k.item = kw(0x03AB3C);
            k.warm = kw(0x03AB3B);
            return k;
        }

        struct ModForms { RE::FormID fuel[2] = {}, bedding[2] = {}, waterskin[3] = {}; };
        const ModForms& GetModForms()
        {
            static ModForms f;
            static bool done = false;
            if (done) return f;
            done = true;
            auto id = [](RE::FormID local, const char* plugin) {
                auto* form = Lookup(local, plugin);
                return form ? form->GetFormID() : 0u;
            };
            for (int i = 0; i < 2; ++i) f.fuel[i] = id(kCampfireFuel[i], "Campfire.esm");
            for (int i = 0; i < 2; ++i) f.bedding[i] = id(kRndBedding[i], "RealisticNeedsandDiseases.esp");
            for (int i = 0; i < 3; ++i) f.waterskin[i] = id(kSunHelmWaterskin[i], "SunHelmSurvival.esp");
            return f;
        }

        template <std::size_t N>
        int IndexIn(const RE::FormID (&list)[N], RE::FormID id)
        {
            for (std::size_t i = 0; i < N; ++i)
                if (id && list[i] == id) return static_cast<int>(i);
            return -1;
        }
        template <std::size_t N>
        bool Contains(const RE::FormID (&list)[N], RE::FormID id) { return IndexIn(list, id) >= 0; }

        std::shared_ptr<const GearCatalog::ActorGear> PlayerGear()
        {
            auto* player = RE::PlayerCharacter::GetSingleton();
            return player ? GearCatalog::Get(player) : nullptr;
        }
    }

    Totals Count()
    {
        Totals t;
        auto gear = PlayerGear();
        if (!gear) return t;
        const auto& tk = GetTentKeywords();
        const auto& mf = GetModForms();
        for (const auto& it : gear->items) {
            const RE::FormID id = it.key.baseID;
            if (id == kFirewood || Contains(mf.fuel, id)) { t.firewood += it.count; continue; }
            if (Contains(mf.bedding, id)) { t.tents += it.count; continue; }
            if (int sips = IndexIn(mf.waterskin, id); sips >= 0) { t.waterskinSips += (sips + 1) * it.count; continue; }
            if (auto* light = RE::TESForm::LookupByID<RE::TESObjectLIGH>(id); light && light->CanBeCarried()) {
                t.torches += it.count;
                continue;
            }
            if (tk.item && it.type == GearCatalog::ItemType::Misc) {
                auto* misc = RE::TESForm::LookupByID<RE::TESObjectMISC>(it.key.baseID);
                if (misc && misc->HasKeyword(tk.item)) t.tents += it.count;
                continue;
            }
            if (it.type != GearCatalog::ItemType::Food && it.type != GearCatalog::ItemType::Potion) continue;
            const Fix f = Classify(it.key.baseID);
            if (f.dirtyWater) t.dirtyWater += it.count;
            else if (f.hunger) t.food += it.count;
            else if (f.thirst) t.drinks += it.count;
        }
        return t;
    }
}
