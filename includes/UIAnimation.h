#pragma once

#include <imgui.h>
#include <string>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <array>
#include <cstdint>

namespace UIAnimation {
    
    
    inline float Lerp(float a, float b, float t) {
        return a + (b - a) * t;
    }
    
    inline float Clamp(float value, float min, float max) {
        return value < min ? min : (value > max ? max : value);
    }
    
    inline float SmoothStep(float t) {
        t = Clamp(t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }
    
    inline float EaseOut(float t) {
        t = Clamp(t, 0.0f, 1.0f);
        return 1.0f - (1.0f - t) * (1.0f - t);
    }
    
    inline float EaseIn(float t) {
        t = Clamp(t, 0.0f, 1.0f);
        return t * t;
    }
    
    
    namespace Duration {
        constexpr float FadeIn = 0.3f;
        constexpr float FadeOut = 0.2f;
        constexpr float SlideIn = 0.25f;
        constexpr float Pulse = 0.4f;
        constexpr float ColorFlash = 1.0f;
        constexpr float Bounce = 0.6f;
        constexpr float Shake  = 0.4f;
        constexpr float HitShake = 0.30f;       // Seconds for discrete hit shake
        constexpr float HitFlash = 0.35f;       // Seconds for discrete hit flash
        constexpr float HealFlash = 0.75f;      // Seconds for emerald heal flash
        constexpr float HealScale = 0.45f;      // Seconds for zoom-in scale swell
    }
    
    
    struct AnimState {
        float alpha = 1.0f;
        float targetAlpha = 1.0f;
        
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        float targetOffsetX = 0.0f;
        float targetOffsetY = 0.0f;
        
        float scale = 1.0f;
        float targetScale = 1.0f;
        
        float flashIntensity = 0.0f;
        int flashDirection = 0;
        
        // Tracking for change detection
        std::string lastValue;
        uint32_t lastFormID = 0;
        float lastNumericValue = 0.0f;
        
        float bounceTimer = 0.0f;
        bool  bounceActive = false;

        float shakeTimer = 0.0f;
        bool  shakeActive = false;
        float hitShakeAmp = 4.0f;

        // Hit flash (impact feedback: linear decay)
        float hitFlashTimer = 0.0f;
        bool  hitFlashActive = false;

        // Heal feedback (rejuvenation pulse + emerald flash)
        float healFlashTimer = 0.0f;
        bool  healFlashActive = false;
        bool  healFlashIsBuff = false;   // Same timers as heal; orange tint instead of emerald
        float healScaleTimer = 0.0f;
        bool  healScaleActive = false;

        // Timing
        float timer = 0.0f;
        bool isAnimating = false;
    };

    enum class AttackCategory : std::uint8_t {
        Melee,
        Ranged,
        SpellFire,
        SpellFrost,
        SpellShock,
        SpellMagic
    };

    struct AttackParticle {
        float angle;      // Radians (0 to 2*PI)
        float distance;   // Current distance from badge center
        float maxDist;    // Maximum expansion distance (16 - 32px)
        float length;     // Shard/streak line length (3 - 8px)
        float speed;      // Velocity factor
        float life;       // Remaining lifespan (1.0f -> 0.0f)
        ImU32 color;      // Particle color with alpha
    };

    struct AttackBurst {
        static constexpr size_t kMaxParticles = 16;
        std::array<AttackParticle, kMaxParticles> particles{};
        size_t count = 0;
        float timer = 0.0f;
        bool active = false;
        AttackCategory category = AttackCategory::Melee;
        bool isPowerAttack = false;
    };
    
    
    class AnimationManager {
    public:
        static AnimationManager& Get() {
            static AnimationManager instance;
            return instance;
        }
        
        AnimState& GetState(const std::string& key);
        
        // Trigger attack particle burst
        void TriggerAttackFeedback(const std::string& key, AttackCategory category, bool isPowerAttack);

        // Render attack particles
        void DrawAttackParticles(ImDrawList* dl, const std::string& key, ImVec2 badgeCenter, float badgeRadius);

        void Update(float deltaTime);
        
        void Clear();
        
        float GetDeltaTime() const { return _deltaTime; }
        
        void BeginFrame();
        
        std::mutex& GetMutex() const { return _mutex; }

    private:
        AnimationManager() = default;
        
        mutable std::mutex _mutex;
        std::unordered_map<std::string, AnimState> _states;
        std::unordered_map<std::string, AttackBurst> _attackBursts;
        float _deltaTime = 0.016f;
        std::chrono::steady_clock::time_point _lastFrameTime;
        bool _firstFrame = true;
    };
    

    void TriggerSlideIn(const std::string& key, int direction, float distance = 20.0f);

    void TriggerBounce(const std::string& key);

    void TriggerShake(const std::string& key);

    // Trigger discrete hit feedback (shake + optional red flash, damped if blocked)
    void TriggerHitFeedback(const std::string& key, bool isBlocked = false);

    // Trigger heal feedback (rejuvenating +5% scale pulse + emerald flash)
    void TriggerHealFeedback(const std::string& key);

    // Trigger buff feedback (same pulse as heal, orange flash). Never overrides an active heal flash.
    void TriggerBuffFeedback(const std::string& key);

    // Trigger attack particle burst around combat badge
    void TriggerAttackFeedback(const std::string& key, AttackCategory category, bool isPowerAttack);

    // Render attack particle burst around combat badge
    void DrawAttackParticles(ImDrawList* dl, const std::string& key, ImVec2 badgeCenter, float badgeRadius);

    bool CheckAndAnimateChange(const std::string& key, const std::string& newValue);

    bool CheckNumericChange(const std::string& key, float newValue);

    float GetAlpha(const std::string& key);

    ImVec2 GetOffset(const std::string& key);

    float GetBounceOffsetY(const std::string& key);

    float GetShakeOffsetX(const std::string& key);

    // Get current hit flash intensity [0.0, 1.0] (0 when not active)
    float GetHitFlashIntensity(const std::string& key);

    // Get current heal flash intensity [0.0, 1.0] (0 when not active)
    float GetHealFlashIntensity(const std::string& key);

    // True when the active heal-slot flash was started by a buff (orange) rather than a heal
    bool GetHealFlashIsBuff(const std::string& key);

    // Get current heal scale multiplier [1.0, 1.05] (1.0 when not active)
    float GetHealScale(const std::string& key);

    ImU32 GetFlashedColor(const std::string& key, ImU32 normalColor,
                          ImU32 increaseColor = IM_COL32(100, 255, 100, 255),
                          ImU32 decreaseColor = IM_COL32(255, 100, 100, 255));

}
