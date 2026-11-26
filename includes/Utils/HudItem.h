#pragma once

#include "HUDDraw.h"
#include "HUDFlow.h"
#include "../UIAnimation.h"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <vector>

namespace HudItem {

    enum class Status : int { Normal = 0, Warning = 1, Bad = 2, Good = 3 };

    struct Content {
        ImTextureID icon = 0;
        const char* label = "";
        const char* description = "";
        int         count = -1;
        const char* value = "";
        Status      status = Status::Normal;
        const char* message = "";
        float       messageProgress = 0.0f;
        float       alpha = 1.0f;
        std::uint32_t color = 0;
    };

    struct Style {
        ImFont* font = nullptr;
        float   fontSize = 0.0f;
        float   descriptionScale = 0.8f;
        float   iconSize = 24.0f;
        float   discSize = 40.0f;
        float   stripMinWidth = 0.0f;
        float   padX = 6.0f;
        float   padY = 4.0f;
        float   gap = 6.0f;
        bool    showDescription = false;
        bool    showCount = true;
        float   alpha = 1.0f;
        bool    pulseBad = true;

        std::uint32_t normalColor = 0xFF8FB8E0;
        std::uint32_t warningColor = 0xFFE0B050;
        std::uint32_t badColor = 0xFFE05A50;
        std::uint32_t goodColor = 0xFF3CD25A;
        std::uint32_t descriptionColor = 0xFFAAAAAA;
        std::uint32_t messageColor = 0xFFEEEEEE;

        std::uint32_t discColor = 0xFF0C0C0C;
        std::uint32_t discBorderColor = 0xFF2D2626;
        float         discBorderWidth = 2.0f;
        float         discRounding = 0.5f;
        std::uint32_t stripColor = 0xB4141414;
        bool  pennant = false;
        HUDDraw::PennantDir clothDirection = HUDDraw::PennantDir::Down;
        bool  messageAtEnd = false;
        float clothWidth = 24.0f;
        float clothLength = 46.0f;
        HUDDraw::IconFx    iconFx{};
        HUDDraw::TextFx    textFx{};
        HUDDraw::BadgeStyle countBadge{};
        bool growLeft = false;
    };

    inline std::uint32_t StatusColor(Status status, const Style& style)
    {
        switch (status) {
        case Status::Warning: return style.warningColor;
        case Status::Bad:     return style.badColor;
        case Status::Good:    return style.goodColor;
        default:              return style.normalColor;
        }
    }

    inline std::uint32_t FadeArgb(std::uint32_t argb, float alpha)
    {
        const float a = static_cast<float>((argb >> 24) & 0xFF) * std::clamp(alpha, 0.0f, 1.0f);
        return (static_cast<std::uint32_t>(a) << 24) | (argb & 0x00FFFFFF);
    }

    inline ImU32 ArgbToPacked(std::uint32_t argb)
    {
        return IM_COL32((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF, (argb >> 24) & 0xFF);
    }

    inline ImFont* FontOf(const Style& style) { return style.font ? style.font : ImGui::GetFont(); }
    inline float FontSizeOf(const Style& style) { return style.fontSize > 0.0f ? style.fontSize : ImGui::GetFontSize(); }

    inline bool HasText(const char* text) { return text && text[0]; }

    inline ImVec2 TextSize(const Style& style, float size, const char* text)
    {
        return HasText(text) ? FontOf(style)->CalcTextSizeA(size, FLT_MAX, 0.0f, text) : ImVec2(0.0f, 0.0f);
    }

    inline HUDDraw::BadgeStyle CountBadgeStyle(const Style& style)
    {
        HUDDraw::BadgeStyle badge = style.countBadge;
        ImFont* font = FontOf(style);
        badge.font = font;
        badge.fontScale = FontSizeOf(style) * 0.85f / font->LegacySize;
        badge.alpha *= style.alpha;
        return badge;
    }

    inline bool ShowsCount(const Content& item, const Style& style) { return style.showCount && (item.count >= 0 || HasText(item.value)); }

    inline void FormatCount(char (&text)[16], const Content& item)
    {
        if (HasText(item.value)) snprintf(text, sizeof(text), "%s", item.value);
        else snprintf(text, sizeof(text), "%d", item.count);
    }

    inline bool ShowsDescription(const Content& item, const Style& style) { return style.showDescription && HasText(item.description); }

    inline bool ShowsText(const Content& item, const Style& style)
    {
        return !style.pennant && (HasText(item.label) || ShowsDescription(item, style));
    }

    inline ImVec2 TextBlockSize(const Content& item, const Style& style)
    {
        const ImVec2 label = TextSize(style, FontSizeOf(style), item.label);
        const ImVec2 description = ShowsDescription(item, style)
            ? TextSize(style, FontSizeOf(style) * style.descriptionScale, item.description) : ImVec2(0.0f, 0.0f);
        return ImVec2((std::max)(label.x, description.x), label.y + description.y);
    }

    inline HUDDraw::PennantDir AutoClothDirection(HUDFlow::Corner corner, HUDFlow::Axis axis)
    {
        if (axis == HUDFlow::Axis::Vertical)
            return HUDFlow::IsLeft(corner) ? HUDDraw::PennantDir::Right : HUDDraw::PennantDir::Left;
        return HUDFlow::IsTop(corner) ? HUDDraw::PennantDir::Down : HUDDraw::PennantDir::Up;
    }

    inline ImVec2 PlateSize(const Style& style)
    {
        if (!style.pennant) return ImVec2(style.discSize, style.discSize);
        const float length = style.discSize * 0.5f + style.clothLength;
        const bool sideways = style.clothDirection == HUDDraw::PennantDir::Left || style.clothDirection == HUDDraw::PennantDir::Right;
        return sideways ? ImVec2(length, style.discSize) : ImVec2(style.discSize, length);
    }

    inline ImVec2 PlateCenter(ImVec2 pos, ImVec2 size, const Style& style)
    {
        const ImVec2 plate = PlateSize(style);
        const float radius = style.discSize * 0.5f;
        const float left = style.growLeft ? pos.x + size.x - plate.x : pos.x;
        const float top = pos.y + (size.y - plate.y) * 0.5f;
        ImVec2 center(left + radius, top + radius);
        if (style.pennant && style.clothDirection == HUDDraw::PennantDir::Left) center.x = left + plate.x - radius;
        if (style.pennant && style.clothDirection == HUDDraw::PennantDir::Up) center.y = top + plate.y - radius;
        return center;
    }

    inline ImVec2 Measure(const Content& item, const Style& style)
    {
        if (!ShowsText(item, style)) return PlateSize(style);
        const ImVec2 text = TextBlockSize(item, style);
        const float strip = (std::max)(text.x + style.padX * 2.0f, style.stripMinWidth);
        return ImVec2(style.discSize + strip, (std::max)(style.discSize, text.y + style.padY * 2.0f));
    }

    inline float Outward(float x, float distance, const Style& style)
    {
        return style.growLeft ? x - distance : x + distance;
    }

    inline float TextLeft(float fromX, float textWidth, const Style& style)
    {
        return style.growLeft ? fromX - textWidth : fromX;
    }

    inline void DrawFadeStrip(ImDrawList* dl, float fromX, float width, float midY, float height,
                              const Style& style, float alpha)
    {
        const float minX = style.growLeft ? fromX - width : fromX;
        HUDDraw::DrawStrip(dl, ImVec2(minX, midY - height * 0.5f), ImVec2(minX + width, midY + height * 0.5f),
                           HUDDraw::StripStyle_Fade, FadeArgb(style.stripColor, alpha), !style.growLeft);
    }

    struct ListResult {
        ImVec2 topLeft{ 0.0f, 0.0f };
        ImVec2 size{ 0.0f, 0.0f };
        std::vector<ImVec2> discCenters;
    };

    inline float MessageProgress(const Content& item)
    {
        return UIAnimation::EaseOut(std::clamp(item.messageProgress, 0.0f, 1.0f));
    }

    inline void DrawMessageText(ImDrawList* dl, ImVec2 pos, const Content& item, const Style& style, float alpha)
    {
        HUDDraw::TextFx fx = style.textFx;
        fx.font = FontOf(style);
        fx.fontSize = FontSizeOf(style);
        HUDDraw::DrawTextStyled(dl, pos, ArgbToPacked(FadeArgb(style.messageColor, alpha)), item.message, fx);
    }

    inline void DrawSideMessage(ImDrawList* dl, float startX, float midY, ImVec2 text, const Content& item,
                                const Style& style, float alpha)
    {
        const float stripWidth = text.x * 1.4f + style.padX * 2.0f;
        DrawFadeStrip(dl, startX, stripWidth, midY, text.y + style.padY * 2.0f, style, alpha);
        const float textX = TextLeft(Outward(startX, style.padX, style), text.x, style);
        DrawMessageText(dl, ImVec2(textX, midY - text.y * 0.5f), item, style, alpha);
    }

    inline void DrawCenteredMessage(ImDrawList* dl, float midX, float midY, ImVec2 text, const Content& item,
                                    const Style& style, float alpha)
    {
        const float half = text.x * 0.8f + style.padX;
        Style strip = style;
        strip.growLeft = false;
        DrawFadeStrip(dl, midX, half, midY, text.y + style.padY * 2.0f, strip, alpha);
        strip.growLeft = true;
        DrawFadeStrip(dl, midX, half, midY, text.y + style.padY * 2.0f, strip, alpha);
        DrawMessageText(dl, ImVec2(midX - text.x * 0.5f, midY - text.y * 0.5f), item, style, alpha);
    }

    inline void DrawMessages(ImDrawList* dl, const std::vector<Content>& items, const std::vector<ImVec2>& positions,
                             const std::vector<ImVec2>& sizes, const ListResult& list, const Style& style,
                             const HUDFlow::Options& flow)
    {
        if (positions.empty()) return;
        const float awayY = HUDFlow::AwayFromAnchor(flow.corner).y;
        const bool column = flow.axis == HUDFlow::Axis::Vertical;
        const bool beside = column != style.messageAtEnd;
        const float sideEdge = style.growLeft ? list.topLeft.x : list.topLeft.x + list.size.x;
        const float anchorEdge = style.growLeft ? list.topLeft.x + list.size.x : list.topLeft.x;
        const float pastEdge = awayY > 0.0f ? list.topLeft.y + list.size.y : list.topLeft.y;
        const float screenW = ImGui::GetIO().DisplaySize.x;

        struct Taken { float minX, maxX, midY; };
        std::vector<Taken> taken;
        for (std::size_t i = 0; i < positions.size(); ++i) {
            const Content& item = items[i];
            if (!HasText(item.message) || item.messageProgress <= 0.0f) continue;

            const ImVec2 text = TextSize(style, FontSizeOf(style), item.message);
            const float lineH = text.y + style.padY * 2.0f;
            const float progress = MessageProgress(item);
            const float alpha = progress * style.alpha * item.alpha;
            const std::size_t host = style.messageAtEnd ? positions.size() - 1 : i;

            float midX = positions[i].x + sizes[i].x * 0.5f;
            float midY = pastEdge + awayY * (style.gap + lineH * 0.5f);
            float minX = sideEdge, maxX = sideEdge;
            if (beside) {
                midY = PlateCenter(positions[host], sizes[host], style).y;
            } else if (style.messageAtEnd) {
                minX = maxX = anchorEdge;
            } else {
                const float half = text.x * 0.5f + style.padX;
                if (screenW > half * 2.0f) midX = std::clamp(midX, half, screenW - half);
                minX = midX - half;
                maxX = midX + half;
            }
            for (std::size_t k = 0; k < taken.size();) {
                const bool clear = maxX < taken[k].minX || minX > taken[k].maxX || std::fabs(midY - taken[k].midY) >= lineH - 0.5f;
                if (clear) { ++k; continue; }
                midY = taken[k].midY + awayY * lineH;
                k = 0;
            }
            taken.push_back({ minX, maxX, midY });

            const float slide = style.gap * 2.0f * (progress - 1.0f);
            if (beside)
                DrawSideMessage(dl, Outward(sideEdge, style.gap + slide, style), midY, text, item, style, alpha);
            else if (style.messageAtEnd)
                DrawSideMessage(dl, anchorEdge, midY + awayY * slide, text, item, style, alpha);
            else
                DrawCenteredMessage(dl, midX, midY + awayY * slide, text, item, style, alpha);
        }
    }

    inline void DrawLabel(ImDrawList* dl, ImVec2 discCenter, ImVec2 size, const Content& item, const Style& style)
    {
        const ImVec2 text = TextBlockSize(item, style);
        const float radius = style.discSize * 0.5f;
        DrawFadeStrip(dl, discCenter.x, size.x - radius, discCenter.y, text.y + style.padY * 2.0f, style, style.alpha);

        HUDDraw::TextFx fx = style.textFx;
        fx.font = FontOf(style);
        fx.fontSize = FontSizeOf(style);
        const float textX = Outward(discCenter.x, radius + style.padX, style);
        const float top = discCenter.y - text.y * 0.5f;
        const ImVec2 label = TextSize(style, fx.fontSize, item.label);
        if (HasText(item.label))
            HUDDraw::DrawTextStyled(dl, ImVec2(TextLeft(textX, label.x, style), top),
                                    ArgbToPacked(FadeArgb(StatusColor(item.status, style), style.alpha)), item.label, fx);
        if (ShowsDescription(item, style)) {
            fx.fontSize *= style.descriptionScale;
            const ImVec2 description = TextSize(style, fx.fontSize, item.description);
            HUDDraw::DrawTextStyled(dl, ImVec2(TextLeft(textX, description.x, style), top + label.y),
                                    ArgbToPacked(FadeArgb(style.descriptionColor, style.alpha)), item.description, fx);
        }
    }

    inline void DrawDisc(ImDrawList* dl, ImVec2 center, const Content& item, const Style& style)
    {
        const float radius = style.discSize * 0.5f;
        const float rounding = style.discSize * style.discRounding;
        HUDDraw::ChipStyle disc;
        disc.rounding = rounding;
        disc.topColor = FadeArgb(HUDDraw::ChipTopFromArgb(style.discColor), style.alpha);
        disc.bottomColor = HUDDraw::ChipBottomFromArgb(style.discColor);
        disc.innerBorder = 0;
        disc.outerBorder = 0;
        if (style.alpha < 0.99f) {
            disc.shadow = false;
            disc.highlight = false;
            disc.grainAlpha = static_cast<int>(static_cast<float>(disc.grainAlpha) * style.alpha);
        }
        if ((style.discColor >> 24) == 0) {
            disc.shadow = disc.highlight = false;
            disc.grainAlpha = 0;
        }
        if (item.status == Status::Bad && style.pulseBad)
            disc.glowColor = FadeArgb(style.badColor, UIAnimation::Pulse(4.0f, 0.25f, 0.9f) * style.alpha);
        HUDDraw::DrawChip(dl, ImVec2(center.x - radius, center.y - radius), ImVec2(center.x + radius, center.y + radius), disc);

        if (item.icon) {
            const float half = style.iconSize * 0.5f;
            HUDDraw::DrawIconStyled(dl, ImVec2(center.x - half, center.y - half), style.iconSize, item.icon,
                                    FadeArgb(item.color ? item.color : 0xFFFFFFFF, style.alpha), style.iconFx);
        }

        const std::uint32_t border = item.status == Status::Normal ? style.discBorderColor : StatusColor(item.status, style);
        if (style.discBorderWidth > 0.0f) {
            const float inset = style.discBorderWidth * 0.5f;
            const float half = radius - inset;
            dl->AddRect(ImVec2(center.x - half, center.y - half), ImVec2(center.x + half, center.y + half),
                        ArgbToPacked(FadeArgb(border, style.alpha)), (std::max)(0.0f, rounding - inset), 0, style.discBorderWidth);
        }
    }

    inline void DrawPennant(ImDrawList* dl, ImVec2 center, const Content& item, const Style& style)
    {
        const std::uint32_t statusColor = FadeArgb(StatusColor(item.status, style), style.alpha);
        const bool normal = item.status == Status::Normal;
        HUDDraw::PennantStyle look;
        look.ringRadius = style.discSize * 0.5f;
        look.direction = style.clothDirection;
        look.clothWidth = style.clothWidth;
        look.clothLength = style.clothLength;
        look.borderWidth = style.discBorderWidth;
        look.iconSize = style.iconSize;
        look.ringColor = look.clothColor = FadeArgb(style.discColor, style.alpha);
        look.borderColor = normal ? FadeArgb(style.discBorderColor, style.alpha) : statusColor;
        look.numberColor = statusColor;
        look.iconColor = FadeArgb(item.color ? item.color : 0xFFFFFFFF, style.alpha);
        look.font = FontOf(style);
        look.fontSize = FontSizeOf(style) * 1.1f;
        look.iconFx = style.iconFx;

        if (item.status == Status::Bad && style.pulseBad) {
            const std::uint32_t glow = FadeArgb(style.badColor, UIAnimation::Pulse(4.0f, 0.25f, 0.9f) * style.alpha);
            dl->AddCircle(center, look.ringRadius + 2.0f, ArgbToPacked(glow), 0, 2.0f);
        }
        char countText[16] = "";
        if (ShowsCount(item, style)) FormatCount(countText, item);
        HUDDraw::DrawPennantBadge(dl, center, countText, item.icon, look);
    }

    inline void DrawCount(ImDrawList* dl, ImVec2 discCenter, const Content& item, const Style& style)
    {
        char countText[16];
        FormatCount(countText, item);
        HUDDraw::BadgeStyle badge = CountBadgeStyle(style);
        if (item.status != Status::Normal) badge.textColor = StatusColor(item.status, style);
        const float radius = style.discSize * 0.5f;
        constexpr int kTopLeftCorner = 0;
        constexpr int kTopRightCorner = 1;
        const float cornerX = style.growLeft ? discCenter.x + radius : discCenter.x - radius;
        HUDDraw::DrawBadge(dl, ImVec2(cornerX, discCenter.y - radius),
                           style.growLeft ? kTopRightCorner : kTopLeftCorner, countText, badge);
    }

    inline void Draw(ImDrawList* dl, ImVec2 pos, ImVec2 size, const Content& item, const Style& baseStyle)
    {
        Style style = baseStyle;
        style.alpha *= item.alpha;
        const ImVec2 discCenter = PlateCenter(pos, size, style);
        if (ShowsText(item, style)) DrawLabel(dl, discCenter, size, item, style);
        if (style.pennant) {
            DrawPennant(dl, discCenter, item, style);
            return;
        }
        DrawDisc(dl, discCenter, item, style);
        if (ShowsCount(item, style)) DrawCount(dl, discCenter, item, style);
    }

    inline ListResult DrawList(ImDrawList* dl, ImVec2 anchor, const std::vector<Content>& items,
                               Style style, const HUDFlow::Options& flow)
    {
        style.growLeft = HUDFlow::AwayFromAnchor(flow.corner).x < 0.0f;

        std::vector<ImVec2> sizes;
        sizes.reserve(items.size());
        for (const Content& item : items)
            sizes.push_back(Measure(item, style));

        const HUDFlow::Result layout = HUDFlow::Arrange(sizes, flow);
        ListResult result;
        result.size = layout.size;
        result.topLeft = HUDFlow::TopLeftFromAnchor(anchor, layout.size, flow.corner);

        std::vector<ImVec2> positions;
        for (int i = 0; i < layout.shown; ++i)
            positions.emplace_back(result.topLeft.x + layout.offsets[i].x, result.topLeft.y + layout.offsets[i].y);

        DrawMessages(dl, items, positions, sizes, result, style, flow);
        for (int i = 0; i < layout.shown; ++i)
            Draw(dl, positions[i], sizes[i], items[i], style);
        for (int i = 0; i < layout.shown; ++i)
            result.discCenters.push_back(PlateCenter(positions[i], sizes[i], style));
        return result;
    }
}
