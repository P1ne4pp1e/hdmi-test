#pragma once

#include <cstdint>
#include <string>

namespace hdmi_test {

class FontRenderer {
 public:
  FontRenderer();
  ~FontRenderer();
  FontRenderer(const FontRenderer&) = delete;
  FontRenderer& operator=(const FontRenderer&) = delete;

  bool ready() const;
  int text_width(const std::string& text, int pixel_size);
  void draw(std::uint32_t* pixels, int width, int height, int x, int baseline,
            const std::string& text, int pixel_size, std::uint32_t color);

 private:
  void* library_ = nullptr;
  void* face_ = nullptr;
};

}  // namespace hdmi_test
