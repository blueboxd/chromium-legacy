// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_alarm_timer_controller_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/assistant/assistant_controller_impl.h"
#include "ash/assistant/assistant_notification_controller.h"
#include "ash/assistant/model/assistant_notification_model.h"
#include "ash/assistant/model/assistant_notification_model_observer.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/public/mojom/assistant_controller.mojom.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "base/test/icu_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

using chromeos::assistant::mojom::AssistantNotification;
using chromeos::assistant::mojom::AssistantNotificationButton;
using chromeos::assistant::mojom::AssistantNotificationButtonPtr;
using chromeos::assistant::mojom::AssistantNotificationPtr;

// Test Structs ----------------------------------------------------------------

// Represents a test instruction to advance the tick of the mock clock and
// assert an |expected_string|.
typedef struct {
  base::TimeDelta advance_clock;
  std::string expected_string;
} TestTick;

// Represents a |locale| specific test case containing assertions to be made at
// various |ticks| of the mock clock.
typedef struct {
  std::string locale;
  std::vector<TestTick> ticks;
} I18nTestCase;

// Timer Events ----------------------------------------------------------------

class TimerEvent {
 public:
  TimerEvent& WithLabel(const std::string& label) {
    timer_->label = label;
    return *this;
  }

  TimerEvent& WithOriginalDuration(base::TimeDelta original_duration) {
    timer_->original_duration = original_duration;
    return *this;
  }

  TimerEvent& WithRemainingTime(base::TimeDelta remaining_time) {
    timer_->fire_time = base::Time::Now() + remaining_time;
    timer_->remaining_time = remaining_time;
    return *this;
  }

 protected:
  TimerEvent(const std::string& id, AssistantTimerState state)
      : timer_(std::make_unique<AssistantTimer>()) {
    timer_->id = id;
    timer_->state = state;
    timer_->fire_time = base::Time::Now();
  }

  ~TimerEvent() {
    std::vector<AssistantTimerPtr> timers;
    timers.push_back(std::move(timer_));
    AssistantAlarmTimerController::Get()->OnTimerStateChanged(
        std::move(timers));
  }

 private:
  AssistantTimerPtr timer_;
};

class FireTimer : public TimerEvent {
 public:
  FireTimer(const std::string& id)
      : TimerEvent(id, AssistantTimerState::kFired) {}
};

class PauseTimer : public TimerEvent {
 public:
  PauseTimer(const std::string& id)
      : TimerEvent(id, AssistantTimerState::kPaused) {}
};

class ScheduleTimer : public TimerEvent {
 public:
  ScheduleTimer(const std::string& id)
      : TimerEvent(id, AssistantTimerState::kScheduled) {}
};

// Expectations ----------------------------------------------------------------

class ExpectButton {
 public:
  explicit ExpectButton(const AssistantNotificationButtonPtr& button)
      : button_(button.get()) {}

  ExpectButton(const ExpectButton&) = delete;
  ExpectButton& operator=(const ExpectButton&) = delete;
  ~ExpectButton() = default;

  const ExpectButton& HasLabel(int message_id) const {
    EXPECT_EQ(l10n_util::GetStringUTF8(message_id), button_->label);
    return *this;
  }

  const ExpectButton& HasActionUrl(const GURL& url) const {
    EXPECT_EQ(url, button_->action_url);
    return *this;
  }

 private:
  const AssistantNotificationButton* button_;
};

// ScopedNotificationModelObserver ---------------------------------------------

class ScopedNotificationModelObserver
    : public AssistantNotificationModelObserver {
 public:
  ScopedNotificationModelObserver() {
    Shell::Get()
        ->assistant_controller()
        ->notification_controller()
        ->model()
        ->AddObserver(this);
  }

  ~ScopedNotificationModelObserver() override {
    Shell::Get()
        ->assistant_controller()
        ->notification_controller()
        ->model()
        ->RemoveObserver(this);
  }

  // AssistantNotificationModelObserver:
  void OnNotificationAdded(const AssistantNotification* notification) override {
    last_notification_ = notification->Clone();
  }

  void OnNotificationUpdated(
      const AssistantNotification* notification) override {
    last_notification_ = notification->Clone();
  }

  const AssistantNotification* last_notification() const {
    return last_notification_.get();
  }

 private:
  AssistantNotificationPtr last_notification_;

  DISALLOW_COPY_AND_ASSIGN(ScopedNotificationModelObserver);
};

}  // namespace

// AssistantAlarmTimerControllerTest -------------------------------------------

class AssistantAlarmTimerControllerTest : public AshTestBase {
 protected:
  AssistantAlarmTimerControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ~AssistantAlarmTimerControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    feature_list_.InitAndDisableFeature(
        chromeos::assistant::features::kAssistantTimersV2);
  }

  // Advances the clock by |time_delta|, running any sequenced tasks in the
  // queue. Note that we don't use |TaskEnvironment::FastForwardBy| because that
  // API will hang when |time_delta| is sufficiently large, ultimately resulting
  // in unittest timeout.
  void AdvanceClock(base::TimeDelta time_delta) {
    task_environment()->AdvanceClock(time_delta);
    task_environment()->RunUntilIdle();
  }

  AssistantAlarmTimerController* controller() {
    return AssistantAlarmTimerController::Get();
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(AssistantAlarmTimerControllerTest);
};

// Tests that a notification is added for a timer and has the expected title.
// NOTE: This test is only applicable to timers v1.
TEST_F(AssistantAlarmTimerControllerTest, TimerNotificationHasExpectedTitle) {
  ASSERT_FALSE(chromeos::assistant::features::IsTimersV2Enabled());

  // Observe notifications.
  ScopedNotificationModelObserver notification_model_observer;

  // Fire a timer.
  FireTimer(/*id=*/"1");

  // We expect that a notification exists.
  auto* last_notification = notification_model_observer.last_notification();
  ASSERT_NE(nullptr, last_notification);
  EXPECT_EQ("assistant/timer1", last_notification->client_id);

  // We expect our title to be internationalized.
  const std::string expected_title =
      l10n_util::GetStringUTF8(IDS_ASSISTANT_TIMER_NOTIFICATION_TITLE);
  EXPECT_EQ(expected_title, last_notification->title);
}

// Tests that a notification is added for a timer and has the expected title at
// various states in its lifecycle.
// NOTE: This test is only applicable to timers v2.
TEST_F(AssistantAlarmTimerControllerTest, TimerNotificationHasExpectedTitleV2) {
  // Enable timers v2.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      chromeos::assistant::features::kAssistantTimersV2);
  ASSERT_TRUE(chromeos::assistant::features::IsTimersV2Enabled());

  // We're going to run our test over a few locales to ensure i18n compliance.
  std::vector<I18nTestCase> i18n_test_cases;

  // We'll test in English (United States).
  i18n_test_cases.push_back({
      /*locale=*/"en_US",
      /*ticks=*/
      {
          {base::TimeDelta(), "1:01:01"},
          {base::TimeDelta::FromHours(1), "1:01"},
          {base::TimeDelta::FromMinutes(1), "0:01"},
          {base::TimeDelta::FromSeconds(1), "0:00"},
          {base::TimeDelta::FromSeconds(1), "-0:01"},
          {base::TimeDelta::FromMinutes(1), "-1:01"},
          {base::TimeDelta::FromHours(1), "-1:01:01"},
      },
  });

  // We'll also test in Slovenian (Slovenia).
  i18n_test_cases.push_back({
      /*locale=*/"sl_SI",
      /*ticks=*/
      {
          {base::TimeDelta(), "1.01.01"},
          {base::TimeDelta::FromHours(1), "1.01"},
          {base::TimeDelta::FromMinutes(1), "0.01"},
          {base::TimeDelta::FromSeconds(1), "0.00"},
          {base::TimeDelta::FromSeconds(1), "-0.01"},
          {base::TimeDelta::FromMinutes(1), "-1.01"},
          {base::TimeDelta::FromHours(1), "-1.01.01"},
      },
  });

  // Run all of our internationalized test cases.
  for (auto& i18n_test_case : i18n_test_cases) {
    base::test::ScopedRestoreICUDefaultLocale locale(i18n_test_case.locale);

    // Observe notifications.
    ScopedNotificationModelObserver notification_model_observer;

    // Schedule a timer.
    ScheduleTimer(/*id=*/"1")
        .WithRemainingTime(base::TimeDelta::FromHours(1) +
                           base::TimeDelta::FromMinutes(1) +
                           base::TimeDelta::FromSeconds(1));

    // Run each tick of the clock in the test.
    for (auto& tick : i18n_test_case.ticks) {
      // Advance clock to next tick.
      AdvanceClock(tick.advance_clock);

      // Make assertions about the notification.
      auto* last_notification = notification_model_observer.last_notification();
      EXPECT_EQ("assistant/timer1", last_notification->client_id);
      EXPECT_EQ(tick.expected_string, last_notification->title);
    }
  }
}

// Tests that a notification is added for a timer and has the expected message
// at various states in its lifecycle.
// NOTE: This test is only applicable to timers v1.
TEST_F(AssistantAlarmTimerControllerTest, TimerNotificationHasExpectedMessage) {
  ASSERT_FALSE(chromeos::assistant::features::IsTimersV2Enabled());

  // We're going to run our test over a few locales to ensure i18n compliance.
  std::vector<I18nTestCase> i18n_test_cases;

  // We'll test in English (United States).
  i18n_test_cases.push_back({
      /*locale=*/"en_US",
      /*ticks=*/
      {
          {base::TimeDelta(), "0:00"},
          {base::TimeDelta::FromSeconds(1), "-0:01"},
          {base::TimeDelta::FromMinutes(1), "-1:01"},
          {base::TimeDelta::FromHours(1), "-1:01:01"},
      },
  });

  // We'll also test in Slovenian (Slovenia).
  i18n_test_cases.push_back({
      /*locale=*/"sl_SI",
      /*ticks=*/
      {
          {base::TimeDelta(), "0.00"},
          {base::TimeDelta::FromSeconds(1), "-0.01"},
          {base::TimeDelta::FromMinutes(1), "-1.01"},
          {base::TimeDelta::FromHours(1), "-1.01.01"},
      },
  });

  // Run all of our internationalized test cases.
  for (auto& i18n_test_case : i18n_test_cases) {
    base::test::ScopedRestoreICUDefaultLocale locale(i18n_test_case.locale);

    // Observe notifications.
    ScopedNotificationModelObserver notification_model_observer;

    // Fire a timer.
    FireTimer(/*id=*/"1");

    // Run each tick of the clock in the test.
    for (auto& tick : i18n_test_case.ticks) {
      // Advance clock to next tick.
      AdvanceClock(tick.advance_clock);

      // Make assertions about the notification.
      auto* last_notification = notification_model_observer.last_notification();
      EXPECT_EQ("assistant/timer1", last_notification->client_id);
      EXPECT_EQ(tick.expected_string, last_notification->message);
    }
  }
}

// TODO(dmblack): Add another locale after string translation.
// Tests that a notification is added for a timer and has the expected message.
// NOTE: This test is only applicable to timers v2.
TEST_F(AssistantAlarmTimerControllerTest,
       TimerNotificationHasExpectedMessageV2) {
  // Enable timers v2.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      chromeos::assistant::features::kAssistantTimersV2);
  ASSERT_TRUE(chromeos::assistant::features::IsTimersV2Enabled());

  constexpr char kEmptyLabel[] = "";
  constexpr base::TimeDelta kOneSec = base::TimeDelta::FromSeconds(1);
  constexpr base::TimeDelta kOneMin = base::TimeDelta::FromMinutes(1);
  constexpr base::TimeDelta kOneHour = base::TimeDelta::FromHours(1);

  // We'll verify the message of our notification with various timers.
  typedef struct {
    base::TimeDelta original_duration;
    std::string label;
    std::string expected_message;
  } TestTimer;

  // We're going to run our test over a few locales to ensure i18n compliance.
  typedef struct {
    std::string locale;
    std::vector<TestTimer> timers;
  } I18nTestCase;

  std::vector<I18nTestCase> i18n_test_cases;

  // We'll test in English (United States).
  i18n_test_cases.push_back({
      /*locale=*/"en_US",
      /*timers=*/
      {
          {kOneSec, kEmptyLabel, "1s timer"},
          {kOneSec, "Eggs", "1s timer · Eggs"},
          {kOneSec + kOneMin, kEmptyLabel, "1m 1s timer"},
          {kOneSec + kOneMin, "Eggs", "1m 1s timer · Eggs"},
          {kOneSec + kOneMin + kOneHour, kEmptyLabel, "1h 1m 1s timer"},
          {kOneSec + kOneMin + kOneHour, "Eggs", "1h 1m 1s timer · Eggs"},
      },
  });

  // Run all of our internationalized test cases.
  for (auto& i18n_test_case : i18n_test_cases) {
    base::test::ScopedRestoreICUDefaultLocale locale(i18n_test_case.locale);

    // Observe notifications.
    ScopedNotificationModelObserver notification_model_observer;

    // Run each timer in the test.
    for (auto& timer : i18n_test_case.timers) {
      // Schedule a timer.
      ScheduleTimer(/*id=*/"1")
          .WithLabel(timer.label)
          .WithOriginalDuration(timer.original_duration);

      // Make assertions about the notification.
      auto* last_notification = notification_model_observer.last_notification();
      ASSERT_NE(nullptr, last_notification);
      EXPECT_EQ("assistant/timer1", last_notification->client_id);
      EXPECT_EQ(timer.expected_message, last_notification->message);
    }
  }
}

// Tests that a notification is added when a timer is fired and has the expected
// buttons.
// NOTE: This test is only applicable to timers v1.
TEST_F(AssistantAlarmTimerControllerTest, TimerNotificationHasExpectedButtons) {
  ASSERT_FALSE(chromeos::assistant::features::IsTimersV2Enabled());

  // Observe notifications.
  ScopedNotificationModelObserver notification_model_observer;

  constexpr char kTimerId[] = "1";

  // Fire a timer.
  FireTimer(std::string(kTimerId));

  // We expect the timer notification to have two buttons.
  auto* last_notification = notification_model_observer.last_notification();
  ASSERT_EQ(2u, last_notification->buttons.size());

  // We expect a "STOP" button which will remove the timer.
  ExpectButton(last_notification->buttons.at(0))
      .HasLabel(IDS_ASSISTANT_TIMER_NOTIFICATION_STOP_BUTTON)
      .HasActionUrl(
          assistant::util::CreateAlarmTimerDeepLink(
              assistant::util::AlarmTimerAction::kRemoveAlarmOrTimer, kTimerId)
              .value());

  // We expect an "ADD 1 MIN" button which will add time to the timer.
  ExpectButton(last_notification->buttons.at(1))
      .HasLabel(IDS_ASSISTANT_TIMER_NOTIFICATION_ADD_1_MIN_BUTTON)
      .HasActionUrl(assistant::util::CreateAlarmTimerDeepLink(
                        assistant::util::AlarmTimerAction::kAddTimeToTimer,
                        kTimerId, base::TimeDelta::FromMinutes(1))
                        .value());
}

// Tests that a notification is added for a timer and has the expected buttons
// at each state in its lifecycle.
// NOTE: This test is only applicable to timers v2.
TEST_F(AssistantAlarmTimerControllerTest,
       TimerNotificationHasExpectedButtonsV2) {
  // Enable timers v2.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      chromeos::assistant::features::kAssistantTimersV2);
  ASSERT_TRUE(chromeos::assistant::features::IsTimersV2Enabled());

  // Observe notifications.
  ScopedNotificationModelObserver notification_model_observer;

  constexpr char kTimerId[] = "1";
  constexpr base::TimeDelta kTimeRemaining = base::TimeDelta::FromMinutes(1);

  // Schedule a timer.
  ScheduleTimer(kTimerId).WithRemainingTime(kTimeRemaining);

  // We expect the timer notification to have two buttons.
  auto* last_notification = notification_model_observer.last_notification();
  ASSERT_EQ(2u, last_notification->buttons.size());

  // We expect a "PAUSE" button which will pause the timer.
  ExpectButton(last_notification->buttons.at(0))
      .HasLabel(IDS_ASSISTANT_TIMER_NOTIFICATION_PAUSE_BUTTON)
      .HasActionUrl(
          assistant::util::CreateAlarmTimerDeepLink(
              assistant::util::AlarmTimerAction::kPauseTimer, kTimerId)
              .value());

  // We expect a "CANCEL" button which will remove the timer.
  ExpectButton(last_notification->buttons.at(1))
      .HasLabel(IDS_ASSISTANT_TIMER_NOTIFICATION_CANCEL_BUTTON)
      .HasActionUrl(
          assistant::util::CreateAlarmTimerDeepLink(
              assistant::util::AlarmTimerAction::kRemoveAlarmOrTimer, kTimerId)
              .value());

  // Pause the timer.
  PauseTimer(kTimerId).WithRemainingTime(kTimeRemaining);

  // We expect the timer notification to have two buttons.
  last_notification = notification_model_observer.last_notification();
  ASSERT_EQ(2u, last_notification->buttons.size());

  // We expect a "RESUME" button which will resume the timer.
  ExpectButton(last_notification->buttons.at(0))
      .HasLabel(IDS_ASSISTANT_TIMER_NOTIFICATION_RESUME_BUTTON)
      .HasActionUrl(
          assistant::util::CreateAlarmTimerDeepLink(
              assistant::util::AlarmTimerAction::kResumeTimer, kTimerId)
              .value());

  // We expect a "CANCEL" button which will remove the timer.
  ExpectButton(last_notification->buttons.at(1))
      .HasLabel(IDS_ASSISTANT_TIMER_NOTIFICATION_CANCEL_BUTTON)
      .HasActionUrl(
          assistant::util::CreateAlarmTimerDeepLink(
              assistant::util::AlarmTimerAction::kRemoveAlarmOrTimer, kTimerId)
              .value());

  // Fire the timer.
  FireTimer(std::string(kTimerId));

  // We expect the timer notification to have two buttons.
  last_notification = notification_model_observer.last_notification();
  ASSERT_EQ(2u, last_notification->buttons.size());

  // We expect a "CANCEL" button which will remove the timer.
  ExpectButton(last_notification->buttons.at(0))
      .HasLabel(IDS_ASSISTANT_TIMER_NOTIFICATION_CANCEL_BUTTON)
      .HasActionUrl(
          assistant::util::CreateAlarmTimerDeepLink(
              assistant::util::AlarmTimerAction::kRemoveAlarmOrTimer, kTimerId)
              .value());

  // We expect an "ADD 1 MIN" button which will add time to the timer.
  ExpectButton(last_notification->buttons.at(1))
      .HasLabel(IDS_ASSISTANT_TIMER_NOTIFICATION_ADD_1_MIN_BUTTON)
      .HasActionUrl(assistant::util::CreateAlarmTimerDeepLink(
                        assistant::util::AlarmTimerAction::kAddTimeToTimer,
                        kTimerId, base::TimeDelta::FromMinutes(1))
                        .value());
}

}  // namespace ash
