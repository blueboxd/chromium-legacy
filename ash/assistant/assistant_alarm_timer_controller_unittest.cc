// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_alarm_timer_controller.h"

#include <memory>
#include <vector>

#include "ash/assistant/assistant_controller_impl.h"
#include "ash/assistant/assistant_notification_controller.h"
#include "ash/assistant/model/assistant_notification_model.h"
#include "ash/assistant/model/assistant_notification_model_observer.h"
#include "ash/public/mojom/assistant_controller.mojom.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "base/test/icu_test_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

// Helpers ---------------------------------------------------------------------

// Creates a timer with the specified |id| which is firing now.
mojom::AssistantTimerPtr CreateFiringTimer(const std::string& id) {
  mojom::AssistantTimerPtr timer = mojom::AssistantTimer::New();
  timer->id = id;
  timer->state = mojom::AssistantTimerState::kFired;
  timer->fire_time = base::Time::Now();
  timer->remaining_time = base::TimeDelta();
  return timer;
}

// ScopedNotificationModelObserver ---------------------------------------------

class ScopedNotificationModelObserver
    : public AssistantNotificationModelObserver {
 public:
  using AssistantNotification =
      chromeos::assistant::mojom::AssistantNotification;
  using AssistantNotificationPtr =
      chromeos::assistant::mojom::AssistantNotificationPtr;

  ScopedNotificationModelObserver() {
    Shell::Get()
        ->assistant_controller()
        ->notification_controller()
        ->AddModelObserver(this);
  }

  ~ScopedNotificationModelObserver() override {
    Shell::Get()
        ->assistant_controller()
        ->notification_controller()
        ->RemoveModelObserver(this);
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

    controller_ =
        Shell::Get()->assistant_controller()->alarm_timer_controller();
    DCHECK(controller_);
  }

  // Advances the clock by |time_delta|, running any sequenced tasks in the
  // queue. Note that we don't use |TaskEnvironment::FastForwardBy| because that
  // API will hang when |time_delta| is sufficiently large, ultimately resulting
  // in unittest timeout.
  void AdvanceClock(base::TimeDelta time_delta) {
    task_environment()->AdvanceClock(time_delta);
    task_environment()->RunUntilIdle();
  }

  AssistantAlarmTimerController* controller() { return controller_; }

 private:
  AssistantAlarmTimerController* controller_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AssistantAlarmTimerControllerTest);
};

// Tests that a notification is added when a timer is fired and that the
// notification is updated appropriately.
TEST_F(AssistantAlarmTimerControllerTest, AddsAndUpdatesTimerNotification) {
  // We're going to run our test over a few locales to ensure i18n compliance.
  typedef struct {
    std::string locale;
    std::string expected_message_at_00_00_00;
    std::string expected_message_at_00_00_01;
    std::string expected_message_at_00_01_01;
    std::string expected_message_at_01_01_01;
  } I18nTestCase;

  std::vector<I18nTestCase> i18n_test_cases;

  // We'll test in English (United States).
  i18n_test_cases.push_back({
      /*locale=*/"en_US",
      /*expected_message_at_00_00_00=*/"0:00",
      /*expected_message_at_00_00_01=*/"-0:01",
      /*expected_message_at_00_01_01=*/"-1:01",
      /*expected_message_at_01_01_01=*/"-1:01:01",
  });

  // We'll also test in Slovenian (Slovenia).
  i18n_test_cases.push_back({
      /*locale=*/"sl_SI",
      /*expected_message_at_00_00_00=*/"0.00",
      /*expected_message_at_00_00_01=*/"-0.01",
      /*expected_message_at_00_01_01=*/"-1.01",
      /*expected_message_at_01_01_01=*/"-1.01.01",
  });

  // Run all of our internationalized test cases.
  for (auto& i18n_test_case : i18n_test_cases) {
    base::test::ScopedRestoreICUDefaultLocale locale(i18n_test_case.locale);

    // Observe notifications.
    ScopedNotificationModelObserver notification_model_observer;

    // Fire a timer.
    std::vector<mojom::AssistantTimerPtr> timers;
    timers.push_back(CreateFiringTimer(/*id=*/"1"));
    controller()->OnTimerStateChanged(std::move(timers));

    // We expect our title to be internationalized.
    const std::string expected_title =
        l10n_util::GetStringUTF8(IDS_ASSISTANT_TIMER_NOTIFICATION_TITLE);

    // Make assertions about the newly added notification.
    auto* last_notification = notification_model_observer.last_notification();
    EXPECT_EQ("assistant/timer1", last_notification->client_id);
    EXPECT_EQ(expected_title, last_notification->title);
    EXPECT_EQ(i18n_test_case.expected_message_at_00_00_00,
              last_notification->message);

    // Advance clock by 1 second.
    AdvanceClock(base::TimeDelta::FromSeconds(1));

    // Make assertions about the updated notification.
    last_notification = notification_model_observer.last_notification();
    EXPECT_EQ("assistant/timer1", last_notification->client_id);
    EXPECT_EQ(expected_title, last_notification->title);
    EXPECT_EQ(i18n_test_case.expected_message_at_00_00_01,
              last_notification->message);

    // Advance clock by 1 minute.
    AdvanceClock(base::TimeDelta::FromMinutes(1));

    // Make assertions about the updated notification.
    last_notification = notification_model_observer.last_notification();
    EXPECT_EQ("assistant/timer1", last_notification->client_id);
    EXPECT_EQ(expected_title, last_notification->title);
    EXPECT_EQ(i18n_test_case.expected_message_at_00_01_01,
              last_notification->message);

    // Advance clock by 1 hour.
    AdvanceClock(base::TimeDelta::FromHours(1));

    // Make assertions about the updated notification.
    last_notification = notification_model_observer.last_notification();
    EXPECT_EQ("assistant/timer1", last_notification->client_id);
    EXPECT_EQ(expected_title, last_notification->title);
    EXPECT_EQ(i18n_test_case.expected_message_at_01_01_01,
              last_notification->message);
  }
}

}  // namespace ash
