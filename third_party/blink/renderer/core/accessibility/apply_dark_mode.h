// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ACCESSIBILITY_APPLY_DARK_MODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ACCESSIBILITY_APPLY_DARK_MODE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings.h"

namespace blink {

// If content has dark color scheme set then return disabled settings, otherwise
// return enabled settings based on force dark mode enabled.
DarkModeSettings CORE_EXPORT
BuildDarkModeSettings(const Settings& settings,
                      bool content_has_dark_color_scheme);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ACCESSIBILITY_APPLY_DARK_MODE_H_
