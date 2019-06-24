// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/tablet_mode_client.h"

#include "base/macros.h"
#include "chrome/browser/ui/ash/fake_tablet_mode_controller.h"
#include "chrome/browser/ui/ash/tablet_mode_client_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestTabletModeClientObserver : public TabletModeClientObserver {
 public:
  TestTabletModeClientObserver() = default;
  ~TestTabletModeClientObserver() override = default;

  void OnTabletModeToggled(bool enabled) override {
    ++toggle_count_;
    last_toggle_ = enabled;
  }

  int toggle_count_ = 0;
  bool last_toggle_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestTabletModeClientObserver);
};

using TabletModeClientTest = testing::Test;

TEST_F(TabletModeClientTest, Construction) {
  // In production, TabletModeController is constructed before TabletModeClient
  // and destroyed before it too. Match that here.
  auto controller = std::make_unique<FakeTabletModeController>();
  TabletModeClient client;
  client.Init();

  // Singleton was initialized.
  EXPECT_EQ(&client, TabletModeClient::Get());

  // Object was set as client.
  EXPECT_TRUE(controller->has_observer());

  controller = nullptr;
}

TEST_F(TabletModeClientTest, Observers) {
  auto controller = std::make_unique<FakeTabletModeController>();
  TestTabletModeClientObserver observer;
  TabletModeClient client;
  client.Init();
  client.AddObserver(&observer);

  // Observer is not notified with state when added.
  EXPECT_EQ(0, observer.toggle_count_);

  // Setting state notifies observer.
  client.OnTabletModeToggled(true);
  EXPECT_EQ(1, observer.toggle_count_);
  EXPECT_TRUE(observer.last_toggle_);

  client.OnTabletModeToggled(false);
  EXPECT_EQ(2, observer.toggle_count_);
  EXPECT_FALSE(observer.last_toggle_);

  client.RemoveObserver(&observer);

  controller = nullptr;
}

}  // namespace
