// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/instance_throttle/arc_instance_throttle.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/test/task_environment.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/boot_phase_monitor/arc_boot_phase_monitor_bridge.h"
#include "chrome/browser/chromeos/throttle_observer.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/test/fake_arc_session.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/core/keyed_service_factory.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

class ArcInstanceThrottleTest : public testing::Test {
 public:
  ArcInstanceThrottleTest()
      : arc_service_manager_(std::make_unique<ArcServiceManager>()),
        arc_session_manager_(std::make_unique<ArcSessionManager>(
            std::make_unique<ArcSessionRunner>(
                base::BindRepeating(FakeArcSession::Create)))),
        testing_profile_(std::make_unique<TestingProfile>()),
        disable_cpu_restriction_counter_(0),
        enable_cpu_restriction_counter_(0),
        record_uma_counter_(0) {
    SetArcAvailableCommandLineForTesting(
        base::CommandLine::ForCurrentProcess());

    ArcBootPhaseMonitorBridge::GetForBrowserContextForTesting(
        testing_profile_.get());
    arc_instance_throttle_ =
        ArcInstanceThrottle::GetForBrowserContextForTesting(
            testing_profile_.get());
    arc_instance_throttle_->set_delegate_for_testing(
        std::make_unique<TestDelegateImpl>(this));
  }

 protected:
  sync_preferences::TestingPrefServiceSyncable* GetPrefs() {
    return testing_profile_->GetTestingPrefService();
  }

  ArcInstanceThrottle* arc_instance_throttle() {
    return arc_instance_throttle_;
  }

  size_t disable_cpu_restriction_counter() const {
    return disable_cpu_restriction_counter_;
  }

  size_t enable_cpu_restriction_counter() const {
    return enable_cpu_restriction_counter_;
  }

  chromeos::ThrottleObserver* critical_observer() {
    return &critical_observer_;
  }
  chromeos::ThrottleObserver* low_observer() { return &low_observer_; }
  const std::string& last_recorded_observer_name() const {
    return last_recorded_observer_name_;
  }
  size_t uma_count() const { return record_uma_counter_; }

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
    void RecordCpuRestrictionDisabledUMA(const std::string& observer_name,
                                         base::TimeDelta delta) override {
      ++(test_->record_uma_counter_);
      test_->last_recorded_observer_name_ = observer_name;
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
  chromeos::ThrottleObserver critical_observer_{
      chromeos::ThrottleObserver::PriorityLevel::CRITICAL, "CriticalObserver"};
  chromeos::ThrottleObserver low_observer_{
      chromeos::ThrottleObserver::PriorityLevel::LOW, "LowObserver"};
  std::string last_recorded_observer_name_;
  size_t record_uma_counter_;

  DISALLOW_COPY_AND_ASSIGN(ArcInstanceThrottleTest);
};

// Tests that ArcInstanceThrottle can be constructed and destructed.

TEST_F(ArcInstanceThrottleTest, TestConstructDestruct) {}

// Tests that ArcInstanceThrottle adjusts throttling when it is notified of
// a change in observers, but skips adjusting throttle if the new level is same
// as before.
TEST_F(ArcInstanceThrottleTest, TestOnObserverStateChanged) {
  std::vector<chromeos::ThrottleObserver*> observers = {critical_observer(),
                                                        low_observer()};
  arc_instance_throttle()->SetObserversForTesting(observers);
  EXPECT_EQ(0U, disable_cpu_restriction_counter());
  EXPECT_EQ(0U, enable_cpu_restriction_counter());

  arc_instance_throttle()->NotifyObserverStateChangedForTesting();
  EXPECT_EQ(1U, enable_cpu_restriction_counter());
  EXPECT_EQ(0U, disable_cpu_restriction_counter());

  // ArcInstanceThrottle level is already LOW, expect no change
  low_observer()->SetActive(true);
  EXPECT_EQ(1U, enable_cpu_restriction_counter());
  EXPECT_EQ(0U, disable_cpu_restriction_counter());

  critical_observer()->SetActive(true);
  EXPECT_EQ(1U, enable_cpu_restriction_counter());
  EXPECT_EQ(1U, disable_cpu_restriction_counter());

  critical_observer()->SetActive(false);
  EXPECT_EQ(2U, enable_cpu_restriction_counter());
  EXPECT_EQ(1U, disable_cpu_restriction_counter());

  arc_instance_throttle()->SetObserversForTesting({});
}

// Tests that ArcInstanceThrottle records the duration that the effective
// observer is active.
TEST_F(ArcInstanceThrottleTest, RecordCpuRestrictionDisabledUMA) {
  std::vector<chromeos::ThrottleObserver*> observers = {critical_observer(),
                                                        low_observer()};
  arc_instance_throttle()->SetObserversForTesting(observers);
  EXPECT_EQ(0U, uma_count());

  // The effective observer transitions from null to critical_observer; no UMA
  // is recorded yet.
  critical_observer()->SetActive(true);
  EXPECT_EQ(0U, uma_count());
  low_observer()->SetActive(true);
  EXPECT_EQ(0U, uma_count());

  // The effective observer transitions from critical_observer to low_observer;
  // UMA should be recorded for critical_observer.
  critical_observer()->SetActive(false);
  EXPECT_EQ(1U, uma_count());
  EXPECT_EQ(critical_observer()->name(), last_recorded_observer_name());

  // Effective observer transitions from low_observer to critical_observer; UMA
  // should be recorded for low_observer.
  critical_observer()->SetActive(true);
  EXPECT_EQ(2U, uma_count());
  EXPECT_EQ(low_observer()->name(), last_recorded_observer_name());

  // Effective observer transitions from critical_observer to null; UMA should
  // be recorded for critical_observer.
  low_observer()->SetActive(false);
  critical_observer()->SetActive(false);
  EXPECT_EQ(3U, uma_count());
  EXPECT_EQ(critical_observer()->name(), last_recorded_observer_name());

  arc_instance_throttle()->SetObserversForTesting({});
}

}  // namespace arc
