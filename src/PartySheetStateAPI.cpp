#include "../includes/PCH.h"
#include "../includes/PartySheetStateAPIInternal.h"
#include "../includes/Settings.h"
#include "../includes/MenuUtils.h"
#include "../includes/ImGui/PartySheet.h"
#include "../includes/ImGui/PartySheetV2.h"
#include "../includes/ImGui/CharacterSheet.h"
#include "../includes/ImGui/InspectCard.h"
#include "../includes/ImGui/Renderer.h"

#include <chrono>
#include <unordered_map>

namespace PartySheetAPI
{
    static StateMsg s_LastPublished{ 1, false, Reason::UserToggle, 0 };
    static bool     s_Published     = false;
    static Reason   s_PendingReason = Reason::UserToggle;
    static bool     s_SuppressEcho  = false;

    static std::uint32_t BuildPanelMask()
    {
        std::uint32_t mask = 0;
        if (PartySheet::IsVisible())                        mask |= PanelBit(Panel::PartySheet);
        if (PartySheetV2::IsVisible())                      mask |= PanelBit(Panel::PartySheetV2);
        if (CharacterSheet::GetSingleton()->IsVisible())    mask |= PanelBit(Panel::CharacterSheet);
        if (InspectCard::IsVisible())                       mask |= PanelBit(Panel::InspectCard);
        if (ImGuiRenderer::IsVisible())                     mask |= PanelBit(Panel::SettingsMenu);
        return mask;
    }

    static const char* ResolveCallerName(SKSE::PluginHandle a_handle)
    {
        if (a_handle == kInvalidPluginHandle) return "<invalid handle>";
        static std::unordered_map<SKSE::PluginHandle, std::string> s_names;
        auto it = s_names.find(a_handle);
        if (it != s_names.end()) return it->second.c_str();
        auto [ins, _] = s_names.emplace(a_handle, std::format("plugin#{}", a_handle));
        return ins->second.c_str();
    }

    static void NoteApiCall(const char* a_caller)
    {
        static std::uint32_t s_count = 0;
        static auto          s_windowStart = std::chrono::steady_clock::now();
        static bool          s_warned = false;

        const auto now = std::chrono::steady_clock::now();
        if (now - s_windowStart >= std::chrono::seconds(1)) {
            s_windowStart = now;
            s_count = 0;
        }
        if (++s_count > 10 && !s_warned) {
            s_warned = true;
            logger::warn("[PartySheetAPI] '{}' called RequestHUDHidden {}x in 1s — likely a "
                         "feedback loop with kMsg_State. Consumers must not call this from "
                         "inside their state handler.", a_caller, s_count);
        }
    }

    static constexpr std::uint32_t kFullscreenMask =
        PanelBit(Panel::PartySheet)     | PanelBit(Panel::PartySheetV2) |
        PanelBit(Panel::CharacterSheet) | PanelBit(Panel::SettingsMenu);

    class StateImpl final : public IPartySheetState1
    {
    public:
        static StateImpl* GetSingleton() { static StateImpl s; return &s; }

        std::uint32_t GetVersion() const override { return 1; }

        bool IsHUDHidden() const override
        {
            auto* s = Settings::GetSingleton();
            return s && s->hudOverrideHidden;
        }

        APIResult RequestHUDHidden(SKSE::PluginHandle a_myPluginHandle,
                                   bool a_hidden, Reason a_reason) override
        {
            const char* caller = ResolveCallerName(a_myPluginHandle);
            logger::debug("[PartySheetAPI] RequestHUDHidden(hidden={}, reason={}) from '{}'",
                          a_hidden, static_cast<std::uint32_t>(a_reason), caller);
            NoteApiCall(caller);

            if (a_myPluginHandle == kInvalidPluginHandle) return APIResult::Refused_BadHandle;

            auto* s = Settings::GetSingleton();
            if (!s) return APIResult::Refused_BadHandle;
            if (s->hudOverrideHidden == a_hidden) return APIResult::AlreadySet;
            if (MenuUtils::IsBlockingMenuOpen())  return APIResult::Refused_MenuOpen;

            s->hudOverrideHidden = a_hidden;
            s_PendingReason = a_reason;
            s_SuppressEcho  = true;
            PublishIfChanged();
            s_SuppressEcho  = false;

            logger::info("[PartySheetAPI] HUD {} by '{}'", a_hidden ? "hidden" : "shown", caller);
            return APIResult::OK;
        }

        bool IsPanelOpen(Panel a_panel) const override
        {
            if (a_panel >= Panel::Count) return false;
            return (BuildPanelMask() & PanelBit(a_panel)) != 0;
        }

        std::uint32_t GetOpenPanels() const override { return BuildPanelMask(); }

        bool IsFullscreenPanelOpen() const override
        {
            return (BuildPanelMask() & kFullscreenMask) != 0;
        }
    };

    void SetPendingReason(Reason a_reason) { s_PendingReason = a_reason; }

    void PublishIfChanged()
    {
        auto* s = Settings::GetSingleton();
        if (!s) return;

        StateMsg now{};
        now.version    = 1;
        now.hudHidden  = s->hudOverrideHidden;
        now.reason     = s_PendingReason;
        now.openPanels = BuildPanelMask();

        if (s_Published &&
            now.hudHidden  == s_LastPublished.hudHidden &&
            now.reason     == s_LastPublished.reason &&
            now.openPanels == s_LastPublished.openPanels) {
            return;
        }

        s_LastPublished = now;
        s_Published     = true;

        logger::debug("[PartySheetAPI] state: hidden={} reason={} panels=0x{:02X} [PS={} V2={} CS={} IC={} SET={}]",
                     now.hudHidden, static_cast<std::uint32_t>(now.reason), now.openPanels,
                     (now.openPanels & PanelBit(Panel::PartySheet))     ? 1 : 0,
                     (now.openPanels & PanelBit(Panel::PartySheetV2))   ? 1 : 0,
                     (now.openPanels & PanelBit(Panel::CharacterSheet)) ? 1 : 0,
                     (now.openPanels & PanelBit(Panel::InspectCard))    ? 1 : 0,
                     (now.openPanels & PanelBit(Panel::SettingsMenu))   ? 1 : 0);

        if (s_SuppressEcho) return;

        if (auto* messaging = SKSE::GetMessagingInterface()) {
            messaging->Dispatch(kMsg_State, &s_LastPublished,
                                static_cast<std::uint32_t>(sizeof(StateMsg)), nullptr);
        }
    }

    void DispatchInterface()
    {
        if (auto* messaging = SKSE::GetMessagingInterface()) {
            messaging->Dispatch(kMsg_Interface, StateImpl::GetSingleton(),
                                static_cast<std::uint32_t>(sizeof(void*)), nullptr);
            logger::info("PartySheetAPI: interface dispatched (v1)");
        }
    }
}
