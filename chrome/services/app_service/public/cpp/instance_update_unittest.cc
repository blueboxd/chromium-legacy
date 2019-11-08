// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/app_service/public/cpp/instance_update.h"
#include "chrome/services/app_service/public/cpp/instance.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"

namespace {

const char app_id[] = "abcdefgh";
const char test_launch_id0[] = "abc";
const char test_launch_id1[] = "xyz";

}  // namespace

class InstanceUpdateTest : public testing::Test {
 protected:
  void ExpectNoChange() {
    expect_launch_id_changed_ = false;
    expect_state_changed_ = false;
    expect_last_updated_time_changed_ = false;
  }

  void CheckExpects(const apps::InstanceUpdate& u) {
    EXPECT_EQ(expect_launch_id_, u.LaunchId());
    EXPECT_EQ(expect_launch_id_changed_, u.LaunchIdChanged());
    EXPECT_EQ(expect_state_, u.State());
    EXPECT_EQ(expect_state_changed_, u.StateChanged());
    EXPECT_EQ(expect_last_updated_time_, u.LastUpdatedTime());
    EXPECT_EQ(expect_last_updated_time_changed_, u.LastUpdatedTimeChanged());
  }

  void TestInstanceUpdate(apps::Instance* state, apps::Instance* delta) {
    apps::InstanceUpdate u(state, delta);
    EXPECT_EQ(app_id, u.AppId());
    EXPECT_EQ(state == nullptr, u.StateIsNull());
    expect_launch_id_ = base::EmptyString();
    expect_state_ = apps::InstanceState::kUnknown;
    expect_last_updated_time_ = base::Time();

    ExpectNoChange();
    CheckExpects(u);

    if (delta) {
      delta->SetLaunchId(test_launch_id0);
      expect_launch_id_ = test_launch_id0;
      expect_launch_id_changed_ = true;
      CheckExpects(u);
    }
    if (state) {
      state->SetLaunchId(test_launch_id0);
      expect_launch_id_ = test_launch_id0;
      expect_launch_id_changed_ = false;
      CheckExpects(u);
    }
    if (state) {
      apps::InstanceUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }
    if (delta) {
      delta->SetLaunchId(test_launch_id1);
      expect_launch_id_ = test_launch_id1;
      expect_launch_id_changed_ = true;
      CheckExpects(u);
    }

    // State and StateTime tests.
    if (state) {
      state->UpdateState(apps::InstanceState::kRunning,
                         base::Time::FromDoubleT(1000.0));
      expect_state_ = apps::InstanceState::kRunning;
      expect_last_updated_time_ = base::Time::FromDoubleT(1000.0);
      expect_state_changed_ = false;
      expect_last_updated_time_changed_ = false;
      CheckExpects(u);
    }
    if (delta) {
      delta->UpdateState(apps::InstanceState::kActive,
                         base::Time::FromDoubleT(2000.0));
      expect_state_ = apps::InstanceState::kActive;
      expect_last_updated_time_ = base::Time::FromDoubleT(2000.0);
      expect_state_changed_ = true;
      expect_last_updated_time_changed_ = true;
      CheckExpects(u);
    }
    if (state) {
      apps::InstanceUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }
  }

  std::string expect_launch_id_;
  bool expect_launch_id_changed_;
  apps::InstanceState expect_state_;
  bool expect_state_changed_;
  base::Time expect_last_updated_time_;
  bool expect_last_updated_time_changed_;
};

TEST_F(InstanceUpdateTest, StateIsNonNull) {
  aura::Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  std::unique_ptr<apps::Instance> state =
      std::make_unique<apps::Instance>(app_id, &window);
  TestInstanceUpdate(state.get(), nullptr);
}

TEST_F(InstanceUpdateTest, DeltaIsNonNull) {
  aura::Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  std::unique_ptr<apps::Instance> delta =
      std::make_unique<apps::Instance>(app_id, &window);
  TestInstanceUpdate(nullptr, delta.get());
}

TEST_F(InstanceUpdateTest, BothAreNonNull) {
  aura::Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  std::unique_ptr<apps::Instance> state =
      std::make_unique<apps::Instance>(app_id, &window);
  std::unique_ptr<apps::Instance> delta =
      std::make_unique<apps::Instance>(app_id, &window);
  TestInstanceUpdate(state.get(), delta.get());
}
