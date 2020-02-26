// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_controller.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"

namespace ash {

class AmbientControllerTest : public AshTestBase {
 public:
  AmbientControllerTest() = default;
  AmbientControllerTest(const AmbientControllerTest&) = delete;
  AmbientControllerTest& operator=(AmbientControllerTest&) = delete;
  ~AmbientControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::features::kAmbientModeFeature);

    AshTestBase::SetUp();
  }

  AmbientController* ambient_controller() {
    return Shell::Get()->ambient_controller();
  }

  void LockScreen() { GetSessionControllerClient()->LockScreen(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AmbientControllerTest, ShowAmbientContainerViewOnLockScreen) {
  EXPECT_FALSE(ambient_controller()->is_showing());

  LockScreen();
  EXPECT_TRUE(ambient_controller()->is_showing());
}

}  // namespace ash
