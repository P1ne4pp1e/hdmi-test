#include "hdmi_test/test_pattern.hpp"

#include <algorithm>
#include <array>
#include <string_view>

namespace hdmi_test {
namespace {

constexpr std::uint32_t kBackground = argb(11, 18, 32);
constexpr std::uint32_t kHeader = argb(20, 32, 52);
constexpr std::uint32_t kGrid = argb(31, 47, 72);
constexpr std::uint32_t kText = argb(224, 231, 255);

void fill_rect(FrameBufferView frame, int left, int top, int right, int bottom,
               std::uint32_t color) {
  left = std::clamp(left, 0, frame.width);
  right = std::clamp(right, 0, frame.width);
  top = std::clamp(top, 0, frame.height);
  bottom = std::clamp(bottom, 0, frame.height);
  for (int y = top; y < bottom; ++y) {
    for (int x = left; x < right; ++x) {
      frame.pixels[y * frame.stride_pixels + x] = color;
    }
  }
}

void draw_glyph(FrameBufferView frame, int left, int top, char glyph,
                std::uint32_t color) {
  constexpr std::array<std::pair<char, std::array<std::uint8_t, 7>>, 16> glyphs{{
      {' ', {0, 0, 0, 0, 0, 0, 0}},
      {'-', {0, 0, 0, 31, 0, 0, 0}},
      {'/', {1, 2, 4, 8, 16, 0, 0}},
      {':', {0, 4, 0, 0, 4, 0, 0}},
      {'0', {14, 17, 19, 21, 25, 17, 14}},
      {'1', {4, 12, 4, 4, 4, 4, 14}},
      {'2', {14, 17, 1, 2, 4, 8, 31}},
      {'3', {30, 1, 1, 14, 1, 1, 30}},
      {'4', {2, 6, 10, 18, 31, 2, 2}},
      {'5', {31, 16, 30, 1, 1, 17, 14}},
      {'6', {6, 8, 16, 30, 17, 17, 14}},
      {'7', {31, 1, 2, 4, 8, 8, 8}},
      {'8', {14, 17, 17, 14, 17, 17, 14}},
      {'9', {14, 17, 17, 15, 1, 2, 12}},
      {'A', {14, 17, 17, 31, 17, 17, 17}},
      {'B', {30, 17, 17, 30, 17, 17, 30}},
  }};

  const auto it = std::find_if(glyphs.begin(), glyphs.end(),
                               [glyph](const auto& item) { return item.first == glyph; });
  const auto& bitmap = it == glyphs.end() ? glyphs.front().second : it->second;
  for (int y = 0; y < 7; ++y) {
    for (int x = 0; x < 5; ++x) {
      if ((bitmap[y] & (1U << (4U - static_cast<unsigned>(x)))) != 0U) {
        fill_rect(frame, left + x, top + y, left + x + 1, top + y + 1, color);
      }
    }
  }
}

void draw_text(FrameBufferView frame, int left, int top, std::string_view text,
               int scale = 1) {
  for (char glyph : text) {
    for (int y = 0; y < scale; ++y) {
      for (int x = 0; x < scale; ++x) {
        draw_glyph(frame, left + x, top + y, glyph, kText);
      }
    }
    left += 6 * scale;
  }
}

}  // namespace

void draw_test_pattern(FrameBufferView frame) {
  if (frame.pixels == nullptr || frame.width <= 0 || frame.height <= 0 ||
      frame.stride_pixels < frame.width) {
    return;
  }

  fill_rect(frame, 0, 0, frame.width, frame.height, kBackground);
  const int header_height = std::max(10, frame.height / 6);
  fill_rect(frame, 0, 0, frame.width, header_height, kHeader);
  draw_text(frame, 8, std::max(1, header_height / 3 - 3), "HDMI TEST", 1);

  for (int x = 0; x < frame.width; x += 16) {
    fill_rect(frame, x, header_height, x + 1, frame.height, kGrid);
  }
  for (int y = header_height; y < frame.height; y += 16) {
    fill_rect(frame, 0, y, frame.width, y + 1, kGrid);
  }

  const int card_top = frame.height / 2;
  const int card_height = std::max(8, frame.height / 4);
  const int card_width = std::max(8, frame.width / 4 - 4);
  fill_rect(frame, frame.width / 12, card_top, frame.width / 12 + card_width,
            card_top + card_height, argb(30, 200, 150));
  fill_rect(frame, frame.width / 3, card_top, frame.width / 3 + card_width,
            card_top + card_height, argb(239, 83, 80));
  fill_rect(frame, 7 * frame.width / 12, card_top,
            7 * frame.width / 12 + card_width, card_top + card_height,
            argb(255, 183, 77));

  const int bar_top = std::max(0, frame.height - std::max(8, frame.height / 8));
  fill_rect(frame, 0, bar_top, frame.width / 3, frame.height, argb(244, 67, 54));
  fill_rect(frame, frame.width / 3, bar_top, 2 * frame.width / 3, frame.height,
            argb(76, 175, 80));
  fill_rect(frame, 2 * frame.width / 3, bar_top, frame.width, frame.height,
            argb(33, 150, 243));
}

}  // namespace hdmi_test
