// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/views_test_helper_aura.h"

#include "ui/aura/client/screen_position_client.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/default_screen_position_client.h"

namespace views {

// static
std::unique_ptr<ViewsTestHelper> ViewsTestHelper::Create() {
  return std::make_unique<ViewsTestHelperAura>();
}

ViewsTestHelperAura::ViewsTestHelperAura() {
  aura_test_helper_.SetUp();

  gfx::NativeWindow root_window = GetContext();
  if (root_window && !aura::client::GetScreenPositionClient(root_window)) {
    screen_position_client_ =
        std::make_unique<wm::DefaultScreenPositionClient>(root_window);
  }
}

ViewsTestHelperAura::~ViewsTestHelperAura() {
  gfx::NativeWindow root_window = GetContext();
  if (root_window) {
    // Ensure all Widgets (and windows) are closed in unit tests. This is done
    // automatically when the RootWindow is torn down, but is an error on
    // platforms that must ensure no Compositors are alive when the
    // ContextFactory is torn down.
    // So, although it's optional, check the root window to detect failures
    // before they hit the CQ on other platforms.
    DCHECK(root_window->children().empty()) << "Not all windows were closed.";
  }

  screen_position_client_.reset();
  aura_test_helper_.TearDown();

  const wm::CaptureController* const controller = wm::CaptureController::Get();
  CHECK(!controller || !controller->is_active());
}

gfx::NativeWindow ViewsTestHelperAura::GetContext() {
  return aura_test_helper_.GetContext();
}

}  // namespace views
