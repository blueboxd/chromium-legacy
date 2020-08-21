// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/util/ambient_util.h"

#include "ash/public/cpp/ambient/ambient_client.h"
#include "base/no_destructor.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/shadow_value.h"

namespace ash {
namespace ambient {
namespace util {

// Appearance of the text shadow.
constexpr int kTextShadowElevation = 2;
constexpr SkColor kTextShadowColor = gfx::kGoogleGrey800;

bool IsShowing(LockScreen::ScreenType type) {
  return LockScreen::HasInstance() && LockScreen::Get()->screen_type() == type;
}

const gfx::FontList& GetDefaultFontlist() {
  static const base::NoDestructor<gfx::FontList> font_list("Google Sans, 64px");
  return *font_list;
}

gfx::ShadowValues GetTextShadowValues() {
  return gfx::ShadowValue::MakeRefreshShadowValues(kTextShadowElevation,
                                                   kTextShadowColor);
}

}  // namespace util
}  // namespace ambient
}  // namespace ash
