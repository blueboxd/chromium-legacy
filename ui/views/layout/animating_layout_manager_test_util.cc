// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/animating_layout_manager_test_util.h"

#include "base/run_loop.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/view.h"

namespace views {
namespace test {

AnimatingLayoutManager* GetAnimatingLayoutManager(View* view) {
  return static_cast<AnimatingLayoutManager*>(view->GetLayoutManager());
}

void WaitForAnimatingLayoutManager(AnimatingLayoutManager* layout_manager) {
  base::RunLoop loop;
  layout_manager->PostOrQueueAction(loop.QuitClosure());
  loop.Run();
}

void WaitForAnimatingLayoutManager(View* view) {
  return WaitForAnimatingLayoutManager(GetAnimatingLayoutManager(view));
}

}  // namespace test
}  // namespace views
