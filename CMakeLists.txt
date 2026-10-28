# It's recommended to set a minimum CMake version.
# If you use CMake features from higher versions, update this to match.
cmake_minimum_required(VERSION 3.21)

# Set your project name. This will be the name of your SKSE .dll file.
project(SkyrimPartySheet VERSION 3.8.0 LANGUAGES CXX)

#
# YOU DO NOT NEED TO EDIT ANYTHING BELOW HERE
#
# Please see the "Project setup" section of README.md
#

# If you're not using a mod manager, you probably want the SKSE plugin to go
# inside of your Skyrim "Data" folder.
#
# To do this automatically, set the `SKYRIM_FOLDER` environment variable
# to the path of your Skyrim Special Edition folder
if(DEFINED ENV{SKYRIM_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_FOLDER}/Data")
    set(OUTPUT_FOLDER "$ENV{SKYRIM_FOLDER}/Data")
endif()

# If you're using Mod Organizer 2 or Vortex, you might want this to go inside
# of your "mods" folder, inside of a subfolder named "<your mod>".
#
# To do this automatically, set the `SKYRIM_MODS_FOLDER` environment variable
# to the path of your "mods" folder
if(DEFINED ENV{SKYRIM_MODS_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_MODS_FOLDER}")
    set(OUTPUT_FOLDER "$ENV{SKYRIM_MODS_FOLDER}/${PROJECT_NAME}")
endif()

# Otherwise, you can set OUTPUT_FOLDER to any place you'd like :)
# set(OUTPUT_FOLDER "C:/path/to/any/folder")

# Setup your SKSE plugin as an SKSE plugin!
# CommonLibSSE-NG 7.5.1 built from source (the vcpkg registry package is 3.7.0, which lacks 1.7.104 support).
# Setup: git clone --depth 1 --branch v7.5.1 https://github.com/alandtse/CommonLibSSE-NG.git extern/CommonLibSSE-NG
if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/extern/CommonLibSSE-NG")
    set(CLIB_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/CommonLibSSE-NG")
elseif(DEFINED ENV{CommonLibSSEPath})
    file(TO_CMAKE_PATH "$ENV{CommonLibSSEPath}" CLIB_SOURCE_DIR)
endif()

if(NOT CLIB_SOURCE_DIR OR NOT EXISTS "${CLIB_SOURCE_DIR}/cmake/CommonLibSSE.cmake")
    message(FATAL_ERROR "CommonLibSSE-NG 7.5.1 source not found. Clone it into extern/CommonLibSSE-NG.")
endif()

message(STATUS "CommonLibSSE-NG: ${CLIB_SOURCE_DIR}")

# NG defaults this ON, which pulls Catch2 and fails configure.
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)

# SE+AE runtime dispatch = one DLL for SE, AE and 1.7.104.
# VR off: we don't target it, and it links openvr_api.lib from an uninitialized submodule.
set(ENABLE_SKYRIM_SE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "" FORCE)

add_subdirectory("${CLIB_SOURCE_DIR}" CommonLibSSE-NG)

# NG only includes this on its prebuilt path (which return()s early), so a source build
# never defines add_commonlibsse_plugin().
include("${CLIB_SOURCE_DIR}/cmake/CommonLibSSE.cmake")
find_package(imgui CONFIG REQUIRED)
find_package(directxtex CONFIG REQUIRED)
find_package(lunasvg CONFIG REQUIRED)

add_commonlibsse_plugin(${PROJECT_NAME} MINIMUM_SKSE_VERSION 2.0.17 SOURCES
    src/plugin.cpp
    src/PartySheetStateAPI.cpp
    src/InputHooks.cpp
    src/Settings.cpp
    src/SettingsIni.cpp
    src/LocalizationManager.cpp
    src/ImGui/Renderer.cpp
    src/InputHandler.cpp
    src/ImGui/FollowerHUD.cpp
    src/EffectCategories.cpp
    src/TextureManager.cpp
    src/ImGui/PartySheet.cpp
    src/ImGui/PartySheetV2.cpp
    src/ImGui/PartySheetV2Modal.cpp
    src/ImGui/PartySheetV2InventoryModal.cpp
    src/ImGui/PartySheetV2StatusModal.cpp
    src/ImGui/InspectCard.cpp
    src/ImGui/InspectCardKYE.cpp
    src/ImGui/CharacterSheet.cpp
    src/Utils/BarRenderer.cpp
    src/Utils/HUDDraw.cpp
    src/Utils/IconRenderer.cpp
    src/Utils/EquipmentStatResolver.cpp
    src/Utils/TraitResolver.cpp
    src/Utils/WornGearStatResolver.cpp
    src/Utils/LocationUtils.cpp
    src/ImGui/WidgetStableMount.cpp
    src/ImGui/WidgetHorsePicker.cpp
    src/ImGui/WidgetCollectibles.cpp
    src/ImGui/WidgetPinnedItems.cpp
    src/ImGui/PlayerHUDResistances.cpp
    src/ImGui/PlayerHUDResources.cpp
    src/ImGui/SettingsHotkeys.cpp
    src/ImGui/SettingsPartySheet.cpp
    src/ImGui/SettingsInspectCard.cpp
    src/ImGui/SettingsCharacterSheet.cpp
    src/ImGui/SettingsFollowerHUD.cpp
    src/ImGui/SettingsCollectibles.cpp
    src/ImGui/SettingsPinned.cpp
    src/ImGui/PlayerHUD.cpp
    src/ImGui/PlayerHUDPenalties.cpp
    src/ImGui/PlayerHUDSurvival.cpp
    src/ImGui/PlayerHUDDisease.cpp
    src/ImGui/PlayerHUDEquipment.cpp
    src/ImGui/PlayerHUDEffects.cpp
    src/ImGui/SettingsPlayerHUDEffects.cpp
    src/ImGui/SettingsPlayerHUD.cpp
    src/AllyEventHandler.cpp
    src/InventoryEventHandler.cpp
    src/ActorStats.cpp
    src/PlayerStatus.cpp
    src/BadgeStyle.cpp
    src/UIAnimation.cpp
    src/HUDVisibility.cpp
    src/SpellIconManager.cpp
    src/FontManager.cpp
    src/BackgroundManager.cpp
    src/HorseRegistry.cpp
    src/PotionAnimationBridge.cpp
    src/QuickHeal.cpp
    src/PNGIIBridge.cpp
)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23) # <--- use C++23 standard
target_precompile_headers(${PROJECT_NAME} PRIVATE includes/PCH.h) # <--- PCH.h is required!

# Always emit a matching PDB, even on Release, so a user's crash-log offsets can be
# resolved later. /Zi generates full debug info; /DEBUG writes the PDB; /OPT:REF /OPT:ICF
# keep the release binary optimized (debug info and optimization coexist fine).
# The PDB is archived (below), NOT shipped with the mod.
target_compile_options(${PROJECT_NAME} PRIVATE /Zi)
target_link_options(${PROJECT_NAME} PRIVATE /DEBUG /OPT:REF /OPT:ICF)

target_link_libraries(
	${PROJECT_NAME}
	PRIVATE
		Microsoft::DirectXTex
		imgui::imgui
		lunasvg::lunasvg
)

# Silence the stdext warnings
target_compile_definitions(${PROJECT_NAME} PRIVATE _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)

# When your SKSE .dll is compiled, this will automatically copy the .dll into your mods folder.
# Only works if you configure DEPLOY_ROOT above (or set the SKYRIM_MODS_FOLDER environment variable)
if(DEFINED OUTPUT_FOLDER)
    # If you specify an <OUTPUT_FOLDER> (including via environment variables)
    # then we'll copy your mod files into Skyrim or a mod manager for you!

    # Copy the SKSE plugin .dll files into the SKSE/Plugins/ folder
    set(DLL_FOLDER "${OUTPUT_FOLDER}/SKSE/Plugins")

    message(STATUS "SKSE plugin output folder: ${DLL_FOLDER}")
    # Copy runtime DLLs (add this section)

    add_custom_command(
        TARGET "${PROJECT_NAME}"
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${DLL_FOLDER}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>" "${DLL_FOLDER}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
        VERBATIM
    )

endif()

# Archive the PDB (symbols) for every build into a version-stamped folder that lives in the
# source tree, NOT the deployed mod folder. This is what lets us resolve a user's crash-log
# offsets later. Never ship these .pdb files in the public release zip.
# Layout: pdb-archive/<project-version>/SkyrimPartySheet.pdb  (+ the matching DLL alongside it,
# so the GUID/age pair is preserved together).
set(PDB_ARCHIVE "${CMAKE_SOURCE_DIR}/pdb-archive/${PROJECT_VERSION}")
add_custom_command(
    TARGET "${PROJECT_NAME}"
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${PDB_ARCHIVE}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_PDB_FILE:${PROJECT_NAME}>" "${PDB_ARCHIVE}/$<TARGET_PDB_FILE_NAME:${PROJECT_NAME}>"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>" "${PDB_ARCHIVE}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
    VERBATIM
)
