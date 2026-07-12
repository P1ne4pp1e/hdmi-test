#include "hdmi_test/font_renderer.hpp"

#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>

namespace hdmi_test {
namespace {

std::string find_font() {
  if (!FcInit()) return {};
  FcPattern* pattern = FcNameParse(reinterpret_cast<const FcChar8*>("Noto Sans CJK SC"));
  FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
  FcDefaultSubstitute(pattern);
  FcResult result{};
  FcPattern* match = FcFontMatch(nullptr, pattern, &result);
  FcPatternDestroy(pattern);
  FcChar8* file = nullptr;
  std::string path;
  if (match && FcPatternGetString(match, FC_FILE, 0, &file) == FcResultMatch) {
    path = reinterpret_cast<const char*>(file);
  }
  if (match) FcPatternDestroy(match);
  return path;
}

}  // namespace

FontRenderer::FontRenderer() {
  FT_Library library{};
  if (FT_Init_FreeType(&library) != 0) return;
  library_ = library;
  const std::string path = find_font();
  FT_Face face{};
  if (path.empty() || FT_New_Face(library, path.c_str(), 0, &face) != 0) return;
  face_ = face;
}

FontRenderer::~FontRenderer() {
  if (face_) FT_Done_Face(static_cast<FT_Face>(face_));
  if (library_) FT_Done_FreeType(static_cast<FT_Library>(library_));
}

bool FontRenderer::ready() const { return face_ != nullptr; }

int FontRenderer::text_width(const std::string& text, int pixel_size) {
  if (!face_) return 0;
  FT_Face face = static_cast<FT_Face>(face_);
  FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(pixel_size));
  int width = 0;
  for (unsigned char character : text) {
    if (FT_Load_Char(face, character, FT_LOAD_DEFAULT) == 0) width += face->glyph->advance.x >> 6;
  }
  return width;
}

void FontRenderer::draw(std::uint32_t* pixels, int width, int height, int x, int baseline,
                        const std::string& text, int pixel_size, std::uint32_t color) {
  if (!face_) return;
  FT_Face face = static_cast<FT_Face>(face_);
  FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(pixel_size));
  const int red = (color >> 16) & 0xff, green = (color >> 8) & 0xff, blue = color & 0xff;
  for (unsigned char character : text) {
    if (FT_Load_Char(face, character, FT_LOAD_RENDER) != 0) continue;
    const FT_Bitmap& bitmap = face->glyph->bitmap;
    const int left = x + face->glyph->bitmap_left;
    const int top = baseline - face->glyph->bitmap_top;
    for (unsigned row = 0; row < bitmap.rows; ++row) {
      const int py = top + static_cast<int>(row);
      if (py < 0 || py >= height) continue;
      for (unsigned column = 0; column < bitmap.width; ++column) {
        const int px = left + static_cast<int>(column);
        if (px < 0 || px >= width) continue;
        const int alpha = bitmap.buffer[row * bitmap.pitch + column];
        std::uint32_t& destination = pixels[py * width + px];
        const int dr = (destination >> 16) & 0xff, dg = (destination >> 8) & 0xff, db = destination & 0xff;
        destination = static_cast<std::uint32_t>(((red * alpha + dr * (255 - alpha)) / 255) << 16 |
                                                ((green * alpha + dg * (255 - alpha)) / 255) << 8 |
                                                ((blue * alpha + db * (255 - alpha)) / 255));
      }
    }
    x += face->glyph->advance.x >> 6;
  }
}

}  // namespace hdmi_test
