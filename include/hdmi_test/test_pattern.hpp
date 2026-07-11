#pragma once

#include <cstdint>

namespace hdmi_test {

struct FrameBufferView {
  std::uint32_t* pixels;
  int width;
  int height;
  int stride_pixels;
};

constexpr std::uint32_t argb(std::uint8_t red, std::uint8_t green,
                             std::uint8_t blue) {
  return 0xFF000000U | (static_cast<std::uint32_t>(red) << 16U) |
         (static_cast<std::uint32_t>(green) << 8U) |
         static_cast<std::uint32_t>(blue);
}

void draw_test_pattern(FrameBufferView frame);

}  // namespace hdmi_test
