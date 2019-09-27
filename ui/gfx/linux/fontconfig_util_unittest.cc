// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/linux/fontconfig_util.h"

#include <fontconfig/fontconfig.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {

TEST(FontConfigUtilTest, GetFontRenderParamsFromFcPatternWithEmptyPattern) {
  ScopedFcPattern pattern(FcPatternCreate());

  FontRenderParams params;
  GetFontRenderParamsFromFcPattern(pattern.get(), &params);

  FontRenderParams empty_params;
  EXPECT_EQ(params, empty_params);
}

TEST(FontConfigUtilTest, GetFontRenderParamsFromFcPatternWithFalseValues) {
  ScopedFcPattern pattern(FcPatternCreate());
  FcPatternAddBool(pattern.get(), FC_ANTIALIAS, FcFalse);
  FcPatternAddBool(pattern.get(), FC_AUTOHINT, FcFalse);
  FcPatternAddBool(pattern.get(), FC_EMBEDDED_BITMAP, FcFalse);

  FontRenderParams params;
  GetFontRenderParamsFromFcPattern(pattern.get(), &params);

  FontRenderParams expected_params;
  expected_params.antialiasing = false;
  expected_params.autohinter = false;
  expected_params.use_bitmaps = false;
  EXPECT_EQ(params, expected_params);
}

TEST(FontConfigUtilTest, GetFontRenderParamsFromFcPatternWithValues) {
  ScopedFcPattern pattern(FcPatternCreate());
  FcPatternAddBool(pattern.get(), FC_ANTIALIAS, FcTrue);
  FcPatternAddBool(pattern.get(), FC_AUTOHINT, FcTrue);
  FcPatternAddInteger(pattern.get(), FC_HINT_STYLE, FC_HINT_MEDIUM);
  FcPatternAddBool(pattern.get(), FC_EMBEDDED_BITMAP, FcTrue);
  FcPatternAddInteger(pattern.get(), FC_RGBA, FC_RGBA_RGB);

  FontRenderParams params;
  GetFontRenderParamsFromFcPattern(pattern.get(), &params);

  FontRenderParams expected_params;
  expected_params.antialiasing = true;
  expected_params.autohinter = true;
  expected_params.hinting = FontRenderParams::HINTING_MEDIUM;
  expected_params.use_bitmaps = true;
  expected_params.subpixel_rendering = FontRenderParams::SUBPIXEL_RENDERING_RGB;

  EXPECT_EQ(params, expected_params);
}

}  // namespace gfx
