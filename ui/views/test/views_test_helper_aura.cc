// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/views_test_helper_aura.h"

#include "base/logging.h"
#include "ui/aura/window.h"
#include "ui/views/test/test_views_delegate.h"

namespace views {

namespace {

ViewsTestHelperAura::TestViewsDelegateFactory g_delegate_factory = nullptr;

}  // namespace

// static
std::unique_ptr<ViewsTestHelper> ViewsTestHelper::Create() {
  return std::make_unique<ViewsTestHelperAura>();
}

ViewsTestHelperAura::ViewsTestHelperAura() {
  aura_test_helper_ = std::make_unique<aura::test::AuraTestHelper>();
}

ViewsTestHelperAura::~ViewsTestHelperAura() {
  // Ensure all Widgets (and Windows) are closed in unit tests.
  //
  // Most tests do not try to create desktop Aura Widgets, and on most platforms
  // will thus create Widgets that are owned by the RootWindow.  These will
  // automatically be destroyed when the RootWindow is torn down (during
  // destruction of the AuraTestHelper).  However, on Mac there are only desktop
  // widgets, so any unclosed widgets will remain unowned, holding a Compositor,
  // and will cause UAFs if closed (e.g. by the test freeing a
  // unique_ptr<Widget> with WIDGET_OWNS_NATIVE_WIDGET) after the ContextFactory
  // is destroyed by our owner.
  //
  // So, although it shouldn't matter for this helper, check for unclosed
  // windows to complain about faulty tests early.
#if DCHECK_IS_ON()
  gfx::NativeWindow root_window = GetContext();
  if (root_window) {
    DCHECK(root_window->children().empty())
        << "Not all windows were closed:\n"
        << root_window->GetWindowHierarchy(0);
  }
#endif
}

std::unique_ptr<TestViewsDelegate>
ViewsTestHelperAura::GetFallbackTestViewsDelegate() {
  // The factory delegate takes priority over the parent default.
  return g_delegate_factory ? (*g_delegate_factory)()
                            : ViewsTestHelper::GetFallbackTestViewsDelegate();
}

void ViewsTestHelperAura::SetUp() {
  aura_test_helper_->SetUp();
}

gfx::NativeWindow ViewsTestHelperAura::GetContext() {
  return aura_test_helper_->GetContext();
}

// static
void ViewsTestHelperAura::SetFallbackTestViewsDelegateFactory(
    TestViewsDelegateFactory factory) {
  DCHECK_NE(g_delegate_factory == nullptr, factory == nullptr);
  g_delegate_factory = factory;
}

}  // namespace views
