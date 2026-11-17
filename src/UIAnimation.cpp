#include "../includes/UIAnimation.h"
#include <algorithm>
#include <cmath>

namespace UIAnimation {

    float KeyClock::Tick(bool& reseed) {
        const auto now = std::chrono::steady_clock::now();
        const float elapsed = started ? std::chrono::duration<float>(now - last).count() : 0.0f;
        reseed = !started || elapsed > kHiddenGap;
        started = true;
        last = now;
        return reseed ? 0.0f : std::clamp(elapsed, 0.0f, kMaxStep);
    }
    
    AnimState& AnimationManager::GetState(const std::string& key) {
        std::lock_guard<std::mutex> lock(_mutex);
        return _states[key];
    }
    
    void AnimationManager::BeginFrame() {
        auto now = std::chrono::steady_clock::now();
        
        if (_firstFrame) {
            _lastFrameTime = now;
            _firstFrame = false;
            _deltaTime = 0.016f;
        } else {
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - _lastFrameTime);
            _deltaTime = duration.count() / 1000000.0f;
            
            _deltaTime = Clamp(_deltaTime, 0.001f, 0.1f);
        }
        
        _lastFrameTime = now;
    }
    
    void AnimationManager::Update(float deltaTime) {
        std::lock_guard<std::mutex> lock(_mutex);
        for (auto& [key, state] : _states) {
            if (state.alpha != state.targetAlpha) {
                float speed = 1.0f / Duration::FadeIn;
                if (state.alpha > state.targetAlpha) {
                    state.alpha -= deltaTime * speed * 2.0f;
                    if (state.alpha < state.targetAlpha) state.alpha = state.targetAlpha;
                } else {
                    state.alpha += deltaTime * speed;
                    if (state.alpha > state.targetAlpha) state.alpha = state.targetAlpha;
                }
            }

            const float slideSpeed = 1.0f / std::max(state.slideDuration, 0.001f);
            
            if (state.offsetX != state.targetOffsetX) {
                state.offsetX = Lerp(state.offsetX, state.targetOffsetX, deltaTime * slideSpeed * 5.0f);
                if (std::abs(state.offsetX - state.targetOffsetX) < 0.1f) {
                    state.offsetX = state.targetOffsetX;
                }
            }
            if (state.offsetY != state.targetOffsetY) {
                state.offsetY = Lerp(state.offsetY, state.targetOffsetY, deltaTime * slideSpeed * 5.0f);
                if (std::abs(state.offsetY - state.targetOffsetY) < 0.1f) {
                    state.offsetY = state.targetOffsetY;
                }
            }

            // Hit flash decay
            if (state.hitFlashActive) {
                state.hitFlashTimer -= deltaTime;
                if (state.hitFlashTimer <= 0.0f) {
                    state.hitFlashTimer = 0.0f;
                    state.hitFlashActive = false;
                }
            }

            // Heal flash decay
            if (state.healFlashActive) {
                state.healFlashTimer -= deltaTime;
                if (state.healFlashTimer <= 0.0f) {
                    state.healFlashTimer = 0.0f;
                    state.healFlashActive = false;
                }
            }

            // Heal scale decay
            if (state.healScaleActive) {
                state.healScaleTimer -= deltaTime;
                if (state.healScaleTimer <= 0.0f) {
                    state.healScaleTimer = 0.0f;
                    state.healScaleActive = false;
                }
            }

            if (state.bounceActive) {
                state.bounceTimer -= deltaTime;
                if (state.bounceTimer <= 0.0f) {
                    state.bounceTimer = 0.0f;
                    state.bounceActive = false;
                    state.offsetY = 0.0f;
                } else {
                    float t = state.bounceTimer / std::max(state.bounceDuration, 0.001f);
                    constexpr float kFreq  = 18.0f;
                    constexpr float kAmp   = 6.0f;
                    state.offsetY = -kAmp * t * sinf(kFreq * state.bounceTimer);
                }
            }

            if (state.shakeActive) {
                state.shakeTimer -= deltaTime;
                if (state.shakeTimer <= 0.0f) {
                    state.shakeTimer = 0.0f;
                    state.shakeActive = false;
                    state.offsetX = 0.0f;
                } else {
                    float t = state.shakeTimer / std::max(state.shakeDuration, 0.001f);
                    constexpr float kFreq = 35.0f;  // fast oscillation
                    state.offsetX = state.hitShakeAmp * t * sinf(kFreq * state.shakeTimer);
                }
            }

            state.isAnimating = (state.alpha != state.targetAlpha) ||
                               (state.offsetX != state.targetOffsetX) ||
                               (state.offsetY != state.targetOffsetY) ||
                               state.bounceActive ||
                               state.shakeActive ||
                               state.hitFlashActive ||
                               state.healFlashActive ||
                               state.healScaleActive;
        }

        // Attack bursts simulation
        for (auto& [key, burst] : _attackBursts) {
            if (!burst.active) continue;
            burst.timer -= deltaTime;
            if (burst.timer <= 0.0f) {
                burst.timer = 0.0f;
                burst.active = false;
            } else {
                float speedDecay = burst.timer / std::max(burst.duration, 0.001f);
                for (size_t i = 0; i < burst.count; ++i) {
                    burst.particles[i].distance += burst.particles[i].speed * deltaTime * speedDecay;
                }
            }
        }
    }
    
    void AnimationManager::Clear() {
        std::lock_guard<std::mutex> lock(_mutex);
        _states.clear();
        _attackBursts.clear();
        _followByName.clear();
        _followById.clear();
        _watchByName.clear();
        _watchById.clear();
        _baselineByName.clear();
        _stepByName.clear();
    }

    void AnimationManager::TriggerAttackFeedback(const std::string& key, AttackCategory category, bool isPowerAttack,
                                                 float seconds) {
        std::lock_guard<std::mutex> lock(_mutex);
        auto& burst = _attackBursts[key];
        burst.category = category;
        burst.isPowerAttack = isPowerAttack;
        burst.duration = std::max(seconds, 0.001f);
        burst.timer = burst.duration;
        burst.active = true;

        burst.count = isPowerAttack ? 15 : 10;
        if (burst.count > AttackBurst::kMaxParticles) {
            burst.count = AttackBurst::kMaxParticles;
        }

        constexpr float kTwoPi = 6.28318530718f;
        for (size_t i = 0; i < burst.count; ++i) {
            auto& p = burst.particles[i];
            float baseAngle = (static_cast<float>(i) / static_cast<float>(burst.count)) * kTwoPi;
            float jitter = ((static_cast<float>(rand() % 100) / 100.0f) - 0.5f) * 0.4f;
            p.angle = baseAngle + jitter;
            p.distance = 0.0f;
            p.maxDist = 16.0f + static_cast<float>(rand() % 16);
            p.length = (isPowerAttack ? 5.0f : 3.0f) + static_cast<float>(rand() % 5);
            p.speed = 70.0f + static_cast<float>(rand() % 70);
            p.life = 1.0f;

            switch (category) {
                case AttackCategory::Melee: {
                    int tone = rand() % 3;
                    p.color = (tone == 0) ? IM_COL32(255, 200, 70, 255) :
                              (tone == 1) ? IM_COL32(255, 230, 110, 255) :
                                            IM_COL32(255, 160, 40, 255);
                    break;
                }
                case AttackCategory::Ranged: {
                    int tone = rand() % 2;
                    p.color = (tone == 0) ? IM_COL32(230, 245, 255, 255) :
                                            IM_COL32(190, 220, 255, 255);
                    break;
                }
                case AttackCategory::SpellFire: {
                    int tone = rand() % 3;
                    p.color = (tone == 0) ? IM_COL32(255, 90, 20, 255) :
                              (tone == 1) ? IM_COL32(255, 150, 30, 255) :
                                            IM_COL32(255, 210, 50, 255);
                    break;
                }
                case AttackCategory::SpellFrost: {
                    int tone = rand() % 2;
                    p.color = (tone == 0) ? IM_COL32(100, 220, 255, 255) :
                                            IM_COL32(180, 245, 255, 255);
                    break;
                }
                case AttackCategory::SpellShock: {
                    int tone = rand() % 2;
                    p.color = (tone == 0) ? IM_COL32(190, 140, 255, 255) :
                                            IM_COL32(140, 200, 255, 255);
                    break;
                }
                case AttackCategory::SpellMagic:
                default: {
                    int tone = rand() % 2;
                    p.color = (tone == 0) ? IM_COL32(255, 220, 60, 255) :
                                            IM_COL32(255, 240, 150, 255);
                    break;
                }
            }
        }
    }

    void AnimationManager::DrawAttackParticles(ImDrawList* dl, const std::string& key, ImVec2 badgeCenter, float badgeRadius) {
        if (!dl) return;
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _attackBursts.find(key);
        if (it == _attackBursts.end() || !it->second.active) return;

        auto& burst = it->second;
        const float fadeAlpha = std::clamp(burst.timer / burst.duration, 0.0f, 1.0f);

        for (size_t i = 0; i < burst.count; ++i) {
            auto& p = burst.particles[i];
            float curDist = badgeRadius + p.distance;
            ImVec2 dir(cosf(p.angle), sinf(p.angle));
            ImVec2 startPos(badgeCenter.x + dir.x * curDist, badgeCenter.y + dir.y * curDist);
            ImVec2 endPos(startPos.x + dir.x * p.length, startPos.y + dir.y * p.length);

            if (burst.category == AttackCategory::SpellFire ||
                burst.category == AttackCategory::SpellFrost ||
                burst.category == AttackCategory::SpellShock ||
                burst.category == AttackCategory::SpellMagic) {
                // Magic: dual-pass glowing motes (stacked-width-soft-edge)
                ImU32 haloCol = (p.color & 0x00FFFFFF) | (static_cast<ImU32>(fadeAlpha * 80.0f) << 24);
                dl->AddCircleFilled(startPos, p.length * 0.7f, haloCol);
                // Pass 2: hot intense core
                ImU32 coreCol = IM_COL32(255, 255, 240, static_cast<int>(fadeAlpha * 220.0f));
                dl->AddCircleFilled(startPos, p.length * 0.3f, coreCol);
            } else {
                // Melee & Ranged: dual-pass luminous sparks (stacked-width-soft-edge)
                ImU32 glowCol = (p.color & 0x00FFFFFF) | (static_cast<ImU32>(fadeAlpha * 90.0f) << 24);
                float glowW = burst.isPowerAttack ? 3.8f : 2.6f;
                dl->AddLine(startPos, endPos, glowCol, glowW);

                // Pass 2: bright white-hot core line (sells emitted light)
                ImU32 coreCol = IM_COL32(255, 255, 245, static_cast<int>(fadeAlpha * 240.0f));
                float coreW = burst.isPowerAttack ? 1.8f : 1.1f;
                dl->AddLine(startPos, endPos, coreCol, coreW);
            }
        }
    }

    void TriggerAttackFeedback(const std::string& key, AttackCategory category, bool isPowerAttack, float seconds) {
        AnimationManager::Get().TriggerAttackFeedback(key, category, isPowerAttack, seconds);
    }

    void DrawAttackParticles(ImDrawList* dl, const std::string& key, ImVec2 badgeCenter, float badgeRadius) {
        AnimationManager::Get().DrawAttackParticles(dl, key, badgeCenter, badgeRadius);
    }

    namespace {
        void StartSlide(AnimState& state, int direction, float distance, float seconds) {
        
        switch (direction) {
            case 0: state.offsetX = -distance; break;
            case 1: state.offsetX = distance; break;
            case 2: state.offsetY = -distance; break;
            case 3: state.offsetY = distance; break;
        }
        
        state.targetOffsetX = 0.0f;
        state.targetOffsetY = 0.0f;
            state.slideDuration = seconds;
        state.isAnimating = true;
    }

        void StartHealSlot(AnimState& state, bool isBuff, float flashSeconds, float swellSeconds) {
            state.healFlashDuration = std::max(flashSeconds, 0.001f);
            state.healFlashTimer    = state.healFlashDuration;
            state.healFlashActive   = true;
            state.healFlashIsBuff   = isBuff;
            state.healScaleDuration = std::max(swellSeconds, 0.001f);
            state.healScaleTimer    = state.healScaleDuration;
            state.healScaleActive   = true;
            state.isAnimating = true;
        }
    }

    void TriggerSlideIn(const std::string& key, int direction, float distance, float seconds) {
        AnimationManager::Get().Edit(key, [&](AnimState& state) {
            StartSlide(state, direction, distance, seconds);
        });
    }

    void TriggerBounce(const std::string& key, float seconds) {
        AnimationManager::Get().Edit(key, [&](AnimState& state) {
            state.bounceDuration = std::max(seconds, 0.001f);
            state.bounceTimer    = state.bounceDuration;
            state.bounceActive   = true;
            state.isAnimating = true;
        });
        }

    void TriggerShake(const std::string& key, float seconds, float amplitude) {
        AnimationManager::Get().Edit(key, [&](AnimState& state) {
            state.hitShakeAmp   = amplitude;
            state.shakeDuration = std::max(seconds, 0.001f);
            state.shakeTimer    = state.shakeDuration;
            state.shakeActive   = true;
            state.isAnimating   = true;
        });
    }

    void TriggerHitFeedback(const std::string& key, bool isBlocked, float shakeSeconds, float flashSeconds) {
        AnimationManager::Get().Edit(key, [&](AnimState& state) {
            state.hitShakeAmp   = isBlocked ? 2.0f : 4.0f;
            state.shakeDuration = std::max(shakeSeconds, 0.001f);
            state.shakeTimer    = state.shakeDuration;
            state.shakeActive   = true;
            state.isAnimating   = true;

            if (!isBlocked) {
                state.hitFlashDuration = std::max(flashSeconds, 0.001f);
                state.hitFlashTimer    = state.hitFlashDuration;
                state.hitFlashActive   = true;
            }
        });
    }

    void TriggerHealFeedback(const std::string& key, float flashSeconds, float swellSeconds) {
        AnimationManager::Get().Edit(key, [&](AnimState& state) {
            StartHealSlot(state, false, flashSeconds, swellSeconds);
        });
    }

    void TriggerBuffFeedback(const std::string& key, float flashSeconds, float swellSeconds) {
        AnimationManager::Get().Edit(key, [&](AnimState& state) {
            if (state.healFlashActive && !state.healFlashIsBuff) return;
            StartHealSlot(state, true, flashSeconds, swellSeconds);
        });
    }

    namespace {
        float StepFollow(AnimationManager::FollowTrack& track, float target, float speedUp, float speedDown) {
            bool reseed = false;
            const float dt = track.clock.Tick(reseed);
            if (reseed) {
                track.value = target;
                return target;
            }

            const float rate = target >= track.value ? speedUp : speedDown;
            const float next = track.value + (target - track.value) * (1.0f - std::exp(-rate * dt));
            const bool settled = std::fabs(target - next) < 0.001f || (dt > 0.0f && next == track.value);
            track.value = settled ? target : next;
            return track.value;
        }

        WatchResult StepWatch(AnimationManager::WatchTrack& track, float value, float flashSeconds, float threshold) {
            bool reseed = false;
            const float dt = track.clock.Tick(reseed);
            if (reseed) {
                track.value = value;
                track.delta = 0.0f;
                track.direction = 0;
                track.flash = 0.0f;
                return {};
            }

            track.flash = std::max(0.0f, track.flash - dt / track.flashSeconds);

            WatchResult result;
            const float diff = value - track.value;
            if (std::fabs(diff) > threshold) {
                track.value = value;
                track.delta = diff;
                track.direction = diff > 0.0f ? 1 : -1;
                track.flash = 1.0f;
                track.flashSeconds = std::max(flashSeconds, 0.001f);
                result.changed = true;
            }

            if (track.flash <= 0.0f) {
                track.delta = 0.0f;
                track.direction = 0;
            }

            result.delta = track.delta;
            result.direction = track.direction;
            result.flash = track.flash;
            return result;
        }
    }

    float AnimationManager::Step(const std::string& key, bool wantOn, float secondsIn, float secondsOut) {
        std::lock_guard<std::mutex> lock(_mutex);
        StepTrack& track = _stepByName[key];
        bool reseed = false;
        const float dt = track.clock.Tick(reseed);
        if (wantOn)
            track.value = std::min(track.value + dt / std::max(secondsIn, 0.001f), 1.0f);
        else
            track.value = std::max(track.value - dt / std::max(secondsOut, 0.001f), 0.0f);
        return track.value;
    }

    void AnimationManager::Snap(const std::string& key, float value) {
        std::lock_guard<std::mutex> lock(_mutex);
        _stepByName[key].value = Clamp(value, 0.0f, 1.0f);
    }

    float AnimationManager::Follow(const std::string& key, float target, float speedUp, float speedDown) {
        std::lock_guard<std::mutex> lock(_mutex);
        return StepFollow(_followByName[key], target, speedUp, speedDown);
    }

    float AnimationManager::Follow(std::uint64_t key, float target, float speedUp, float speedDown) {
        std::lock_guard<std::mutex> lock(_mutex);
        return StepFollow(_followById[key], target, speedUp, speedDown);
    }

    WatchResult AnimationManager::Watch(const std::string& key, float value, float flashSeconds, float threshold) {
        std::lock_guard<std::mutex> lock(_mutex);
        return StepWatch(_watchByName[key], value, flashSeconds, threshold);
    }

    float AnimationManager::Swap(const std::string& key, std::uint64_t id, float seconds, bool animateFirstSight) {
        std::lock_guard<std::mutex> lock(_mutex);
        auto& track = _swapByName[key];
        bool reseed = false;
        const float dt = track.clock.Tick(reseed);
        if (reseed) {
            const bool firstSight = !track.seen;
            track.seen = true;
            track.id = id;
            track.progress = (firstSight && animateFirstSight) ? 0.0f : 1.0f;
            return track.progress;
        }
        if (id != track.id) {
            track.id = id;
            track.progress = 0.0f;
        }
        track.progress = std::min(1.0f, track.progress + dt / std::max(seconds, 0.001f));
        return track.progress;
    }

    float AnimationManager::SinceShown(const std::string& key, float value) {
        std::lock_guard<std::mutex> lock(_mutex);
        auto& track = _baselineByName[key];
        bool reseed = false;
        track.clock.Tick(reseed);
        if (reseed) track.baseline = value;
        return value - track.baseline;
    }

    WatchResult AnimationManager::Watch(std::uint64_t key, float value, float flashSeconds, float threshold) {
        std::lock_guard<std::mutex> lock(_mutex);
        return StepWatch(_watchById[key], value, flashSeconds, threshold);
    }

    float Follow(const std::string& key, float target, float speedUp, float speedDown) {
        return AnimationManager::Get().Follow(key, target, speedUp, speedDown);
    }

    float Step(const std::string& key, bool wantOn, float secondsIn, float secondsOut) {
        return AnimationManager::Get().Step(key, wantOn, secondsIn, secondsOut);
    }

    void Snap(const std::string& key, float value) {
        AnimationManager::Get().Snap(key, value);
    }

    float Follow(std::uint64_t key, float target, float speedUp, float speedDown) {
        return AnimationManager::Get().Follow(key, target, speedUp, speedDown);
    }

    WatchResult Watch(const std::string& key, float value, float flashSeconds, float threshold) {
        return AnimationManager::Get().Watch(key, value, flashSeconds, threshold);
    }

    WatchResult Watch(std::uint64_t key, float value, float flashSeconds, float threshold) {
        return AnimationManager::Get().Watch(key, value, flashSeconds, threshold);
    }

    float Swap(const std::string& key, std::uint64_t id, float seconds, bool animateFirstSight) {
        return AnimationManager::Get().Swap(key, id, seconds, animateFirstSight);
    }

    float SinceShown(const std::string& key, float value) {
        return AnimationManager::Get().SinceShown(key, value);
    }

    float Pulse(float speed, float low, float high) {
        static const auto start = std::chrono::steady_clock::now();
        const double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        const float wave = static_cast<float>(std::sin(t * speed)) * 0.5f + 0.5f;
        return low + (high - low) * wave;
    }

    float AgeFade(float age, float fadeIn, float hold, float fadeOut) {
        if (age < 0.0f) return 0.0f;
        if (age < fadeIn) return age / fadeIn;
        const float outStart = fadeIn + hold;
        if (age <= outStart) return 1.0f;
        if (fadeOut <= 0.0f) return 0.0f;
        return std::max(0.0f, 1.0f - (age - outStart) / fadeOut);
    }

    ImVec2 GetOffset(const std::string& key) {
        return AnimationManager::Get().Read(key, ImVec2(0.0f, 0.0f),
            [](const AnimState& s) { return ImVec2(s.offsetX, s.offsetY); });
    }

    float GetBounceOffsetY(const std::string& key) {
        return AnimationManager::Get().Read(key, 0.0f,
            [](const AnimState& s) { return s.bounceActive ? s.offsetY : 0.0f; });
    }

    float GetShakeOffsetX(const std::string& key) {
        return AnimationManager::Get().Read(key, 0.0f,
            [](const AnimState& s) { return s.shakeActive ? s.offsetX : 0.0f; });
    }

    float GetHitFlashIntensity(const std::string& key) {
        return AnimationManager::Get().Read(key, 0.0f, [](const AnimState& s) {
            if (!s.hitFlashActive || s.hitFlashTimer <= 0.0f) return 0.0f;
            return std::clamp(s.hitFlashTimer / s.hitFlashDuration, 0.0f, 1.0f);
        });
    }

    float GetHealFlashIntensity(const std::string& key) {
        return AnimationManager::Get().Read(key, 0.0f, [](const AnimState& s) {
            if (!s.healFlashActive || s.healFlashTimer <= 0.0f) return 0.0f;
            return sqrtf(std::clamp(s.healFlashTimer / s.healFlashDuration, 0.0f, 1.0f));
        });
    }

    bool GetHealFlashIsBuff(const std::string& key) {
        return AnimationManager::Get().Read(key, false,
            [](const AnimState& s) { return s.healFlashActive && s.healFlashIsBuff; });
    }

    float GetHealScale(const std::string& key) {
        return AnimationManager::Get().Read(key, 1.0f, [](const AnimState& s) {
            if (!s.healScaleActive || s.healScaleTimer <= 0.0f) return 1.0f;
            const float t = 1.0f - (s.healScaleTimer / s.healScaleDuration);
        constexpr float kMaxSwell = 0.06f;
        return 1.0f + kMaxSwell * sinf(t * 3.14159265359f);
        });
    }

    ImU32 FlashColor(const WatchResult& watch, ImU32 normalColor, ImU32 increaseColor, ImU32 decreaseColor) {
        if (watch.flash <= 0.0f || watch.direction == 0) return normalColor;
        const ImVec4 from = ImGui::ColorConvertU32ToFloat4(normalColor);
        const ImVec4 to = ImGui::ColorConvertU32ToFloat4(watch.direction > 0 ? increaseColor : decreaseColor);
        const float t = watch.flash;
        return ImGui::ColorConvertFloat4ToU32(ImVec4(Lerp(from.x, to.x, t), Lerp(from.y, to.y, t),
                                                     Lerp(from.z, to.z, t), Lerp(from.w, to.w, t)));
    }
}
