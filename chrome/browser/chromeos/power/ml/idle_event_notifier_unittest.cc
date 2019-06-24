// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/ml/idle_event_notifier.h"

#include <memory>

#include "base/test/test_mock_time_task_runner.h"
#include "base/time/clock.h"
#include "chrome/browser/chromeos/power/ml/fake_boot_clock.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"

namespace chromeos {
namespace power {
namespace ml {

namespace {

bool operator==(const IdleEventNotifier::ActivityData& x,
                const IdleEventNotifier::ActivityData& y) {
  return x.last_activity_day == y.last_activity_day &&
         x.last_activity_time_of_day == y.last_activity_time_of_day &&
         x.last_user_activity_time_of_day == y.last_user_activity_time_of_day &&
         x.recent_time_active == y.recent_time_active &&
         x.time_since_last_key == y.time_since_last_key &&
         x.time_since_last_mouse == y.time_since_last_mouse &&
         x.time_since_last_touch == y.time_since_last_touch &&
         x.video_playing_time == y.video_playing_time &&
         x.time_since_video_ended == y.time_since_video_ended &&
         x.key_events_in_last_hour == y.key_events_in_last_hour &&
         x.mouse_events_in_last_hour == y.mouse_events_in_last_hour &&
         x.touch_events_in_last_hour == y.touch_events_in_last_hour;
}

base::TimeDelta GetTimeSinceMidnight(base::Time time) {
  return time - time.LocalMidnight();
}

UserActivityEvent_Features_DayOfWeek GetDayOfWeek(base::Time time) {
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);
  return static_cast<UserActivityEvent_Features_DayOfWeek>(
      exploded.day_of_week);
}

class TestObserver : public IdleEventNotifier::Observer {
 public:
  TestObserver() : idle_event_count_(0) {}
  ~TestObserver() override {}

  int idle_event_count() const { return idle_event_count_; }
  const IdleEventNotifier::ActivityData& activity_data() const {
    return activity_data_;
  }

  // IdleEventNotifier::Observer overrides:
  void OnIdleEventObserved(
      const IdleEventNotifier::ActivityData& activity_data) override {
    ++idle_event_count_;
    activity_data_ = activity_data;
  }

 private:
  int idle_event_count_;
  IdleEventNotifier::ActivityData activity_data_;
  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

}  // namespace

class IdleEventNotifierTest : public testing::Test {
 public:
  IdleEventNotifierTest()
      : scoped_task_env_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME,
            base::test::ScopedTaskEnvironment::ThreadPoolExecutionMode::
                QUEUED) {}

  ~IdleEventNotifierTest() override = default;

  void SetUp() override {
    PowerManagerClient::InitializeFake();
    viz::mojom::VideoDetectorObserverPtr observer;
    idle_event_notifier_ = std::make_unique<IdleEventNotifier>(
        PowerManagerClient::Get(), &user_activity_detector_,
        mojo::MakeRequest(&observer));
    idle_event_notifier_->SetClockForTesting(
        scoped_task_env_.GetMainThreadTaskRunner(),
        const_cast<base::Clock*>(scoped_task_env_.GetMockClock()),
        std::make_unique<FakeBootClock>(&scoped_task_env_,
                                        base::TimeDelta::FromSeconds(10)));
    idle_event_notifier_->AddObserver(&test_observer_);
    ac_power_.set_external_power(
        power_manager::PowerSupplyProperties_ExternalPower_AC);
    disconnected_power_.set_external_power(
        power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED);
  }

  void TearDown() override {
    idle_event_notifier_.reset();
    PowerManagerClient::Shutdown();
  }

 protected:
  void ReportScreenDimImminent() {
    FakePowerManagerClient::Get()->SendScreenDimImminent();
  }

  void ReportIdleEventAndCheckResults(
      int expected_idle_count,
      const IdleEventNotifier::ActivityData& expected_activity_data) {
    ReportScreenDimImminent();
    EXPECT_EQ(expected_idle_count, test_observer_.idle_event_count());
    EXPECT_TRUE(expected_activity_data == test_observer_.activity_data());
  }

  base::test::ScopedTaskEnvironment scoped_task_env_;

  TestObserver test_observer_;
  std::unique_ptr<IdleEventNotifier> idle_event_notifier_;
  power_manager::PowerSupplyProperties ac_power_;
  power_manager::PowerSupplyProperties disconnected_power_;
  ui::UserActivityDetector user_activity_detector_;

 private:
  DISALLOW_COPY_AND_ASSIGN(IdleEventNotifierTest);
};

// After initialization, |external_power_| is not set up.
TEST_F(IdleEventNotifierTest, CheckInitialValues) {
  EXPECT_FALSE(idle_event_notifier_->external_power_);
}

// Lid is opened, followed by an idle event.
TEST_F(IdleEventNotifierTest, LidOpenEventReceived) {
  base::Time now = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->LidEventReceived(
      chromeos::PowerManagerClient::LidState::OPEN,
      base::TimeTicks::UnixEpoch());
  IdleEventNotifier::ActivityData data;
  data.last_activity_day = GetDayOfWeek(now);
  const base::TimeDelta time_of_day = GetTimeSinceMidnight(now);
  data.last_activity_time_of_day = time_of_day;
  data.last_user_activity_time_of_day = time_of_day;
  data.recent_time_active = base::TimeDelta();
  ReportIdleEventAndCheckResults(1, data);
}

// PowerChanged signal is received but source isn't changed, so it won't change
// ActivityData that gets reported when an idle event is received.
TEST_F(IdleEventNotifierTest, PowerSourceNotChanged) {
  base::Time now = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->PowerChanged(ac_power_);
  IdleEventNotifier::ActivityData data;
  data.last_activity_day = GetDayOfWeek(now);
  const base::TimeDelta time_of_day = GetTimeSinceMidnight(now);
  data.last_activity_time_of_day = time_of_day;
  data.last_user_activity_time_of_day = time_of_day;
  data.recent_time_active = base::TimeDelta();
  ReportIdleEventAndCheckResults(1, data);

  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(10));
  idle_event_notifier_->PowerChanged(ac_power_);
  ReportIdleEventAndCheckResults(2, data);
}

// PowerChanged signal is received and source is changed, so a different
// ActivityData gets reported when the 2nd idle event is received.
TEST_F(IdleEventNotifierTest, PowerSourceChanged) {
  base::Time now_1 = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->PowerChanged(ac_power_);
  IdleEventNotifier::ActivityData data_1;
  data_1.last_activity_day = GetDayOfWeek(now_1);
  const base::TimeDelta time_of_day_1 = GetTimeSinceMidnight(now_1);
  data_1.last_activity_time_of_day = time_of_day_1;
  data_1.last_user_activity_time_of_day = time_of_day_1;
  data_1.recent_time_active = base::TimeDelta();
  ReportIdleEventAndCheckResults(1, data_1);

  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(100));
  base::Time now_2 = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->PowerChanged(disconnected_power_);
  IdleEventNotifier::ActivityData data_2;
  data_2.last_activity_day = GetDayOfWeek(now_2);
  const base::TimeDelta time_of_day_2 = GetTimeSinceMidnight(now_2);
  data_2.last_activity_time_of_day = time_of_day_2;
  data_2.last_user_activity_time_of_day = time_of_day_2;
  data_2.recent_time_active = base::TimeDelta();
  ReportIdleEventAndCheckResults(2, data_2);
}

// Short sleep duration does not break up recent time active.
TEST_F(IdleEventNotifierTest, ShortSuspendDone) {
  base::Time now_1 = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->PowerChanged(ac_power_);

  scoped_task_env_.FastForwardBy(IdleEventNotifier::kIdleDelay / 2);
  idle_event_notifier_->SuspendDone(IdleEventNotifier::kIdleDelay / 2);

  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(5));
  idle_event_notifier_->PowerChanged(disconnected_power_);
  base::Time now_2 = scoped_task_env_.GetMockClock()->Now();

  IdleEventNotifier::ActivityData data;
  data.last_activity_day = GetDayOfWeek(now_2);
  const base::TimeDelta time_of_day = GetTimeSinceMidnight(now_2);
  data.last_activity_time_of_day = time_of_day;
  data.last_user_activity_time_of_day = time_of_day;
  data.recent_time_active = now_2 - now_1;
  ReportIdleEventAndCheckResults(1, data);
}

// Long sleep duration recalc recent time active.
TEST_F(IdleEventNotifierTest, LongSuspendDone) {
  idle_event_notifier_->PowerChanged(ac_power_);

  scoped_task_env_.FastForwardBy(IdleEventNotifier::kIdleDelay +
                                 base::TimeDelta::FromSeconds(10));
  base::Time now_1 = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->SuspendDone(IdleEventNotifier::kIdleDelay +
                                    base::TimeDelta::FromSeconds(10));

  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(10));
  base::Time now_2 = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->PowerChanged(disconnected_power_);

  IdleEventNotifier::ActivityData data;
  data.last_activity_day = GetDayOfWeek(now_2);
  const base::TimeDelta time_of_day = GetTimeSinceMidnight(now_2);
  data.last_activity_time_of_day = time_of_day;
  data.last_user_activity_time_of_day = time_of_day;
  data.recent_time_active = now_2 - now_1;
  ReportIdleEventAndCheckResults(1, data);
}

TEST_F(IdleEventNotifierTest, UserActivityKey) {
  base::Time now = scoped_task_env_.GetMockClock()->Now();
  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::EF_NONE);
  idle_event_notifier_->OnUserActivity(&key_event);
  IdleEventNotifier::ActivityData data;
  data.last_activity_day = GetDayOfWeek(now);
  const base::TimeDelta time_of_day = GetTimeSinceMidnight(now);
  data.last_activity_time_of_day = time_of_day;
  data.last_user_activity_time_of_day = time_of_day;
  data.recent_time_active = base::TimeDelta();
  data.time_since_last_key = base::TimeDelta::FromSeconds(10);
  data.key_events_in_last_hour = 1;
  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(10));
  ReportIdleEventAndCheckResults(1, data);
}

TEST_F(IdleEventNotifierTest, UserActivityMouse) {
  base::Time now = scoped_task_env_.GetMockClock()->Now();
  ui::MouseEvent mouse_event(ui::ET_MOUSE_EXITED, gfx::Point(0, 0),
                             gfx::Point(0, 0), base::TimeTicks(), 0, 0);

  idle_event_notifier_->OnUserActivity(&mouse_event);
  IdleEventNotifier::ActivityData data;
  data.last_activity_day = GetDayOfWeek(now);
  const base::TimeDelta time_of_day = GetTimeSinceMidnight(now);
  data.last_activity_time_of_day = time_of_day;
  data.last_user_activity_time_of_day = time_of_day;
  data.recent_time_active = base::TimeDelta();
  data.time_since_last_mouse = base::TimeDelta::FromSeconds(10);
  data.mouse_events_in_last_hour = 1;
  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(10));
  ReportIdleEventAndCheckResults(1, data);
}

TEST_F(IdleEventNotifierTest, UserActivityOther) {
  base::Time now = scoped_task_env_.GetMockClock()->Now();
  ui::GestureEvent gesture_event(0, 0, 0, base::TimeTicks(),
                                 ui::GestureEventDetails(ui::ET_GESTURE_TAP));

  idle_event_notifier_->OnUserActivity(&gesture_event);
  IdleEventNotifier::ActivityData data;
  data.last_activity_day = GetDayOfWeek(now);
  const base::TimeDelta time_of_day = GetTimeSinceMidnight(now);
  data.last_activity_time_of_day = time_of_day;
  data.last_user_activity_time_of_day = time_of_day;
  data.recent_time_active = base::TimeDelta();
  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(10));
  ReportIdleEventAndCheckResults(1, data);
}

// Two consecutive activities separated by 2sec only. Only 1 idle event with
// different timestamps for the two activities.
TEST_F(IdleEventNotifierTest, TwoQuickUserActivities) {
  base::Time now_1 = scoped_task_env_.GetMockClock()->Now();
  ui::MouseEvent mouse_event(ui::ET_MOUSE_EXITED, gfx::Point(0, 0),
                             gfx::Point(0, 0), base::TimeTicks(), 0, 0);
  idle_event_notifier_->OnUserActivity(&mouse_event);

  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(2));
  base::Time now_2 = scoped_task_env_.GetMockClock()->Now();
  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::EF_NONE);
  idle_event_notifier_->OnUserActivity(&key_event);

  IdleEventNotifier::ActivityData data;
  data.last_activity_day = GetDayOfWeek(now_2);
  const base::TimeDelta time_of_day = GetTimeSinceMidnight(now_2);
  data.last_activity_time_of_day = time_of_day;
  data.last_user_activity_time_of_day = time_of_day;
  data.recent_time_active = now_2 - now_1;
  data.time_since_last_key = base::TimeDelta::FromSeconds(10);
  data.time_since_last_mouse =
      base::TimeDelta::FromSeconds(10) + (now_2 - now_1);
  data.key_events_in_last_hour = 1;
  data.mouse_events_in_last_hour = 1;
  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(10));
  ReportIdleEventAndCheckResults(1, data);
}

TEST_F(IdleEventNotifierTest, ActivityAfterVideoStarts) {
  ui::MouseEvent mouse_event(ui::ET_MOUSE_EXITED, gfx::Point(0, 0),
                             gfx::Point(0, 0), base::TimeTicks(), 0, 0);
  base::Time now_1 = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->OnVideoActivityStarted();

  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  base::Time now_2 = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->OnUserActivity(&mouse_event);

  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(2));
  base::Time now_3 = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->OnVideoActivityEnded();

  IdleEventNotifier::ActivityData data;
  data.last_activity_day = GetDayOfWeek(now_3);
  data.last_activity_time_of_day = GetTimeSinceMidnight(now_3);
  data.last_user_activity_time_of_day = GetTimeSinceMidnight(now_2);
  data.recent_time_active = now_3 - now_1;
  data.time_since_last_mouse = base::TimeDelta::FromSeconds(10) + now_3 - now_2;
  data.video_playing_time = now_3 - now_1;
  data.time_since_video_ended = base::TimeDelta::FromSeconds(10);
  data.mouse_events_in_last_hour = 1;
  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(10));
  ReportIdleEventAndCheckResults(1, data);
}

TEST_F(IdleEventNotifierTest, IdleEventFieldReset) {
  base::Time now_1 = scoped_task_env_.GetMockClock()->Now();
  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::EF_NONE);
  idle_event_notifier_->OnUserActivity(&key_event);

  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(10));
  base::Time now_2 = scoped_task_env_.GetMockClock()->Now();
  ui::MouseEvent mouse_event(ui::ET_MOUSE_EXITED, gfx::Point(0, 0),
                             gfx::Point(0, 0), base::TimeTicks(), 0, 0);
  idle_event_notifier_->OnUserActivity(&mouse_event);

  IdleEventNotifier::ActivityData data_1;
  data_1.last_activity_day = GetDayOfWeek(now_2);
  const base::TimeDelta time_of_day_2 = GetTimeSinceMidnight(now_2);
  data_1.last_activity_time_of_day = time_of_day_2;
  data_1.last_user_activity_time_of_day = time_of_day_2;
  data_1.recent_time_active = now_2 - now_1;
  data_1.time_since_last_key = base::TimeDelta::FromSeconds(10) + now_2 - now_1;
  data_1.time_since_last_mouse = base::TimeDelta::FromSeconds(10);
  data_1.key_events_in_last_hour = 1;
  data_1.mouse_events_in_last_hour = 1;
  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(10));
  ReportIdleEventAndCheckResults(1, data_1);

  idle_event_notifier_->PowerChanged(ac_power_);
  base::Time now_3 = scoped_task_env_.GetMockClock()->Now();

  IdleEventNotifier::ActivityData data_2;
  data_2.last_activity_day = GetDayOfWeek(now_3);
  const base::TimeDelta time_of_day_3 = GetTimeSinceMidnight(now_3);
  data_2.last_activity_time_of_day = time_of_day_3;
  data_2.last_user_activity_time_of_day = time_of_day_3;
  data_2.recent_time_active = base::TimeDelta();
  data_2.time_since_last_key = base::TimeDelta::FromSeconds(20) + now_3 - now_1;
  data_2.time_since_last_mouse =
      base::TimeDelta::FromSeconds(20) + now_3 - now_2;
  data_2.key_events_in_last_hour = 1;
  data_2.mouse_events_in_last_hour = 1;
  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(20));
  ReportIdleEventAndCheckResults(2, data_2);
}

TEST_F(IdleEventNotifierTest, TwoConsecutiveVideoPlaying) {
  // Two video playing sessions with a gap shorter than kIdleDelay. They are
  // merged into one playing session.
  base::Time now_1 = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->OnVideoActivityStarted();

  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(2));
  idle_event_notifier_->OnVideoActivityEnded();

  scoped_task_env_.FastForwardBy(IdleEventNotifier::kIdleDelay / 2);
  idle_event_notifier_->OnVideoActivityStarted();

  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(20));
  base::Time now_2 = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->OnVideoActivityEnded();

  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(10));
  base::Time now_3 = scoped_task_env_.GetMockClock()->Now();
  ui::MouseEvent mouse_event(ui::ET_MOUSE_EXITED, gfx::Point(0, 0),
                             gfx::Point(0, 0), base::TimeTicks(), 0, 0);
  idle_event_notifier_->OnUserActivity(&mouse_event);

  IdleEventNotifier::ActivityData data;
  data.last_activity_day = GetDayOfWeek(now_3);
  data.last_activity_time_of_day = GetTimeSinceMidnight(now_3);
  data.last_user_activity_time_of_day = GetTimeSinceMidnight(now_3);
  data.recent_time_active = now_3 - now_1;
  data.time_since_last_mouse = base::TimeDelta::FromSeconds(25);
  data.mouse_events_in_last_hour = 1;
  data.video_playing_time = now_2 - now_1;
  data.time_since_video_ended =
      base::TimeDelta::FromSeconds(25) + now_3 - now_2;
  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(25));
  ReportIdleEventAndCheckResults(1, data);
}

TEST_F(IdleEventNotifierTest, TwoVideoPlayingFarApartOneIdleEvent) {
  // Two video playing sessions with a gap larger than kIdleDelay.
  base::Time now_1 = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->OnVideoActivityStarted();

  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(2));
  idle_event_notifier_->OnVideoActivityEnded();

  ui::MouseEvent mouse_event(ui::ET_MOUSE_EXITED, gfx::Point(0, 0),
                             gfx::Point(0, 0), base::TimeTicks(), 0, 0);

  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(20));
  idle_event_notifier_->OnUserActivity(&mouse_event);
  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(20));
  idle_event_notifier_->OnUserActivity(&mouse_event);

  base::Time now_2 = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->OnVideoActivityStarted();
  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(20));
  base::Time now_3 = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->OnVideoActivityEnded();

  IdleEventNotifier::ActivityData data;
  data.last_activity_day = GetDayOfWeek(now_3);
  data.last_activity_time_of_day = GetTimeSinceMidnight(now_3);
  data.last_user_activity_time_of_day = GetTimeSinceMidnight(now_2);
  data.recent_time_active = now_3 - now_1;
  data.time_since_last_mouse = base::TimeDelta::FromSeconds(25) + now_3 - now_2;
  data.mouse_events_in_last_hour = 2;
  data.video_playing_time = now_3 - now_2;
  data.time_since_video_ended = base::TimeDelta::FromSeconds(25);
  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(25));
  ReportIdleEventAndCheckResults(1, data);
}

TEST_F(IdleEventNotifierTest, TwoVideoPlayingFarApartTwoIdleEvents) {
  // Two video playing sessions with a gap equal to kIdleDelay. An idle event
  // is generated in between, both video sessions are reported.
  base::Time now_1 = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->OnVideoActivityStarted();

  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(2));
  base::Time now_2 = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->OnVideoActivityEnded();

  IdleEventNotifier::ActivityData data_1;
  data_1.last_activity_day = GetDayOfWeek(now_2);
  data_1.last_activity_time_of_day = GetTimeSinceMidnight(now_2);
  data_1.recent_time_active = now_2 - now_1;
  data_1.video_playing_time = now_2 - now_1;
  data_1.time_since_video_ended =
      IdleEventNotifier::kIdleDelay + base::TimeDelta::FromSeconds(10);
  scoped_task_env_.FastForwardBy(IdleEventNotifier::kIdleDelay +
                                 base::TimeDelta::FromSeconds(10));
  ReportIdleEventAndCheckResults(1, data_1);

  base::Time now_3 = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->OnVideoActivityStarted();
  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(20));
  base::Time now_4 = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->OnVideoActivityEnded();

  IdleEventNotifier::ActivityData data_2;
  data_2.last_activity_day = GetDayOfWeek(now_4);
  data_2.last_activity_time_of_day = GetTimeSinceMidnight(now_4);
  data_2.recent_time_active = now_4 - now_3;
  data_2.video_playing_time = now_4 - now_3;
  data_2.time_since_video_ended = base::TimeDelta();
  ReportIdleEventAndCheckResults(2, data_2);
}

TEST_F(IdleEventNotifierTest, TwoVideoPlayingSeparatedByAnIdleEvent) {
  // Two video playing sessions with gap shorter than kIdleDelay but separated
  // by an idle event. They are considered as two video sessions.
  const base::Time kNow1 = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->OnVideoActivityStarted();

  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(2));
  const base::Time kNow2 = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->OnVideoActivityEnded();

  IdleEventNotifier::ActivityData data_1;
  data_1.last_activity_day = GetDayOfWeek(kNow2);
  data_1.last_activity_time_of_day = GetTimeSinceMidnight(kNow2);
  data_1.recent_time_active = kNow2 - kNow1;
  data_1.video_playing_time = kNow2 - kNow1;
  data_1.time_since_video_ended = base::TimeDelta::FromSeconds(1);
  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  ReportIdleEventAndCheckResults(1, data_1);

  const base::Time kNow3 = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->OnVideoActivityStarted();
  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(20));
  const base::Time kNow4 = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->OnVideoActivityEnded();

  IdleEventNotifier::ActivityData data_2;
  data_2.last_activity_day = GetDayOfWeek(kNow4);
  data_2.last_activity_time_of_day = GetTimeSinceMidnight(kNow4);
  data_2.recent_time_active = kNow4 - kNow3;
  data_2.video_playing_time = kNow4 - kNow3;
  data_2.time_since_video_ended = base::TimeDelta();
  ReportIdleEventAndCheckResults(2, data_2);
}

TEST_F(IdleEventNotifierTest, VideoPlayingPausedByShortSuspend) {
  base::Time now_1 = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->OnVideoActivityStarted();

  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(100));
  base::Time now_2 = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->SuspendDone(IdleEventNotifier::kIdleDelay / 2);

  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(20));
  base::Time now_3 = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->OnVideoActivityEnded();

  IdleEventNotifier::ActivityData data;
  data.last_activity_day = GetDayOfWeek(now_3);
  data.last_activity_time_of_day = GetTimeSinceMidnight(now_3);
  data.last_user_activity_time_of_day = GetTimeSinceMidnight(now_2);
  data.recent_time_active = now_3 - now_1;
  data.video_playing_time = now_3 - now_1;
  data.time_since_video_ended = base::TimeDelta();
  ReportIdleEventAndCheckResults(1, data);
}

TEST_F(IdleEventNotifierTest, VideoPlayingPausedByLongSuspend) {
  idle_event_notifier_->OnVideoActivityStarted();

  scoped_task_env_.FastForwardBy(2 * IdleEventNotifier::kIdleDelay);
  base::Time now_1 = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->SuspendDone(2 * IdleEventNotifier::kIdleDelay);

  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(20));
  base::Time now_2 = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->OnVideoActivityEnded();

  IdleEventNotifier::ActivityData data;
  data.last_activity_day = GetDayOfWeek(now_2);
  data.last_activity_time_of_day = GetTimeSinceMidnight(now_2);
  data.last_user_activity_time_of_day = GetTimeSinceMidnight(now_1);
  data.recent_time_active = now_2 - now_1;
  data.video_playing_time = now_2 - now_1;
  data.time_since_video_ended = base::TimeDelta();
  ReportIdleEventAndCheckResults(1, data);
}

TEST_F(IdleEventNotifierTest, UserInputEventsOneIdleEvent) {
  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::EF_NONE);
  ui::MouseEvent mouse_event(ui::ET_MOUSE_EXITED, gfx::Point(0, 0),
                             gfx::Point(0, 0), base::TimeTicks(), 0, 0);
  ui::TouchEvent touch_event(
      ui::ET_TOUCH_PRESSED, gfx::Point(0, 0), base::TimeTicks::UnixEpoch(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));

  base::Time first_activity_time = scoped_task_env_.GetMockClock()->Now();
  // This key event will be too old to be counted.
  idle_event_notifier_->OnUserActivity(&key_event);

  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(10));
  base::Time video_start_time = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->OnVideoActivityStarted();

  scoped_task_env_.FastForwardBy(base::TimeDelta::FromMinutes(11));
  base::Time last_key_time = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->OnUserActivity(&key_event);

  scoped_task_env_.FastForwardBy(base::TimeDelta::FromMinutes(11));
  idle_event_notifier_->OnUserActivity(&mouse_event);

  scoped_task_env_.FastForwardBy(base::TimeDelta::FromMinutes(11));
  idle_event_notifier_->OnUserActivity(&touch_event);

  scoped_task_env_.FastForwardBy(base::TimeDelta::FromMinutes(11));
  idle_event_notifier_->OnUserActivity(&touch_event);

  scoped_task_env_.FastForwardBy(base::TimeDelta::FromMinutes(11));
  base::Time last_touch_time = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->OnUserActivity(&touch_event);

  scoped_task_env_.FastForwardBy(base::TimeDelta::FromMinutes(11));
  base::Time last_mouse_time = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->OnUserActivity(&mouse_event);

  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(10));
  base::Time video_end_time = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->OnVideoActivityEnded();

  IdleEventNotifier::ActivityData data;
  data.last_activity_day = GetDayOfWeek(video_end_time);
  data.last_activity_time_of_day = GetTimeSinceMidnight(video_end_time);
  data.last_user_activity_time_of_day = GetTimeSinceMidnight(last_mouse_time);
  data.recent_time_active = video_end_time - first_activity_time;
  data.video_playing_time = video_end_time - video_start_time;
  data.time_since_video_ended = base::TimeDelta::FromSeconds(30);
  data.time_since_last_key =
      base::TimeDelta::FromSeconds(30) + video_end_time - last_key_time;
  data.time_since_last_mouse =
      base::TimeDelta::FromSeconds(30) + video_end_time - last_mouse_time;
  data.time_since_last_touch =
      base::TimeDelta::FromSeconds(30) + video_end_time - last_touch_time;

  data.key_events_in_last_hour = 1;
  data.mouse_events_in_last_hour = 2;
  data.touch_events_in_last_hour = 3;

  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(30));
  ReportIdleEventAndCheckResults(1, data);
}

TEST_F(IdleEventNotifierTest, UserInputEventsTwoIdleEvents) {
  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::EF_NONE);
  ui::MouseEvent mouse_event(ui::ET_MOUSE_EXITED, gfx::Point(0, 0),
                             gfx::Point(0, 0), base::TimeTicks(), 0, 0);
  ui::TouchEvent touch_event(
      ui::ET_TOUCH_PRESSED, gfx::Point(0, 0), base::TimeTicks::UnixEpoch(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));

  base::Time now_1 = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->OnUserActivity(&key_event);

  IdleEventNotifier::ActivityData data_1;
  data_1.last_activity_day = GetDayOfWeek(now_1);
  data_1.last_activity_time_of_day = GetTimeSinceMidnight(now_1);
  data_1.last_user_activity_time_of_day = GetTimeSinceMidnight(now_1);
  data_1.recent_time_active = base::TimeDelta();
  data_1.time_since_last_key = base::TimeDelta::FromSeconds(30);
  data_1.key_events_in_last_hour = 1;
  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(30));
  ReportIdleEventAndCheckResults(1, data_1);

  scoped_task_env_.FastForwardBy(base::TimeDelta::FromMinutes(11));
  base::Time last_key_time = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->OnUserActivity(&key_event);

  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(10));
  base::Time video_start_time = scoped_task_env_.GetMockClock()->Now();
  // Keep playing video so we won't run into an idle event.
  idle_event_notifier_->OnVideoActivityStarted();

  scoped_task_env_.FastForwardBy(base::TimeDelta::FromMinutes(11));
  idle_event_notifier_->OnUserActivity(&mouse_event);

  scoped_task_env_.FastForwardBy(base::TimeDelta::FromMinutes(11));
  idle_event_notifier_->OnUserActivity(&touch_event);

  scoped_task_env_.FastForwardBy(base::TimeDelta::FromMinutes(11));
  idle_event_notifier_->OnUserActivity(&touch_event);

  scoped_task_env_.FastForwardBy(base::TimeDelta::FromMinutes(11));
  base::Time last_touch_time = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->OnUserActivity(&touch_event);

  scoped_task_env_.FastForwardBy(base::TimeDelta::FromMinutes(11));
  base::Time last_mouse_time = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->OnUserActivity(&mouse_event);

  scoped_task_env_.FastForwardBy(base::TimeDelta::FromSeconds(10));
  base::Time video_end_time = scoped_task_env_.GetMockClock()->Now();
  idle_event_notifier_->OnVideoActivityEnded();

  IdleEventNotifier::ActivityData data_2;
  data_2.last_activity_day = GetDayOfWeek(video_end_time);
  data_2.last_activity_time_of_day = GetTimeSinceMidnight(video_end_time);
  data_2.last_user_activity_time_of_day = GetTimeSinceMidnight(last_mouse_time);
  data_2.recent_time_active = video_end_time - last_key_time;
  data_2.video_playing_time = video_end_time - video_start_time;
  data_2.time_since_video_ended = base::TimeDelta();
  data_2.time_since_last_key = video_end_time - last_key_time;
  data_2.time_since_last_mouse = video_end_time - last_mouse_time;
  data_2.time_since_last_touch = video_end_time - last_touch_time;

  data_2.key_events_in_last_hour = 1;
  data_2.mouse_events_in_last_hour = 2;
  data_2.touch_events_in_last_hour = 3;

  ReportIdleEventAndCheckResults(2, data_2);
}

}  // namespace ml
}  // namespace power
}  // namespace chromeos
