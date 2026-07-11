#include "hdmi_test/test_pattern.hpp"

#include <array>
#include <cassert>
#include <cstdint>

int main() {
  static_assert(hdmi_test::argb(0x12, 0x34, 0x56) == 0xFF123456U);

  constexpr int width = 96;
  constexpr int height = 64;
  std::array<std::uint32_t, width * height> pixels{};
  hdmi_test::draw_test_pattern({pixels.data(), width, height, width});

  assert(pixels[0] == hdmi_test::argb(20, 32, 52));
  assert(pixels[20 * width + 5] == hdmi_test::argb(11, 18, 32));
  assert(pixels[40 * width + 10] == hdmi_test::argb(30, 200, 150));
  assert(pixels[40 * width + 35] == hdmi_test::argb(239, 83, 80));
  assert(pixels[40 * width + 60] == hdmi_test::argb(255, 183, 77));
  assert(pixels[60 * width + 5] == hdmi_test::argb(244, 67, 54));
  assert(pixels[60 * width + 35] == hdmi_test::argb(76, 175, 80));
  assert(pixels[60 * width + 65] == hdmi_test::argb(33, 150, 243));
  return 0;
}
