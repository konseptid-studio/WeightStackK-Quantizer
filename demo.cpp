
---

### 2. `demo.cpp`

```cpp
// ===================================================================
// WeightStackK Quantizer – command-line demo
//
// Build (requires stb_image.h in the same folder or include path):
//   g++ -std=c++17 -O2 demo.cpp -o weightstackk_demo
//
// Usage:
//   ./weightstackk_demo input.png 12 [vibrancy=1.25] [levels=12] [layers=1]
//
// Output: ranked palette in hex + RGB
// ===================================================================

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "WeightStackK.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::printf("Usage: %s <image> <max_colors> [vibrancy=1.25] [levels=12] [layers=1]\n", argv[0]);
        std::printf("  vibrancy : 0.0 = pure frequency, 1.0–1.6 recommended\n");
        std::printf("  levels   : bins per channel (12 recommended, 16 also excellent)\n");
        std::printf("  layers   : 1 = classic, 2–3 = optical layer simulation\n");
        return 1;
    }

    int w = 0, h = 0, n = 0;
    unsigned char* data = stbi_load(argv[1], &w, &h, &n, 4);
    if (!data) {
        std::fprintf(stderr, "Failed to load \"%s\"\n", argv[1]);
        return 1;
    }

    weightstackk::QuantizeParams params;
    params.max_colors = std::max(1, std::atoi(argv[2]));
    params.vibrancy   = (argc > 3) ? float(std::atof(argv[3])) : 1.25f;
    params.levels     = (argc > 4) ? std::max(2, std::atoi(argv[4])) : 12;
    params.layers     = (argc > 5) ? std::max(1, std::atoi(argv[5])) : 1;
    params.ignore_alpha = true;
    params.sort_by_luminance = true;

    auto palette = weightstackk::ExtractPalette(data, w, h, params);

    std::printf("WeightStackK palette  (%zu colors, levels=%d, vibrancy=%.2f, layers=%d)\n\n",
                palette.size(), params.levels, params.vibrancy, params.layers);

    for (std::size_t i = 0; i < palette.size(); ++i) {
        const auto& e = palette[i];
        std::printf("%2zu: #%02X%02X%02X   RGB(%3d,%3d,%3d)   weight=%6d  chroma=%5.1f\n",
                    i,
                    e.color.r, e.color.g, e.color.b,
                    e.color.r, e.color.g, e.color.b,
                    e.weight, e.chroma);
    }

    std::printf("\nCSS: %s\n", weightstackk::PaletteToHexList(palette).c_str());

    stbi_image_free(data);
    return 0;
}
