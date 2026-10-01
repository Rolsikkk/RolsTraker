#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "ftxui_image.hpp"
#include <vector>
#include <algorithm>

using namespace ftxui;

ftxui::Element renderImage(const std::string& path, int maxWidth, int maxHeight) {
    int width, height, channels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 3);
    if (!data) {
        return text(" Ошибка загрузки изображения: " + path + " ") | color(Color::Red);
    }

    // A terminal character is usually 2 times taller than it is wide.
    // Plus we use half-blocks which fits 2 vertical pixels in 1 character.
    // So 1 character = 1 original pixel wide, 2 pixels high.
    int outW = maxWidth;
    int outH = maxHeight * 2;
    
    float ratioW = (float)outW / width;
    float ratioH = (float)outH / height;
    float ratio = std::min({ratioW, ratioH, 1.0f}); // Don't scale up unnecessarily

    int finalW = width * ratio;
    int finalH = height * ratio;
    if (finalW <= 0) finalW = 1;
    if (finalH <= 0) finalH = 1;

    // We need finalH to be even so it matches characters exactly (2 pixels per char)
    if (finalH % 2 != 0) finalH++;

    std::vector<ftxui::Element> rows;
    for (int y = 0; y < finalH; y += 2) {
        ftxui::Elements row;
        for (int x = 0; x < finalW; ++x) {
            int srcX = x * width / finalW;
            int srcY_top = y * height / finalH;
            int srcY_bottom = (y + 1) * height / finalH;
            
            if (srcX >= width) srcX = width - 1;
            if (srcY_top >= height) srcY_top = height - 1;
            if (srcY_bottom >= height) srcY_bottom = height - 1;

            int idxTop = (srcY_top * width + srcX) * 3;
            int idxBot = (srcY_bottom * width + srcX) * 3;

            ftxui::Color colorTop = ftxui::Color::RGB(data[idxTop], data[idxTop+1], data[idxTop+2]);
            ftxui::Color colorBot = ftxui::Color::RGB(data[idxBot], data[idxBot+1], data[idxBot+2]);

            row.push_back(text("▀") | color(colorTop) | bgcolor(colorBot));
        }
        rows.push_back(hbox(std::move(row)));
    }

    stbi_image_free(data);
    return vbox(std::move(rows));
}
