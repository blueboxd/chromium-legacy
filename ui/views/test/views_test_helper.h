// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_VIEWS_TEST_HELPER_H_
#define UI_VIEWS_TEST_VIEWS_TEST_HELPER_H_

#include <memory>

#include "base/macros.h"
#include "ui/gfx/native_widget_types.h"

namespace views {

class TestViewsDelegate;

// A helper class owned by tests that performs platform specific initialization
// required for running tests.
class ViewsTestHelper {
 public:
  // Create a platform specific instance.
  static std::unique_ptr<ViewsTestHelper> Create();

  virtual ~ViewsTestHelper() = default;

  // Does any additional necessary setup of the provided |delegate|.
  virtual void SetUpTestViewsDelegate(TestViewsDelegate* delegate);

  // Returns a context view. In aura builds, this will be the RootWindow.
  // Everywhere else, null.
  virtual gfx::NativeWindow GetContext();

 protected:
  ViewsTestHelper() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ViewsTestHelper);
};

}  // namespace views

#endif  // UI_VIEWS_TEST_VIEWS_TEST_HELPER_H_
