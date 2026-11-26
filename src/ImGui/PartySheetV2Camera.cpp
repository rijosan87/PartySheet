
#define SMOOTHCAM_API_COMMONLIB
#include "../../includes/SmoothCamAPI.h"
#include "../../includes/ImGui/PartySheetV2Camera.h"
#include "../../includes/ImGui/PartySheetV2Light.h"
#include "../../includes/MenuUtils.h"
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
        RE::BSTSmartPointer<RE::TESCameraState> s_framedState;
        int s_loggedStateId = -1;
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

        int StateId(const RE::PlayerCamera* camera) {
            return camera && camera->currentState ? static_cast<int>(camera->currentState->id) : -1;
        }

        void LogCamera(const char* when, RE::PlayerCamera* camera, const RE::ThirdPersonState* tps) {
            if (!camera || !tps) {
                logger::debug("[V2Cam] {}: state={} no third person state", when, StateId(camera));
                return;
            }
            auto* player = RE::PlayerCharacter::GetSingleton();
            auto target = camera->cameraTarget.get();
            logger::debug("[V2Cam] {}: state={} target={:08X} freeRot=({:.3f}, {:.3f}) freeOn={} animCam={} "
                          "pitchZoom={:.3f} zoom={:.3f}/{:.3f} offset=({:.1f}, {:.1f}, {:.1f}) fov={:.1f} pitch={:.3f} heading={:.3f}",
                          when, StateId(camera), target ? target->GetFormID() : 0,
                          tps->freeRotation.x, tps->freeRotation.y, tps->freeRotationEnabled, tps->toggleAnimCam,
                          tps->pitchZoomOffset, tps->currentZoomOffset, tps->targetZoomOffset,
                          tps->posOffsetExpected.x, tps->posOffsetExpected.y, tps->posOffsetExpected.z,
                          camera->GetRuntimeData2().worldFOV,
                          player ? player->data.angle.x : 0.0f, player ? player->data.angle.z : 0.0f);
        }

        RE::Setting* Ini(std::size_t i) {
            auto* c = RE::INISettingCollection::GetSingleton();
            return c ? c->GetSetting(kIni[i]) : nullptr;
        }
        void SetIni(std::size_t i, float v) {
            if (auto* s = Ini(i)) s->data.f = v;
        }

        void SetActivateEnabled(bool enabled) {
            auto* pc = RE::PlayerControls::GetSingleton();
            if (pc && pc->activateHandler) pc->activateHandler->SetInputEventHandlingEnabled(enabled);
        }

        float s_yawNudge = 0.0f;
        float s_zNudge   = 0.0f;
        float s_distNudge = 0.0f;
        float s_baseZ    = 0.0f;
        float s_baseDist  = 0.0f;
        int   s_pendingFrames = 0;
        bool  s_inRetry = false;
        bool  s_keyA = false, s_keyD = false, s_keyQ = false, s_keyE = false, s_keyW = false, s_keyS = false;
        bool  s_savePending = false;
        bool  s_liveSetting = false;

        int s_appliedCamX = 0, s_appliedCamZ = 0, s_appliedCamDist = 0, s_appliedCamYaw = 0;

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

        int ActiveCamX(const Settings* set) {
            return set->psV2CharacterRight ? set->psV2LiveCamXRight : set->psV2LiveCamX;
        }

        int ActiveCamYaw(const Settings* set) {
            return set->psV2CharacterRight ? set->psV2LiveCamYawRight : 0;
        }

        void ApplyFraming(RE::PlayerCamera* camera, RE::ThirdPersonState* tps, RE::Actor* actor) {
            auto* state = actor->AsActorState();
            const bool  drawn  = state && state->IsWeaponDrawn();
            const auto  sit    = state ? state->GetSitSleepState() : RE::SIT_SLEEP_STATE::kNormal;
            const bool  seated = sit >= RE::SIT_SLEEP_STATE::kWantToSit &&
                                 sit <= RE::SIT_SLEEP_STATE::kWantToStand;
            const float h = actor->GetHeight();
            const auto* set = Settings::GetSingleton();
            s_appliedCamX    = ActiveCamX(set);
            s_appliedCamZ    = set->psV2LiveCamZ;
            s_appliedCamDist = set->psV2LiveCamDist;
            const float extraDist = static_cast<float>(set->psV2LiveCamDist);
            float x = static_cast<float>(s_appliedCamX), z = -22.0f + (h - 128.0f), dist = 155.0f + extraDist;
            if (seated)     { z = -53.0f + (h - 130.0f); }
            else if (drawn) { z = -32.0f + (h - 128.0f); dist = 165.0f + extraDist; x -= 5.0f; }
            z += static_cast<float>(set->psV2LiveCamZ);
            s_baseZ = z;
            s_baseDist = dist;
            s_appliedCamYaw = ActiveCamYaw(set);
            s_yawNudge = static_cast<float>(s_appliedCamYaw) * kPi / 180.0f;
            s_zNudge = s_distNudge = 0.0f;

            tps->toggleAnimCam       = true;
            tps->freeRotationEnabled = true;
            tps->freeRotation        = { kPi + s_yawNudge, 0.0f };
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
        s_liveSetting = Settings::GetSingleton()->psV2LiveCharacter;
        if (!s_liveSetting) return false;
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
        s_framedState   = camera->currentState;
        s_loggedStateId = StateId(camera);
        s_active = true;
        LogCamera("enter", camera, tps);

        HoldTdm(player);
        ApplyFraming(camera, tps, player);
        MenuUtils::SetHUDMenusHidden(true);
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
        if (const int id = StateId(camera); id != s_loggedStateId) {
            logger::debug("[V2Cam] camera mode changed while open: {} -> {}", s_loggedStateId, id);
            s_loggedStateId = id;
        }
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
        else if (const auto* set = Settings::GetSingleton();
                 ActiveCamX(set) != s_appliedCamX || ActiveCamYaw(set) != s_appliedCamYaw || set->psV2LiveCamZ != s_appliedCamZ ||
                 set->psV2LiveCamDist != s_appliedCamDist) {
            ApplyFraming(camera, tps, want);
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

    void FollowLiveSetting() {
        const bool want = Settings::GetSingleton()->psV2LiveCharacter;
        if (want == s_liveSetting) return;
        s_liveSetting = want;
        if (want) {
            Enter();
        } else {
            s_pendingFrames = 0;
            Exit();
        }
    }

    void SetNudgeKey(std::uint32_t scancode, bool down) {
        bool* key = nullptr;
        bool  savesSetting = false;
        switch (scancode) {
            case 0x1E: key = &s_keyA; savesSetting = Settings::GetSingleton()->psV2CharacterRight; break;
            case 0x20: key = &s_keyD; savesSetting = Settings::GetSingleton()->psV2CharacterRight; break;
            case 0x10: key = &s_keyQ; savesSetting = true; break;
            case 0x12: key = &s_keyE; savesSetting = true; break;
            case 0x11: key = &s_keyW; savesSetting = true; break;
            case 0x1F: key = &s_keyS; savesSetting = true; break;
            default: return;
        }
        if (savesSetting && *key && !down) s_savePending = true;
        *key = down;
    }

    namespace {
        void SaveNudge() {
            auto* set = Settings::GetSingleton();
            const int z    = std::clamp(static_cast<int>(std::lround(s_appliedCamZ + s_zNudge)), -60, 60);
            const int dist = std::clamp(static_cast<int>(std::lround(s_appliedCamDist + s_distNudge)), -120, 400);
            const int yaw  = set->psV2CharacterRight
                ? std::clamp(static_cast<int>(std::lround(s_yawNudge * 180.0f / kPi)), -45, 45)
                : set->psV2LiveCamYawRight;
            if (z == set->psV2LiveCamZ && dist == set->psV2LiveCamDist && yaw == set->psV2LiveCamYawRight) return;
            set->psV2LiveCamYawRight = yaw;
            if (set->psV2CharacterRight) s_appliedCamYaw = yaw;
            const float dz = static_cast<float>(z - s_appliedCamZ);
            const float dd = static_cast<float>(dist - s_appliedCamDist);
            s_baseZ += dz;    s_zNudge -= dz;
            s_baseDist += dd; s_distNudge -= dd;
            set->psV2LiveCamZ = s_appliedCamZ = z;
            set->psV2LiveCamDist = s_appliedCamDist = dist;
            set->Save();
            logger::debug("PartySheetV2Camera: saved nudge height={} distance={} turn={}", z, dist, yaw);
        }
    }

    void Nudge(float dt) {
        const int yawDir = (s_keyD ? 1 : 0) - (s_keyA ? 1 : 0);
        const int zDir   = (s_keyQ ? 1 : 0) - (s_keyE ? 1 : 0);
        const int distDir = (s_keyS ? 1 : 0) - (s_keyW ? 1 : 0);
        if (s_active && s_savePending && !s_keyQ && !s_keyE && !s_keyW && !s_keyS && !s_keyA && !s_keyD) {
            s_savePending = false;
            SaveNudge();
        }
        if (!s_active || (!yawDir && !zDir && !distDir)) return;
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
            s_zNudge = std::clamp(s_zNudge + zDir * 60.0f * dt,
                                  -60.0f - s_appliedCamZ, 60.0f - s_appliedCamZ);
            const float z = s_baseZ + s_zNudge;
            SetIni(2, z); SetIni(4, z);
            tps->posOffsetExpected.z = tps->posOffsetActual.z = z;
        }
        if (distDir) {
            s_distNudge = std::clamp(s_distNudge + distDir * 150.0f * dt,
                                     -120.0f - s_appliedCamDist, 400.0f - s_appliedCamDist);
            const float dist = s_baseDist + s_distNudge;
            SetIni(5, dist); SetIni(6, dist);
        }
        camera->Update();
    }

    void Exit() {
        PartySheetV2Light::Off();
        if (s_active) {
            for (std::size_t i = 0; i < kIni.size(); ++i)
                if (i != 8) SetIni(i, s_saved.ini[i]);
        }
        if (s_activateDisabled) {
            SetActivateEnabled(true);
            MenuUtils::SetHUDMenusHidden(false);
            s_activateDisabled = false;
        }
        auto* camera = RE::PlayerCamera::GetSingleton();
        auto* framed = skyrim_cast<RE::ThirdPersonState*>(s_framedState.get());
        if (s_active) LogCamera("exit before restore", camera, framed);
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
                if (auto* tps = framed) {
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
        if (s_active) {
            LogCamera("exit after restore", camera, framed);
            logger::debug("PartySheetV2Camera: exited");
        }
        s_framedState.reset();
        s_active = s_ownsSmoothCam = s_wasFirstPerson = false;
        s_keyA = s_keyD = s_keyQ = s_keyE = false;
        s_pendingFrames = 0;
        s_target.reset();
    }
}
