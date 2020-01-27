// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_TEST_OVERVIEW_ANIMATION_WAITER_H_
#define ASH_SHELF_TEST_OVERVIEW_ANIMATION_WAITER_H_

#include "ash/shell.h"
#include "ash/wm/overview/overview_observer.h"
#include "base/run_loop.h"

namespace ash {

class OverviewAnimationWaiter : public OverviewObserver {
 public:
  explicit OverviewAnimationWaiter();
  ~OverviewAnimationWaiter() override;

  // Note this could only be called once because RunLoop would not run after
  // Quit is called. Create a new instance if there's need to wait again.
  void Wait();

  // OverviewObserver:
  void OnOverviewModeStartingAnimationComplete(bool cancel) override;
  void OnOverviewModeEndingAnimationComplete(bool cancel) override;

 private:
  base::RunLoop run_loop_;
};

}  // namespace ash

#endif  // ASH_SHELF_TEST_OVERVIEW_ANIMATION_WAITER_H_
