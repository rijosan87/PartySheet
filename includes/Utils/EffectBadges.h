#pragma once

#include <chrono>
#include "../HUDSymptoms.h"
#include "EffectAttribution.h"
#include "HUDDraw.h"
#include <imgui.h>
#include <string>
#include <vector>

struct ID3D11ShaderResourceView;

namespace EffectBadges {

    inline constexpr std::uint32_t kBuffGreen = 0xFF5FD16Fu;

    struct Badge {
        std::string               name;
        ID3D11ShaderResourceView* icon     = nullptr;
        bool                      buff     = false;
        float                     timeLeft = -1.0f;
        int                       count    = 1;
        EffectAttribution::GroupKey key;
    };

    struct RowStyle {
        int   preset    = 1;
        float size      = 16.0f;
        int   max       = 3;
        float fontScale = 1.0f;
        float gap       = 2.0f;
        float alpha     = 1.0f;
    };

    ID3D11ShaderResourceView* SourceIcon(const HUDSymptoms::Cause& c);

    void AddFromCauses(std::vector<Badge>& out, const HUDSymptoms::Causes& from, bool buff);

    class ShownGroups {
    public:
        void Remember(const EffectAttribution::GroupKey& key);
        bool Contains(const EffectAttribution::GroupKey& key) const;

    private:
        struct Entry {
            EffectAttribution::GroupKey key;
            std::chrono::steady_clock::time_point seen;
        };
        std::vector<Entry> entries_;
    };

    struct Row {
        std::vector<HUDDraw::BadgeStyle> styles;
        std::vector<ImVec2>              sizes;
        int    shown = 0;
        ImVec2 total{ 0.0f, 0.0f };
        char   more[8] = {};
    };
    Row Measure(const std::vector<Badge>& badges, const RowStyle& style);

    template <class F>
    int Draw(ImDrawList* dl, ImVec2 topLeft, const std::vector<Badge>& badges, const Row& row, const RowStyle& style, F&& onShown);
    void DrawOne(ImDrawList* dl, ImVec2 pos, ImVec2 size, const Badge& b, const HUDDraw::BadgeStyle& bs, const RowStyle& style);
    void DrawMore(ImDrawList* dl, ImVec2 pos, const Row& row);

    template <class F>
    int Draw(ImDrawList* dl, ImVec2 topLeft, const std::vector<Badge>& badges, const Row& row, const RowStyle& style, F&& onShown)
    {
        float x = topLeft.x;
        for (std::size_t i = 0; i < row.sizes.size(); ++i) {
            const ImVec2 pos(x, topLeft.y + (row.total.y - row.sizes[i].y) * 0.5f);
            if (static_cast<int>(i) == row.shown) {
                DrawMore(dl, pos, row);
            } else {
                DrawOne(dl, pos, row.sizes[i], badges[i], row.styles[i], style);
                onShown(badges[i]);
            }
            x += row.sizes[i].x + style.gap;
        }
        return row.shown;
    }

}
