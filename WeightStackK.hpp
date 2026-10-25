#include "WeightStackK.hpp"

weightstackk::QuantizeParams p;
p.max_colors = 12;
p.levels     = 12;     // recommended default
p.vibrancy   = 1.25f;  // 1.0–1.6 typical
p.layers     = 1;      // or 2–3 for optical mix look

auto palette = weightstackk::ExtractPalette(rgba_data, width, height, p);

// Optional: recolor the image
auto quantized = weightstackk::QuantizeImage(
    reinterpret_cast<const weightstackk::RGBA8*>(rgba_data),
    width * height,
    palette);
