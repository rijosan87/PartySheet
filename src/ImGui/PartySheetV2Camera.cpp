
#define SMOOTHCAM_API_COMMONLIB
#include "../../includes/SmoothCamAPI.h"
#include "../../includes/ImGui/PartySheetV2Camera.h"
#include "../../includes/Settings.h"
#include "../../includes/TrueDirectionalMovementAPI.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <algorithm>
#include <array>
#include <string_view>

namespace PartySheetV2Camera {
    namespace {
        constexpr float kPi    = 3.14159265f;
        constexpr float kFOV   = 50.0f;

        constexpr std::array<std::string_view, 10> kIni{
            "fOverShoulderCombatPosX:Camera", "fOverShoulderCombatAddY:Camera",
            "fOverShoulderCombatPosZ:Camera", "fOverShoulderPosX:Camera",
            "fOverShoulderPosZ:Camera",       "fVanityModeMinDist:Camera",
            "fVanityModeMaxDist:Camera",      "fAutoVanityModeDelay:Camera",
            "fMouseWheelZoomSpeed:Camera",    "fTogglePOVDelay:Controls" };

        struct Saved {
            RE::NiPoint3 posOffsetExpected;
            RE::NiPoint2 freeRotation;
            float targetZoomOffset = 0.0f;
            float pitchZoomOffset  = 0.0f;
            float worldFOV         = 0.0f;
            float pitch            = 0.0f;
            bool  freeRotationEnabled = false;
            bool  toggleAnimCam       = false;
            std::array<float, 10> ini{};
        };
        Saved s_saved{};
        bool  s_active = false;
        bool  s_ownsSmoothCam = false;
        bool  s_wasFirstPerson = false;
        bool  s_activateDisabled = false;
        SmoothCamAPI::IVSmoothCam2* g_smoothCam = nullptr;

        bool s_tdmYaw = false;
        bool s_tdmDirMove = false;
        TDM_API::IVTDM2* Tdm() {
            static TDM_API::IVTDM2* api = static_cast<TDM_API::IVTDM2*>(
                TDM_API::RequestPluginAPI(TDM_API::InterfaceVersion::V2));
            return api;
        }
        void HoldTdm(RE::PlayerCharacter* player) {
            auto* tdm = Tdm();
            if (!tdm) return;
            const auto h = SKSE::GetPluginHandle();
            const auto d = tdm->RequestDisableDirectionalMovement(h);
            s_tdmDirMove = d == TDM_API::APIResult::OK || d == TDM_API::APIResult::AlreadyGiven;
            const auto y = tdm->RequestYawControl(h, 0.0f);
            s_tdmYaw = y == TDM_API::APIResult::OK || y == TDM_API::APIResult::AlreadyGiven;
            if (s_tdmYaw) (void)tdm->SetPlayerYaw(h, player->data.angle.z);
            logger::debug("PartySheetV2Camera: TDM hold dirMove={} yaw={} lock={}",
                          static_cast<int>(d), static_cast<int>(y), tdm->GetTargetLockState());
        }
        void ReleaseTdm() {
            auto* tdm = Tdm();
            if (!tdm) return;
            const auto h = SKSE::GetPluginHandle();
            if (s_tdmYaw) (void)tdm->ReleaseYawControl(h);
            if (s_tdmDirMove) (void)tdm->ReleaseDisableDirectionalMovement(h);
            s_tdmYaw = s_tdmDirMove = false;
        }

        RE::Setting* Ini(std::size_t i) {
            auto* c = RE::INISettingCollection::GetSingleton();
            return c ? c->GetSetting(kIni[i]) : nullptr;
        }
        void SetIni(std::size_t i, float v) {
            if (auto* s = Ini(i)) s->data.f = v;
        }

        void SetHUDMenusHidden(bool hidden) {
            auto* ui = RE::UI::GetSingleton();
            if (!ui) return;
            for (std::string_view name : { RE::HUDMenu::MENU_NAME, std::string_view{ "TrueHUD" },
                                           std::string_view{ "BTPS Menu" } }) {
                auto menu = ui->GetMenu(name);
                if (menu && menu->uiMovie) menu->uiMovie->SetVisible(!hidden);
            }
        }

        void SetActivateEnabled(bool enabled) {
            auto* pc = RE::PlayerControls::GetSingleton();
            if (pc && pc->activateHandler) pc->activateHandler->SetInputEventHandlingEnabled(enabled);
        }

        float s_yawNudge = 0.0f;
        float s_zNudge   = 0.0f;
        float s_baseZ    = 0.0f;
        int   s_pendingFrames = 0;
        bool  s_inRetry = false;
        bool  s_keyA = false, s_keyD = false, s_keyQ = false, s_keyE = false;

        constexpr float kMaxDist = 1500.0f;
        RE::ActorHandle s_target;
        float           s_targetPitch = 0.0f;

        bool CanShow(RE::Actor* a, RE::PlayerCharacter* player) {
            if (!a || !a->Is3DLoaded() || a->IsDead()) return false;
            if (!a->HasKeywordString("ActorTypeNPC")) return false;
            if (a->IsInCombat() || a->IsOnMount()) return false;
            if (auto* st = a->AsActorState(); st && st->IsBleedingOut()) return false;
            if (a->GetPosition().GetDistance(player->GetPosition()) > kMaxDist) return false;
            auto* pc = player->GetParentCell();
            auto* ac = a->GetParentCell();
            if (!pc || !ac) return false;
            return pc->IsInteriorCell() ? ac == pc : !ac->IsInteriorCell();
        }

        void ApplyFraming(RE::PlayerCamera* camera, RE::ThirdPersonState* tps, RE::Actor* actor) {
            auto* state = actor->AsActorState();
            const bool  drawn  = state && state->IsWeaponDrawn();
            const auto  sit    = state ? state->GetSitSleepState() : RE::SIT_SLEEP_STATE::kNormal;
            const bool  seated = sit >= RE::SIT_SLEEP_STATE::kWantToSit &&
                                 sit <= RE::SIT_SLEEP_STATE::kWantToStand;
            const float h = actor->GetHeight();
            const auto* set = Settings::GetSingleton();
            const float extraDist = static_cast<float>(set->psV2LiveCamDist);
            float x = static_cast<float>(set->psV2LiveCamX), z = -22.0f + (h - 128.0f), dist = 155.0f + extraDist;
            if (seated)     { z = -53.0f + (h - 130.0f); }
            else if (drawn) { z = -32.0f + (h - 128.0f); dist = 165.0f + extraDist; x -= 5.0f; }
            z += static_cast<float>(set->psV2LiveCamZ);
            s_baseZ = z;
            s_yawNudge = s_zNudge = 0.0f;

            tps->toggleAnimCam       = true;
            tps->freeRotationEnabled = true;
            tps->freeRotation        = { kPi, 0.0f };
            tps->pitchZoomOffset     = 0.1f;
            actor->data.angle.x      = 0.1f;

            SetIni(0, x); SetIni(1, 0.0f); SetIni(2, z);
            SetIni(3, x);                  SetIni(4, z);
            SetIni(5, dist); SetIni(6, dist);
            SetIni(7, 10800.0f);
            SetIni(8, 10000.0f);
            SetIni(9, 10800.0f);
            tps->posOffsetExpected = tps->posOffsetActual = RE::NiPoint3(x, 0.0f, z);
            camera->GetRuntimeData2().worldFOV = kFOV;
            camera->Update();
        }

        void ReleaseTarget() {
            if (s_target) {
                if (auto prev = s_target.get()) prev->data.angle.x = s_targetPitch;
                s_target.reset();
            }
        }

        void Retarget(RE::PlayerCamera* camera, RE::ThirdPersonState* tps,
                      RE::PlayerCharacter* player, RE::Actor* next) {
            ReleaseTarget();
            if (next != player) {
                s_target      = next->GetHandle();
                s_targetPitch = next->data.angle.x;
            }
            camera->cameraTarget = next->GetHandle();
            ApplyFraming(camera, tps, next);
            logger::debug("PartySheetV2Camera: target -> {:08X}", next->GetFormID());        }

    }

    void RegisterSmoothCam() {
        const bool ok = SmoothCamAPI::RegisterInterfaceLoaderCallback(SKSE::GetMessagingInterface(),
            [](void* inst, SmoothCamAPI::InterfaceVersion ver) {
                if (ver == SmoothCamAPI::InterfaceVersion::V2) {
                    g_smoothCam = reinterpret_cast<SmoothCamAPI::IVSmoothCam2*>(inst);
                    logger::info("SmoothCam API V2 acquired");
                }
            });
        logger::debug("PartySheetV2Camera: SmoothCam loader callback registered={}", ok);
    }

    void RequestSmoothCam() {
        const bool listened = SmoothCamAPI::RequestInterface(SKSE::GetMessagingInterface(),
                                                             SmoothCamAPI::InterfaceVersion::V2);
        logger::debug("PartySheetV2Camera: SmoothCam interface requested, listener={}", listened);
    }

    bool IsActive() { return s_active; }

    bool Enter() {
        if (s_active) return true;
        if (!Settings::GetSingleton()->psV2LiveCharacter) return false;
        auto* camera = RE::PlayerCamera::GetSingleton();
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!camera || !player || player->IsOnMount()) return false;
        s_wasFirstPerson = camera->IsInFirstPerson();
        if (!s_wasFirstPerson && !camera->IsInThirdPerson()) {
            s_wasFirstPerson = false;
            if (!s_inRetry) s_pendingFrames = 120;
            logger::debug("PartySheetV2Camera: camera state {} not first/third, retry pending",
                          camera->currentState ? static_cast<int>(camera->currentState->id) : -1);
            return false;
        }

        if (g_smoothCam && g_smoothCam->IsCameraEnabled()) {
            const auto r = g_smoothCam->RequestCameraControl(SKSE::GetPluginHandle());
            if (r != SmoothCamAPI::APIResult::OK && r != SmoothCamAPI::APIResult::AlreadyGiven) {
                logger::info("PartySheetV2Camera: SmoothCam refused control ({}), live character off",
                             static_cast<int>(r));
                s_wasFirstPerson = false;
                return false;
            }
            (void)g_smoothCam->RequestInterpolatorUpdates(SKSE::GetPluginHandle(), true);
            s_ownsSmoothCam = true;
        }

        if (s_wasFirstPerson) camera->ForceThirdPerson();
        auto* tps = skyrim_cast<RE::ThirdPersonState*>(camera->currentState.get());
        if (!tps) {
            Exit();
            return false;
        }

        s_saved.posOffsetExpected   = tps->posOffsetExpected;
        s_saved.freeRotation        = tps->freeRotation;
        s_saved.targetZoomOffset    = tps->targetZoomOffset;
        s_saved.pitchZoomOffset     = tps->pitchZoomOffset;
        s_saved.freeRotationEnabled = tps->freeRotationEnabled;
        s_saved.toggleAnimCam       = tps->toggleAnimCam;
        s_saved.worldFOV            = camera->GetRuntimeData2().worldFOV;
        s_saved.pitch               = player->data.angle.x;
        for (std::size_t i = 0; i < kIni.size(); ++i) {
            auto* s = Ini(i);
            s_saved.ini[i] = s ? s->GetFloat() : 0.0f;
        }
        s_active = true;

        HoldTdm(player);
        ApplyFraming(camera, tps, player);
        SetHUDMenusHidden(true);
        SetActivateEnabled(false);
        s_activateDisabled = true;
        logger::debug("PartySheetV2Camera: entered (firstPerson={}, smoothCam={})", s_wasFirstPerson, s_ownsSmoothCam);
        return true;
    }

    bool ShowSubject(RE::Actor* subject) {
        if (!s_active || !subject) return false;
        auto* camera = RE::PlayerCamera::GetSingleton();
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!camera || !player) return false;
        auto* tps = skyrim_cast<RE::ThirdPersonState*>(camera->currentState.get());
        if (!tps) return false;

        RE::Actor* want = subject->IsPlayerRef() ? player
                        : (CanShow(subject, player) ? subject : nullptr);
        RE::ActorPtr curHold = s_target ? s_target.get() : RE::ActorPtr{};
        RE::Actor* cur = s_target ? curHold.get() : player;

        if (!want) {
            if (cur != player) Retarget(camera, tps, player, player);
            return false;
        }
        if (want != cur) {
            Retarget(camera, tps, player, want);
        }
        return true;
    }

    void RetryPending() {
        if (s_active || s_pendingFrames <= 0) return;
        --s_pendingFrames;
        s_inRetry = true;
        if (Enter()) s_pendingFrames = 0;
        s_inRetry = false;
    }

    void SetNudgeKey(std::uint32_t scancode, bool down) {
        switch (scancode) {
            case 0x1E: s_keyA = down; break;
            case 0x20: s_keyD = down; break;
            case 0x10: s_keyQ = down; break;
            case 0x12: s_keyE = down; break;
            default: break;
        }
    }

    void Nudge(float dt) {
        const int yawDir = (s_keyD ? 1 : 0) - (s_keyA ? 1 : 0);
        const int zDir   = (s_keyQ ? 1 : 0) - (s_keyE ? 1 : 0);
        if (!s_active || (!yawDir && !zDir)) return;
        auto* camera = RE::PlayerCamera::GetSingleton();
        auto* tps = camera ? skyrim_cast<RE::ThirdPersonState*>(camera->currentState.get()) : nullptr;
        if (!tps) return;
        if (yawDir) {
            s_yawNudge += yawDir * 1.6f * dt;
            if (s_yawNudge >  kPi) s_yawNudge -= 2.0f * kPi;
            if (s_yawNudge < -kPi) s_yawNudge += 2.0f * kPi;
            tps->freeRotation.x = kPi + s_yawNudge;
        }
        if (zDir) {
            s_zNudge = std::clamp(s_zNudge + zDir * 60.0f * dt, -80.0f, 80.0f);
            const float z = s_baseZ + s_zNudge;
            SetIni(2, z); SetIni(4, z);
            tps->posOffsetExpected.z = tps->posOffsetActual.z = z;
        }
        camera->Update();
    }

    void Exit() {
        if (s_active) {
            for (std::size_t i = 0; i < kIni.size(); ++i)
                if (i != 8) SetIni(i, s_saved.ini[i]);
        }
        if (s_activateDisabled) {
            SetActivateEnabled(true);
            SetHUDMenusHidden(false);
            s_activateDisabled = false;
        }
        auto* camera = RE::PlayerCamera::GetSingleton();
        if (s_active) {
            ReleaseTarget();
            if (camera)
                if (auto* p = RE::PlayerCharacter::GetSingleton())
                    camera->cameraTarget = p->GetHandle();
        }
        if (s_active) {
            if (auto* player = RE::PlayerCharacter::GetSingleton())
                player->data.angle.x = s_saved.pitch;
            if (camera) {
                if (auto* tps = skyrim_cast<RE::ThirdPersonState*>(camera->currentState.get())) {
                    tps->toggleAnimCam       = s_saved.toggleAnimCam;
                    tps->freeRotationEnabled = s_saved.freeRotationEnabled;
                    tps->freeRotation        = s_saved.freeRotation;
                    tps->targetZoomOffset    = s_saved.targetZoomOffset;
                    tps->pitchZoomOffset     = s_saved.pitchZoomOffset;
                    tps->posOffsetExpected = tps->posOffsetActual = s_saved.posOffsetExpected;
                }
                camera->GetRuntimeData2().worldFOV = s_saved.worldFOV;
            }
        }
        if (camera && s_wasFirstPerson && !camera->IsInFirstPerson()) camera->ForceFirstPerson();
        if (s_active && camera) camera->Update();
        if (s_active) SetIni(8, s_saved.ini[8]);
        ReleaseTdm();
        if (s_ownsSmoothCam && g_smoothCam) {
            const auto r = g_smoothCam->ReleaseCameraControl(SKSE::GetPluginHandle());
            logger::debug("PartySheetV2Camera: SmoothCam release result {}", static_cast<int>(r));
        }
        if (s_active) logger::debug("PartySheetV2Camera: exited");
        s_active = s_ownsSmoothCam = s_wasFirstPerson = false;
        s_keyA = s_keyD = s_keyQ = s_keyE = false;
        s_pendingFrames = 0;
        s_target.reset();
    }
}
