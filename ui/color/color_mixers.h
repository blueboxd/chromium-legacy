// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_COLOR_MIXERS_H_
#define UI_COLOR_COLOR_MIXERS_H_

#include "base/component_export.h"

namespace ui {

class ColorProvider;

// Adds native color mixers to |provider| that provide kColorSetNative, as well
// as mappings from this set to cross-platform IDs.  This function should be
// implemented on a per-platform basis in relevant subdirectories.
void AddNativeColorMixers(ui::ColorProvider* provider);

// TODO(pkasting): Other color mixers, e.g.:
//   * Chrome default colors
//   * Native/Chrome priority ordering mixer
//   * All ui/ control and other colors, created from the above

}  // namespace ui

#endif  // UI_COLOR_COLOR_MIXERS_H_
