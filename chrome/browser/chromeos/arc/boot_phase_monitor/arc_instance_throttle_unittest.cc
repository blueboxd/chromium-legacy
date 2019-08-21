// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/boot_phase_monitor/arc_instance_throttle.h"

#include <memory>

#include "ash/public/cpp/app_types.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/boot_phase_monitor/arc_boot_phase_monitor_bridge.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/arc_stop_reason.h"
#include "components/arc/test/fake_arc_session.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/core/keyed_service_factory.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_windows.h"
#include "ui/wm/public/activation_change_observer.h"

namespace arc {
namespace {

class ArcInstanceThrottleTest : public testing::Test {
 public:
  ArcInstanceThrottleTest()
      : arc_service_manager_(std::make_unique<ArcServiceManager>()),
        arc_session_manager_(std::make_unique<ArcSessionManager>(
            std::make_unique<ArcSessionRunner>(
                base::BindRepeating(FakeArcSession::Create)))),
        testing_profile_(std::make_unique<TestingProfile>()),
        disable_cpu_restriction_counter_(0),
        enable_cpu_restriction_counter_(0) {
    SetArcAvailableCommandLineForTesting(
        base::CommandLine::ForCurrentProcess());

    ArcBootPhaseMonitorBridgeFactory::GetInstance()->SetTestingFactoryAndUse(
        testing_profile_.get(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::unique_ptr<KeyedService>();
        }));
    arc_instance_throttle_ =
        ArcInstanceThrottle::GetForBrowserContextForTesting(
            testing_profile_.get());
    arc_instance_throttle_->SetDelegateForTesting(
        std::make_unique<TestDelegateImpl>(this));
  }
  ~ArcInstanceThrottleTest() override { arc_instance_throttle_->Shutdown(); }

 protected:
  ArcInstanceThrottle* arc_instance_throttle() const {
    return arc_instance_throttle_;
  }

  size_t disable_cpu_restriction_counter() const {
    return disable_cpu_restriction_counter_;
  }

  size_t enable_cpu_restriction_counter() const {
    return enable_cpu_restriction_counter_;
  }

  sync_preferences::TestingPrefServiceSyncable* GetPrefs() const {
    return testing_profile_->GetTestingPrefService();
  }

 private:
  class TestDelegateImpl : public ArcInstanceThrottle::Delegate {
   public:
    explicit TestDelegateImpl(ArcInstanceThrottleTest* test) : test_(test) {}
    ~TestDelegateImpl() override = default;
    void SetCpuRestriction(bool restrict) override {
      if (!restrict)
        ++(test_->disable_cpu_restriction_counter_);
      else
        ++(test_->enable_cpu_restriction_counter_);
    }

   private:
    ArcInstanceThrottleTest* test_;
    DISALLOW_COPY_AND_ASSIGN(TestDelegateImpl);
  };
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  std::unique_ptr<TestingProfile> testing_profile_;
  ArcInstanceThrottle* arc_instance_throttle_;
  size_t disable_cpu_restriction_counter_;
  size_t enable_cpu_restriction_counter_;

  DISALLOW_COPY_AND_ASSIGN(ArcInstanceThrottleTest);
};

// Tests that ArcInstanceThrottle starts/stop observing windows when it receives
// notification that the container has started/stopped.
TEST_F(ArcInstanceThrottleTest, TestOnBootCompleted) {
  arc_instance_throttle()->OnArcStarted();
  EXPECT_FALSE(arc_instance_throttle()->observing_window_activations());
  arc_instance_throttle()->OnArcInitialStart();
  EXPECT_TRUE(arc_instance_throttle()->observing_window_activations());
  arc_instance_throttle()->OnArcSessionStopped(ArcStopReason::SHUTDOWN);
  EXPECT_FALSE(arc_instance_throttle()->observing_window_activations());
  arc_instance_throttle()->OnBootCompleted();
  EXPECT_TRUE(arc_instance_throttle()->observing_window_activations());
  arc_instance_throttle()->OnArcSessionRestarting();
  EXPECT_FALSE(arc_instance_throttle()->observing_window_activations());
}

// Tests that container is unthrottled on OnArcStarted,
// OnArcPlayStoreEnabledChanged, OnArcSessionStopped, and OnArcSessionRestarting
TEST_F(ArcInstanceThrottleTest, TestOnArcStarted) {
  EXPECT_EQ(0U, disable_cpu_restriction_counter());
  arc_instance_throttle()->OnArcStarted();
  EXPECT_EQ(1U, disable_cpu_restriction_counter());

  arc_instance_throttle()->OnArcPlayStoreEnabledChanged(true);
  EXPECT_EQ(2U, disable_cpu_restriction_counter());

  arc_instance_throttle()->OnArcInitialStart();
  arc_instance_throttle()->OnArcSessionStopped(ArcStopReason::SHUTDOWN);
  EXPECT_EQ(3U, disable_cpu_restriction_counter());

  arc_instance_throttle()->OnBootCompleted();
  arc_instance_throttle()->OnArcSessionRestarting();
  EXPECT_EQ(4U, disable_cpu_restriction_counter());
}

// Tests that CPU restriction is disabled when tab restoration is done.
TEST_F(ArcInstanceThrottleTest, TestOnSessionRestoreFinishedLoadingTabs) {
  EXPECT_EQ(0U, disable_cpu_restriction_counter());
  arc_instance_throttle()->OnSessionRestoreFinishedLoadingTabs();
  EXPECT_EQ(1U, disable_cpu_restriction_counter());
}

// Tests that nothing happens if tab restoration is done after ARC boot.
TEST_F(ArcInstanceThrottleTest,
       TestOnSessionRestoreFinishedLoadingTabs_BootFirst) {
  // OnSessionRestoreFinishedLoadingTabs() should be no-op when the
  // instance has already fully started.
  arc_instance_throttle()->OnBootCompleted();
  EXPECT_EQ(0U, disable_cpu_restriction_counter());
  arc_instance_throttle()->OnSessionRestoreFinishedLoadingTabs();
  EXPECT_EQ(0U, disable_cpu_restriction_counter());
}

// Tests that OnExtensionsReady() disables throttling iff instance boot is not
// completed yet.
TEST_F(ArcInstanceThrottleTest, TestOnExtensionsReady) {
  arc_instance_throttle()->OnExtensionsReadyForTesting();
  EXPECT_EQ(1U, disable_cpu_restriction_counter());
  arc_instance_throttle()->OnBootCompleted();
  arc_instance_throttle()->OnExtensionsReadyForTesting();
  EXPECT_EQ(1U, disable_cpu_restriction_counter());
}

// Tests that CPU restriction is disabled when ARC window is in focus,
// and enabled when ARC window is unfocused.
TEST_F(ArcInstanceThrottleTest, TestOnWindowActivated) {
  aura::test::TestWindowDelegate dummy_delegate;
  aura::Window* arc_window = aura::test::CreateTestWindowWithDelegate(
      &dummy_delegate, 1, gfx::Rect(), nullptr);
  aura::Window* chrome_window = aura::test::CreateTestWindowWithDelegate(
      &dummy_delegate, 2, gfx::Rect(), nullptr);
  arc_window->SetProperty(aura::client::kAppType,
                          static_cast<int>(ash::AppType::ARC_APP));
  chrome_window->SetProperty(aura::client::kAppType,
                             static_cast<int>(ash::AppType::BROWSER));

  EXPECT_EQ(0U, disable_cpu_restriction_counter());
  EXPECT_EQ(0U, enable_cpu_restriction_counter());
  arc_instance_throttle()->OnWindowActivated(
      ArcInstanceThrottle::ActivationReason::INPUT_EVENT, arc_window,
      chrome_window);
  EXPECT_EQ(1U, disable_cpu_restriction_counter());
  arc_instance_throttle()->OnWindowActivated(
      ArcInstanceThrottle::ActivationReason::INPUT_EVENT, chrome_window,
      arc_window);
  EXPECT_EQ(1U, disable_cpu_restriction_counter());
  EXPECT_EQ(1U, enable_cpu_restriction_counter());
}

}  // namespace
}  // namespace arc
