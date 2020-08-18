// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/ui_controls_factory_aura.h"

#include "base/notreached.h"

#if defined(USE_X11)
#include "ui/aura/test/ui_controls_aurax11.h"
#endif

#if defined(USE_OZONE)
#include "ui/aura/test/ui_controls_ozone.h"
#include "ui/base/ui_base_features.h"
#endif

namespace aura {
namespace test {

ui_controls::UIControlsAura* CreateUIControlsAura(WindowTreeHost* host) {
#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform())
    return new UIControlsOzone(host);
#endif
#if defined(USE_X11)
  return new UIControlsX11(host);
#endif
  NOTREACHED();
  return nullptr;
}

}  // namespace test
}  // namespace aura
