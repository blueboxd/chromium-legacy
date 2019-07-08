// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/test_native_theme.h"

namespace ui {

TestNativeTheme::TestNativeTheme() {}
TestNativeTheme::~TestNativeTheme() {}

SkColor TestNativeTheme::GetSystemColor(ColorId color_id) const {
  return SK_ColorRED;
}

gfx::Size TestNativeTheme::GetPartSize(Part part,
                                       State state,
                                       const ExtraParams& extra) const {
  return gfx::Size();
}

void TestNativeTheme::Paint(cc::PaintCanvas* canvas,
                            Part part,
                            State state,
                            const gfx::Rect& rect,
                            const ExtraParams& extra) const {}

bool TestNativeTheme::SupportsNinePatch(Part part) const {
  return false;
}

gfx::Size TestNativeTheme::GetNinePatchCanvasSize(Part part) const {
  return gfx::Size();
}

gfx::Rect TestNativeTheme::GetNinePatchAperture(Part part) const {
  return gfx::Rect();
}

bool TestNativeTheme::UsesHighContrastColors() const {
  return high_contrast_;
}

bool TestNativeTheme::SystemDarkModeEnabled() const {
  return dark_mode_;
}

}  // namespace ui
