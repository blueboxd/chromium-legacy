// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/linux/fontconfig_util.h"

#include <fontconfig/fontconfig.h>

#include "ui/gfx/font_render_params.h"

namespace gfx {

namespace {

// Converts Fontconfig FC_HINT_STYLE to FontRenderParams::Hinting.
FontRenderParams::Hinting ConvertFontconfigHintStyle(int hint_style) {
  switch (hint_style) {
    case FC_HINT_SLIGHT:
      return FontRenderParams::HINTING_SLIGHT;
    case FC_HINT_MEDIUM:
      return FontRenderParams::HINTING_MEDIUM;
    case FC_HINT_FULL:
      return FontRenderParams::HINTING_FULL;
    default:
      return FontRenderParams::HINTING_NONE;
  }
}

// Converts Fontconfig FC_RGBA to FontRenderParams::SubpixelRendering.
FontRenderParams::SubpixelRendering ConvertFontconfigRgba(int rgba) {
  switch (rgba) {
    case FC_RGBA_RGB:
      return FontRenderParams::SUBPIXEL_RENDERING_RGB;
    case FC_RGBA_BGR:
      return FontRenderParams::SUBPIXEL_RENDERING_BGR;
    case FC_RGBA_VRGB:
      return FontRenderParams::SUBPIXEL_RENDERING_VRGB;
    case FC_RGBA_VBGR:
      return FontRenderParams::SUBPIXEL_RENDERING_VBGR;
    default:
      return FontRenderParams::SUBPIXEL_RENDERING_NONE;
  }
}

// Extracts a string property from a font-config pattern (e.g. FcPattern).
std::string GetFontConfigPropertyAsString(FcPattern* pattern,
                                          const char* property) {
  FcChar8* text = nullptr;
  if (FcPatternGetString(pattern, property, 0, &text) != FcResultMatch ||
      text == nullptr) {
    return std::string();
  }
  return std::string(reinterpret_cast<const char*>(text));
}

// Extracts an integer property from a font-config pattern (e.g. FcPattern).
int GetFontConfigPropertyAsInt(FcPattern* pattern,
                               const char* property,
                               int default_value) {
  int value = -1;
  if (FcPatternGetInteger(pattern, property, 0, &value) != FcResultMatch)
    return default_value;
  return value;
}

// Extracts an boolean property from a font-config pattern (e.g. FcPattern).
bool GetFontConfigPropertyAsBool(FcPattern* pattern, const char* property) {
  FcBool value = FcFalse;
  if (FcPatternGetBool(pattern, property, 0, &value) != FcResultMatch)
    return false;
  return value != FcFalse;
}

}  // namespace

std::string GetFontName(FcPattern* pattern) {
  return GetFontConfigPropertyAsString(pattern, FC_FAMILY);
}

std::string GetFilename(FcPattern* pattern) {
  return GetFontConfigPropertyAsString(pattern, FC_FILE);
}

base::FilePath GetFontPath(FcPattern* pattern) {
  std::string filename = GetFilename(pattern);
  // Paths may be specified with a heading slash (e.g.
  // /test_fonts/DejaVuSans.ttf).
  if (!filename.empty() && base::FilePath::IsSeparator(filename[0]))
    filename = filename.substr(1);

  if (filename.empty())
    return base::FilePath();

  // Obtains the system root directory in 'config' if available. All files
  // (including file properties in patterns) obtained from this 'config' are
  // relative to this system root directory.
  const char* sysroot =
      reinterpret_cast<const char*>(FcConfigGetSysRoot(nullptr));
  if (!sysroot)
    return base::FilePath(filename);
  return base::FilePath(sysroot).Append(filename);
}

int GetFontTtcIndex(FcPattern* pattern) {
  return GetFontConfigPropertyAsInt(pattern, FC_INDEX, 0);
}

bool IsFontBold(FcPattern* pattern) {
  int weight = GetFontConfigPropertyAsInt(pattern, FC_WEIGHT, FC_WEIGHT_NORMAL);
  return weight >= FC_WEIGHT_BOLD;
}

bool IsFontItalic(FcPattern* pattern) {
  int slant = GetFontConfigPropertyAsInt(pattern, FC_SLANT, FC_SLANT_ROMAN);
  return slant != FC_SLANT_ROMAN;
}

bool IsFontScalable(FcPattern* pattern) {
  return GetFontConfigPropertyAsBool(pattern, FC_SCALABLE);
}

std::string GetFontFormat(FcPattern* pattern) {
  return GetFontConfigPropertyAsString(pattern, FC_FONTFORMAT);
}

void GetFontRenderParamsFromFcPattern(FcPattern* pattern,
                                      FontRenderParams* param_out) {
  FcBool fc_antialias = 0;
  if (FcPatternGetBool(pattern, FC_ANTIALIAS, 0, &fc_antialias) ==
      FcResultMatch) {
    param_out->antialiasing = fc_antialias;
  }

  FcBool fc_autohint = 0;
  if (FcPatternGetBool(pattern, FC_AUTOHINT, 0, &fc_autohint) ==
      FcResultMatch) {
    param_out->autohinter = fc_autohint;
  }

  FcBool fc_bitmap = 0;
  if (FcPatternGetBool(pattern, FC_EMBEDDED_BITMAP, 0, &fc_bitmap) ==
      FcResultMatch) {
    param_out->use_bitmaps = fc_bitmap;
  }

  FcBool fc_hinting = 0;
  if (FcPatternGetBool(pattern, FC_HINTING, 0, &fc_hinting) == FcResultMatch) {
    int fc_hint_style = FC_HINT_NONE;
    if (fc_hinting) {
      FcPatternGetInteger(pattern, FC_HINT_STYLE, 0, &fc_hint_style);
    }
    param_out->hinting = ConvertFontconfigHintStyle(fc_hint_style);
  }

  int fc_rgba = FC_RGBA_NONE;
  if (FcPatternGetInteger(pattern, FC_RGBA, 0, &fc_rgba) == FcResultMatch)
    param_out->subpixel_rendering = ConvertFontconfigRgba(fc_rgba);
}

}  // namespace gfx
