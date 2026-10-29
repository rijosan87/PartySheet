#include "../includes/PCH.h"
#include "../includes/PartySheetStateAPIInternal.h"
#include "../includes/Settings.h"
#include "../includes/MenuUtils.h"
#include "../includes/ImGui/PartySheet.h"
#include "../includes/ImGui/PartySheetV2.h"
#include "../includes/ImGui/CharacterSheet.h"
#include "../includes/ImGui/InspectCard.h"
#include "../includes/ImGui/Renderer.h"

#include <array>
#include <chrono>
#include <mutex>
#include <set>
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

    // Callers may be on any thread; the input sink reads on the main thread.
    static std::mutex s_SuppressMutex;
    static std::array<std::set<SKSE::PluginHandle>, static_cast<std::size_t>(Panel::Count)> s_Suppressors;

    // PartySheet and PartySheetV2 share partySheetHotkey and one Open/Close route.
    static Panel CanonicalPanel(Panel a_panel)
    {
        return a_panel == Panel::PartySheetV2 ? Panel::PartySheet : a_panel;
    }

    bool IsHotkeySuppressedInternal(Panel a_panel)
    {
        if (a_panel >= Panel::Count) return false;
        std::scoped_lock lock(s_SuppressMutex);
        return !s_Suppressors[static_cast<std::size_t>(CanonicalPanel(a_panel))].empty();
    }

    static bool IsSheetOpen(Panel a_panel)
    {
        switch (CanonicalPanel(a_panel)) {
        case Panel::PartySheet:     return PartySheet::IsVisible() || PartySheetV2::IsVisible();
        case Panel::CharacterSheet: return CharacterSheet::GetSingleton()->IsVisible();
        case Panel::InspectCard:    return InspectCard::IsVisible();
        case Panel::SettingsMenu:   return ImGuiRenderer::IsVisible();
        default:                    return false;
        }
    }

    // Main thread. Gates mirror the hotkeys and are re-checked since a menu may have opened.
    static void RunOpen(Panel a_panel, const std::string& a_caller)
    {
        auto* s = Settings::GetSingleton();
        if (!s || IsSheetOpen(a_panel)) return;

        switch (CanonicalPanel(a_panel)) {
        case Panel::PartySheet:
            if (MenuUtils::ShouldBlockCharacterSheet()) break;
            PlayPanelSound(true);
            if (s->psUseFullscreenMode) PartySheetV2::Toggle();
            else                        PartySheet::Show();
            break;
        case Panel::CharacterSheet:
            if (!s->enableCharacterSheet || MenuUtils::ShouldBlockCharacterSheet()) break;
            CharacterSheet::GetSingleton()->Open();
            if (CharacterSheet::GetSingleton()->IsVisible()) PlayPanelSound(true);
            break;
        case Panel::InspectCard:
            if (MenuUtils::IsBlockingMenuOpen()) break;
            if (OpenInspectCardFromAPI() != APIResult::OK) {
                logger::info("[PartySheetAPI] OpenPanel(InspectCard) from '{}' dropped: no target or gate failed", a_caller);
            }
            break;
        case Panel::SettingsMenu:
            ImGuiRenderer::Toggle();
            break;
        default:
            break;
        }
        PublishIfChanged();
    }

    static void RunClose(Panel a_panel)
    {
        switch (CanonicalPanel(a_panel)) {
        case Panel::PartySheet:
            // Toggle, not a raw flag flip: V1's Toggle also unfreezes time.
            if (PartySheet::IsVisible())   { PlayPanelSound(false); PartySheet::Toggle(); }
            if (PartySheetV2::IsVisible()) { PlayPanelSound(false); PartySheetV2::Toggle(); }
            break;
        case Panel::CharacterSheet:
            if (CharacterSheet::GetSingleton()->IsVisible()) {
                PlayPanelSound(false);
                CharacterSheet::GetSingleton()->Close();
            }
            break;
        case Panel::InspectCard:
            if (InspectCard::IsVisible()) CloseInspectCardFromAPI();
            break;
        case Panel::SettingsMenu:
            if (ImGuiRenderer::IsVisible()) ImGuiRenderer::Toggle();
            break;
        default:
            break;
        }
        PublishIfChanged();
    }

    class StateImpl final : public IPartySheetState2
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

        std::uint32_t GetInterfaceVersion() const override { return 2; }

        APIResult OpenPanel(SKSE::PluginHandle a_myPluginHandle, Panel a_panel) override
        {
            return QueuePanel(a_myPluginHandle, a_panel, true);
        }

        APIResult ClosePanel(SKSE::PluginHandle a_myPluginHandle, Panel a_panel) override
        {
            return QueuePanel(a_myPluginHandle, a_panel, false);
        }

        APIResult SetHotkeySuppressed(SKSE::PluginHandle a_myPluginHandle,
                                      Panel a_panel, bool a_suppressed) override
        {
            if (a_myPluginHandle == kInvalidPluginHandle) return APIResult::Refused_BadHandle;
            if (a_panel >= Panel::Count)                  return APIResult::Refused_BadPanel;

            bool changed;
            {
                std::scoped_lock lock(s_SuppressMutex);
                auto& holders = s_Suppressors[static_cast<std::size_t>(CanonicalPanel(a_panel))];
                changed = a_suppressed ? holders.insert(a_myPluginHandle).second
                                       : holders.erase(a_myPluginHandle) > 0;
            }
            if (!changed) return APIResult::AlreadySet;

            logger::info("[PartySheetAPI] hotkey for panel {} {} by '{}'",
                         static_cast<std::uint32_t>(a_panel),
                         a_suppressed ? "suppressed" : "released",
                         ResolveCallerName(a_myPluginHandle));
            return APIResult::OK;
        }

        bool IsHotkeySuppressed(Panel a_panel) const override
        {
            return IsHotkeySuppressedInternal(a_panel);
        }

    private:
        static APIResult QueuePanel(SKSE::PluginHandle a_handle, Panel a_panel, bool a_open)
        {
            const char* caller = ResolveCallerName(a_handle);
            logger::debug("[PartySheetAPI] {}Panel({}) from '{}'", a_open ? "Open" : "Close",
                          static_cast<std::uint32_t>(a_panel), caller);

            if (a_handle == kInvalidPluginHandle) return APIResult::Refused_BadHandle;
            if (a_panel >= Panel::Count)          return APIResult::Refused_BadPanel;
            if (IsSheetOpen(a_panel) == a_open)   return APIResult::AlreadySet;

            if (a_open) {
                auto* s = Settings::GetSingleton();
                if (!s) return APIResult::Refused_Unavailable;
                if (CanonicalPanel(a_panel) == Panel::CharacterSheet && !s->enableCharacterSheet)
                    return APIResult::Refused_Unavailable;
                if (CanonicalPanel(a_panel) != Panel::SettingsMenu && MenuUtils::IsBlockingMenuOpen())
                    return APIResult::Refused_MenuOpen;
            }

            auto* tasks = SKSE::GetTaskInterface();
            if (!tasks) return APIResult::Refused_Unavailable;
            std::string callerName = caller;
            tasks->AddTask([a_panel, a_open, callerName]() {
                if (a_open) RunOpen(a_panel, callerName);
                else        RunClose(a_panel);
            });
            return APIResult::OK;
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
            // v1 consumers require GetVersion()==1, so v2 gets its own message.
            auto* impl = StateImpl::GetSingleton();
            messaging->Dispatch(kMsg_Interface, static_cast<IPartySheetState1*>(impl),
                                static_cast<std::uint32_t>(sizeof(void*)), nullptr);
            messaging->Dispatch(kMsg_Interface2, static_cast<IPartySheetState2*>(impl),
                                static_cast<std::uint32_t>(sizeof(void*)), nullptr);
            logger::info("PartySheetAPI: interface dispatched (v1, v2)");
        }
    }
}
