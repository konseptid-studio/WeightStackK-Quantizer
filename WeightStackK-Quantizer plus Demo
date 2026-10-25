// ===================================================================
// WeightStackK Quantizer - Generalized
// A robust color quantizer for photography and video
//
// Origin: evolved from the WeightStackK Layered Quantizer
// (KonseptID Studio / Ivan Kolev) and the Oasis Tracer separation pipeline.
//
// Design notes (from testing):
//   - 32 levels (5-bit)  → too greedy on strongest chroma peaks
//   - 16 levels          → excellent balance
//   - 12 levels          → sweet spot for real photos/video (recommended default)
//   - Below ~10 levels   → starts losing fine hue separation
//
// The frequency × chroma scoring is what keeps distinctive colors alive
// when bin resolution drops, which is exactly where classic k-means /
// median-cut often fail.
//
// Licensed under the Apache License, Version 2.0
// ===================================================================

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace weightstackk {

struct RGBA8 {
    std::uint8_t r = 0, g = 0, b = 0, a = 255;

    bool operator==(const RGBA8& o) const {
        return r == o.r && g == o.g && b == o.b && a == o.a;
    }
};

struct PaletteEntry {
    RGBA8 color;
    int   weight = 0;     // approximate pixel mass
    float chroma = 0.f;
    float score  = 0.f;
};

struct QuantizeParams {
    int   max_colors       = 16;
    float vibrancy         = 1.25f;  // 0 = pure frequency, 1.0–1.6 typical, >2 very aggressive
    int   levels           = 12;     // bins per channel (12 recommended, 16 also excellent)
    int   layers           = 1;      // 1 = classic, 2–3 = optical layer simulation
    bool  ignore_alpha     = true;   // skip pixels with a < 16
    float min_bin_coverage = 0.0004f;
    bool  sort_by_luminance = true;
};

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
namespace detail {

inline double Dist2(const RGBA8& a, const RGBA8& b) {
    const double dr = double(a.r) - double(b.r);
    const double dg = double(a.g) - double(b.g);
    const double db = double(a.b) - double(b.b);
    return dr*dr + dg*dg + db*db;
}

inline double Luma(const RGBA8& c) {
    return 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b;
}

inline int Chroma(const RGBA8& c) {
    const int mx = std::max({int(c.r), int(c.g), int(c.b)});
    const int mn = std::min({int(c.r), int(c.g), int(c.b)});
    return mx - mn;
}

inline RGBA8 ToRGBA(double r, double g, double b) {
    auto clamp = [](double v) -> std::uint8_t {
        return static_cast<std::uint8_t>(std::clamp(std::lround(v), 0L, 255L));
    };
    return {clamp(r), clamp(g), clamp(b), 255};
}

inline std::uint32_t BinKey(const RGBA8& c, int levels) {
    // levels = number of bins per channel (e.g. 12, 16, 20, 32)
    // Map 0..255 → 0..levels-1
    const int lev = std::max(2, std::min(levels, 64)); // safety
    const std::uint32_t r = std::uint32_t(c.r * (lev - 1) / 255);
    const std::uint32_t g = std::uint32_t(c.g * (lev - 1) / 255);
    const std::uint32_t b = std::uint32_t(c.b * (lev - 1) / 255);
    // Pack into 32 bits (supports up to 32 levels cleanly; higher still works)
    return r | (g << 6) | (b << 12);
}

// Simple alpha-over style blend used for layered simulation
inline RGBA8 BlendOverlay(const RGBA8& bottom, const RGBA8& top, double opacity) {
    auto lerp = [](double a, double b, double t) { return a + (b - a) * t; };
    return ToRGBA(
        lerp(bottom.r, top.r, opacity),
        lerp(bottom.g, top.g, opacity),
        lerp(bottom.b, top.b, opacity)
    );
}

// Fixed optical weights inspired by the original 3-layer stack
inline RGBA8 MixSlots(const RGBA8* slots, int count) {
    if (count <= 1) return slots[0];
    const double w3[3] = {0.58, 0.28, 0.14};
    const double w2[2] = {0.70, 0.30};

    double sr = 0, sg = 0, sb = 0;
    if (count >= 3) {
        for (int i = 0; i < 3; ++i) {
            sr += w3[i] * slots[i].r;
            sg += w3[i] * slots[i].g;
            sb += w3[i] * slots[i].b;
        }
    } else {
        for (int i = 0; i < 2; ++i) {
            sr += w2[i] * slots[i].r;
            sg += w2[i] * slots[i].g;
            sb += w2[i] * slots[i].b;
        }
    }
    return ToRGBA(sr, sg, sb);
}

} // namespace detail

// ---------------------------------------------------------------------------
// Core algorithm
// ---------------------------------------------------------------------------

/**
 * Extract a palette from raw RGBA pixels.
 */
inline std::vector<PaletteEntry> ExtractPalette(
    const RGBA8* pixels,
    std::size_t  pixel_count,
    const QuantizeParams& params = {})
{
    using namespace detail;

    if (!pixels || pixel_count == 0 || params.max_colors < 1)
        return {{ {0,0,0,255}, 0, 0.f, 0.f }};

    const int levels = std::max(2, std::min(params.levels, 64));

    // 1. Binning ----------------------------------------------------------------
    struct Bin {
        double r = 0, g = 0, b = 0;
        int    count = 0;
    };
    std::unordered_map<std::uint32_t, Bin> bins;
    bins.reserve(pixel_count / 6 + 32);

    int valid_pixels = 0;
    for (std::size_t i = 0; i < pixel_count; ++i) {
        const RGBA8& p = pixels[i];
        if (params.ignore_alpha && p.a < 16) continue;
        auto& bin = bins[BinKey(p, levels)];
        bin.r += p.r;
        bin.g += p.g;
        bin.b += p.b;
        ++bin.count;
        ++valid_pixels;
    }

    if (bins.empty() || valid_pixels == 0)
        return {{ {0,0,0,255}, 0, 0.f, 0.f }};

    const int min_count = std::max(1, int(valid_pixels * params.min_bin_coverage));

    // 2. Build scored samples ---------------------------------------------------
    struct Sample {
        RGBA8 color;
        int   weight;
        float chroma;
        float score;
    };
    std::vector<Sample> samples;
    samples.reserve(bins.size());

    for (const auto& kv : bins) {
        const Bin& b = kv.second;
        if (b.count < min_count) continue;

        RGBA8 col = ToRGBA(b.r / b.count, b.g / b.count, b.b / b.count);
        const float ch = float(Chroma(col));

        // Core WeightStackK score
        // At low level counts (12–16) this formula is what keeps distinctive
        // hues alive while classic methods start washing them out.
        const float score = float(b.count) * (8.0f + ch * (12.0f + params.vibrancy * 12.0f));

        samples.push_back({col, b.count, ch, score});
    }

    if (samples.empty()) {
        for (const auto& kv : bins) {
            const Bin& b = kv.second;
            samples.push_back({ToRGBA(b.r/b.count, b.g/b.count, b.b/b.count),
                               b.count, 0.f, float(b.count)});
        }
    }

    // 3. Rank by score ----------------------------------------------------------
    std::sort(samples.begin(), samples.end(),
              [](const Sample& a, const Sample& b) { return a.score > b.score; });

    const int n = std::min(params.max_colors, int(samples.size()));
    std::vector<RGBA8> base;
    base.reserve(n);
    for (int i = 0; i < n; ++i)
        base.push_back(samples[i].color);

    // 4. Optional layered optical simulation ------------------------------------
    std::vector<PaletteEntry> result;
    result.reserve(n);

    if (params.layers <= 1) {
        for (int i = 0; i < n; ++i) {
            result.push_back({
                base[i],
                samples[i].weight,
                samples[i].chroma,
                samples[i].score
            });
        }
    } else {
        for (int i = 0; i < n; ++i) {
            RGBA8 slots[3];
            slots[0] = base[i];
            slots[1] = (i + 1 < n) ? base[i + 1] : base[i];
            slots[2] = (i + 2 < n) ? base[i + 2] : slots[1];

            RGBA8 mixed = (params.layers >= 3) ? MixSlots(slots, 3) : MixSlots(slots, 2);

            result.push_back({
                mixed,
                samples[i].weight,
                float(Chroma(mixed)),
                samples[i].score
            });
        }
    }

    if (params.sort_by_luminance) {
        std::stable_sort(result.begin(), result.end(),
            [](const PaletteEntry& a, const PaletteEntry& b) {
                return Luma(a.color) < Luma(b.color);
            });
    }

    return result;
}

/**
 * Convenience overload for byte buffers (RGBA).
 */
inline std::vector<PaletteEntry> ExtractPalette(
    const std::uint8_t* rgba,
    int width, int height,
    const QuantizeParams& params = {})
{
    return ExtractPalette(reinterpret_cast<const RGBA8*>(rgba),
                          std::size_t(width) * std::size_t(height),
                          params);
}

/**
 * Quantize an image to the given palette (nearest color in RGB).
 */
inline std::vector<RGBA8> QuantizeImage(
    const RGBA8* pixels,
    std::size_t  pixel_count,
    const std::vector<PaletteEntry>& palette)
{
    std::vector<RGBA8> out(pixel_count);
    if (palette.empty()) {
        std::fill(out.begin(), out.end(), RGBA8{0,0,0,255});
        return out;
    }

    for (std::size_t i = 0; i < pixel_count; ++i) {
        const RGBA8& p = pixels[i];
        int best = 0;
        double best_d = detail::Dist2(p, palette[0].color);
        for (int k = 1; k < int(palette.size()); ++k) {
            const double d = detail::Dist2(p, palette[k].color);
            if (d < best_d) {
                best_d = d;
                best = k;
            }
        }
        out[i] = palette[best].color;
        out[i].a = p.a;
    }
    return out;
}

inline std::string PaletteToHexList(const std::vector<PaletteEntry>& palette) {
    std::string s;
    for (const auto& e : palette) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "#%02X%02X%02X ", e.color.r, e.color.g, e.color.b);
        s += buf;
    }
    if (!s.empty()) s.pop_back();
    return s;
}

} // namespace weightstackk
