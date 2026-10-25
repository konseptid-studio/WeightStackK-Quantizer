# WeightStackK Quantizer

**A robust, saturation-aware color quantizer for photography and video.**

WeightStackK extracts clean, usable palettes from real-world images — including noisy, dark, low-contrast or heavily compressed sources. It was originally developed for multi-layer filament color separation and has been generalized for everyday image and video work.

## Why it exists

Classic algorithms (median-cut, k-means, octree…) optimise for average error.  
When you drop channel resolution (especially ≤16 levels), they often lose distinctive hues.

WeightStackK’s frequency × chroma scoring continues to protect saturated colors even at low bin counts. Testing showed:

| Levels per channel | Behaviour |
|--------------------|---------|
| 32 (5-bit)         | Too greedy on strongest chroma peaks |
| 16                 | Excellent balance |
| **12**             | **Sweet spot for real photos/video (recommended default)** |
| ≤10                | Starts losing fine hue separation |

## Quick start

```bash
# single header – just drop WeightStackK.hpp into your project

# or build the demo (needs stb_image.h)
g++ -std=c++17 -O2 demo.cpp -o weightstackk_demo
./weightstackk_demo photo.jpg 12 1.25 12 1

### Minimal C++ usage
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
