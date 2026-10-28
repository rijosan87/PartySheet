#include "../includes/TextureManager.h"
#include <RE/Skyrim.h>
#include <DirectXTex.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <lunasvg.h>
#include <spdlog/spdlog.h>
#include <Windows.h>

namespace logger = SKSE::log;

namespace TextureManager {
    
    static ID3D11Device* g_Device = nullptr;
    static std::unordered_map<std::string, ID3D11ShaderResourceView*> g_TextureCache;
    static size_t g_TotalVramBytes = 0;

    static size_t ComputeImageVramBytes(const DirectX::TexMetadata& md) {
        const size_t bpp = DirectX::BitsPerPixel(md.format);
        if (bpp == 0) return 0;
        size_t total = 0;
        for (size_t mip = 0; mip < md.mipLevels; ++mip) {
            size_t w = std::max<size_t>(1, md.width  >> mip);
            size_t h = std::max<size_t>(1, md.height >> mip);
            total += (w * h * bpp) / 8;
        }
        return total * std::max<size_t>(1, md.arraySize);
    }
    
    static constexpr const char* PORTRAIT_PATH = "Data\\Interface\\PartySheet\\Portraits\\";
    static constexpr const char* ICON_PATH     = "Data\\Interface\\PartySheet\\Icons\\";
    static constexpr const char* ITEMS_PATH    = "Data\\Interface\\PartySheet\\Items\\";
    static constexpr const char* PARTY_PATH    = "Data\\Interface\\PartySheet\\Party\\";
    static constexpr const char* FRAMES_PATH   = "Data\\Interface\\PartySheet\\Frames\\";

    
    static std::wstring UTF8ToWide(const std::string& utf8) {
        if (utf8.empty()) return L"";
        
        int wideLen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
        if (wideLen <= 0) return L"";
        
        std::wstring wide(wideLen - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wide[0], wideLen);
        return wide;
    }
    
    static bool IsASCII(const std::string& str) {
        for (unsigned char c : str) {
            if (c > 127) return false;
        }
        return true;
    }
    
    // PNGII's shipped glyph pack is all SVG. 128 px plus the mip chain covers every Equipment slot size.
    static constexpr int SVG_RASTER_PX = 128;

    static bool IsSVGPath(const std::string& path) {
        if (path.size() < 4) return false;
        std::string ext = path.substr(path.size() - 4);
        for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return ext == ".svg";
    }

    // Square canvas, art fitted with a margin and centered (PNGII RasterizeSVG math): the glyph draws into a square,
    static HRESULT RasterizeSVGFile(const std::wstring& wpath, int side, DirectX::ScratchImage& out) {
        std::ifstream file(wpath, std::ios::binary);
        if (!file) return E_FAIL;
        const std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        const auto doc = lunasvg::Document::loadFromData(bytes.data(), bytes.size());
        if (!doc || doc->width() <= 0.0f || doc->height() <= 0.0f) return E_FAIL;

        const float docW = doc->width();
        const float docH = doc->height();
        // 8% clear border per side: PNGII packs crop the viewBox tight to the art, and art touching the
        const float scale = static_cast<float>(side) * 0.84f / (std::max)(docW, docH);
        lunasvg::Bitmap bitmap(side, side);
        if (bitmap.isNull()) return E_FAIL;
        bitmap.clear(0x00000000);
        doc->render(bitmap, lunasvg::Matrix(scale, 0, 0, scale, (side - docW * scale) / 2, (side - docH * scale) / 2));
        // lunasvg renders premultiplied ARGB32; straight RGBA matches what WIC hands the PNG path.
        bitmap.convertToRGBA();
        if (FAILED(out.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, side, side, 1, 1))) return E_FAIL;
        const auto* img = out.GetImage(0, 0, 0);
        for (int y = 0; y < side; ++y) {
            std::memcpy(img->pixels + y * img->rowPitch, bitmap.data() + y * bitmap.stride(), static_cast<size_t>(side) * 4);
        }
        return S_OK;
    }

    void Init(ID3D11Device* device) {
        g_Device = device;
        logger::info("TextureManager initialized");
    }
    
    void Shutdown() {
        for (auto& [path, srv] : g_TextureCache) {
            if (srv) {
                srv->Release();
            }
        }
        g_TextureCache.clear();
        g_TotalVramBytes = 0;
        g_Device = nullptr;
        logger::info("TextureManager shutdown");
    }
    
    ID3D11ShaderResourceView* GetTexture(const std::string& path) {
        if (!g_Device) return nullptr;
        
        auto it = g_TextureCache.find(path);
        if (it != g_TextureCache.end()) {
            return it->second;
        }
        

        if (!std::filesystem::exists(UTF8ToWide(path))) {
            // logger::warn("TextureManager: File not found: {}", path);  // Too verbose
            g_TextureCache[path] = nullptr;
            return nullptr;
        }
        
        DirectX::ScratchImage image;
        std::wstring wpath = UTF8ToWide(path);

        HRESULT hr = IsSVGPath(path)
            ? RasterizeSVGFile(wpath, SVG_RASTER_PX, image)
            : DirectX::LoadFromWICFile(
                wpath.c_str(),
                DirectX::WIC_FLAGS_IGNORE_SRGB,
                nullptr,
                image
            );
        
        if (FAILED(hr)) {
            logger::warn("Failed to load texture: {}", path);
            g_TextureCache[path] = nullptr;
            return nullptr;
        }
        
        bool generateMipmaps =
            path.find("\\Icons\\")   != std::string::npos ||
            path.find("\\Party\\")   != std::string::npos ||
            path.find("\\Status\\")  != std::string::npos ||
            path.find("\\Widgets\\") != std::string::npos ||
            path.find("\\Items\\")   != std::string::npos ||
            path.find("\\Spells\\")  != std::string::npos ||
            path.find("\\Portraits\\")  != std::string::npos ||
          // path.find("\\Bars\\")    != std::string::npos ||
            path.find("\\Frames\\")  != std::string::npos ||
            path.find("\\PNGIconInjector\\") != std::string::npos;  // PNGII glyphs drawn small on the Equipment HUD

        DirectX::ScratchImage mipChain;
        const DirectX::ScratchImage* imageToUse = &image;

        if (generateMipmaps && image.GetImageCount() > 0) {
            hr = DirectX::GenerateMipMaps(
                image.GetImages()[0],
                DirectX::TEX_FILTER_LINEAR,
                0,
                mipChain
            );
            if (SUCCEEDED(hr)) {
                imageToUse = &mipChain;
            } else {
                logger::warn("GenerateMipMaps failed for '{}' hr=0x{:08X} — using single-level texture",
                    path, static_cast<uint32_t>(hr));
            }
        }
        
        ID3D11Resource* texture = nullptr;
        hr = DirectX::CreateTexture(
            g_Device,
            imageToUse->GetImages(),
            imageToUse->GetImageCount(),
            imageToUse->GetMetadata(),
            &texture
        );
        
        if (FAILED(hr)) {
            logger::warn("Failed to create texture resource: {}", path);
            g_TextureCache[path] = nullptr;
            return nullptr;
        }
        
        ID3D11ShaderResourceView* srv = nullptr;
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = imageToUse->GetMetadata().format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = static_cast<UINT>(imageToUse->GetMetadata().mipLevels);
        srvDesc.Texture2D.MostDetailedMip = 0;

        hr = g_Device->CreateShaderResourceView(texture, &srvDesc, &srv);
        texture->Release();
        
        if (FAILED(hr)) {
            logger::warn("Failed to create SRV: {}", path);
            g_TextureCache[path] = nullptr;
            return nullptr;
        }
        
        g_TextureCache[path] = srv;
        g_TotalVramBytes += ComputeImageVramBytes(imageToUse->GetMetadata());
        return srv;
    }
    
    static ID3D11ShaderResourceView* TryLoadPortrait(const std::string& baseName) {
        if (baseName.empty()) return nullptr;

        std::string jpg = std::string(PORTRAIT_PATH) + baseName + ".jpg";
        if (auto srv = GetTexture(jpg)) return srv;

        std::string png = std::string(PORTRAIT_PATH) + baseName + ".png";
        return GetTexture(png);
    }
    
    static std::string ToLower(const std::string& str) {
        std::string result = str;
        for (auto& c : result) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return result;
    }

    bool IsWarhammer(RE::TESObjectWEAP* weapon) {
        // Magic static: called from the render thread (GetItemIcon) and the UI thread (PNGII label map).
        static RE::BGSKeyword* const s_WeapTypeWarhammer =
            RE::TESForm::LookupByEditorID<RE::BGSKeyword>("WeapTypeWarhammer");
        return weapon && s_WeapTypeWarhammer && weapon->HasKeyword(s_WeapTypeWarhammer);
    }

    bool IsBoltAmmo(RE::TESAmmo* ammo) {
        if (!ammo) return false;
        const char* ammoName = ammo->GetName();
        const bool nameHasBolt = ammoName && ToLower(ammoName).find("bolt") != std::string::npos;
        const bool hasNonBoltFlag = ammo->GetRuntimeData().data.flags.all(RE::AMMO_DATA::Flag::kNonBolt);
        return nameHasBolt || !hasNonBoltFlag;
    }
    
    ID3D11ShaderResourceView* GetPortrait(RE::Actor* actor) {
        if (!actor) return nullptr;
        
        std::string name = actor->GetDisplayFullName();
        if (!name.empty() && IsASCII(name)) {
            if (auto srv = TryLoadPortrait(name)) return srv;
        }
        
        if (auto npcBase = actor->GetActorBase()) {
            if (auto* sourceFile = npcBase->GetFile(0)) {
                std::string pluginName(sourceFile->GetFilename());
                uint32_t localFormID = npcBase->GetLocalFormID() & 0x00FFFFFF;
                
                char pluginFormIdStr[256];
                snprintf(pluginFormIdStr, sizeof(pluginFormIdStr), "%s-0x%X", pluginName.c_str(), localFormID);
                if (auto srv = TryLoadPortrait(pluginFormIdStr)) return srv;
            } else {
            }
        }
        
        char formIdStr[16];
        if (auto actorBase = actor->GetActorBase()) {
            snprintf(formIdStr, sizeof(formIdStr), "%08X", actorBase->GetFormID());
            if (auto srv = TryLoadPortrait(formIdStr)) return srv;
        }
        
        auto race = actor->GetRace();
        std::string raceName = race ? ToLower(race->GetFullName()) : "";
        
        auto actorBase = actor->GetActorBase();
        bool isMale = actorBase ? (actorBase->GetSex() == RE::SEX::kMale) : true;
        std::string sexStr = isMale ? "male" : "female";
        
        bool isCreature = false;
        if (race) {
            auto npcKeyword = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("ActorTypeNPC");
            isCreature = npcKeyword && !race->HasKeyword(npcKeyword);
        }
        
        bool isSummonedActor = actor->IsSummoned() || actor->IsCommandedActor();
        
        if (isSummonedActor || isCreature) {
            if (!raceName.empty() && IsASCII(raceName)) {
                if (!isMale) {
                    if (auto srv = TryLoadPortrait("summon female " + raceName)) return srv;
                }
                if (auto srv = TryLoadPortrait("summon " + raceName)) return srv;
            }
            if (auto srv = TryLoadPortrait("summon")) return srv;
            return TryLoadPortrait("default");
        }
        
        
        if (auto npc = actorBase ? actorBase->As<RE::TESNPC>() : nullptr) {
             if (auto npcClass = npc->npcClass) {
                std::string className = npcClass->GetFullName();
                if (!className.empty() && IsASCII(className)) {
                    if (!isMale) {
                        if (auto srv = TryLoadPortrait("female " + className)) return srv;
                    }
                    if (auto srv = TryLoadPortrait(className)) return srv;
                }
             }
        }

        if (!raceName.empty() && IsASCII(raceName)) {
            if (!isMale) {
                if (auto srv = TryLoadPortrait("female " + raceName)) return srv;
            }
            if (auto srv = TryLoadPortrait(raceName)) return srv;
        }

        return TryLoadPortrait("default");
    }
    
    ID3D11ShaderResourceView* GetEffectIcon(const std::string& iconId) {
        if (iconId.empty()) return nullptr;
        
        std::string iconPath = std::string(ICON_PATH) + iconId + ".png";
        return GetTexture(iconPath);
    }
    
    ID3D11ShaderResourceView* GetPartyIcon(const std::string& iconName) {
        if (iconName.empty()) return nullptr;

        std::string iconPath = std::string(PARTY_PATH) + iconName + ".png";
        return GetTexture(iconPath);
    }

    ID3D11ShaderResourceView* GetFrameTexture(const std::string& name) {
        if (name.empty()) return nullptr;
        std::string png = std::string(FRAMES_PATH) + name + ".png";
        if (auto srv = GetTexture(png)) return srv;
        std::string dds = std::string(FRAMES_PATH) + name + ".dds";
        return GetTexture(dds);
    }
    
    static ID3D11ShaderResourceView* TryLoadItemIcon(const std::string& baseName) {
        if (baseName.empty()) return nullptr;
        std::string path = std::string(ITEMS_PATH) + baseName + ".png";
        return GetTexture(path);
    }
    
    ID3D11ShaderResourceView* GetItemIcon(RE::TESForm* item, const std::string& slotType) {
        if (!item) {
            return TryLoadItemIcon("slot_" + slotType);
        }

        std::string normalizedSlotType = slotType;
        if (normalizedSlotType.rfind("type_", 0) == 0) {
            normalizedSlotType = normalizedSlotType.substr(5);
        } else if (normalizedSlotType.rfind("slot_", 0) == 0) {
            normalizedSlotType = normalizedSlotType.substr(5);
        }
        
        const char* itemName = item->GetName();
        if (itemName && itemName[0] != '\0' && IsASCII(itemName)) {
            if (auto srv = TryLoadItemIcon(itemName)) return srv;
        }

        if (auto* sourceFile = item->GetFile(0)) {
            std::string pluginName(sourceFile->GetFilename());
            uint32_t localFormID = item->GetLocalFormID() & 0x00FFFFFF;

            char pluginFormIdStr[256];
            snprintf(pluginFormIdStr, sizeof(pluginFormIdStr), "%s-0x%X", pluginName.c_str(), localFormID);
            if (auto srv = TryLoadItemIcon(pluginFormIdStr)) return srv;
        } else {
            logger::info("  P2 SKIPPED: GetFile(0) is null");
        }
        
        char formIdStr[16];
        snprintf(formIdStr, sizeof(formIdStr), "%08X", item->GetFormID());
        if (auto srv = TryLoadItemIcon(formIdStr)) return srv;
        
        {
            struct MaterialEntry {
                const char* editorID;
                const char* prefix;
                RE::BGSKeyword* keyword;
            };
            static MaterialEntry s_Materials[] = {
                { "ArmorMaterialImperialLight",   "imperial", nullptr },
                { "ArmorMaterialImperialStudded", "imperial", nullptr },
                { "ArmorMaterialImperialHeavy",   "imperial", nullptr },
                { "ArmorMaterialIron",       "iron",        nullptr },
                { "ArmorMaterialIronBanded", "iron",        nullptr },
                { "WeapMaterialIron",        "iron",        nullptr },
                { "ArmorMaterialSteel",      "steel",       nullptr },
                { "WeapMaterialSteel",       "steel",       nullptr },
                { "ArmorMaterialSteelPlate", "steelplate",  nullptr },
                { "ArmorMaterialStudded",    "studded",     nullptr },
                { "ArmorMaterialScaled",     "scaled",      nullptr },
                { "ArmorMaterialLeather",    "leather",     nullptr },
                { "ArmorMaterialHide",       "hide",        nullptr },
                { "ArmorMaterialDwarven",    "dwarven",     nullptr },
                { "WeapMaterialDwarven",     "dwarven",     nullptr },
                { "ArmorMaterialOrcish",     "orcish",      nullptr },
                { "WeapMaterialOrcish",      "orcish",      nullptr },
                { "ArmorMaterialElven",      "elven",       nullptr },
                { "ArmorMaterialElvenGilded","elven",       nullptr },
                { "WeapMaterialElven",       "elven",       nullptr },
                { "ArmorMaterialGlass",      "glass",       nullptr },
                { "WeapMaterialGlass",       "glass",       nullptr },
                { "ArmorMaterialEbony",      "ebony",       nullptr },
                { "WeapMaterialEbony",       "ebony",       nullptr },
                { "ArmorMaterialDragonplate","dragonplate",  nullptr },
                { "ArmorMaterialDragonscale","dragonscale",  nullptr },
                { "DLC1WeapMaterialDragonbone","dragonbone", nullptr },
                { "ArmorMaterialDaedric",    "daedric",     nullptr },
                { "WeapMaterialDaedric",     "daedric",     nullptr },
                { "DLC2ArmorMaterialNordicHeavy","nordic",  nullptr },
                { "DLC2WeaponMaterialNordic","nordic",      nullptr },
                { "DLC2ArmorMaterialStalhrimHeavy", "stalhrim", nullptr },
                { "DLC2ArmorMaterialStalhrimLight", "stalhriml", nullptr },
                { "DLC2WeaponMaterialStalhrim","stalhrim",  nullptr },
                { "WeapMaterialImperial",    "imperial",    nullptr },
                { "WeapMaterialDraugr",      "draugr",      nullptr },
                { "WeapMaterialDraugrHoned", "draugr",      nullptr },
                { "WeapMaterialFalmerHoned", "falmer",      nullptr },
                { "ccBGSSSE025_ArmorMaterialAmber",  "amber",  nullptr },
                { "ccBGSSSE025_ArmorMaterialDark",   "dark",   nullptr },
                { "ccBGSSSE025_ArmorMaterialGolden", "golden", nullptr },
                { "ccBGSSSE025_WeapMaterialAmber",   "amber",  nullptr },
                { "ccBGSSSE025_WeapMaterialMadness", "madness", nullptr },
                { "ccBGSSSE025_WeapMaterialDark",   "dark",   nullptr },
                { "ccBGSSSE025_WeapMaterialGolden", "golden", nullptr },
                { "WeapMaterialSilver",      "silver",      nullptr },
                { "WeapMaterialWood",        "wood",        nullptr },
                { "WeapMaterialFalmer",      "falmer",      nullptr },
                { "ArmorMaterialForsworn",   "forsworn",    nullptr },
                { "ArmorMaterialBlades",     "blades",      nullptr },
                { "ArmorNightingale",            "nightingale",  nullptr },
                { "ArmorMaterialThievesGuild",   "thievesguild", nullptr },
                { "ArmorMaterialThievesGuildLeader", "thievesguild", nullptr },
                { "DLC1ArmorMaterielFalmerHeavyOriginal", "falmer", nullptr },
                { "DLC1ArmorMaterialFalmerHardened", "falmerh",  nullptr },
                { "ArmorMaterialFalmer",         "falmer",       nullptr },
                { "DLC1ArmorMaterialDawnguard",  "dawnguard",    nullptr },
                { "DLC1ArmorMaterialHunter",     "dawnguard",    nullptr },
                { "DLC1ArmorMaterialVampire",    "vampire",      nullptr },
                { "DLC2ArmorMaterialBonemoldLight", "bonemold",  nullptr },
                { "DLC2ArmorMaterialBonemoldHeavy", "bonemold",  nullptr },
                { "DLC2ArmorMaterialChitinLight",   "chitinl",   nullptr },
                { "DLC2ArmorMaterialChitinHeavy",   "chitin",    nullptr },
                { nullptr,                   nullptr,       nullptr },
            };
            static bool s_MaterialsInit = false;
            if (!s_MaterialsInit) {
                s_MaterialsInit = true;
                for (auto& m : s_Materials) {
                    if (!m.prefix) break;
                    m.keyword = RE::TESForm::LookupByEditorID<RE::BGSKeyword>(m.editorID);
                }
            }

            std::string materialSlot;
            const char* foundMaterial = nullptr;

            if (auto weapon = item->As<RE::TESObjectWEAP>()) {
                switch (weapon->GetWeaponType()) {
                    case RE::WEAPON_TYPE::kOneHandSword:  materialSlot = "sword"; break;
                    case RE::WEAPON_TYPE::kOneHandDagger:  materialSlot = "dagger"; break;
                    case RE::WEAPON_TYPE::kOneHandAxe:     materialSlot = "axe"; break;
                    case RE::WEAPON_TYPE::kOneHandMace:    materialSlot = "mace"; break;
                    case RE::WEAPON_TYPE::kTwoHandSword:   materialSlot = "greatsword"; break;
                    case RE::WEAPON_TYPE::kTwoHandAxe: {
                        static RE::BGSKeyword* s_WeapTypeWarhammer = nullptr;
                        static bool s_WarhammerKWInit = false;
                        if (!s_WarhammerKWInit) {
                            s_WarhammerKWInit = true;
                            s_WeapTypeWarhammer = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("WeapTypeWarhammer");
                        }
                        materialSlot = (s_WeapTypeWarhammer && weapon->HasKeyword(s_WeapTypeWarhammer))
                            ? "warhammer" : "battleaxe";
                        break;
                    }
                    case RE::WEAPON_TYPE::kBow:            materialSlot = "bow"; break;
                    case RE::WEAPON_TYPE::kCrossbow:       materialSlot = "crossbow"; break;
                    case RE::WEAPON_TYPE::kStaff:          materialSlot = "staff"; break;
                    default: break;
                }
                if (!materialSlot.empty()) {
                    for (const auto& m : s_Materials) {
                        if (!m.prefix) break;
                        if (m.keyword && weapon->HasKeyword(m.keyword)) {
                            foundMaterial = m.prefix;
                            break;
                        }
                    }
                    if (itemName && ToLower(itemName).rfind("nord hero", 0) == 0) {
                        foundMaterial = "nordhero";
                    }
                }
            } else if (auto armor = item->As<RE::TESObjectARMO>()) {
                static RE::BGSKeyword* s_kwCircletRetag = nullptr;
                static bool s_kwCircletRetagInit = false;
                if (!s_kwCircletRetagInit) {
                    s_kwCircletRetagInit = true;
                    s_kwCircletRetag = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("ClothingCirclet");
                }
                if (normalizedSlotType == "helmet" &&
                    s_kwCircletRetag && armor->HasKeyword(s_kwCircletRetag)) {
                    normalizedSlotType = "circlet";
                }

                if (normalizedSlotType == "helmet")
                    materialSlot = "helmet";
                else if (normalizedSlotType == "circlet")
                    materialSlot = "circlet";
                else if (normalizedSlotType == "cuirass" || normalizedSlotType == "armor" || normalizedSlotType == "body")
                    materialSlot = "cuirass";
                else if (normalizedSlotType == "gauntlets" || normalizedSlotType == "hands")
                    materialSlot = "gauntlets";
                else if (normalizedSlotType == "boots" || normalizedSlotType == "feet")
                    materialSlot = "boots";
                else if (normalizedSlotType == "shield")
                    materialSlot = "shield";
                else if (armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kHead) ||
                    armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kHair))
                    materialSlot = "helmet";
                else if (armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kCirclet))
                    materialSlot = "circlet";
                else if (armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kBody))
                    materialSlot = "cuirass";
                else if (armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kHands))
                    materialSlot = "gauntlets";
                else if (armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kFeet))
                    materialSlot = "boots";
                else if (armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kShield))
                    materialSlot = "shield";

                if (!materialSlot.empty()) {
                    for (const auto& m : s_Materials) {
                        if (!m.prefix) break;
                        if (m.keyword && armor->HasKeyword(m.keyword)) {
                            foundMaterial = m.prefix;
                            break;
                        }
                    }
                }
            }

            if (foundMaterial && !materialSlot.empty()) {
                std::string materialFile = std::string(foundMaterial) + "_" + materialSlot;
                if (auto srv = TryLoadItemIcon(materialFile)) return srv;

                static const std::pair<const char*, const char*> s_MaterialFallbacks[] = {
                    { "steelplate",  "steel"       },
                    { "dragonscale", "dragonplate"  },
                    { "dragonbone",  "dragonplate"  },
                };
                for (const auto& [from, to] : s_MaterialFallbacks) {
                    if (strcmp(foundMaterial, from) == 0) {
                        std::string fallbackFile = std::string(to) + "_" + materialSlot;
                        if (auto srv = TryLoadItemIcon(fallbackFile)) return srv;
                        break;
                    }
                }
            }
        }

        if (auto weapon = item->As<RE::TESObjectWEAP>()) {
            switch (weapon->GetWeaponType()) {
                case RE::WEAPON_TYPE::kOneHandSword: if (auto srv = TryLoadItemIcon("type_sword")) return srv; break;
                case RE::WEAPON_TYPE::kOneHandDagger: if (auto srv = TryLoadItemIcon("type_dagger")) return srv; break;
                case RE::WEAPON_TYPE::kOneHandAxe: if (auto srv = TryLoadItemIcon("type_axe")) return srv; break;
                case RE::WEAPON_TYPE::kOneHandMace: if (auto srv = TryLoadItemIcon("type_mace")) return srv; break;
                case RE::WEAPON_TYPE::kTwoHandSword: if (auto srv = TryLoadItemIcon("type_greatsword")) return srv; break;
                case RE::WEAPON_TYPE::kTwoHandAxe: {
                    if (IsWarhammer(weapon)) {
                        if (auto srv = TryLoadItemIcon("type_warhammer")) return srv;
                    }
                    if (auto srv = TryLoadItemIcon("type_battleaxe")) return srv;
                    break;
                }
                case RE::WEAPON_TYPE::kBow: if (auto srv = TryLoadItemIcon("type_bow")) return srv; break;
                case RE::WEAPON_TYPE::kCrossbow: if (auto srv = TryLoadItemIcon("type_crossbow")) return srv; break;
                case RE::WEAPON_TYPE::kStaff: if (auto srv = TryLoadItemIcon("type_staff")) return srv; break;
                default: break;
            }
            if (auto srv = TryLoadItemIcon("type_weapon")) return srv;
        }
        
        if (auto armor = item->As<RE::TESObjectARMO>()) {
            {
                static RE::BGSKeyword* kwClothing  = nullptr;
                static RE::BGSKeyword* kwClBody     = nullptr;
                static RE::BGSKeyword* kwClHead     = nullptr;
                static RE::BGSKeyword* kwClHands    = nullptr;
                static RE::BGSKeyword* kwClFeet     = nullptr;
                static RE::BGSKeyword* kwClCirclet  = nullptr;
                static bool s_ClInit = false;
                if (!s_ClInit) {
                    s_ClInit = true;
                    kwClothing = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("ArmorClothing");
                    kwClBody    = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("ClothingBody");
                    kwClHead    = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("ClothingHead");
                    kwClHands   = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("ClothingHands");
                    kwClFeet    = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("ClothingFeet");
                    kwClCirclet = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("ClothingCirclet");
                }
                if (kwClothing && armor->HasKeyword(kwClothing)) {
                    if ((kwClHead && armor->HasKeyword(kwClHead)) ||
                        armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kHead) ||
                        armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kHair)) {
                        if (auto srv = TryLoadItemIcon("type_hood")) return srv;
                    }
                    if (kwClCirclet && armor->HasKeyword(kwClCirclet)) {
                        if (auto srv = TryLoadItemIcon("type_circlet")) return srv;
                    }
                    if (kwClHands && armor->HasKeyword(kwClHands)) {
                        if (auto srv = TryLoadItemIcon("type_gloves")) return srv;
                    }
                    if (kwClFeet && armor->HasKeyword(kwClFeet)) {
                        if (auto srv = TryLoadItemIcon("type_shoes")) return srv;
                    }
                    if ((kwClBody && armor->HasKeyword(kwClBody)) ||
                        armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kBody)) {
                        if (auto srv = TryLoadItemIcon("type_robe")) return srv;
                    }
                }
            }

            if (normalizedSlotType == "helmet") {
                if (auto srv = TryLoadItemIcon("type_helmet")) return srv;
            }
            if (normalizedSlotType == "circlet") {
                if (auto srv = TryLoadItemIcon("type_circlet")) return srv;
            }
            if (normalizedSlotType == "cuirass" || normalizedSlotType == "armor" || normalizedSlotType == "body") {
                if (auto srv = TryLoadItemIcon("type_cuirass")) return srv;
            }
            if (normalizedSlotType == "gauntlets" || normalizedSlotType == "hands") {
                if (auto srv = TryLoadItemIcon("type_gauntlets")) return srv;
            }
            if (normalizedSlotType == "boots" || normalizedSlotType == "feet") {
                if (auto srv = TryLoadItemIcon("type_boots")) return srv;
            }
            if (normalizedSlotType == "shield") {
                if (auto srv = TryLoadItemIcon("type_shield")) return srv;
            }
            
            if (armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kHead) ||
                armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kHair)) {
                if (auto srv = TryLoadItemIcon("type_helmet")) return srv;
            }

            if (armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kCirclet)) {
                if (auto srv = TryLoadItemIcon("type_circlet")) return srv;
                return TryLoadItemIcon("slot_" + slotType);
            }

            if (armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kBody)) {
                if (auto srv = TryLoadItemIcon("type_cuirass")) return srv;
            }
            if (armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kHands)) {
                if (auto srv = TryLoadItemIcon("type_gauntlets")) return srv;
            }
            if (armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kFeet)) {
                if (auto srv = TryLoadItemIcon("type_boots")) return srv;
            }
            if (armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kShield)) {
                if (auto srv = TryLoadItemIcon("type_shield")) return srv;
            }
            
            if (armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kRing)) {
                if (auto srv = TryLoadItemIcon("type_ring")) return srv;
            }
            static RE::BGSKeyword* clothingRingKW = nullptr;
            static bool ringKWInit = false;
            if (!ringKWInit) {
                clothingRingKW = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("ClothingRing");
                ringKWInit = true;
            }
            if (clothingRingKW && armor->HasKeyword(clothingRingKW)) {
                if (auto srv = TryLoadItemIcon("type_ring")) return srv;
            }
            
            if (armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kAmulet)) {
                if (auto srv = TryLoadItemIcon("type_amulet")) return srv;
            }
            static RE::BGSKeyword* clothingNecklaceKW = nullptr;
            static RE::BGSKeyword* vendorJewelryKW = nullptr;
            static bool necklaceKWInit = false;
            if (!necklaceKWInit) {
                clothingNecklaceKW = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("ClothingNecklace");
                vendorJewelryKW = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("VendorItemJewelry");
                necklaceKWInit = true;
            }
            if (clothingNecklaceKW && armor->HasKeyword(clothingNecklaceKW)) {
                if (auto srv = TryLoadItemIcon("type_amulet")) return srv;
            }
            if (vendorJewelryKW && armor->HasKeyword(vendorJewelryKW)) {
                if (!armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kRing) &&
                    !armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kHead) &&
                    !armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kHair) &&
                    !armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kCirclet) &&
                    !(clothingRingKW && armor->HasKeyword(clothingRingKW))) {
                    if (auto srv = TryLoadItemIcon("type_amulet")) return srv;
                }
            }
            
            if (auto srv = TryLoadItemIcon("type_armor")) return srv;
        }
        
        if (auto ammo = item->As<RE::TESAmmo>()) {
            const bool isBolt = IsBoltAmmo(ammo);

            const char* ammoName = ammo->GetName();
            std::string nameLower = ammoName ? ToLower(ammoName) : "";

            const char* ammoTypeSuffix = isBolt ? "bolt" : "arrow";
            if (!nameLower.empty()) {
                static const char* s_AmmoMaterials[] = {
                    "iron", "steel", "dwarven", "orcish", "elven", "glass",
                    "ebony", "daedric", "dragonbone", "nordic", "stalhrim",
                    "falmer", "forsworn", "ancient nord", "nord hero", "bound",
                    "fire", "ice", "shock",
                    nullptr
                };
                for (int i = 0; s_AmmoMaterials[i]; ++i) {
                    if (nameLower.find(s_AmmoMaterials[i]) != std::string::npos) {
                        std::string materialFile = std::string(s_AmmoMaterials[i]) + "_" + ammoTypeSuffix;
                        for (char& c : materialFile) if (c == ' ') c = '_';
                        if (auto srv = TryLoadItemIcon(materialFile)) return srv;
                        break;
                    }
                }
            }

            if (isBolt) {
                if (auto srv = TryLoadItemIcon("type_bolt")) return srv;
            }
            if (auto srv = TryLoadItemIcon("type_quiver")) return srv;
        }

        if (auto* alch = item->As<RE::AlchemyItem>()) {
            if (alch->IsFood()) {
                if (auto srv = TryLoadItemIcon("type_food")) return srv;
            }
            if (auto srv = TryLoadItemIcon("type_potion")) return srv;
        }
        if (item->As<RE::IngredientItem>()) {
            if (auto srv = TryLoadItemIcon("type_ingredient")) return srv;
        }
        if (item->As<RE::TESSoulGem>()) {
            if (auto srv = TryLoadItemIcon("type_soulgem")) return srv;
        }
        if (item->As<RE::TESKey>()) {
            if (auto srv = TryLoadItemIcon("type_key")) return srv;
        }
        if (item->As<RE::TESObjectBOOK>()) {
            if (auto srv = TryLoadItemIcon("type_book")) return srv;
        }
        if (item->As<RE::ScrollItem>()) {
            if (auto srv = TryLoadItemIcon("type_scroll")) return srv;
        }
        if (item->As<RE::TESObjectLIGH>()) {
            if (auto srv = TryLoadItemIcon("type_light")) return srv;
            if (auto srv = TryLoadItemIcon("type_torch")) return srv;
        }

        if (item->As<RE::TESBoundObject>() && !item->As<RE::MagicItem>()) {
            if (auto srv = TryLoadItemIcon("type_misc")) return srv;
        }

        return TryLoadItemIcon("slot_" + slotType);
    }
    

    void PreloadPortraits() {
    }
    
    void PreloadEffectIcons() {
    }
    
    CacheStats GetCacheStats() {
        CacheStats stats = {0, 0, 0, 0};
        stats.totalCached    = static_cast<int>(g_TextureCache.size());
        stats.totalVramBytes = g_TotalVramBytes;
        for (const auto& [path, srv] : g_TextureCache) {
            if (srv) {
                stats.successfulLoads++;
            } else {
                stats.failedLoads++;
            }
        }
        return stats;
    }
}
