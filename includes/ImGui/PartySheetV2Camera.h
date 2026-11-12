#pragma once

namespace PartySheetV2Camera {
    void RegisterSmoothCam();
    void RequestSmoothCam();
    bool Enter();
    void Exit();
    bool IsActive();
    bool ShowSubject(RE::Actor* subject);
    void SetNudgeKey(std::uint32_t scancode, bool down);
    void Nudge(float dt);
    void RetryPending();
}
