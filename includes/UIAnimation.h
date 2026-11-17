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
        constexpr float ColorFlash = 1.0f;
        constexpr float Bounce = 0.6f;
        constexpr float Shake  = 0.4f;
        constexpr float HitShake = 0.30f;       // Seconds for discrete hit shake
        constexpr float HitFlash = 0.35f;       // Seconds for discrete hit flash
        constexpr float HealFlash = 0.75f;      // Seconds for emerald heal flash
        constexpr float HealScale = 0.45f;      // Seconds for zoom-in scale swell
        constexpr float AttackBurst = 0.35f;
    }

    constexpr float kHiddenGap = 0.25f;

    constexpr float kMaxStep = 0.1f;

    struct KeyClock {
        std::chrono::steady_clock::time_point last{};
        bool started = false;

        float Tick(bool& reseed);
    };
    
    struct AnimState {
        float alpha = 1.0f;
        float targetAlpha = 1.0f;
        
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        float targetOffsetX = 0.0f;
        float targetOffsetY = 0.0f;
        float slideDuration = Duration::SlideIn;
        
        float bounceTimer = 0.0f;
        float bounceDuration = Duration::Bounce;
        bool  bounceActive = false;

        float shakeTimer = 0.0f;
        float shakeDuration = Duration::HitShake;
        bool  shakeActive = false;
        float hitShakeAmp = 4.0f;

        // Hit flash (impact feedback: linear decay)
        float hitFlashTimer = 0.0f;
        float hitFlashDuration = Duration::HitFlash;
        bool  hitFlashActive = false;

        // Heal feedback (rejuvenation pulse + emerald flash)
        float healFlashTimer = 0.0f;
        float healFlashDuration = Duration::HealFlash;
        bool  healFlashActive = false;
        bool  healFlashIsBuff = false;   // Same timers as heal; orange tint instead of emerald
        float healScaleTimer = 0.0f;
        float healScaleDuration = Duration::HealScale;
        bool  healScaleActive = false;
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
        float duration = Duration::AttackBurst;
        bool active = false;
        AttackCategory category = AttackCategory::Melee;
        bool isPowerAttack = false;
    };

    struct WatchResult {
        bool  changed = false;
        float delta = 0.0f;
        int   direction = 0;
        float flash = 0.0f;
    };
    
    class AnimationManager {
    public:
        static AnimationManager& Get() {
            static AnimationManager instance;
            return instance;
        }

        template <class Fn>
        decltype(auto) Edit(const std::string& key, Fn&& fn) {
            std::lock_guard<std::mutex> lock(_mutex);
            return fn(_states[key]);
        }

        template <class T, class Fn>
        T Read(const std::string& key, T fallback, Fn&& fn) const {
            std::lock_guard<std::mutex> lock(_mutex);
            auto it = _states.find(key);
            return it == _states.end() ? fallback : fn(it->second);
        }
        
        AnimState& GetState(const std::string& key);

        void TriggerAttackFeedback(const std::string& key, AttackCategory category, bool isPowerAttack,
                                   float seconds);

        // Render attack particles
        void DrawAttackParticles(ImDrawList* dl, const std::string& key, ImVec2 badgeCenter, float badgeRadius);

        float Follow(const std::string& key, float target, float speedUp, float speedDown);
        float Follow(std::uint64_t key, float target, float speedUp, float speedDown);
        WatchResult Watch(const std::string& key, float value, float flashSeconds, float threshold);
        WatchResult Watch(std::uint64_t key, float value, float flashSeconds, float threshold);
        float Swap(const std::string& key, std::uint64_t id, float seconds, bool animateFirstSight);
        float SinceShown(const std::string& key, float value);
        float Step(const std::string& key, bool wantOn, float secondsIn, float secondsOut);
        void  Snap(const std::string& key, float value);

        void Update(float deltaTime);
        void BeginFrame();
        
        void Clear();
        
        float GetDeltaTime() const { return _deltaTime; }
        
        std::mutex& GetMutex() const { return _mutex; }

        struct FollowTrack {
            float value = 0.0f;
            KeyClock clock;
        };

        struct BaselineTrack {
            float baseline = 0.0f;
            KeyClock clock;
        };

        struct SwapTrack {
            std::uint64_t id = 0;
            float progress = 1.0f;
            bool seen = false;
            KeyClock clock;
        };

        struct StepTrack {
            float value = 0.0f;
            KeyClock clock;
        };

        struct WatchTrack {
            float value = 0.0f;
            float delta = 0.0f;
            int   direction = 0;
            float flash = 0.0f;
            float flashSeconds = Duration::ColorFlash;
            KeyClock clock;
        };

    private:
        AnimationManager() = default;
        
        mutable std::mutex _mutex;
        std::unordered_map<std::string, AnimState> _states;
        std::unordered_map<std::string, AttackBurst> _attackBursts;
        std::unordered_map<std::string, FollowTrack> _followByName;
        std::unordered_map<std::uint64_t, FollowTrack> _followById;
        std::unordered_map<std::string, WatchTrack> _watchByName;
        std::unordered_map<std::uint64_t, WatchTrack> _watchById;
        std::unordered_map<std::string, BaselineTrack> _baselineByName;
        std::unordered_map<std::string, SwapTrack> _swapByName;
        std::unordered_map<std::string, StepTrack> _stepByName;
        float _deltaTime = 0.016f;
        std::chrono::steady_clock::time_point _lastFrameTime;
        bool _firstFrame = true;
    };

    void TriggerSlideIn(const std::string& key, int direction, float distance = 20.0f,
                        float seconds = Duration::SlideIn);

    void TriggerBounce(const std::string& key, float seconds = Duration::Bounce);

    void TriggerShake(const std::string& key, float seconds = Duration::Shake,
                      float amplitude = 4.0f * Duration::Shake / Duration::HitShake);

    void TriggerHitFeedback(const std::string& key, bool isBlocked = false,
                            float shakeSeconds = Duration::HitShake, float flashSeconds = Duration::HitFlash);

    void TriggerHealFeedback(const std::string& key, float flashSeconds = Duration::HealFlash,
                             float swellSeconds = Duration::HealScale);

    void TriggerBuffFeedback(const std::string& key, float flashSeconds = Duration::HealFlash,
                             float swellSeconds = Duration::HealScale);

    void TriggerAttackFeedback(const std::string& key, AttackCategory category, bool isPowerAttack,
                               float seconds = Duration::AttackBurst);

    // Render attack particle burst around combat badge
    void DrawAttackParticles(ImDrawList* dl, const std::string& key, ImVec2 badgeCenter, float badgeRadius);

    float Follow(const std::string& key, float target, float speedUp = 20.0f, float speedDown = 9.0f);
    float Follow(std::uint64_t key, float target, float speedUp = 20.0f, float speedDown = 9.0f);

    WatchResult Watch(const std::string& key, float value, float flashSeconds = Duration::ColorFlash,
                      float threshold = 0.1f);
    WatchResult Watch(std::uint64_t key, float value, float flashSeconds = Duration::ColorFlash,
                      float threshold = 0.1f);

    float Swap(const std::string& key, std::uint64_t id, float seconds = Duration::FadeIn,
               bool animateFirstSight = false);

    float SinceShown(const std::string& key, float value);

    float Step(const std::string& key, bool wantOn, float secondsIn, float secondsOut);

    void Snap(const std::string& key, float value);

    float Pulse(float speed, float low, float high);

    float AgeFade(float age, float fadeIn, float hold, float fadeOut);

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

    ImU32 FlashColor(const WatchResult& watch, ImU32 normalColor,
                          ImU32 increaseColor = IM_COL32(100, 255, 100, 255),
                          ImU32 decreaseColor = IM_COL32(255, 100, 100, 255));

}
