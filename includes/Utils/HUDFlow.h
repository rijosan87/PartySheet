#pragma once

#include <imgui.h>

#include <algorithm>
#include <vector>

namespace HUDFlow {

    enum class Corner : int { TopLeft = 0, TopRight = 1, BottomLeft = 2, BottomRight = 3 };
    enum class Axis : int { Vertical = 0, Horizontal = 1 };

    struct Options {
        Corner corner = Corner::TopLeft;
        Axis   axis = Axis::Vertical;
        int    wrapCount = 0;
        int    maxItems = 0;
        float  gapX = 0.0f;
        float  gapY = 0.0f;
    };

    struct Result {
        ImVec2              size{ 0.0f, 0.0f };
        std::vector<ImVec2> offsets;
        int                 shown = 0;
    };

    inline bool IsLeft(Corner c) { return c == Corner::TopLeft || c == Corner::BottomLeft; }
    inline bool IsTop(Corner c) { return c == Corner::TopLeft || c == Corner::TopRight; }

    inline ImVec2 AwayFromAnchor(Corner c)
    {
        return ImVec2(IsLeft(c) ? 1.0f : -1.0f, IsTop(c) ? 1.0f : -1.0f);
    }

    inline ImVec2 TopLeftFromAnchor(ImVec2 anchor, ImVec2 size, Corner c)
    {
        return ImVec2(IsLeft(c) ? anchor.x : anchor.x - size.x,
                      IsTop(c) ? anchor.y : anchor.y - size.y);
    }

    inline ImVec2 AnchorFromTopLeft(ImVec2 topLeft, ImVec2 size, Corner c)
    {
        return ImVec2(IsLeft(c) ? topLeft.x : topLeft.x + size.x,
                      IsTop(c) ? topLeft.y : topLeft.y + size.y);
    }

    inline Result Arrange(const std::vector<ImVec2>& itemSizes, const Options& opt)
    {
        Result result;
        int count = static_cast<int>(itemSizes.size());
        if (opt.maxItems > 0) count = (std::min)(count, opt.maxItems);
        result.offsets.resize(static_cast<std::size_t>(count));
        result.shown = count;
        if (count == 0) return result;

        const bool  vertical = opt.axis == Axis::Vertical;
        const int   perLine = opt.wrapCount > 0 ? opt.wrapCount : count;
        const float mainGap = vertical ? opt.gapY : opt.gapX;
        const float crossGap = vertical ? opt.gapX : opt.gapY;
        auto mainSize = [&](const ImVec2& s) { return vertical ? s.y : s.x; };
        auto crossSize = [&](const ImVec2& s) { return vertical ? s.x : s.y; };

        std::vector<float> mainPos(static_cast<std::size_t>(count)), crossPos(static_cast<std::size_t>(count));
        float crossCursor = 0.0f;
        float longestLine = 0.0f;
        for (int lineStart = 0; lineStart < count; lineStart += perLine) {
            const int lineEnd = (std::min)(count, lineStart + perLine);
            float mainCursor = 0.0f;
            float thickness = 0.0f;
            for (int i = lineStart; i < lineEnd; ++i) {
                mainPos[i] = mainCursor;
                crossPos[i] = crossCursor;
                mainCursor += mainSize(itemSizes[i]) + mainGap;
                thickness = (std::max)(thickness, crossSize(itemSizes[i]));
            }
            longestLine = (std::max)(longestLine, mainCursor - mainGap);
            crossCursor += thickness + crossGap;
        }
        const float totalCross = crossCursor - crossGap;
        result.size = vertical ? ImVec2(totalCross, longestLine) : ImVec2(longestLine, totalCross);

        for (int i = 0; i < count; ++i) {
            float x = vertical ? crossPos[i] : mainPos[i];
            float y = vertical ? mainPos[i] : crossPos[i];
            if (!IsLeft(opt.corner)) x = result.size.x - x - itemSizes[i].x;
            if (!IsTop(opt.corner)) y = result.size.y - y - itemSizes[i].y;
            result.offsets[i] = ImVec2(x, y);
        }
        return result;
    }

    inline Result ArrangeGrid(int count, ImVec2 cellSize, const Options& opt)
    {
        return Arrange(std::vector<ImVec2>(static_cast<std::size_t>((std::max)(count, 0)), cellSize), opt);
    }

    enum class Side : int { Below = 0, Above = 1, Left = 2, Right = 3 };
    enum class Align : int { Start = 0, Center = 1, End = 2 };

    inline ImVec2 PlaceLabel(ImVec2 hostMin, ImVec2 hostMax, ImVec2 labelSize, Side side, Align align, float gap)
    {
        auto alignedX = [&]() {
            if (align == Align::Center) return (hostMin.x + hostMax.x - labelSize.x) * 0.5f;
            if (align == Align::End) return hostMax.x - labelSize.x;
            return hostMin.x;
        };
        const float midY = (hostMin.y + hostMax.y - labelSize.y) * 0.5f;
        switch (side) {
        case Side::Above: return ImVec2(alignedX(), hostMin.y - gap - labelSize.y);
        case Side::Left:  return ImVec2(hostMin.x - gap - labelSize.x, midY);
        case Side::Right: return ImVec2(hostMax.x + gap, midY);
        default:          return ImVec2(alignedX(), hostMax.y + gap);
        }
    }

    enum class Spot : int { TopLeft = 0, Top, TopRight, Left, Center, Right, BottomLeft, Bottom, BottomRight };

    inline ImVec2 PlaceBadge(ImVec2 hostMin, ImVec2 hostMax, ImVec2 badgeSize, Spot spot, ImVec2 overhang)
    {
        const int col = static_cast<int>(spot) % 3, row = static_cast<int>(spot) / 3;
        auto along = [](float lo, float hi, float size, int slot, float over) {
            if (slot == 0) return lo - size * over;
            if (slot == 2) return hi - size * (1.0f - over);
            return (lo + hi - size) * 0.5f;
        };
        return ImVec2(along(hostMin.x, hostMax.x, badgeSize.x, col, overhang.x),
                      along(hostMin.y, hostMax.y, badgeSize.y, row, overhang.y));
    }
}
