// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_fallback.h"

#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_fallback_skia_impl.h"

namespace gfx {

std::vector<Font> GetFallbackFonts(const Font& font) {
  return std::vector<Font>();
}

bool GetFallbackFont(const Font& font,
                     const std::string& locale,
                     base::StringPiece16 text,
                     Font* result) {
  TRACE_EVENT0("fonts", "gfx::GetFallbackFont");

  if (text.empty())
    return false;

  std::string skia_family_name =
      GetFallbackFontFamilyNameSkia(font, locale, text);

  if (skia_family_name.empty())
    return false;

  *result = Font(std::string(skia_family_name.c_str(), skia_family_name.size()),
                 font.GetFontSize());
  return true;
}

}  // namespace gfx
