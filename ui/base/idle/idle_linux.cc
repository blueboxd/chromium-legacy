// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/idle/idle.h"

#include "ui/base/idle/idle_internal.h"

#if defined(USE_X11)
#include "ui/base/idle/idle_query_x11.h"
#include "ui/base/idle/screensaver_window_finder_x11.h"
#include "ui/base/ui_base_features.h"
#endif

namespace ui {

int CalculateIdleTime() {
// TODO(https://crbug.com/1098201): calculate idle time for Ozone/Linux.
#if defined(USE_X11)
  if (!features::IsUsingOzonePlatform()) {
    IdleQueryX11 idle_query;
    return idle_query.IdleTime();
  }
#endif
  return 0;
}

bool CheckIdleStateIsLocked() {
  if (IdleStateForTesting().has_value())
    return IdleStateForTesting().value() == IDLE_STATE_LOCKED;

// TODO(https://crbug.com/1098202): fix screensaver.
#if defined(USE_X11)
  if (!features::IsUsingOzonePlatform()) {
    // Usually the screensaver is used to lock the screen.
    return ScreensaverWindowFinder::ScreensaverWindowExists();
  }
#endif
  return false;
}

}  // namespace ui
