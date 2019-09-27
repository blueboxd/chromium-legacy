// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_LINUX_FONTCONFIG_UTIL_H_
#define UI_GFX_LINUX_FONTCONFIG_UTIL_H_

#include <fontconfig/fontconfig.h>

#include "ui/gfx/font_render_params.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

struct FcPatternDeleter {
  void operator()(FcPattern* ptr) const { FcPatternDestroy(ptr); }
};
using ScopedFcPattern = std::unique_ptr<FcPattern, FcPatternDeleter>;

// Returns the appropriate parameters for rendering the font represented by the
// font config pattern.
GFX_EXPORT void GetFontRenderParamsFromFcPattern(FcPattern* pattern,
                                                 FontRenderParams* param_out);

}  // namespace gfx

#endif  // UI_GFX_LINUX_FONTCONFIG_UTIL_H_