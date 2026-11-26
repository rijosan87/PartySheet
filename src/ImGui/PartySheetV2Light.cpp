
#include "../../includes/ImGui/PartySheetV2Light.h"

#include "../../includes/Settings.h"

namespace PartySheetV2Light {
    namespace {
        constexpr float kRadius  = 260.0f;
        constexpr float kForward = 70.0f;
        constexpr float kSide    = 25.0f;

        RE::NiPointer<RE::NiPointLight> s_light;
        RE::ActorHandle                 s_owner;

        RE::ActorHandle s_wantOwner;
        int             s_wantStrength = 0;

        void RemoveNow() {
            if (!s_light) return;
            if (auto* scene = RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0])
                scene->RemoveLight(s_light.get());
            if (auto* parent = s_light->parent)
                parent->DetachChild(s_light.get());
            s_light.reset();
            s_owner.reset();
        }

        void ApplyNow(RE::ActorHandle owner, int strength) {
            auto actor = owner.get();
            auto* root = actor ? actor->Get3D() : nullptr;
            auto* node = root ? root->AsNode() : nullptr;
            if (!node || strength <= 0) { RemoveNow(); return; }
            if (s_light && s_owner != owner) RemoveNow();

            if (!s_light) {
                auto* scene = RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0];
                if (!scene) return;
                s_light.reset(RE::NiPointLight::Create());
                if (!s_light) return;
                s_light->GetLightRuntimeData().ambient = {};
                node->AttachChild(s_light.get(), true);
                RE::ShadowSceneNode::LIGHT_CREATE_PARAMS params{};
                params.dynamic      = true;
                params.affectLand   = true;
                params.affectWater  = true;
                params.neverFades   = true;
                params.fov          = 1.0f;
                params.falloff      = 1.0f;
                params.nearDistance = 5.0f;
                scene->AddLight(s_light.get(), params);
                s_owner = owner;
            }

            auto& data   = s_light->GetLightRuntimeData();
            data.radius  = { kRadius, kRadius, kRadius };
            s_light->SetLightAttenuation(kRadius);
            data.diffuse = { 1.0f, 0.95f, 0.88f };
            data.fade    = strength * 0.02f;
            const float headZ = actor->GetHeight() * 0.9f;
            s_light->local.translate = { kSide, kForward, headZ };
            RE::NiUpdateData ctx;
            s_light->Update(ctx);
        }
    }

    void Follow(RE::Actor* subject) {
        const int strength = subject ? Settings::GetSingleton()->psV2LiveLight : 0;
        const RE::ActorHandle owner = (subject && strength > 0) ? subject->GetHandle() : RE::ActorHandle{};
        if (owner == s_wantOwner && strength == s_wantStrength) return;
        s_wantOwner    = owner;
        s_wantStrength = strength;
        if (auto* tasks = SKSE::GetTaskInterface())
            tasks->AddTask([owner, strength]() { ApplyNow(owner, strength); });
    }

    void Off() {
        if (!s_wantOwner && s_wantStrength == 0) return;
        s_wantOwner.reset();
        s_wantStrength = 0;
        if (auto* tasks = SKSE::GetTaskInterface())
            tasks->AddTask([]() { RemoveNow(); });
    }
}
