// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/pip/arc_picture_in_picture_window_controller_impl.h"

#include "ash/public/cpp/ash_pref_names.h"
#include "chrome/browser/chromeos/arc/pip/arc_pip_bridge.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_pip_instance.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

class ArcPipBridgeTest : public testing::Test {
 public:
  ArcPipBridgeTest() = default;
  ~ArcPipBridgeTest() override = default;

  void SetUp() override {
    arc_bridge_service_ = std::make_unique<ArcBridgeService>();
    bridge_ = std::make_unique<ArcPipBridge>(&testing_profile_,
                                             arc_bridge_service_.get());
    pip_instance_ = std::make_unique<arc::FakePipInstance>();
    arc_bridge_service_->pip()->SetInstance(pip_instance_.get());
    WaitForInstanceReady(arc_bridge_service_->pip());
  }

  void TearDown() override {
    arc_bridge_service_->pip()->CloseInstance(pip_instance_.get());
    pip_instance_.reset();
    bridge_.reset();
    arc_bridge_service_.reset();
  }

  ArcPipBridge* bridge() { return bridge_.get(); }
  FakePipInstance* pip_instance() { return pip_instance_.get(); }

 protected:
  void SetSpokenFeedbackEnabled(bool enabled) {
    return testing_profile_.GetPrefs()->SetBoolean(
        ash::prefs::kAccessibilitySpokenFeedbackEnabled, enabled);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile testing_profile_;
  std::unique_ptr<ArcBridgeService> arc_bridge_service_;
  std::unique_ptr<ArcPipBridge> bridge_;
  std::unique_ptr<FakePipInstance> pip_instance_;
};

TEST_F(ArcPipBridgeTest, ClosingPipTellsAndroidToClosePip) {
  bridge()->ClosePip();
  EXPECT_EQ(1, pip_instance()->num_closed());
}

// Test that when PictureInPictureWindowManager notices one Android PIP
// is being replaced by another we don't erroneously send a close command
// to Android.
TEST_F(ArcPipBridgeTest, OpeningAndroidPipTwiceElidesCloseCall) {
  bridge()->OnPipEvent(arc::mojom::ArcPipEvent::ENTER);
  EXPECT_EQ(0, pip_instance()->num_closed());

  bridge()->OnPipEvent(arc::mojom::ArcPipEvent::ENTER);
  EXPECT_EQ(0, pip_instance()->num_closed());
}

TEST_F(ArcPipBridgeTest, ChromeVoxSuppressesPip) {
  // Initialized as "false" on connection ready.
  EXPECT_TRUE(pip_instance()->suppressed().has_value());
  EXPECT_FALSE(pip_instance()->suppressed().value());

  // Enabling spoken-feedback suppresses pip.
  SetSpokenFeedbackEnabled(true);
  EXPECT_TRUE(pip_instance()->suppressed().value());

  // Disabling spoken-feedback unsuppresses pip.
  SetSpokenFeedbackEnabled(false);
  EXPECT_FALSE(pip_instance()->suppressed().value());
}

}  // namespace arc
