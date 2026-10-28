#pragma once

// STATUS: Working
// Processes inventory changes and updates caches.

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

namespace InventoryEvents {
    class ContainerChangedSink : public RE::BSTEventSink<RE::TESContainerChangedEvent> {
    public:
        static ContainerChangedSink* GetSingleton() {
            static ContainerChangedSink singleton;
            return &singleton;
        }
        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESContainerChangedEvent* event,
            RE::BSTEventSource<RE::TESContainerChangedEvent>*) override;
    private:
        ContainerChangedSink() = default;
        ContainerChangedSink(const ContainerChangedSink&) = delete;
        ContainerChangedSink& operator=(const ContainerChangedSink&) = delete;
    };

    void RegisterEventSinks();
}
