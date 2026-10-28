#include "../includes/UIAnimation.h"
#include <cmath>

namespace UIAnimation {
    
    
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
            
            if (state.offsetX != state.targetOffsetX) {
                float speed = 1.0f / Duration::SlideIn;
                state.offsetX = Lerp(state.offsetX, state.targetOffsetX, deltaTime * speed * 5.0f);
                if (std::abs(state.offsetX - state.targetOffsetX) < 0.1f) {
                    state.offsetX = state.targetOffsetX;
                }
            }
            if (state.offsetY != state.targetOffsetY) {
                float speed = 1.0f / Duration::SlideIn;
                state.offsetY = Lerp(state.offsetY, state.targetOffsetY, deltaTime * speed * 5.0f);
                if (std::abs(state.offsetY - state.targetOffsetY) < 0.1f) {
                    state.offsetY = state.targetOffsetY;
                }
            }
            
            if (state.scale != state.targetScale) {
                float speed = 1.0f / Duration::Pulse;
                state.scale = Lerp(state.scale, state.targetScale, deltaTime * speed * 8.0f);
                if (std::abs(state.scale - state.targetScale) < 0.01f) {
                    state.scale = state.targetScale;
                }
            }
            
            if (state.flashIntensity > 0.0f) {
                state.flashIntensity -= deltaTime * (1.0f / Duration::ColorFlash);
                if (state.flashIntensity < 0.0f) state.flashIntensity = 0.0f;
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
                    float t = state.bounceTimer / Duration::Bounce;
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
                    float dur = state.hitShakeAmp > 0.0f ? Duration::HitShake : Duration::Shake;
                    float t = state.shakeTimer / dur;
                    constexpr float kFreq = 35.0f;  // fast oscillation
                    float kAmp = state.hitShakeAmp > 0.0f ? state.hitShakeAmp : 4.0f;
                    state.offsetX = kAmp * t * sinf(kFreq * state.shakeTimer);
                }
            }

            state.isAnimating = (state.alpha != state.targetAlpha) ||
                               (state.offsetX != state.targetOffsetX) ||
                               (state.offsetY != state.targetOffsetY) ||
                               (state.scale != state.targetScale) ||
                               (state.flashIntensity > 0.0f) ||
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
                float speedDecay = burst.timer / 0.35f;
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
    }
    

    void TriggerBounce(const std::string& key) {
        auto& state = AnimationManager::Get().GetState(key);
        state.bounceTimer  = Duration::Bounce;
        state.bounceActive = true;
        state.isAnimating  = true;
    }

    void TriggerShake(const std::string& key) {
        auto& state = AnimationManager::Get().GetState(key);
        state.hitShakeAmp = 4.0f;
        state.shakeTimer  = Duration::Shake;
        state.shakeActive = true;
        state.isAnimating = true;
    }

    void TriggerHitFeedback(const std::string& key, bool isBlocked) {
        auto& state = AnimationManager::Get().GetState(key);
        state.hitShakeAmp = isBlocked ? 2.0f : 4.0f;
        state.shakeTimer  = Duration::HitShake;
        state.shakeActive = true;
        state.isAnimating = true;

        if (!isBlocked) {
            state.hitFlashTimer  = Duration::HitFlash;
            state.hitFlashActive = true;
        }
    }

    void TriggerHealFeedback(const std::string& key) {
        auto& state = AnimationManager::Get().GetState(key);
        state.healFlashTimer  = Duration::HealFlash;
        state.healFlashActive = true;
        state.healFlashIsBuff = false;
        state.healScaleTimer  = Duration::HealScale;
        state.healScaleActive = true;
        state.isAnimating     = true;
    }

    void TriggerBuffFeedback(const std::string& key) {
        auto& state = AnimationManager::Get().GetState(key);
        // A running heal flash wins; don't recolor it.
        if (state.healFlashActive && !state.healFlashIsBuff) return;
        state.healFlashTimer  = Duration::HealFlash;
        state.healFlashActive = true;
        state.healFlashIsBuff = true;
        state.healScaleTimer  = Duration::HealScale;
        state.healScaleActive = true;
        state.isAnimating     = true;
    }

    void AnimationManager::TriggerAttackFeedback(const std::string& key, AttackCategory category, bool isPowerAttack) {
        std::lock_guard<std::mutex> lock(_mutex);
        auto& burst = _attackBursts[key];
        burst.category = category;
        burst.isPowerAttack = isPowerAttack;
        burst.timer = 0.35f;
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
        const float fadeAlpha = std::clamp(burst.timer / 0.35f, 0.0f, 1.0f);

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

    void TriggerAttackFeedback(const std::string& key, AttackCategory category, bool isPowerAttack) {
        AnimationManager::Get().TriggerAttackFeedback(key, category, isPowerAttack);
    }

    void DrawAttackParticles(ImDrawList* dl, const std::string& key, ImVec2 badgeCenter, float badgeRadius) {
        AnimationManager::Get().DrawAttackParticles(dl, key, badgeCenter, badgeRadius);
    }

    void TriggerSlideIn(const std::string& key, int direction, float distance) {
        auto& state = AnimationManager::Get().GetState(key);
        
        switch (direction) {
            case 0: state.offsetX = -distance; break;
            case 1: state.offsetX = distance; break;
            case 2: state.offsetY = -distance; break;
            case 3: state.offsetY = distance; break;
        }
        
        state.targetOffsetX = 0.0f;
        state.targetOffsetY = 0.0f;
        state.isAnimating = true;
    }
    
    
    bool CheckAndAnimateChange(const std::string& key, const std::string& newValue) {
        auto& state = AnimationManager::Get().GetState(key);
        
        if (state.lastValue != newValue) {
            state.lastValue = newValue;
            
            state.alpha = 0.0f;
            state.targetAlpha = 1.0f;
            state.isAnimating = true;
            TriggerSlideIn(key, 3, 30.0f);
            
            return true;
        }
        return false;
    }
    

    bool CheckNumericChange(const std::string& key, float newValue) {
        auto& state = AnimationManager::Get().GetState(key);
        
        float diff = newValue - state.lastNumericValue;
        if (std::abs(diff) > 0.1f) {
            state.lastNumericValue = newValue;
            state.flashIntensity = 1.0f;
            state.flashDirection = (diff > 0) ? 1 : -1;
            state.isAnimating = true;
            return true;
        }
        return false;
    }
    

    float GetAlpha(const std::string& key) {
        return AnimationManager::Get().GetState(key).alpha;
    }

    ImVec2 GetOffset(const std::string& key) {
        auto& state = AnimationManager::Get().GetState(key);
        return ImVec2(state.offsetX, state.offsetY);
    }

    float GetBounceOffsetY(const std::string& key) {
        auto& state = AnimationManager::Get().GetState(key);
        return state.bounceActive ? state.offsetY : 0.0f;
    }

    float GetShakeOffsetX(const std::string& key) {
        auto& state = AnimationManager::Get().GetState(key);
        return state.shakeActive ? state.offsetX : 0.0f;
    }

    float GetHitFlashIntensity(const std::string& key) {
        auto& state = AnimationManager::Get().GetState(key);
        if (!state.hitFlashActive || state.hitFlashTimer <= 0.0f) return 0.0f;
        return std::clamp(state.hitFlashTimer / Duration::HitFlash, 0.0f, 1.0f);
    }

    float GetHealFlashIntensity(const std::string& key) {
        auto& state = AnimationManager::Get().GetState(key);
        if (!state.healFlashActive || state.healFlashTimer <= 0.0f) return 0.0f;
        float t = std::clamp(state.healFlashTimer / Duration::HealFlash, 0.0f, 1.0f);
        return sqrtf(t);
    }

    bool GetHealFlashIsBuff(const std::string& key) {
        auto& state = AnimationManager::Get().GetState(key);
        return state.healFlashActive && state.healFlashIsBuff;
    }

    float GetHealScale(const std::string& key) {
        auto& state = AnimationManager::Get().GetState(key);
        if (!state.healScaleActive || state.healScaleTimer <= 0.0f) return 1.0f;
        float t = 1.0f - (state.healScaleTimer / Duration::HealScale);
        constexpr float kMaxSwell = 0.06f;
        return 1.0f + kMaxSwell * sinf(t * 3.14159265359f);
    }

    ImU32 GetFlashedColor(const std::string& key, ImU32 normalColor, ImU32 increaseColor, ImU32 decreaseColor) {
        auto& state = AnimationManager::Get().GetState(key);
        
        if (state.flashIntensity <= 0.0f) {
            return normalColor;
        }
        
        ImU32 flashColor = (state.flashDirection > 0) ? increaseColor : decreaseColor;
        
        float t = state.flashIntensity;
        int nr = (normalColor >> 0) & 0xFF;
        int ng = (normalColor >> 8) & 0xFF;
        int nb = (normalColor >> 16) & 0xFF;
        int na = (normalColor >> 24) & 0xFF;
        
        int fr = (flashColor >> 0) & 0xFF;
        int fg = (flashColor >> 8) & 0xFF;
        int fb = (flashColor >> 16) & 0xFF;
        int fa = (flashColor >> 24) & 0xFF;
        
        int r = static_cast<int>(nr + (fr - nr) * t);
        int g = static_cast<int>(ng + (fg - ng) * t);
        int b = static_cast<int>(nb + (fb - nb) * t);
        int a = static_cast<int>(na + (fa - na) * t);
        
        return IM_COL32(r, g, b, a);
    }
}
