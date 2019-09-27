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

}  // namespace

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
