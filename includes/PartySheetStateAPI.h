/*
* For modders: Copy this file into your own project if you wish to use this API
*/
#pragma once
#include <cstdint>
#include "SKSE/SKSE.h"

namespace PartySheetAPI
{
    inline constexpr SKSE::PluginHandle kInvalidPluginHandle = static_cast<SKSE::PluginHandle>(-1);
    inline constexpr const char* kSender             = "SkyrimPartySheet";
    inline constexpr const char* kPartySheetPluginName = "SkyrimPartySheet";

    enum class APIResult : std::uint8_t {
        OK = 0,
        AlreadySet,
        Refused_MenuOpen,
        Refused_BadHandle,
        Refused_BadPanel,       // v2 only
        Refused_Unavailable,    // v2 only: panel disabled, or Inspect Card has no target
    };

    enum : std::uint32_t {
        kMsg_Interface  = 0x50534831,
        kMsg_State      = 0x50534832,
        kMsg_Interface2 = 0x50534833,  // data is IPartySheetState2*
    };

    enum class Panel : std::uint32_t {
        PartySheet     = 0,
        PartySheetV2   = 1,
        CharacterSheet = 2,
        InspectCard    = 3,
        SettingsMenu   = 4,
        Count          = 5,
    };

    enum class Reason : std::uint32_t {
        UserToggle = 0,
        MenuOpen   = 1,
    };

    inline constexpr std::uint32_t PanelBit(Panel p) {
        return 1u << static_cast<std::uint32_t>(p);
    }

    struct StateMsg {
        std::uint32_t version;
        bool          hudHidden;
        Reason        reason;
        std::uint32_t openPanels;
    };

    class IPartySheetState1 {
    public:
        virtual ~IPartySheetState1() = default;

        virtual std::uint32_t GetVersion() const = 0;

        virtual bool IsHUDHidden() const = 0;
        virtual APIResult RequestHUDHidden(SKSE::PluginHandle a_myPluginHandle,
                                           bool a_hidden, Reason a_reason) = 0;

        virtual bool          IsPanelOpen(Panel a_panel) const = 0;
        virtual std::uint32_t GetOpenPanels() const = 0;
        virtual bool IsFullscreenPanelOpen() const = 0;
    };

    // v1 stays frozen (GetVersion() == 1); v2 arrives only via kMsg_Interface2.
    // PartySheet and PartySheetV2 both act on the sheet psUseFullscreenMode selects.
    class IPartySheetState2 : public IPartySheetState1 {
    public:
        virtual std::uint32_t GetInterfaceVersion() const = 0;   // == 2

        // Queued to the main thread: OK means queued. Read IsPanelOpen or kMsg_State for the result.
        virtual APIResult OpenPanel(SKSE::PluginHandle a_myPluginHandle, Panel a_panel) = 0;
        virtual APIResult ClosePanel(SKSE::PluginHandle a_myPluginHandle, Panel a_panel) = 0;

        // Runtime only, nothing saved. Held per plugin; the key returns when the last holder releases.
        virtual APIResult SetHotkeySuppressed(SKSE::PluginHandle a_myPluginHandle,
                                              Panel a_panel, bool a_suppressed) = 0;
        virtual bool IsHotkeySuppressed(Panel a_panel) const = 0;
    };
}

