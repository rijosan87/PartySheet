#include "../../includes/Utils/HUDDraw.h"
#include "../../includes/Utils/ColorUtils.h"
#include "../../includes/TextureManager.h"

#include <imgui_internal.h>
#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstring>
#include <unordered_map>

namespace HUDDraw {

    static ImU32 Argb(std::uint32_t argb)
    {
        return ImGui::ColorConvertFloat4ToU32(ColorUtils::ArgbToVec4(argb));
    }

    void DrawChip(ImDrawList* dl, const ImVec2& bMin, const ImVec2& bMax, const ChipStyle& style)
    {
        const float r = style.rounding;

        if (style.shadow) {
            for (int i = 2; i >= 1; --i) {
                float grow = static_cast<float>(i);
                dl->AddRectFilled(ImVec2(bMin.x - grow + 1.0f, bMin.y - grow + 2.0f),
                                  ImVec2(bMax.x + grow + 1.0f, bMax.y + grow + 2.0f),
                                  IM_COL32(0, 0, 0, 28), r + grow);
            }
        }

        uint8_t fillA = static_cast<uint8_t>((style.topColor >> 24) & 0xFF);
        int vtx0 = dl->VtxBuffer.Size;
        dl->AddRectFilled(bMin, bMax, IM_COL32(255, 255, 255, fillA), r);
        int vtx1 = dl->VtxBuffer.Size;
        ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, vtx0, vtx1,
            bMin, ImVec2(bMin.x, bMax.y),
            Argb(style.topColor) | IM_COL32_A_MASK,
            Argb(style.bottomColor) | IM_COL32_A_MASK);

        if (style.grainAlpha > 0) {
            if (auto* grain = TextureManager::GetTexture("Data\\Interface\\PartySheet\\Frames\\ChipGrain.png")) {
                const float kGrainTexSize = 256.0f;
                ImVec2 uv0(bMin.x / kGrainTexSize, bMin.y / kGrainTexSize);
                ImVec2 uv1(bMax.x / kGrainTexSize, bMax.y / kGrainTexSize);
                uint8_t ga = static_cast<uint8_t>(std::clamp(style.grainAlpha, 0, 255));
                dl->AddImageRounded(reinterpret_cast<ImTextureID>(grain), bMin, bMax,
                                    uv0, uv1, IM_COL32(255, 255, 255, ga), r);
            }
        }

        dl->AddRect(ImVec2(bMin.x + 1.5f, bMin.y + 1.5f), ImVec2(bMax.x - 1.5f, bMax.y - 1.5f),
                    IM_COL32(0, 0, 0, 45), (std::max)(r - 1.0f, 0.0f), 0, 3.0f);

        if (style.highlight)
            dl->AddRectFilled(ImVec2(bMin.x + r, bMin.y + 1.0f),
                              ImVec2(bMax.x - r, bMin.y + 2.0f),
                              IM_COL32(255, 255, 255, 26));

        if (style.accentColor & 0xFF000000u) {
            dl->PushClipRect(ImVec2(bMin.x, bMax.y - 3.0f), bMax, true);
            dl->AddRectFilled(bMin, bMax, Argb(style.accentColor), r);
            dl->PopClipRect();
        }

        if (style.innerBorder & 0xFF000000u)
            dl->AddRect(ImVec2(bMin.x + 1.0f, bMin.y + 1.0f), ImVec2(bMax.x - 1.0f, bMax.y - 1.0f),
                        Argb(style.innerBorder), (std::max)(r - 1.0f, 0.0f), 0, 1.0f);
        if (style.outerBorder & 0xFF000000u)
            dl->AddRect(bMin, bMax, Argb(style.outerBorder), r, 0, 1.2f);
        if (style.glowColor & 0xFF000000u)
            dl->AddRect(ImVec2(bMin.x - 1.0f, bMin.y - 1.0f), ImVec2(bMax.x + 1.0f, bMax.y + 1.0f),
                        Argb(style.glowColor), r + 1.0f, 0, 1.5f);
    }

    void DrawFadeBg(ImDrawList* dl, const ImVec2& bbMin, const ImVec2& bbMax, const FadeBgStyle& style)
    {
        const float w = bbMax.x - bbMin.x;
        const float h = bbMax.y - bbMin.y;
        if (w <= 0.5f || h <= 0.5f) return;

        const ImU32 solid = Argb(style.color);
        const ImU32 clear = solid & 0x00FFFFFF;
        const uint8_t peakA = static_cast<uint8_t>((style.color >> 24) & 0xFF);
        if (peakA == 0) return;

        const float solidFrac = std::clamp(style.solidFrac, 0.0f, 0.6f);
        const float fadeFrac  = std::clamp(style.fadeFrac, 0.1f, 1.0f);

        const uint8_t a1 = static_cast<uint8_t>(peakA * 0.45f);
        const uint8_t a2 = static_cast<uint8_t>(peakA * 0.12f);
        auto withA = [&](uint8_t a) { return (solid & 0x00FFFFFF) | (static_cast<ImU32>(a) << IM_COL32_A_SHIFT); };

        const ImVec2 uvWhite = dl->_Data->TexUvWhitePixel;

        auto linearH = [&](bool toRight) {
            const float solidPx = w * solidFrac;
            const float fadePx  = (w - solidPx) * fadeFrac;
            if (toRight) {
                dl->AddRectFilled(bbMin, ImVec2(bbMin.x + solidPx, bbMax.y), withA(peakA));
                float x0 = bbMin.x + solidPx, x3 = x0 + fadePx;
                float x1 = x0 + (x3 - x0) * 0.4f, x2 = x0 + (x3 - x0) * 0.72f;
                dl->AddRectFilledMultiColor(ImVec2(x0, bbMin.y), ImVec2(x1, bbMax.y), withA(peakA), withA(a1), withA(a1), withA(peakA));
                dl->AddRectFilledMultiColor(ImVec2(x1, bbMin.y), ImVec2(x2, bbMax.y), withA(a1), withA(a2), withA(a2), withA(a1));
                dl->AddRectFilledMultiColor(ImVec2(x2, bbMin.y), ImVec2(x3, bbMax.y), withA(a2), clear, clear, withA(a2));
            } else {
                dl->AddRectFilled(ImVec2(bbMax.x - solidPx, bbMin.y), bbMax, withA(peakA));
                float x0 = bbMax.x - solidPx, x3 = x0 - fadePx;
                float x1 = x0 - (x0 - x3) * 0.4f, x2 = x0 - (x0 - x3) * 0.72f;
                dl->AddRectFilledMultiColor(ImVec2(x1, bbMin.y), ImVec2(x0, bbMax.y), withA(a1), withA(peakA), withA(peakA), withA(a1));
                dl->AddRectFilledMultiColor(ImVec2(x2, bbMin.y), ImVec2(x1, bbMax.y), withA(a2), withA(a1), withA(a1), withA(a2));
                dl->AddRectFilledMultiColor(ImVec2(x3, bbMin.y), ImVec2(x2, bbMax.y), clear, withA(a2), withA(a2), clear);
            }
        };
        auto linearV = [&](bool toDown) {
            const float solidPx = h * solidFrac;
            const float fadePx  = (h - solidPx) * fadeFrac;
            if (toDown) {
                dl->AddRectFilled(bbMin, ImVec2(bbMax.x, bbMin.y + solidPx), withA(peakA));
                float y0 = bbMin.y + solidPx, y3 = y0 + fadePx;
                float y1 = y0 + (y3 - y0) * 0.4f, y2 = y0 + (y3 - y0) * 0.72f;
                dl->AddRectFilledMultiColor(ImVec2(bbMin.x, y0), ImVec2(bbMax.x, y1), withA(peakA), withA(peakA), withA(a1), withA(a1));
                dl->AddRectFilledMultiColor(ImVec2(bbMin.x, y1), ImVec2(bbMax.x, y2), withA(a1), withA(a1), withA(a2), withA(a2));
                dl->AddRectFilledMultiColor(ImVec2(bbMin.x, y2), ImVec2(bbMax.x, y3), withA(a2), withA(a2), clear, clear);
            } else {
                dl->AddRectFilled(ImVec2(bbMin.x, bbMax.y - solidPx), bbMax, withA(peakA));
                float y0 = bbMax.y - solidPx, y3 = y0 - fadePx;
                float y1 = y0 - (y0 - y3) * 0.4f, y2 = y0 - (y0 - y3) * 0.72f;
                dl->AddRectFilledMultiColor(ImVec2(bbMin.x, y1), ImVec2(bbMax.x, y0), withA(a1), withA(a1), withA(peakA), withA(peakA));
                dl->AddRectFilledMultiColor(ImVec2(bbMin.x, y2), ImVec2(bbMax.x, y1), withA(a2), withA(a2), withA(a1), withA(a1));
                dl->AddRectFilledMultiColor(ImVec2(bbMin.x, y3), ImVec2(bbMax.x, y2), clear, clear, withA(a2), withA(a2));
            }
        };

        auto diagonal = [&](bool fromTL) {
            const ImVec2 origin = fromTL ? bbMin : ImVec2(bbMin.x, bbMax.y);
            const float  L      = std::sqrt(w * w + h * h);
            ImVec2 dir  = fromTL ? ImVec2(w / L, h / L) : ImVec2(w / L, -h / L);
            ImVec2 perp(-dir.y, dir.x);
            const float B = L;

            const float solidPx = L * solidFrac;
            const float fadePx  = (L - solidPx) * fadeFrac;
            const float t0 = solidPx, t3 = solidPx + fadePx;
            const float t1 = t0 + (t3 - t0) * 0.4f, t2 = t0 + (t3 - t0) * 0.72f;

            auto strip = [&](float ta, float tb, uint8_t aa, uint8_t ab) {
                if (tb <= ta) return;
                ImVec2 pa(origin.x + dir.x * ta, origin.y + dir.y * ta);
                ImVec2 pb(origin.x + dir.x * tb, origin.y + dir.y * tb);
                const ImU32 ca = withA(aa), cb = withA(ab);
                dl->PrimReserve(6, 4);
                ImDrawIdx base = static_cast<ImDrawIdx>(dl->_VtxCurrentIdx);
                dl->PrimWriteVtx(ImVec2(pa.x - perp.x * B, pa.y - perp.y * B), uvWhite, ca);
                dl->PrimWriteVtx(ImVec2(pa.x + perp.x * B, pa.y + perp.y * B), uvWhite, ca);
                dl->PrimWriteVtx(ImVec2(pb.x + perp.x * B, pb.y + perp.y * B), uvWhite, cb);
                dl->PrimWriteVtx(ImVec2(pb.x - perp.x * B, pb.y - perp.y * B), uvWhite, cb);
                dl->PrimWriteIdx(base); dl->PrimWriteIdx(static_cast<ImDrawIdx>(base + 1)); dl->PrimWriteIdx(static_cast<ImDrawIdx>(base + 2));
                dl->PrimWriteIdx(base); dl->PrimWriteIdx(static_cast<ImDrawIdx>(base + 2)); dl->PrimWriteIdx(static_cast<ImDrawIdx>(base + 3));
            };

            dl->PushClipRect(bbMin, bbMax, true);
            strip(-1.0f, t0, peakA, peakA);
            strip(t0, t1, peakA, a1);
            strip(t1, t2, a1, a2);
            strip(t2, t3, a2, 0);
            dl->PopClipRect();
        };

        auto radialFan = [&](const ImVec2& center, float rx, float ry) {
            const int   kRim = 48;
            const float solid = std::clamp(solidFrac, 0.0f, 0.6f);
            const float outer = std::clamp(fadeFrac, 0.1f, 1.0f);
            const float rSolidScale = solid;
            const float rRimScale   = (std::max)(outer, solid + 0.01f);

            if (rSolidScale > 0.01f) {
                const float ex = rx * rSolidScale, ey = ry * rSolidScale;
                dl->PrimReserve(kRim * 3, kRim + 1);
                ImDrawIdx base = static_cast<ImDrawIdx>(dl->_VtxCurrentIdx);
                dl->PrimWriteVtx(center, uvWhite, withA(peakA));
                for (int i = 0; i < kRim; ++i) {
                    float ang = 6.2831853f * static_cast<float>(i) / static_cast<float>(kRim);
                    dl->PrimWriteVtx(ImVec2(center.x + std::cos(ang) * ex, center.y + std::sin(ang) * ey),
                                     uvWhite, withA(peakA));
                }
                for (int i = 0; i < kRim; ++i) {
                    dl->PrimWriteIdx(base);
                    dl->PrimWriteIdx(static_cast<ImDrawIdx>(base + 1 + i));
                    dl->PrimWriteIdx(static_cast<ImDrawIdx>(base + 1 + ((i + 1) % kRim)));
                }
            }

            const uint8_t stops[5] = {
                peakA,
                static_cast<uint8_t>(peakA * 0.45f),
                static_cast<uint8_t>(peakA * 0.12f),
                static_cast<uint8_t>(peakA * 0.03f),
                0,
            };
            for (int band = 0; band < 4; ++band) {
                float sIn  = rSolidScale + (rRimScale - rSolidScale) * (static_cast<float>(band)     / 4.0f);
                float sOut = rSolidScale + (rRimScale - rSolidScale) * (static_cast<float>(band + 1) / 4.0f);
                const float exIn = rx * sIn,  eyIn = ry * sIn;
                const float exOut = rx * sOut, eyOut = ry * sOut;
                const ImU32 colIn  = withA(stops[band]);
                const ImU32 colOut = withA(stops[band + 1]);

                dl->PrimReserve(kRim * 6, kRim * 2);
                ImDrawIdx base = static_cast<ImDrawIdx>(dl->_VtxCurrentIdx);
                for (int i = 0; i < kRim; ++i) {
                    float ang = 6.2831853f * static_cast<float>(i) / static_cast<float>(kRim);
                    float cs = std::cos(ang), sn = std::sin(ang);
                    dl->PrimWriteVtx(ImVec2(center.x + cs * exIn,  center.y + sn * eyIn),  uvWhite, colIn);
                    dl->PrimWriteVtx(ImVec2(center.x + cs * exOut, center.y + sn * eyOut), uvWhite, colOut);
                }
                for (int i = 0; i < kRim; ++i) {
                    int ni = (i + 1) % kRim;
                    ImDrawIdx i0 = static_cast<ImDrawIdx>(base + i * 2);
                    ImDrawIdx i1 = static_cast<ImDrawIdx>(base + i * 2 + 1);
                    ImDrawIdx i2 = static_cast<ImDrawIdx>(base + ni * 2);
                    ImDrawIdx i3 = static_cast<ImDrawIdx>(base + ni * 2 + 1);
                    dl->PrimWriteIdx(i0); dl->PrimWriteIdx(i1); dl->PrimWriteIdx(i3);
                    dl->PrimWriteIdx(i0); dl->PrimWriteIdx(i3); dl->PrimWriteIdx(i2);
                }
            }
        };

        switch (style.mode) {
            case FadeBgMode::FadeRight: linearH(true);  break;
            case FadeBgMode::FadeLeft:  linearH(false); break;
            case FadeBgMode::FadeDown:  linearV(true);  break;
            case FadeBgMode::FadeUp:    linearV(false); break;
            case FadeBgMode::DiagonalTL: diagonal(true);  break;
            case FadeBgMode::DiagonalBL: diagonal(false); break;
            case FadeBgMode::CenterOut:
                radialFan(ImVec2(bbMin.x + w * 0.5f, bbMin.y + h * 0.5f), w * 0.5f, h * 0.5f);
                break;
            case FadeBgMode::EdgeOrigin:
                radialFan(ImVec2(bbMin.x, bbMin.y + h * 0.5f), w, h * 0.5f);
                break;
        }
    }

    void DrawIconStyled(ImDrawList* dl, const ImVec2& pos, const ImVec2& size, ImTextureID tex,
                        std::uint32_t argb, const IconFx& fx,
                        const ImVec2& uvMin, const ImVec2& uvMax)
    {
        uint8_t rC = (argb >> 16) & 0xFF, gC = (argb >> 8) & 0xFF, bC = argb & 0xFF, aC = (argb >> 24) & 0xFF;
        const float w = (std::max)(1.0f, size.x);
        const float h = (std::max)(1.0f, size.y);
        const float s = (std::max)(w, h);

        if (fx.glow) {
            const float g = 2.0f * (std::max)(1.0f, s / 24.0f);
            static constexpr float kGlowDirs[8][2] = {
                { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
                { 0.707f, 0.707f }, { -0.707f, 0.707f }, { 0.707f, -0.707f }, { -0.707f, -0.707f },
            };
            for (auto& d : kGlowDirs)
                dl->AddImage(tex, ImVec2(pos.x + d[0] * g, pos.y + d[1] * g),
                             ImVec2(pos.x + w + d[0] * g, pos.y + h + d[1] * g),
                             uvMin, uvMax, IM_COL32(rC, gC, bC, 22));
        }

        if (fx.shadow) {
            const float sc  = (std::max)(1.0f, s / 24.0f);
            const float rim = 1.0f * sc;
            dl->AddImage(tex, ImVec2(pos.x - rim, pos.y - rim),
                         ImVec2(pos.x + w - rim, pos.y + h - rim),
                         uvMin, uvMax, IM_COL32(255, 255, 255, 60));
            const float shOff = 1.5f * sc;
            dl->AddImage(tex, ImVec2(pos.x + shOff, pos.y + shOff + 0.5f),
                         ImVec2(pos.x + w + shOff, pos.y + h + shOff + 0.5f),
                         uvMin, uvMax, IM_COL32(0, 0, 0, 70));
            dl->AddImage(tex, ImVec2(pos.x + shOff * 0.6f, pos.y + shOff * 0.6f),
                         ImVec2(pos.x + w + shOff * 0.6f, pos.y + h + shOff * 0.6f),
                         uvMin, uvMax, IM_COL32(0, 0, 0, 90));
        }

        if (fx.outline) {
            const float olW = static_cast<float>(std::clamp(fx.outlineWidth, 1, 3));
            const float o = olW * (std::min)(1.5f, (std::max)(1.0f, s / 32.0f));
            const int   n = (o >= 2.0f) ? 16 : 8;
            ImU32 olCol = Argb(fx.outlineColor);
            for (int i = 0; i < n; ++i) {
                float a  = 6.2831853f * static_cast<float>(i) / static_cast<float>(n);
                float dx = std::cos(a) * o, dy = std::sin(a) * o;
                dl->AddImage(tex, ImVec2(pos.x + dx, pos.y + dy),
                             ImVec2(pos.x + w + dx, pos.y + h + dy),
                             uvMin, uvMax, olCol);
            }
        }

        if (!fx.duotone) {
            dl->AddImage(tex, pos, ImVec2(pos.x + w, pos.y + h),
                         uvMin, uvMax, IM_COL32(rC, gC, bC, aC));
        } else {
            auto lift = [](uint8_t c) { return static_cast<uint8_t>((std::min)(255.0f, c * 1.15f + 30.0f)); };
            ImU32 top = IM_COL32(lift(rC), lift(gC), lift(bC), 255);
            ImU32 bot = IM_COL32(static_cast<uint8_t>(rC * 0.55f), static_cast<uint8_t>(gC * 0.55f),
                                 static_cast<uint8_t>(bC * 0.55f), 255);
            int v0 = dl->VtxBuffer.Size;
            dl->AddImage(tex, pos, ImVec2(pos.x + w, pos.y + h),
                         uvMin, uvMax, IM_COL32(255, 255, 255, aC));
            int v1 = dl->VtxBuffer.Size;
            ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, v0, v1,
                pos, ImVec2(pos.x, pos.y + h), top, bot);
        }

        if (fx.gloss) {
            dl->PushClipRect(pos, ImVec2(pos.x + w, pos.y + h * 0.45f), true);
            dl->AddImage(tex, pos, ImVec2(pos.x + w, pos.y + h),
                         uvMin, uvMax, IM_COL32(255, 255, 255, 45));
            dl->PopClipRect();
        }
    }

    static void DrawTextStyledRange(ImDrawList* dl, const ImVec2& pos, ImU32 baseCol,
                                    const char* txt, const char* txtEnd, const TextFx& fx)
    {
        ImFont*     font     = fx.font ? fx.font : ImGui::GetFont();
        const float fontSize = fx.fontSize > 0.0f ? fx.fontSize : ImGui::GetFontSize();

        uint8_t rT = (baseCol >> IM_COL32_R_SHIFT) & 0xFF;
        uint8_t gT = (baseCol >> IM_COL32_G_SHIFT) & 0xFF;
        uint8_t bT = (baseCol >> IM_COL32_B_SHIFT) & 0xFF;
        uint8_t aT = (baseCol >> IM_COL32_A_SHIFT) & 0xFF;

        const float aScale = aT / 255.0f;

        const float sScale = std::clamp(fontSize / font->FontSize, 0.35f, 1.0f);

        if (fx.shadow) {
            const ImVec2 sc(pos.x + 1.0f * sScale, pos.y + 1.5f * sScale);
            dl->AddText(font, fontSize, sc, IM_COL32(0, 0, 0, static_cast<int>(150 * aScale)), txt, txtEnd);
            static constexpr float kSpread[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
            for (auto& d : kSpread)
                dl->AddText(font, fontSize, ImVec2(sc.x + d[0] * sScale, sc.y + d[1] * sScale), IM_COL32(0, 0, 0, static_cast<int>(55 * aScale)), txt, txtEnd);
        }

        if (fx.outline) {
            uint8_t olA = static_cast<uint8_t>(std::clamp(fx.outlineOpacity, 0, 255) * aScale);
            static constexpr float kDirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
            for (auto& d : kDirs)
                dl->AddText(font, fontSize, ImVec2(pos.x + d[0] * sScale, pos.y + d[1] * sScale), IM_COL32(8, 6, 10, olA), txt, txtEnd);
        }

        if (!fx.duotone) {
            dl->AddText(font, fontSize, pos, baseCol, txt, txtEnd);
            return;
        }

        auto lift = [](uint8_t c) { return static_cast<uint8_t>((std::min)(255.0f, c * 1.10f + 20.0f)); };
        ImU32 top = IM_COL32(lift(rT), lift(gT), lift(bT), 255);
        ImU32 bot = IM_COL32(static_cast<uint8_t>(rT * 0.78f), static_cast<uint8_t>(gT * 0.78f),
                             static_cast<uint8_t>(bT * 0.78f), 255);
        int v0 = dl->VtxBuffer.Size;
        dl->AddText(font, fontSize, pos, IM_COL32(255, 255, 255, aT), txt, txtEnd);
        int v1 = dl->VtxBuffer.Size;
        ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, v0, v1,
            pos, ImVec2(pos.x, pos.y + fontSize), top, bot);
    }

    void DrawTextStyled(ImDrawList* dl, const ImVec2& pos, ImU32 baseCol, const char* txt,
                        const TextFx& fx)
    {
        DrawTextStyledRange(dl, pos, baseCol, txt, nullptr, fx);
    }

    float DrawTextStyledWrapped(ImDrawList* dl, const ImVec2& pos, float maxWidth,
                                ImU32 baseCol, const char* txt, const TextFx& fx,
                                bool centered)
    {
        ImFont*     font     = fx.font ? fx.font : ImGui::GetFont();
        const float fontSize = fx.fontSize > 0.0f ? fx.fontSize : ImGui::GetFontSize();
        const float scale    = fontSize / font->FontSize;
        const float lineH    = fontSize;
        const char* textEnd  = txt + strlen(txt);

        float y = pos.y;
        const char* s = txt;
        while (s < textEnd) {
            const char* lineEnd = font->CalcWordWrapPositionA(scale, s, textEnd, maxWidth);
            if (lineEnd == s)
                lineEnd++;
            float x = pos.x;
            if (centered) {
                float lw = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, s, lineEnd).x;
                x += (std::max)(0.0f, (maxWidth - lw) * 0.5f);
            }
            DrawTextStyledRange(dl, ImVec2(x, y), baseCol, s, lineEnd, fx);
            y += lineH;
            s = lineEnd;
            while (s < textEnd && (*s == ' ' || *s == '\t'))
                s++;
            if (s < textEnd && *s == '\n')
                s++;
        }
        return y - pos.y;
    }

    static void BadgeMetrics(const char* txt, const BadgeStyle& s,
                             ImFont*& font, float& fs, ImVec2& ts,
                             float& iconSz, float& gap, float& w, float& h)
    {
        font = s.font ? s.font : ImGui::GetFont();
        fs   = font->FontSize * s.fontScale;
        const bool hasText = txt && txt[0];
        ts = hasText ? font->CalcTextSizeA(fs, 3.402823466e+38f /*FLT_MAX*/, 0.0f, txt)
                     : ImVec2(0.0f, 0.0f);
        const bool hasIcon = s.icon != 0;
        iconSz = hasIcon ? (s.iconSize > 0.0f ? s.iconSize : (hasText ? ts.y : fs)) : 0.0f;
        gap    = (hasIcon && hasText) ? 3.0f : 0.0f;
        w = iconSz + gap + ts.x + s.padX * 2.0f;
        h = (std::max)(ts.y, iconSz) + s.padY * 2.0f;
    }

    ImVec2 MeasureBadge(const char* txt, const BadgeStyle& s)
    {
        ImFont* font; float fs, iconSz, gap, w, h; ImVec2 ts;
        BadgeMetrics(txt, s, font, fs, ts, iconSz, gap, w, h);
        return ImVec2(w, h);
    }

    BadgeRect DrawBadge(ImDrawList* dl, const ImVec2& anchor, int corner, const char* txt,
                        const BadgeStyle& s)
    {
        ImFont* font; float fs, iconSz, gap, w, h; ImVec2 ts;
        BadgeMetrics(txt, s, font, fs, ts, iconSz, gap, w, h);
        const bool hasText = txt && txt[0];
        const bool hasIcon = s.icon != 0;

        ImVec2 bMin;
        switch (corner) {
            case 1:  bMin = ImVec2(anchor.x - w, anchor.y);     break;
            case 2:  bMin = ImVec2(anchor.x - w, anchor.y - h); break;
            case 3:  bMin = ImVec2(anchor.x,     anchor.y - h); break;
            default: bMin = anchor;                             break;
        }
        const ImVec2 bMax(bMin.x + w, bMin.y + h);
        const float  r = s.rounding < 0.0f ? h * 0.5f
            : (std::min)(s.rounding, (std::min)(w, h) * 0.5f);

        auto A = [&](int base) { return static_cast<uint8_t>(std::clamp(base * s.alpha, 0.0f, 255.0f)); };
        auto Col = [&](std::uint32_t argb) {
            float aF = ((argb >> 24) & 0xFF) * s.alpha;
            return IM_COL32((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF,
                            static_cast<uint8_t>(std::clamp(aF, 0.0f, 255.0f)));
        };

        if (s.shadow) {
            for (int i = 2; i >= 1; --i) {
                float grow = static_cast<float>(i);
                dl->AddRectFilled(ImVec2(bMin.x - grow + 1.0f, bMin.y - grow + 2.0f),
                                  ImVec2(bMax.x + grow + 1.0f, bMax.y + grow + 2.0f),
                                  IM_COL32(0, 0, 0, A(28)), r + grow);
            }
        }

        {
            uint8_t fillA = A((s.bgTop >> 24) & 0xFF);
            int v0 = dl->VtxBuffer.Size;
            dl->AddRectFilled(bMin, bMax, IM_COL32(255, 255, 255, fillA), r);
            int v1 = dl->VtxBuffer.Size;
            ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, v0, v1,
                bMin, ImVec2(bMin.x, bMax.y),
                Argb(s.bgTop) | IM_COL32_A_MASK,
                Argb(s.bgBottom) | IM_COL32_A_MASK);
        }

        if (s.highlight) {
            dl->PushClipRect(ImVec2(bMin.x, bMin.y + 1.0f), ImVec2(bMax.x, bMin.y + 2.0f), true);
            dl->AddRectFilled(bMin, bMax, IM_COL32(255, 255, 255, A(30)), r);
            dl->PopClipRect();
        }

        if (s.innerBorder & 0xFF000000u)
            dl->AddRect(ImVec2(bMin.x + 1.0f, bMin.y + 1.0f), ImVec2(bMax.x - 1.0f, bMax.y - 1.0f),
                        Col(s.innerBorder), (std::max)(r - 1.0f, 0.0f), 0, 1.0f);
        if (s.outerBorder & 0xFF000000u)
            dl->AddRect(bMin, bMax, Col(s.outerBorder), r, 0, 1.0f);

        if (hasIcon) {
            const float iy = bMin.y + (h - iconSz) * 0.5f;
            dl->AddImage(s.icon, ImVec2(bMin.x + s.padX, iy),
                         ImVec2(bMin.x + s.padX + iconSz, iy + iconSz),
                         ImVec2(0, 0), ImVec2(1, 1), Col(s.iconTint));
        }
        if (!hasText) return { bMin, bMax };

        {
            const ImVec2 tp(bMin.x + s.padX + iconSz + gap, bMin.y + (h - ts.y) * 0.5f);
            uint8_t tA = A((s.textColor >> 24) & 0xFF);
            uint8_t tR = (s.textColor >> 16) & 0xFF, tG = (s.textColor >> 8) & 0xFF, tB = s.textColor & 0xFF;
            if (s.textShadow)
                dl->AddText(font, fs, ImVec2(tp.x + 1.0f, tp.y + 1.0f), IM_COL32(0, 0, 0, A(150)), txt);
            if (!s.textDuotone) {
                dl->AddText(font, fs, tp, IM_COL32(tR, tG, tB, tA), txt);
            } else {
                auto lift = [](uint8_t c) { return static_cast<uint8_t>((std::min)(255.0f, c * 1.10f + 20.0f)); };
                ImU32 top = IM_COL32(lift(tR), lift(tG), lift(tB), 255);
                ImU32 bot = IM_COL32(static_cast<uint8_t>(tR * 0.78f), static_cast<uint8_t>(tG * 0.78f),
                                     static_cast<uint8_t>(tB * 0.78f), 255);
                int v0 = dl->VtxBuffer.Size;
                dl->AddText(font, fs, tp, IM_COL32(255, 255, 255, tA), txt);
                int v1 = dl->VtxBuffer.Size;
                ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, v0, v1,
                    tp, ImVec2(tp.x, tp.y + ts.y), top, bot);
            }
        }

        return { bMin, bMax };
    }

    float MeasurePipRow(int count, const PipStyle& s)
    {
        return count > 0 ? count * (s.radius * 2.0f + s.gap) - s.gap : 0.0f;
    }

    void DrawPipRow(ImDrawList* dl, const ImVec2& pos, int lit, int count, const PipStyle& s)
    {
        if (count <= 0) return;
        auto A = [&](int base) { return static_cast<uint8_t>(std::clamp(base * s.alpha, 0.0f, 255.0f)); };

        const float pitch = s.radius * 2.0f + s.gap;
        const float drawR = (s.shape == 1) ? s.radius * 1.35f : s.radius;
        const float cy    = pos.y;
        float       cx    = pos.x + s.radius;

        const uint8_t lR = (s.litColor >> 16) & 0xFF, lG = (s.litColor >> 8) & 0xFF, lB = s.litColor & 0xFF;
        const uint8_t eR = (s.emptyColor >> 16) & 0xFF, eG = (s.emptyColor >> 8) & 0xFF, eB = s.emptyColor & 0xFF;

        auto FillShape = [&](float x, float y, float r, ImU32 col) {
            if (s.shape == 1) {
                ImVec2 p[4] = { { x, y - r }, { x + r, y }, { x, y + r }, { x - r, y } };
                dl->AddConvexPolyFilled(p, 4, col);
            } else {
                dl->AddCircleFilled(ImVec2(x, y), r, col, 0);
            }
        };
        auto StrokeShape = [&](float x, float y, float r, ImU32 col, float th) {
            if (s.shape == 1) {
                ImVec2 p[4] = { { x, y - r }, { x + r, y }, { x, y + r }, { x - r, y } };
                dl->AddPolyline(p, 4, col, ImDrawFlags_Closed, th);
            } else {
                dl->AddCircle(ImVec2(x, y), r, col, 0, th);
            }
        };
        auto FillDuotone = [&](float x, float y, float r, uint8_t cR, uint8_t cG, uint8_t cB, uint8_t fillA, bool inset) {
            auto lift = [](uint8_t c) { return static_cast<uint8_t>((std::min)(255.0f, c * 1.22f + 18.0f)); };
            auto sink = [](uint8_t c) { return static_cast<uint8_t>(c * 0.62f); };
            const ImU32 top = inset ? IM_COL32(sink(cR), sink(cG), sink(cB), 255)
                                    : IM_COL32(lift(cR), lift(cG), lift(cB), 255);
            const ImU32 bot = inset ? IM_COL32(cR, cG, cB, 255)
                                    : IM_COL32(sink(cR), sink(cG), sink(cB), 255);
            int v0 = dl->VtxBuffer.Size;
            FillShape(x, y, r, IM_COL32(255, 255, 255, fillA));
            int v1 = dl->VtxBuffer.Size;
            ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, v0, v1,
                ImVec2(x, y - r), ImVec2(x, y + r), top, bot);
        };

        for (int i = 0; i < count; ++i, cx += pitch) {
            const bool on = i < lit;

            if (s.socket)
                FillShape(cx + 0.5f, cy + 1.0f, drawR + 0.5f, IM_COL32(0, 0, 0, A(70)));

            if (on) {
                if (s.glow) {
                    FillShape(cx, cy, drawR + 2.5f, IM_COL32(lR, lG, lB, A(26)));
                    FillShape(cx, cy, drawR + 1.2f, IM_COL32(lR, lG, lB, A(48)));
                }
                const uint8_t fillA = A((s.litColor >> 24) & 0xFF);
                if (s.duotone) FillDuotone(cx, cy, drawR, lR, lG, lB, fillA, false);
                else           FillShape(cx, cy, drawR, IM_COL32(lR, lG, lB, fillA));
                if (s.specular && drawR >= 3.0f)
                    dl->AddCircleFilled(ImVec2(cx - drawR * 0.30f, cy - drawR * 0.35f),
                                        (std::max)(1.0f, drawR * 0.28f), IM_COL32(255, 255, 255, A(110)));
                if (s.contour) StrokeShape(cx, cy, drawR, IM_COL32(0, 0, 0, A(140)), 1.0f);
            } else {
                const uint8_t eA = A((s.emptyColor >> 24) & 0xFF);
                if (s.duotone) FillDuotone(cx, cy, drawR, eR, eG, eB, eA, true);
                else           FillShape(cx, cy, drawR, IM_COL32(eR, eG, eB, eA));
                if (s.contour) StrokeShape(cx, cy, drawR, IM_COL32(0, 0, 0, A(90)), 1.0f);
            }
        }
    }

    static int BuildBarShape(ImVec2* pts, const ImVec2& bMin, const ImVec2& bMax,
                             int leftStyle, int rightStyle, float inset = 0.0f)
    {
        float h          = bMax.y - bMin.y;
        float slant      = h * 0.5f;
        float arrowWidth = h * 0.6f;
        float diamondW   = h * 0.4f;

        float lx = bMin.x + inset;
        float rx = bMax.x - inset;
        float ty = bMin.y + inset;
        float by = bMax.y - inset;
        float cy = bMin.y + h * 0.5f;
        float s = (std::max)(0.0f, slant      - inset);
        float a = (std::max)(0.0f, arrowWidth - inset);
        float d = (std::max)(0.0f, diamondW   - inset);

        int idx = 0;
        switch (leftStyle) {
            case 1: pts[idx++] = ImVec2(lx + s, ty); break;
            case 3: pts[idx++] = ImVec2(lx + a, ty); break;
            case 4: pts[idx++] = ImVec2(lx + d, ty); break;
            default: pts[idx++] = ImVec2(lx, ty); break;
        }
        switch (rightStyle) {
            case 2: pts[idx++] = ImVec2(rx - s, ty); break;
            case 3: pts[idx++] = ImVec2(rx - a, ty); pts[idx++] = ImVec2(rx, cy); break;
            case 4: pts[idx++] = ImVec2(rx - d, ty); pts[idx++] = ImVec2(rx, cy); break;
            default: pts[idx++] = ImVec2(rx, ty); break;
        }
        switch (rightStyle) {
            case 1: pts[idx++] = ImVec2(rx - s, by); break;
            case 3: pts[idx++] = ImVec2(rx - a, by); break;
            case 4: pts[idx++] = ImVec2(rx - d, by); break;
            default: pts[idx++] = ImVec2(rx, by); break;
        }
        switch (leftStyle) {
            case 2: pts[idx++] = ImVec2(lx + s, by); break;
            case 3: pts[idx++] = ImVec2(lx + a, by); pts[idx++] = ImVec2(lx, cy); break;
            case 4: pts[idx++] = ImVec2(lx + d, by); pts[idx++] = ImVec2(lx, cy); break;
            default: pts[idx++] = ImVec2(lx, by); break;
        }
        return idx;
    }

    static void BarSpanAtX(float x, const ImVec2& bMin, const ImVec2& bMax,
                           int leftStyle, int rightStyle, float rounding,
                           float& top, float& bot)
    {
        const float h  = bMax.y - bMin.y;
        const float cy = bMin.y + h * 0.5f;
        top = bMin.y; bot = bMax.y;
        auto capW = [&](int st) -> float {
            switch (st) {
                case 1: case 2: return h * 0.5f;
                case 3:         return h * 0.6f;
                case 4:         return h * 0.4f;
                default:        return 0.0f;
            }
        };
        if (leftStyle != 0 || rightStyle != 0) {
            float lw = capW(leftStyle);
            if (lw > 0.0f && x < bMin.x + lw) {
                float t = std::clamp((x - bMin.x) / lw, 0.0f, 1.0f);
                switch (leftStyle) {
                    case 1: top = (std::max)(top, bMax.y - t * h); break;
                    case 2: bot = (std::min)(bot, bMin.y + t * h); break;
                    default: top = (std::max)(top, cy - t * h * 0.5f);
                             bot = (std::min)(bot, cy + t * h * 0.5f); break;
                }
            }
            float rw = capW(rightStyle);
            if (rw > 0.0f && x > bMax.x - rw) {
                float t = std::clamp((bMax.x - x) / rw, 0.0f, 1.0f);
                switch (rightStyle) {
                    case 1: bot = (std::min)(bot, bMin.y + t * h); break;
                    case 2: top = (std::max)(top, bMax.y - t * h); break;
                    default: top = (std::max)(top, cy - t * h * 0.5f);
                             bot = (std::min)(bot, cy + t * h * 0.5f); break;
                }
            }
        } else if (rounding > 0.5f) {
            if (x < bMin.x + rounding) {
                float dx = bMin.x + rounding - x;
                float inset = rounding - std::sqrt((std::max)(0.0f, rounding * rounding - dx * dx));
                top += inset; bot -= inset;
            }
            if (x > bMax.x - rounding) {
                float dx = x - (bMax.x - rounding);
                float inset = rounding - std::sqrt((std::max)(0.0f, rounding * rounding - dx * dx));
                top += inset; bot -= inset;
            }
        }
    }

    static void ShadeBarFill(ImDrawList* dl, int v0, int v1, const ImVec2& bMin, const ImVec2& bMax,
                             std::uint32_t argb, std::uint32_t argb2, bool duotone)
    {
        float w = (std::max)(1.0f, bMax.x - bMin.x);
        float h = (std::max)(1.0f, bMax.y - bMin.y);
        float r1 = static_cast<float>((argb >> 16) & 0xFF), g1 = static_cast<float>((argb >> 8) & 0xFF), b1 = static_cast<float>(argb & 0xFF);
        float r2 = r1, g2 = g1, b2 = b1;
        if (argb2 & 0xFF000000u) {
            r2 = static_cast<float>((argb2 >> 16) & 0xFF);
            g2 = static_cast<float>((argb2 >> 8) & 0xFF);
            b2 = static_cast<float>(argb2 & 0xFF);
        }
        ImDrawVert* vtx = dl->VtxBuffer.Data;
        for (int i = v0; i < v1; ++i) {
            float tx = std::clamp((vtx[i].pos.x - bMin.x) / w, 0.0f, 1.0f);
            float ty = std::clamp((vtx[i].pos.y - bMin.y) / h, 0.0f, 1.0f);
            float r = r1 + (r2 - r1) * tx;
            float g = g1 + (g2 - g1) * tx;
            float b = b1 + (b2 - b1) * tx;
            if (duotone) {
                float mul = 1.28f + (0.52f - 1.28f) * ty;
                float add = 26.0f * (1.0f - ty);
                r = r * mul + add; g = g * mul + add; b = b * mul + add;
            }
            ImU32 keepA = vtx[i].col & IM_COL32_A_MASK;
            vtx[i].col = keepA | IM_COL32(
                static_cast<uint8_t>(std::clamp(r, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(g, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(b, 0.0f, 255.0f)), 0);
        }
    }

    void DrawBarStyled(ImDrawList* dl, const ImVec2& pos, const BarStyle& s)
    {
        const ImVec2 bMax(pos.x + s.width, pos.y + s.height);
        const bool  shaped = (s.leftEnd != 0 || s.rightEnd != 0);
        const float frac   = std::clamp(s.fraction, 0.0f, 1.0f);
        const float fillEdge = pos.x + s.width * frac;
        const float r = shaped ? 0.0f
            : (std::min)(s.rounding, (std::min)(s.width, s.height) * 0.5f);

        auto A = [&](int base) { return static_cast<uint8_t>(std::clamp(base * s.alpha, 0.0f, 255.0f)); };
        auto Col = [&](std::uint32_t argb) {
            float aF = ((argb >> 24) & 0xFF) * s.alpha;
            return IM_COL32((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF,
                            static_cast<uint8_t>(std::clamp(aF, 0.0f, 255.0f)));
        };
        ImVec2 poly[8];
        int    polyN = shaped ? BuildBarShape(poly, pos, bMax, s.leftEnd, s.rightEnd) : 0;
        auto FillSilhouette = [&](ImU32 col) {
            if (!shaped) dl->AddRectFilled(pos, bMax, col, r);
            else         dl->AddConvexPolyFilled(poly, polyN, col);
        };
        auto StrokeSilhouette = [&](ImU32 col, float inset, float thickness) {
            if (!shaped) {
                dl->AddRect(ImVec2(pos.x + inset, pos.y + inset), ImVec2(bMax.x - inset, bMax.y - inset),
                            col, (std::max)(r - inset, 0.0f), 0, thickness);
            } else {
                ImVec2 ip[8];
                int n = BuildBarShape(ip, pos, bMax, s.leftEnd, s.rightEnd, inset);
                for (int i = 0; i < n; ++i) dl->PathLineTo(ip[i]);
                dl->PathLineTo(ip[0]);
                dl->PathStroke(col, 0, thickness);
            }
        };

        uint8_t fR = (s.fillColor >> 16) & 0xFF, fG = (s.fillColor >> 8) & 0xFF, fB = s.fillColor & 0xFF;

        int   segEff    = 0;
        float segGapEff = 0.0f;
        if (s.segments > 1) {
            segEff = (std::min)(s.segments,
                static_cast<int>(std::floor(s.width / (s.minSegmentPx + s.segmentGap))));
            if (segEff >= 2) {
                segGapEff = s.segmentGap;
                if (segGapEff > 0.0f && (segEff - 1) * segGapEff > 0.25f * s.width)
                    segGapEff = (0.25f * s.width) / static_cast<float>(segEff - 1);
            } else {
                segEff = 0;
            }
        }
        auto InGap = [&](float x) -> bool {
            if (segEff < 2 || segGapEff <= 0.0f) return false;
            for (int i = 1; i < segEff; ++i) {
                float gx = pos.x + s.width * (static_cast<float>(i) / segEff);
                if (x > gx - segGapEff * 0.5f && x < gx + segGapEff * 0.5f) return true;
            }
            return false;
        };

        if (s.glow && s.drawFill && frac > 0.0f && !shaped) {
            const float expands[] = { 2.0f, 4.0f, 6.0f };
            const int   alphas[]  = { 45, 30, 15 };
            for (int i = 2; i >= 0; --i)
                dl->AddRectFilled(ImVec2(pos.x - expands[i], pos.y - expands[i]),
                                  ImVec2(fillEdge + expands[i], bMax.y + expands[i]),
                                  IM_COL32(fR, fG, fB, A(alphas[i])), r + expands[i]);
        }

        if (s.shadow) {
            if (!shaped) {
                for (int i = 2; i >= 1; --i) {
                    float grow = static_cast<float>(i);
                    dl->AddRectFilled(ImVec2(pos.x - grow + 1.0f, pos.y - grow + 2.0f),
                                      ImVec2(bMax.x + grow + 1.0f, bMax.y + grow + 2.0f),
                                      IM_COL32(0, 0, 0, A(30)), r + grow);
                }
            } else {
                ImVec2 sp[8];
                static constexpr float kOff[2][2] = { { 2.0f, 3.0f }, { 1.0f, 2.0f } };
                for (auto& o : kOff) {
                    for (int i = 0; i < polyN; ++i) sp[i] = ImVec2(poly[i].x + o[0], poly[i].y + o[1]);
                    dl->AddConvexPolyFilled(sp, polyN, IM_COL32(0, 0, 0, A(30)));
                }
            }
        }

        if (s.drawTrack) {
            int v0 = dl->VtxBuffer.Size;
            FillSilhouette(IM_COL32(255, 255, 255, A((s.trackTop >> 24) & 0xFF)));
            int v1 = dl->VtxBuffer.Size;
            ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, v0, v1,
                pos, ImVec2(pos.x, bMax.y),
                Col(s.trackTop) | IM_COL32_A_MASK, Col(s.trackBottom) | IM_COL32_A_MASK);
            // Inner shadow under the top edge (inset look) + light catching the bottom lip.
            if (s.trackDepth) {
                dl->PushClipRect(ImVec2(pos.x, pos.y + 1.0f), ImVec2(bMax.x, pos.y + 2.5f), true);
                FillSilhouette(IM_COL32(0, 0, 0, A(90)));
                dl->PopClipRect();
                dl->PushClipRect(ImVec2(pos.x, bMax.y - 1.5f), ImVec2(bMax.x, bMax.y - 0.5f), true);
                FillSilhouette(IM_COL32(255, 255, 255, A(14)));
                dl->PopClipRect();
            }
        }

        if (s.ghostFraction > frac + 0.001f) {
            float ghostEdge = pos.x + s.width * std::clamp(s.ghostFraction, 0.0f, 1.0f);
            dl->PushClipRect(ImVec2(fillEdge, pos.y), ImVec2(ghostEdge, bMax.y), true);
            FillSilhouette(Col(s.ghostColor));
            dl->PopClipRect();
        }

        if (s.drawFill && frac > 0.001f) {
            dl->PushClipRect(pos, ImVec2(fillEdge, bMax.y), true);

            uint8_t fillA = A((s.fillColor >> 24) & 0xFF);
            int v0 = dl->VtxBuffer.Size;
            FillSilhouette(IM_COL32(255, 255, 255, fillA));
            int v1 = dl->VtxBuffer.Size;
            ShadeBarFill(dl, v0, v1, pos, bMax, s.fillColor, s.fillColor2, s.duotone);

            if (s.edgeLight && !InGap(fillEdge)) {
                float d = (std::min)(s.edgeLightSize, s.width * frac);
                if (d > 2.0f) {
                    auto lift = [](uint8_t c) { return static_cast<uint8_t>((std::min)(255.0f, c * 0.35f + 190.0f)); };
                    const uint8_t lR = lift(fR), lG = lift(fG), lB = lift(fB);
                    const float x0 = fillEdge - d, x1 = fillEdge;
                    auto RampCol = [&](float x) {
                        float t = std::clamp((x - x0) / d, 0.0f, 1.0f);
                        return IM_COL32(lR, lG, lB, A(static_cast<int>(95.0f * t)));
                    };
                    float bp[12]; int nbp = 0;
                    bp[nbp++] = x0; bp[nbp++] = x1;
                    auto AddBp = [&](float x) {
                        if (nbp < 12 && x > x0 + 0.25f && x < x1 - 0.25f) bp[nbp++] = x;
                    };
                    if (shaped) {
                        AddBp(pos.x + s.height * 0.6f);
                        AddBp(bMax.x - s.height * 0.6f);
                        AddBp(pos.x + s.height * 0.5f);
                        AddBp(bMax.x - s.height * 0.5f);
                        AddBp(pos.x + s.height * 0.4f);
                        AddBp(bMax.x - s.height * 0.4f);
                    } else if (r > 0.5f) {
                        for (int i = 0; i <= 6; ++i) {
                            AddBp(pos.x + r * i / 6.0f);
                            AddBp(bMax.x - r + r * i / 6.0f);
                        }
                    }
                    std::sort(bp, bp + nbp);
                    auto PadAt = [&](float x) {
                        return (!shaped && r > 0.5f && (x < pos.x + r + 0.5f || x > bMax.x - r - 0.5f))
                            ? 1.25f : 1.0f;
                    };
                    const ImVec2 uvWhite = dl->_Data->TexUvWhitePixel;
                    for (int i = 0; i + 1 < nbp; ++i) {
                        float xa = bp[i], xb = bp[i + 1];
                        if (xb - xa < 0.25f) continue;
                        float ta, ba, tb, bb;
                        BarSpanAtX(xa, pos, bMax, s.leftEnd, s.rightEnd, r, ta, ba);
                        BarSpanAtX(xb, pos, bMax, s.leftEnd, s.rightEnd, r, tb, bb);
                        const float padA = PadAt(xa), padB = PadAt(xb);
                        ta += padA; ba -= padA; tb += padB; bb -= padB;
                        if (ba - ta < 0.5f && bb - tb < 0.5f) continue;
                        ta = (std::min)(ta, ba); tb = (std::min)(tb, bb);
                        ImU32 colA = RampCol(xa), colB = RampCol(xb);
                        dl->PrimReserve(6, 4);
                        ImDrawIdx base = static_cast<ImDrawIdx>(dl->_VtxCurrentIdx);
                        dl->PrimWriteIdx(base); dl->PrimWriteIdx(static_cast<ImDrawIdx>(base + 1)); dl->PrimWriteIdx(static_cast<ImDrawIdx>(base + 2));
                        dl->PrimWriteIdx(base); dl->PrimWriteIdx(static_cast<ImDrawIdx>(base + 2)); dl->PrimWriteIdx(static_cast<ImDrawIdx>(base + 3));
                        dl->PrimWriteVtx(ImVec2(xa, ta), uvWhite, colA);
                        dl->PrimWriteVtx(ImVec2(xb, tb), uvWhite, colB);
                        dl->PrimWriteVtx(ImVec2(xb, bb), uvWhite, colB);
                        dl->PrimWriteVtx(ImVec2(xa, ba), uvWhite, colA);
                    }
                }
            }

            if (s.gloss) {
                dl->PushClipRect(pos, ImVec2(fillEdge, pos.y + s.height * 0.45f), true);
                FillSilhouette(IM_COL32(255, 255, 255, A(34)));
                dl->PopClipRect();
            }

            if (s.highlight) {
                dl->PushClipRect(ImVec2(pos.x, pos.y + 1.0f), ImVec2(fillEdge, pos.y + 2.0f), true);
                FillSilhouette(IM_COL32(255, 255, 255, A(30)));
                dl->PopClipRect();
            }
            dl->PopClipRect();

            if (s.tip && frac < 0.995f && !InGap(fillEdge)) {
                dl->PushClipRect(ImVec2(fillEdge - 4.0f, pos.y + 1.0f), ImVec2(fillEdge + 2.0f, bMax.y - 1.0f), true);
                FillSilhouette(IM_COL32(fR, fG, fB, A(55)));
                dl->PopClipRect();
                dl->PushClipRect(ImVec2(fillEdge - 2.0f, pos.y + 1.0f), ImVec2(fillEdge, bMax.y - 1.0f), true);
                FillSilhouette(IM_COL32(255, 255, 255, A(150)));
                dl->PopClipRect();
            }
        }

        if (segEff > 1) {
            if (segGapEff <= 0.0f) {
                for (int i = 1; i < segEff; ++i) {
                    float x = pos.x + s.width * (static_cast<float>(i) / segEff);
                    dl->AddLine(ImVec2(x, pos.y + 1.0f), ImVec2(x, bMax.y - 1.0f), IM_COL32(0, 0, 0, A(110)), 2.0f);
                }
            } else {
                const ImU32 gapCol = s.drawTrack
                    ? Col(s.trackTop) | IM_COL32_A_MASK
                    : IM_COL32(0, 0, 0, A(200));
                const float gy0 = pos.y + 1.0f, gy1 = bMax.y - 1.0f;
                for (int i = 1; i < segEff; ++i) {
                    float gx = pos.x + s.width * (static_cast<float>(i) / segEff);
                    float x0 = gx - segGapEff * 0.5f, x1 = gx + segGapEff * 0.5f;
                    dl->AddRectFilled(ImVec2(x0, gy0), ImVec2(x1, gy1), gapCol);
                    if (segGapEff >= 2.0f) {
                        dl->AddLine(ImVec2(x0, gy0), ImVec2(x0, gy1), IM_COL32(0, 0, 0, A(140)), 1.0f);
                        dl->AddLine(ImVec2(x1, gy0), ImVec2(x1, gy1), IM_COL32(255, 255, 255, A(40)), 1.0f);
                    }
                }
            }
        }

        if (s.innerBorderWidth > 0.0f && (s.innerBorderColor & 0xFF000000u)) {
            float inset = s.outerBorderWidth > 0.0f ? s.outerBorderWidth + 1.0f : 1.0f;
            StrokeSilhouette(Col(s.innerBorderColor), inset, s.innerBorderWidth);
        }
        if (s.outerBorderWidth > 0.0f && (s.outerBorderColor & 0xFF000000u)) {
            auto Tone = [&](float mul, float add) {
                auto ch = [&](int sh) {
                    return static_cast<uint8_t>(std::clamp(
                        ((s.outerBorderColor >> sh) & 0xFF) * mul + add, 0.0f, 255.0f));
                };
                float aF = ((s.outerBorderColor >> 24) & 0xFF) * s.alpha;
                return IM_COL32(ch(16), ch(8), ch(0),
                                static_cast<uint8_t>(std::clamp(aF, 0.0f, 255.0f)));
            };
            StrokeSilhouette(Col(s.outerBorderColor), 0.0f, s.outerBorderWidth);
            const float cy = pos.y + s.height * 0.5f;
            dl->PushClipRect(ImVec2(pos.x - 2.0f, pos.y - 2.0f), ImVec2(bMax.x + 2.0f, cy), true);
            StrokeSilhouette(Tone(1.45f, 34.0f), 0.0f, s.outerBorderWidth);
            dl->PopClipRect();
            dl->PushClipRect(ImVec2(pos.x - 2.0f, cy), ImVec2(bMax.x + 2.0f, bMax.y + 2.0f), true);
            StrokeSilhouette(Tone(0.55f, 0.0f), 0.0f, s.outerBorderWidth);
            dl->PopClipRect();
        }
    }

    void DrawTimerBarV(ImDrawList* dl, const ImVec2& pos, const TimerBarStyle& s)
    {
        const ImVec2 bMax(pos.x + s.width, pos.y + s.height);
        const float  frac = std::clamp(s.fraction, 0.0f, 1.0f);
        const float  r    = s.rounding < 0.0f ? s.width * 0.5f
            : (std::min)(s.rounding, (std::min)(s.width, s.height) * 0.5f);
        const float  fillTop = bMax.y - s.height * frac;

        auto A = [&](int base) { return static_cast<uint8_t>(std::clamp(base * s.alpha, 0.0f, 255.0f)); };
        const uint8_t fR = (s.fillColor >> 16) & 0xFF, fG = (s.fillColor >> 8) & 0xFF, fB = s.fillColor & 0xFF;

        if (s.glow && frac > 0.003f) {
            const float expands[] = { 2.0f, 4.0f, 6.0f };
            const int   alphas[]  = { 45, 30, 15 };
            for (int i = 2; i >= 0; --i)
                dl->AddRectFilled(ImVec2(pos.x - expands[i], fillTop - expands[i]),
                                  ImVec2(bMax.x + expands[i], bMax.y + expands[i]),
                                  IM_COL32(fR, fG, fB, A(alphas[i])), r + expands[i]);
        }

        if (s.shadow) {
            for (int i = 2; i >= 1; --i) {
                float grow = static_cast<float>(i);
                dl->AddRectFilled(ImVec2(pos.x - grow + 1.0f, pos.y - grow + 1.0f),
                                  ImVec2(bMax.x + grow + 1.0f, bMax.y + grow + 1.0f),
                                  IM_COL32(0, 0, 0, A(30)), r + grow);
            }
        }

        if (s.drawTrack) {
            int v0 = dl->VtxBuffer.Size;
            dl->AddRectFilled(pos, bMax, IM_COL32(255, 255, 255, A((s.trackTop >> 24) & 0xFF)), r);
            int v1 = dl->VtxBuffer.Size;
            auto Col = [&](std::uint32_t argb) {
                return IM_COL32((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF, 255);
            };
            ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, v0, v1,
                pos, ImVec2(bMax.x, pos.y), Col(s.trackTop), Col(s.trackBottom));
            dl->PushClipRect(ImVec2(pos.x, pos.y + 1.0f), ImVec2(bMax.x, pos.y + 2.5f), true);
            dl->AddRectFilled(pos, bMax, IM_COL32(0, 0, 0, A(90)), r);
            dl->PopClipRect();
            dl->PushClipRect(ImVec2(pos.x, bMax.y - 1.5f), ImVec2(bMax.x, bMax.y - 0.5f), true);
            dl->AddRectFilled(pos, bMax, IM_COL32(255, 255, 255, A(14)), r);
            dl->PopClipRect();
        }

        if (frac > 0.003f) {
            dl->PushClipRect(ImVec2(pos.x, fillTop), ImVec2(bMax.x, bMax.y), true);

            uint8_t fillA = A((s.fillColor >> 24) & 0xFF);
            int v0 = dl->VtxBuffer.Size;
            dl->AddRectFilled(pos, bMax, IM_COL32(255, 255, 255, fillA), r);
            int v1 = dl->VtxBuffer.Size;
            if (s.duotone) {
                float span = (std::max)(1.0f, bMax.y - fillTop);
                ImDrawVert* vtx = dl->VtxBuffer.Data;
                for (int i = v0; i < v1; ++i) {
                    float ty  = std::clamp((vtx[i].pos.y - fillTop) / span, 0.0f, 1.0f);
                    float mul = 1.28f + (0.52f - 1.28f) * ty;
                    float add = 26.0f * (1.0f - ty);
                    ImU32 keepA = vtx[i].col & IM_COL32_A_MASK;
                    vtx[i].col = keepA | IM_COL32(
                        static_cast<uint8_t>(std::clamp(fR * mul + add, 0.0f, 255.0f)),
                        static_cast<uint8_t>(std::clamp(fG * mul + add, 0.0f, 255.0f)),
                        static_cast<uint8_t>(std::clamp(fB * mul + add, 0.0f, 255.0f)), 0);
                }
            } else if (fillA) {
                ImDrawVert* vtx = dl->VtxBuffer.Data;
                for (int i = v0; i < v1; ++i)
                    vtx[i].col = (vtx[i].col & IM_COL32_A_MASK) | IM_COL32(fR, fG, fB, 0);
            }

            if (s.edgeLight) {
                float d = (std::min)(s.edgeLightSize, s.height * frac);
                if (d > 2.0f) {
                    auto lift = [](uint8_t c) { return static_cast<uint8_t>((std::min)(255.0f, c * 0.35f + 190.0f)); };
                    const uint8_t lR = lift(fR), lG = lift(fG), lB = lift(fB);
                    float y0 = (std::max)(fillTop, pos.y + r);
                    float y1 = (std::min)(fillTop + d, bMax.y - r);
                    if (y1 - y0 > 0.5f) {
                        auto RampA = [&](float y) {
                            float t = 1.0f - std::clamp((y - fillTop) / d, 0.0f, 1.0f);
                            return A(static_cast<int>(95.0f * t));
                        };
                        const ImVec2 uvWhite = dl->_Data->TexUvWhitePixel;
                        const float  xi = pos.x + 1.0f, xa = bMax.x - 1.0f;
                        ImU32 cTop = IM_COL32(lR, lG, lB, RampA(y0));
                        ImU32 cBot = IM_COL32(lR, lG, lB, RampA(y1));
                        dl->PrimReserve(6, 4);
                        ImDrawIdx base = static_cast<ImDrawIdx>(dl->_VtxCurrentIdx);
                        dl->PrimWriteIdx(base); dl->PrimWriteIdx(static_cast<ImDrawIdx>(base + 1)); dl->PrimWriteIdx(static_cast<ImDrawIdx>(base + 2));
                        dl->PrimWriteIdx(base); dl->PrimWriteIdx(static_cast<ImDrawIdx>(base + 2)); dl->PrimWriteIdx(static_cast<ImDrawIdx>(base + 3));
                        dl->PrimWriteVtx(ImVec2(xi, y0), uvWhite, cTop);
                        dl->PrimWriteVtx(ImVec2(xa, y0), uvWhite, cTop);
                        dl->PrimWriteVtx(ImVec2(xa, y1), uvWhite, cBot);
                        dl->PrimWriteVtx(ImVec2(xi, y1), uvWhite, cBot);
                    }
                }
            }

            dl->PushClipRect(ImVec2(pos.x + 1.0f, fillTop), ImVec2(pos.x + 2.0f, bMax.y), true);
            dl->AddRectFilled(pos, bMax, IM_COL32(255, 255, 255, A(30)), r);
            dl->PopClipRect();

            dl->PopClipRect();

            if (s.tip && frac < 0.995f) {
                dl->PushClipRect(ImVec2(pos.x + 0.5f, fillTop - 2.0f), ImVec2(bMax.x - 0.5f, fillTop + 4.0f), true);
                dl->AddRectFilled(pos, bMax, IM_COL32(fR, fG, fB, A(55)), r);
                dl->PopClipRect();
                dl->PushClipRect(ImVec2(pos.x + 0.5f, fillTop), ImVec2(bMax.x - 0.5f, fillTop + 1.5f), true);
                dl->AddRectFilled(pos, bMax, IM_COL32(255, 255, 255, A(150)), r);
                dl->PopClipRect();
            }
        }
    }

    void DrawTimerArc(ImDrawList* dl, const ImVec2& center, const TimerArcStyle& s)
    {
        const float frac  = std::clamp(s.fraction, 0.0f, 1.0f);
        const float thick = (std::max)(s.thickness, s.roundCaps ? 0.5f : 1.0f);
        const float sweep = std::clamp(s.sweepAngle, 1.0f, 360.0f) * IM_PI / 180.0f;
        const float trackStart = s.startAngle * IM_PI / 180.0f;
        const float trackEnd = trackStart + sweep;
        const float a0 = trackStart + (s.centeredFill ? sweep * (1.0f - frac) * 0.5f : 0.0f);
        const float a1 = a0 + sweep * frac;
        auto StrokeArc = [&](const ImVec2& c, float radius, float start, float end, ImU32 color,
                             float width, bool closeFull) {
            if (radius <= 0.0f || end <= start) return;
            if (closeFull && end - start >= 2.0f * IM_PI - 0.0001f) {
                dl->AddCircle(c, radius, color, 48, width);
                return;
            }
            dl->PathArcTo(c, radius, start, end, 48);
            dl->PathStroke(color, 0, width);
            if (s.roundCaps) {
                dl->AddCircleFilled(ImVec2(c.x + radius * std::cos(start), c.y + radius * std::sin(start)), width * 0.5f, color);
                dl->AddCircleFilled(ImVec2(c.x + radius * std::cos(end), c.y + radius * std::sin(end)), width * 0.5f, color);
            }
        };

        auto A = [&](int base) { return static_cast<uint8_t>(std::clamp(base * s.alpha, 0.0f, 255.0f)); };
        auto Col = [&](std::uint32_t argb) {
            float aF = ((argb >> 24) & 0xFF) * s.alpha;
            return IM_COL32((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF,
                            static_cast<uint8_t>(std::clamp(aF, 0.0f, 255.0f)));
        };
        const uint8_t fR = (s.fillColor >> 16) & 0xFF, fG = (s.fillColor >> 8) & 0xFF, fB = s.fillColor & 0xFF;

        if (s.shadow)
            StrokeArc(ImVec2(center.x + 1.0f, center.y + 2.0f), s.radius, trackStart, trackEnd,
                      IM_COL32(0, 0, 0, A(40)), thick, true);

        if (s.drawTrack) {
            StrokeArc(center, s.radius, trackStart, trackEnd, Col(s.trackColor), thick, true);
            if (s.trackDepth) {
                StrokeArc(center, s.radius + thick * 0.5f, trackStart, trackEnd, IM_COL32(0, 0, 0, A(70)), 1.0f, true);
                if (s.radius - thick * 0.5f > 1.0f)
                    StrokeArc(center, s.radius - thick * 0.5f, trackStart, trackEnd, IM_COL32(0, 0, 0, A(70)), 1.0f, true);
            }
        }

        if (frac <= 0.003f) return;

        if (s.glow) {
            StrokeArc(center, s.radius, a0, a1, IM_COL32(fR, fG, fB, A(20)), thick + 5.0f, true);
            StrokeArc(center, s.radius, a0, a1, IM_COL32(fR, fG, fB, A(38)), thick + 2.0f, true);
        }

        uint8_t fillA = A((s.fillColor >> 24) & 0xFF);
        int v0 = dl->VtxBuffer.Size;
        StrokeArc(center, s.radius, a0, a1, IM_COL32(255, 255, 255, fillA), thick, s.roundCaps);
        int v1 = dl->VtxBuffer.Size;
        {
            auto lift = [](uint8_t c) { return (std::min)(255.0f, c * 0.35f + 190.0f); };
            const float lR = lift(fR), lG = lift(fG), lB = lift(fB);
            const float span = (std::max)(a1 - a0, 0.001f);
            const float edgeSpan = (std::min)(1.2f, span * 0.35f);
            ImDrawVert* vtx = dl->VtxBuffer.Data;
            for (int i = v0; i < v1; ++i) {
                float a = std::atan2(vtx[i].pos.y - center.y, vtx[i].pos.x - center.x);
                // Rotation may cross atan2's +/-pi seam. Unwrap near the fill midpoint.
                a += std::round(((a0 + a1) * 0.5f - a) / (2.0f * IM_PI)) * (2.0f * IM_PI);
                const float edgeDistance = s.centeredFill ? (std::max)(0.0f, (std::min)(a - a0, a1 - a)) : a1 - a;
                float t = std::clamp(edgeDistance / (s.centeredFill ? span * 0.5f : span), 0.0f, 1.0f);
                float r = fR, g = fG, b = fB;
                if (s.duotone) {
                    float mul = 1.25f + (0.62f - 1.25f) * t;
                    float add = 24.0f * (1.0f - t);
                    r = r * mul + add; g = g * mul + add; b = b * mul + add;
                }
                if (s.edgeLight && edgeSpan > 0.01f) {
                    float w = std::clamp(1.0f - edgeDistance / edgeSpan, 0.0f, 1.0f) * 0.85f;
                    r += (lR - r) * w; g += (lG - g) * w; b += (lB - b) * w;
                }
                ImU32 keepA = vtx[i].col & IM_COL32_A_MASK;
                vtx[i].col = keepA | IM_COL32(
                    static_cast<uint8_t>(std::clamp(r, 0.0f, 255.0f)),
                    static_cast<uint8_t>(std::clamp(g, 0.0f, 255.0f)),
                    static_cast<uint8_t>(std::clamp(b, 0.0f, 255.0f)), 0);
            }
        }

        if (s.tip && frac < 0.985f) {
            ImVec2 tipPos(center.x + s.radius * std::cos(a1), center.y + s.radius * std::sin(a1));
            dl->AddCircleFilled(tipPos, thick * 1.6f, IM_COL32(fR, fG, fB, A(70)));
            dl->AddCircleFilled(tipPos, thick * 0.6f, IM_COL32(255, 255, 255, A(190)));
        }
    }

    void DrawResourceRing(ImDrawList* dl, const ImVec2& center, const ResourceRingStyle& s)
    {
        if (!s.segments || s.segmentCount <= 0) return;
        const float thick = (std::max)(s.thickness, 1.0f);
        constexpr float kPi = 3.14159265f;
        const float gapRad = (s.gapDeg * kPi / 180.0f);
        const float startRad = s.startAngleDeg * kPi / 180.0f;

        if (s.shadowColor & 0xFF000000u)
            dl->AddCircle(center, s.radius, Argb(s.shadowColor), 48, thick + 2.0f);

        // 2) Segment layout: first segment centered on startAngleDeg, remaining
        float cursor = startRad - (s.segments[0].share * kPi);
        for (int i = 0; i < s.segmentCount; ++i) {
            const RingSegment& seg = s.segments[i];
            const float span = seg.share * 2.0f * kPi;
            const float a0 = cursor + gapRad * 0.5f;
            const float a1 = cursor + span - gapRad * 0.5f;
            cursor += span;
            if (a1 <= a0) continue;

            const uint8_t cR = (seg.color >> 16) & 0xFF, cG = (seg.color >> 8) & 0xFF, cB = seg.color & 0xFF;
            const uint8_t cA = (seg.color >> 24) & 0xFF;
            const int nSeg = (std::max)(6, static_cast<int>(48.0f * (a1 - a0) / (2.0f * kPi)));

            dl->PathArcTo(center, s.radius, a0, a1, nSeg);
            dl->PathStroke(Argb(s.trackColor), 0, thick);

            const float frac = std::clamp(seg.fraction, 0.0f, 1.0f);
            if (frac <= 0.003f) continue;
            const float fillEnd = a0 + (a1 - a0) * frac;

            auto dark  = [](uint8_t c) { return static_cast<uint8_t>(c * 0.6f); };
            auto bright = [](uint8_t c) { return static_cast<uint8_t>((std::min)(255.0f, c * 1.25f)); };
            dl->PathArcTo(center, s.radius, a0, fillEnd, nSeg);
            dl->PathStroke(IM_COL32(dark(cR), dark(cG), dark(cB), cA), 0, thick);
            dl->PathArcTo(center, s.radius, a0, fillEnd, nSeg);
            dl->PathStroke(IM_COL32(bright(cR), bright(cG), bright(cB), cA), 0, thick * 0.55f);

            if (s.hotTip && frac < 0.997f) {
                ImVec2 tipPos(center.x + s.radius * std::cos(fillEnd), center.y + s.radius * std::sin(fillEnd));
                dl->AddCircleFilled(tipPos, thick * 0.55f, IM_COL32(bright(cR), bright(cG), bright(cB), cA));
            }

            if (s.gloss) {
                const float glossA0 = (std::max)(a0, -kPi);
                const float glossA1 = (std::min)(fillEnd, 0.0f);
                if (glossA1 > glossA0) {
                    dl->PathArcTo(center, s.radius + thick * 0.5f, glossA0, glossA1, nSeg);
                    dl->PathStroke(IM_COL32(255, 255, 255, 0x2E), 0, thick * 0.3f);
                }
            }
        }
    }

    static void PortraitSpanAtY(float y, const ImVec2& bMin, const ImVec2& bMax,
                                int shape, float r, float& left, float& right)
    {
        left = bMin.x; right = bMax.x;
        if (shape == 1) {
            const float cy = (bMin.y + bMax.y) * 0.5f;
            const float halfH = (bMax.y - bMin.y) * 0.5f;
            const float halfW = (bMax.x - bMin.x) * 0.5f;
            const float cx = (bMin.x + bMax.x) * 0.5f;
            float t = 1.0f - std::fabs(y - cy) / (std::max)(1.0f, halfH);
            t = std::clamp(t, 0.0f, 1.0f);
            left  = cx - halfW * t;
            right = cx + halfW * t;
        } else if (r > 0.5f) {
            if (y < bMin.y + r) {
                float dy = bMin.y + r - y;
                float inset = r - std::sqrt((std::max)(0.0f, r * r - dy * dy));
                left += inset; right -= inset;
            }
            if (y > bMax.y - r) {
                float dy = y - (bMax.y - r);
                float inset = r - std::sqrt((std::max)(0.0f, r * r - dy * dy));
                left += inset; right -= inset;
            }
        }
    }

    static void PortraitVGradient(ImDrawList* dl, const ImVec2& bMin, const ImVec2& bMax,
                                  int shape, float r, float y0, float y1,
                                  ImU32 rgb, int a0, int a1)
    {
        if (y1 - y0 < 1.0f) return;
        float bp[132]; int nbp = 0;
        bp[nbp++] = y0; bp[nbp++] = y1;
        auto AddBp = [&](float y) {
            if (nbp < IM_ARRAYSIZE(bp) && y > y0 + 0.25f && y < y1 - 0.25f) bp[nbp++] = y;
        };
        if (shape == 1) {
            AddBp((bMin.y + bMax.y) * 0.5f);
        } else if (r > 0.5f) {
            // Uniform angles keep large circular portraits from exposing straight chords.
            const int slices = std::clamp(static_cast<int>(std::ceil(r / 3.0f)), 8, 64);
            for (int i = 0; i <= slices; ++i) {
                const float inset = r * (1.0f - std::cos(1.57079632679f * i / slices));
                AddBp(bMin.y + inset);
                AddBp(bMax.y - inset);
            }
        }
        std::sort(bp, bp + nbp);
        auto RampCol = [&](float y) {
            float t = std::clamp((y - y0) / (y1 - y0), 0.0f, 1.0f);
            int a = static_cast<int>(a0 + (a1 - a0) * t);
            return (rgb & ~IM_COL32_A_MASK) | (static_cast<ImU32>(std::clamp(a, 0, 255)) << IM_COL32_A_SHIFT);
        };
        const ImVec2 uvWhite = dl->_Data->TexUvWhitePixel;
        const float pad = 1.0f;
        for (int i = 0; i + 1 < nbp; ++i) {
            float ya = bp[i], yb = bp[i + 1];
            if (yb - ya < 0.25f) continue;
            float la, ra, lb, rb;
            PortraitSpanAtY(ya, bMin, bMax, shape, r, la, ra);
            PortraitSpanAtY(yb, bMin, bMax, shape, r, lb, rb);
            la += pad; ra -= pad; lb += pad; rb -= pad;
            if (ra - la < 0.5f && rb - lb < 0.5f) continue;
            la = (std::min)(la, ra); lb = (std::min)(lb, rb);
            ImU32 colA = RampCol(ya), colB = RampCol(yb);
            dl->PrimReserve(6, 4);
            ImDrawIdx base = static_cast<ImDrawIdx>(dl->_VtxCurrentIdx);
            dl->PrimWriteIdx(base); dl->PrimWriteIdx(static_cast<ImDrawIdx>(base + 1)); dl->PrimWriteIdx(static_cast<ImDrawIdx>(base + 2));
            dl->PrimWriteIdx(base); dl->PrimWriteIdx(static_cast<ImDrawIdx>(base + 2)); dl->PrimWriteIdx(static_cast<ImDrawIdx>(base + 3));
            dl->PrimWriteVtx(ImVec2(la, ya), uvWhite, colA);
            dl->PrimWriteVtx(ImVec2(ra, ya), uvWhite, colA);
            dl->PrimWriteVtx(ImVec2(rb, yb), uvWhite, colB);
            dl->PrimWriteVtx(ImVec2(lb, yb), uvWhite, colB);
        }
    }

    static void RadialGlow(ImDrawList* dl, const ImVec2& center, float radius, ImU32 rgb, int aCenter)
    {
        constexpr int kSegs = 24;
        const ImVec2 uvWhite = dl->_Data->TexUvWhitePixel;
        ImU32 cCol = (rgb & ~IM_COL32_A_MASK) | (static_cast<ImU32>(std::clamp(aCenter, 0, 255)) << IM_COL32_A_SHIFT);
        ImU32 rCol = rgb & ~IM_COL32_A_MASK;
        dl->PrimReserve(kSegs * 3, kSegs + 2);
        ImDrawIdx base = static_cast<ImDrawIdx>(dl->_VtxCurrentIdx);
        dl->PrimWriteVtx(center, uvWhite, cCol);
        for (int i = 0; i <= kSegs; ++i) {
            float a = 6.2831853f * static_cast<float>(i) / kSegs;
            dl->PrimWriteVtx(ImVec2(center.x + std::cos(a) * radius, center.y + std::sin(a) * radius),
                             uvWhite, rCol);
        }
        for (int i = 0; i < kSegs; ++i) {
            dl->PrimWriteIdx(base);
            dl->PrimWriteIdx(static_cast<ImDrawIdx>(base + 1 + i));
            dl->PrimWriteIdx(static_cast<ImDrawIdx>(base + 2 + i));
        }
    }

    static void CirclePortraitGlow(ImDrawList* dl, const ImVec2& center, float glowRadius,
                                   const ImVec2& circleCenter, float circleRadius, ImU32 rgb, int alpha)
    {
        constexpr int segments = 64;
        const ImVec2 uv = dl->_Data->TexUvWhitePixel;
        const ImVec2 offset(center.x - circleCenter.x, center.y - circleCenter.y);
        const float offsetSquared = offset.x * offset.x + offset.y * offset.y;
        dl->PrimReserve(segments * 3, segments + 2);
        const ImDrawIdx base = static_cast<ImDrawIdx>(dl->_VtxCurrentIdx);
        dl->PrimWriteVtx(center, uv, (rgb & ~IM_COL32_A_MASK) | (static_cast<ImU32>(alpha) << IM_COL32_A_SHIFT));
        for (int i = 0; i <= segments; ++i) {
            const float angle = 6.2831853f * i / segments;
            const ImVec2 direction(std::cos(angle), std::sin(angle));
            const float dot = offset.x * direction.x + offset.y * direction.y;
            // Clip each ray to the image circle, preserving the glow's radial falloff.
            const float boundary = -dot + std::sqrt((std::max)(0.0f, dot * dot + circleRadius * circleRadius - offsetSquared));
            const float distance = (std::min)(glowRadius, boundary);
            const int edgeAlpha = static_cast<int>(alpha * (1.0f - distance / glowRadius));
            dl->PrimWriteVtx(ImVec2(center.x + direction.x * distance, center.y + direction.y * distance), uv,
                (rgb & ~IM_COL32_A_MASK) | (static_cast<ImU32>(edgeAlpha) << IM_COL32_A_SHIFT));
        }
        for (int i = 0; i < segments; ++i) {
            dl->PrimWriteIdx(base);
            dl->PrimWriteIdx(static_cast<ImDrawIdx>(base + i + 1));
            dl->PrimWriteIdx(static_cast<ImDrawIdx>(base + i + 2));
        }
    }

    void DrawPortraitBackHalo(ImDrawList* dl, const ImVec2& pMin, const ImVec2& pMax,
                              std::uint32_t rgb)
    {
        const float h = pMax.y - pMin.y;
        const ImVec2 c((pMin.x + pMax.x) * 0.5f, (pMin.y + pMax.y) * 0.5f);
        // Only this under-image glow may escape the child; portrait and overlays keep their clips.
        dl->PushClipRectFullScreen();
        RadialGlow(dl, c, h * 0.72f,
                   IM_COL32((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF, 0), 60);
        dl->PopClipRect();
    }

    void DrawPortraitFx(ImDrawList* dl, const ImVec2& pMin, const ImVec2& pMax,
                        int shape, float rounding, const PortraitFx& fx)
    {
        const float h = pMax.y - pMin.y;
        const float w = pMax.x - pMin.x;
        if (h < 8.0f || w < 8.0f) return;
        const float r = (shape == 1) ? 0.0f : (std::min)(rounding, (std::min)(w, h) * 0.5f);
        auto SA = [&](int base) {
            return std::clamp(static_cast<int>(base * fx.strength), 0, 255);
        };

        if (fx.backlight) {
            const std::uint32_t rgb = fx.backlightColor;
            dl->PushClipRect(pMin, pMax, true);
            const bool circle = shape != 1 && std::fabs(w - h) < 0.5f && r >= h * 0.5f - 0.5f;
            if (circle) {
                static float lastGlowSize = -1.0f;
                if (lastGlowSize != h) {
                    logger::debug("[PortraitFX] Circle backlight: size={}, imageRadius={}, glowRadius={}", h, h * 0.5f - 1.0f, h * 0.62f);
                    lastGlowSize = h;
                }
            }
            if (circle)
                CirclePortraitGlow(dl, ImVec2((pMin.x + pMax.x) * 0.5f, pMin.y + h * 0.38f), h * 0.62f,
                    ImVec2((pMin.x + pMax.x) * 0.5f, (pMin.y + pMax.y) * 0.5f), h * 0.5f - 1.0f,
                    IM_COL32((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF, 0), SA(58));
            else
                RadialGlow(dl, ImVec2((pMin.x + pMax.x) * 0.5f, pMin.y + h * 0.38f),
                       h * 0.62f,
                       IM_COL32((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF, 0), SA(58));
            dl->PopClipRect();
        }

        if (fx.topLight)
            PortraitVGradient(dl, pMin, pMax, shape, r, pMin.y, pMin.y + h * 0.26f,
                              IM_COL32(255, 250, 240, 0), SA(42), 0);

        if (fx.scrim)
            PortraitVGradient(dl, pMin, pMax, shape, r, pMax.y - h * 0.36f, pMax.y,
                              IM_COL32(8, 6, 10, 0), 0, SA(96));

        if (fx.vignette) {
            const float th = (std::max)(2.0f, h * 0.06f);
            ImU32 col = IM_COL32(0, 0, 0, SA(46));
            if (shape == 1) {
                const float cx = (pMin.x + pMax.x) * 0.5f, cy = (pMin.y + pMax.y) * 0.5f;
                const float in = th * 0.5f + 1.0f;
                dl->PathLineTo(ImVec2(cx, pMin.y + in));
                dl->PathLineTo(ImVec2(pMax.x - in, cy));
                dl->PathLineTo(ImVec2(cx, pMax.y - in));
                dl->PathLineTo(ImVec2(pMin.x + in, cy));
                dl->PathLineTo(ImVec2(cx, pMin.y + in));
                dl->PathStroke(col, 0, th);
            } else {
                dl->AddRect(ImVec2(pMin.x + 1.5f, pMin.y + 1.5f), ImVec2(pMax.x - 1.5f, pMax.y - 1.5f),
                            col, (std::max)(r - 1.5f, 0.0f), 0, th);
            }
        }

        if (fx.rim && shape != 1) {
            dl->PushClipRect(ImVec2(pMin.x, pMin.y + 1.0f), ImVec2(pMax.x, pMin.y + 2.0f), true);
            dl->AddRectFilled(pMin, pMax, IM_COL32(255, 255, 255, SA(34)), r);
            dl->PopClipRect();
        }
    }

    float TickBarValue(std::uint64_t id, float target)
    {
        struct State { float value; std::chrono::steady_clock::time_point last; };
        static std::unordered_map<std::uint64_t, State> s_Values;

        const auto now = std::chrono::steady_clock::now();
        auto [it, inserted] = s_Values.try_emplace(id, State{ target, now });
        State& st = it->second;
        if (inserted) return st.value;

        float dt = std::chrono::duration<float>(now - st.last).count();
        st.last = now;
        dt = std::clamp(dt, 0.0f, 0.1f);

        const float k = (target < st.value) ? 9.0f : 20.0f;
        st.value += (target - st.value) * (1.0f - std::exp(-k * dt));
        if (std::fabs(target - st.value) < 0.002f) st.value = target;
        return st.value;
    }

}
