// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webthemeengine_impl_mac.h"

namespace content {

blink::ForcedColors WebThemeEngineMac::GetForcedColors() const {
  return forced_colors_;
}

void WebThemeEngineMac::SetForcedColors(
    const blink::ForcedColors forced_colors) {
  forced_colors_ = forced_colors;
}

}  // namespace content
