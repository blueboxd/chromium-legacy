// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/autotest_ambient_api.h"

#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/test/ambient_ash_test_base.h"
#include "base/run_loop.h"

namespace ash {

using AutotestAmbientApiTest = AmbientAshTestBase;

TEST_F(AutotestAmbientApiTest,
       ShouldSuccessfullyWaitForPhotoTransitionAnimation) {
  AutotestAmbientApi test_api;

  // Start ambient mode.
  ambient_controller()->Toggle();

  // Wait for 10 photo transition animation to complete.
  base::RunLoop run_loop;
  test_api.WaitForPhotoTransitionAnimationCompleted(
      /*refresh_interval_s=*/2,
      /*num_completions=*/10, run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace ash
