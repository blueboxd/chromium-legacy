// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/test_compositor_host.h"

#include "base/notreached.h"

#if defined(USE_OZONE)
#include "ui/base/ui_base_features.h"
#include "ui/compositor/test/test_compositor_host_ozone.h"
#endif

#if defined(USE_X11)
#include "ui/compositor/test/test_compositor_host_x11.h"
#endif

namespace ui {

// static
TestCompositorHost* TestCompositorHost::Create(
    const gfx::Rect& bounds,
    ui::ContextFactory* context_factory) {
#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform())
    return new TestCompositorHostOzone(bounds, context_factory);
#endif
#if defined(USE_X11)
  return new TestCompositorHostX11(bounds, context_factory);
#endif
  NOTREACHED();
  return nullptr;
}

}  // namespace ui
