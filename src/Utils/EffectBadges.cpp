#include "../../includes/Utils/EffectBadges.h"
#include "../../includes/Utils/EffectIconRenderer.h"
#include "../../includes/ImGui/WidgetDrawHelpers.h"
#include "../../includes/TextureManager.h"
#include <algorithm>
#include <cstdio>

namespace EffectBadges {

    namespace {
        std::uint32_t WithAlpha(std::uint32_t argb, float alpha)
        {
            const auto a = static_cast<std::uint32_t>(((argb >> 24) & 0xFF) * std::clamp(alpha, 0.0f, 1.0f));
            return (argb & 0x00FFFFFFu) | (a << 24);
        }

        bool IsOff(int preset) { return preset == HUDDraw::BadgePreset_Text; }

        std::uint32_t Tone(const Badge& b) { return b.buff ? kBuffGreen : WidgetDraw::kSymptomRedArgb; }

        HUDDraw::BadgeStyle PlateStyle(const Badge& b, const RowStyle& style)
        {
            HUDDraw::BadgeStyle bs = HUDDraw::BadgePresetStyle(style.preset);
            bs.padX = bs.padY = style.size * 0.5f;
            if (!IsOff(style.preset)) HUDDraw::TintBadge(bs, Tone(b));
            bs.alpha *= style.alpha;
            return bs;
        }

        void DrawPlate(ImDrawList* dl, const Badge& b, ImVec2 min, ImVec2 max, const HUDDraw::BadgeStyle& bs)
        {
            if (b.timeLeft < 0.0f) { HUDDraw::DrawBadge(dl, min, 0, nullptr, bs); return; }
            HUDDraw::BadgeStyle faded = bs;
            faded.alpha = bs.alpha * 0.35f;
            HUDDraw::DrawBadge(dl, min, 0, nullptr, faded);
            const float top = max.y - (max.y - min.y) * b.timeLeft;
            dl->PushClipRect(ImVec2(min.x - 2.0f, top), ImVec2(max.x + 2.0f, max.y + 3.0f), true);
            HUDDraw::DrawBadge(dl, min, 0, nullptr, bs);
            dl->PopClipRect();
        }

        void DrawIcon(ImDrawList* dl, const Badge& b, ImVec2 min, ImVec2 max, bool off, float alpha)
        {
            if (!b.icon) return;
            const float side = (std::min)(max.x - min.x, max.y - min.y) * (off ? 1.0f : 0.72f);
            const ImVec2 pos((min.x + max.x - side) * 0.5f, (min.y + max.y - side) * 0.5f);
            HUDDraw::IconFx fx;
            fx.duotone = true;
            fx.shadow  = true;
            fx.outline = fx.glow = fx.gloss = false;
            HUDDraw::DrawIconStyled(dl, pos, side, reinterpret_cast<ImTextureID>(b.icon), WithAlpha(off ? Tone(b) : 0xFFFFFFFFu, alpha), fx);
        }

        void DrawCount(ImDrawList* dl, const Badge& b, ImVec2 max, float side, float alpha)
        {
            if (b.count < 2) return;
            char buf[8];
            snprintf(buf, sizeof(buf), "%d", b.count);
            ImFont*     font = ImGui::GetFont();
            const float fs   = (std::max)(side * 0.5f, 9.0f);
            const ImVec2 ts  = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, buf);
            const ImVec2 at(max.x - ts.x + 1.0f, max.y - ts.y + 2.0f);
            dl->AddText(font, fs, ImVec2(at.x + 1.0f, at.y + 1.0f), IM_COL32(0, 0, 0, static_cast<int>(220 * alpha)), buf);
            dl->AddText(font, fs, at, IM_COL32(255, 255, 255, static_cast<int>(255 * alpha)), buf);
        }
    }

    ID3D11ShaderResourceView* SourceIcon(const HUDSymptoms::Cause& c)
    {
        if (c.icon) return c.icon;
        const auto info = EffectIconRenderer::DescribeEffect(c.effect);
        if (!info.sourceIconId.empty())
            if (auto* tex = TextureManager::GetEffectIcon(info.sourceIconId)) return tex;
        return info.iconId.empty() ? nullptr : TextureManager::GetEffectIcon(info.iconId);
    }

    void AddFromCauses(std::vector<Badge>& out, const HUDSymptoms::Causes& from, bool buff)
    {
        for (const auto& c : from) {
            if (!c.effect) continue;
            const bool seen = std::any_of(out.begin(), out.end(),
                [&](const Badge& x) { return x.name == c.name && x.buff == buff; });
            if (seen) continue;
            const float dur  = c.effect->duration;
            const float left = dur > 0.0f ? std::clamp((dur - c.effect->elapsedSeconds) / dur, 0.0f, 1.0f) : -1.0f;
            out.push_back({ c.name, SourceIcon(c), buff, left, c.count, EffectAttribution::GroupKeyOf(c.effect) });
        }
    }

    static constexpr auto kShownFor = std::chrono::milliseconds(250);

    void ShownGroups::Remember(const EffectAttribution::GroupKey& key)
    {
        const auto now = std::chrono::steady_clock::now();
        std::erase_if(entries_, [&](const Entry& e) { return e.key == key || now - e.seen > kShownFor; });
        entries_.push_back({ key, now });
    }

    bool ShownGroups::Contains(const EffectAttribution::GroupKey& key) const
    {
        const auto now = std::chrono::steady_clock::now();
        return std::any_of(entries_.begin(), entries_.end(),
            [&](const Entry& e) { return e.key == key && now - e.seen <= kShownFor; });
    }

    Row Measure(const std::vector<Badge>& badges, const RowStyle& style)
    {
        Row row;
        const int count = static_cast<int>(badges.size());
        row.shown = (std::min)(count, (std::max)(style.max, 1));
        for (int i = 0; i < row.shown; ++i) {
            row.styles.push_back(PlateStyle(badges[i], style));
            row.sizes.push_back(HUDDraw::MeasureBadge(nullptr, row.styles.back()));
        }
        if (count > row.shown) {
            snprintf(row.more, sizeof(row.more), "+%d", count - row.shown);
            HUDDraw::BadgeStyle bs = HUDDraw::BadgePresetStyle(style.preset);
            bs.fontScale = style.fontScale;
            bs.alpha    *= style.alpha;
            row.styles.push_back(bs);
            row.sizes.push_back(HUDDraw::MeasureBadge(row.more, bs));
        }
        for (const auto& z : row.sizes) { row.total.x += z.x; row.total.y = (std::max)(row.total.y, z.y); }
        if (!row.sizes.empty()) row.total.x += style.gap * (row.sizes.size() - 1);
        return row;
    }

    void DrawOne(ImDrawList* dl, ImVec2 pos, ImVec2 size, const Badge& b, const HUDDraw::BadgeStyle& bs, const RowStyle& style)
    {
        const ImVec2 end(pos.x + size.x, pos.y + size.y);
        const bool   off = IsOff(style.preset);
        if (!off) DrawPlate(dl, b, pos, end, bs);
        DrawIcon(dl, b, pos, end, off, style.alpha);
        DrawCount(dl, b, end, (std::min)(size.x, size.y), style.alpha);
    }

    void DrawMore(ImDrawList* dl, ImVec2 pos, const Row& row)
    {
        HUDDraw::DrawBadge(dl, pos, 0, row.more, row.styles.back());
    }

}
