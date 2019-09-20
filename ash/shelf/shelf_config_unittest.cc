// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shelf_config.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "chromeos/constants/chromeos_switches.h"

namespace ash {

class ShelfConfigTest : public AshTestBase {
 public:
  ShelfConfigTest() = default;
  ~ShelfConfigTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        chromeos::switches::kShelfHotseat);

    AshTestBase::SetUp();
  }

 protected:
  bool is_dense() { return ShelfConfig::Get()->is_dense_; }

  bool IsTabletMode() {
    return Shell::Get()->tablet_mode_controller()->InTabletMode();
  }

  void SetTabletMode(bool is_tablet_mode) {
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(is_tablet_mode);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfConfigTest);
};

// Make sure ShelfConfig is dense when screen becomes small in tablet mode.
TEST_F(ShelfConfigTest, SmallDisplayIsDense) {
  UpdateDisplay("1000x1000");
  SetTabletMode(true);

  ASSERT_TRUE(IsTabletMode());
  ASSERT_FALSE(is_dense());

  // Change display to have a small width, and check that ShelfConfig is dense.
  UpdateDisplay("300x1000");
  ASSERT_TRUE(is_dense());

  // Set the display size back.
  UpdateDisplay("1000x1000");
  ASSERT_FALSE(is_dense());

  // Change display to have a small height, and check that ShelfConfig is dense.
  UpdateDisplay("1000x300");
  ASSERT_TRUE(is_dense());
}

// Make sure ShelfConfig switches between dense and not dense when switching
// between clamshell and tablet mode.
TEST_F(ShelfConfigTest, DenseChangeOnTabletModeChange) {
  UpdateDisplay("1000x1000");

  ASSERT_FALSE(IsTabletMode());
  ASSERT_TRUE(is_dense());

  SetTabletMode(true);

  ASSERT_TRUE(IsTabletMode());
  ASSERT_FALSE(is_dense());

  SetTabletMode(false);

  ASSERT_FALSE(IsTabletMode());
  ASSERT_TRUE(is_dense());
}

}  // namespace ash
