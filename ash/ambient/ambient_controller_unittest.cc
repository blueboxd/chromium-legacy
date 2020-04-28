// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_controller.h"

#include "ash/ambient/test/ambient_ash_test_base.h"

namespace ash {

using AmbientControllerTest = AmbientAshTestBase;

TEST_F(AmbientControllerTest, ShowAmbientContainerViewOnLockScreen) {
  EXPECT_FALSE(ambient_controller()->is_showing());

  LockScreen();
  EXPECT_TRUE(ambient_controller()->is_showing());
}

}  // namespace ash
