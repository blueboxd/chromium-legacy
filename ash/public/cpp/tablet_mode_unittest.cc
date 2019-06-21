// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/tablet_mode.h"

#include "ash/public/cpp/tablet_mode_toggle_observer.h"
#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class TestTabletModeClientObserver : public TabletModeToggleObserver {
 public:
  TestTabletModeClientObserver() = default;
  ~TestTabletModeClientObserver() override = default;

  // TabletModeToggleObserver:
  void OnTabletModeToggled(bool enabled) override {
    ++toggle_count_;
    last_toggle_ = enabled;
  }

  int toggle_count_ = 0;
  bool last_toggle_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestTabletModeClientObserver);
};

class TestTabletModeDelegate : public TabletMode::Delegate {
 public:
  TestTabletModeDelegate() = default;
  ~TestTabletModeDelegate() override = default;

  // TabletMode::Delegate:
  bool InTabletMode() const override { return in_tablet_mode_; }

  void SetEnabledForTest(bool enabled) override {
    in_tablet_mode_ = enabled;
    TabletMode::Get()->NotifyTabletModeChanged();
  }

 private:
  bool in_tablet_mode_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestTabletModeDelegate);
};

using TabletModeTest = testing::Test;

TEST_F(TabletModeTest, Observers) {
  TestTabletModeDelegate delegate;

  TestTabletModeClientObserver observer;
  TabletMode::Get()->AddObserver(&observer);

  // Observer is not notified with state when added.
  EXPECT_EQ(0, observer.toggle_count_);

  // Setting state notifies observer.
  TabletMode::Get()->SetEnabledForTest(true);
  EXPECT_EQ(1, observer.toggle_count_);
  EXPECT_TRUE(observer.last_toggle_);

  TabletMode::Get()->SetEnabledForTest(false);
  EXPECT_EQ(2, observer.toggle_count_);
  EXPECT_FALSE(observer.last_toggle_);

  TabletMode::Get()->RemoveObserver(&observer);
}

}  // namespace ash
