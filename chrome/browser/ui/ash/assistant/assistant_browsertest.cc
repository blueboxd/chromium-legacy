// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/ui/ash/assistant/assistant_test_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "chromeos/services/assistant/public/features.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace chromeos {
namespace assistant {

namespace {

constexpr int kStartBrightnessPercent = 50;

// Ensures that |str_| starts with |prefix_|. If it doesn't, this will print a
// nice error message.
#define EXPECT_STARTS_WITH(str_, prefix_)                                      \
  ({                                                                           \
    EXPECT_TRUE(base::StartsWith(str_, prefix_, base::CompareCase::SENSITIVE)) \
        << "Expected '" << str_ << "'' to start with '" << prefix_ << "'";     \
  })

// Ensures that |value_| is within the range {min_, max_}. If it isn't, this
// will print a nice error message.
#define EXPECT_WITHIN_RANGE(min_, value_, max_)                \
  ({                                                           \
    EXPECT_TRUE(min_ <= value_ && value_ <= max_)              \
        << "Expected " << value_ << " to be within the range " \
        << "{" << min_ << ", " << max_ << "}.";                \
  })

}  // namespace

class AssistantBrowserTest : public MixinBasedInProcessBrowserTest {
 public:
  AssistantBrowserTest() = default;
  ~AssistantBrowserTest() override = default;

  void ShowAssistantUi() {
    if (!tester()->IsVisible())
      tester()->PressAssistantKey();
  }

  AssistantTestMixin* tester() { return &tester_; }

  void InitializeBrightness() {
    auto* power_manager = chromeos::PowerManagerClient::Get();
    power_manager::SetBacklightBrightnessRequest request;
    request.set_percent(kStartBrightnessPercent);
    request.set_transition(
        power_manager::SetBacklightBrightnessRequest_Transition_INSTANT);
    request.set_cause(
        power_manager::SetBacklightBrightnessRequest_Cause_USER_REQUEST);
    chromeos::PowerManagerClient::Get()->SetScreenBrightness(request);

    // Wait for the initial value to settle.
    tester()->ExpectResult(
        true, base::BindLambdaForTesting([&]() {
          constexpr double kEpsilon = 0.1;
          auto current_brightness = tester()->SyncCall(base::BindOnce(
              &chromeos::PowerManagerClient::GetScreenBrightnessPercent,
              base::Unretained(power_manager)));
          return current_brightness &&
                 std::abs(kStartBrightnessPercent -
                          current_brightness.value()) < kEpsilon;
        }));
  }

  void ExpectBrightnessUp() {
    auto* power_manager = chromeos::PowerManagerClient::Get();
    // Check the brightness changes
    tester()->ExpectResult(
        true, base::BindLambdaForTesting([&]() {
          constexpr double kEpsilon = 1;
          auto current_brightness = tester()->SyncCall(base::BindOnce(
              &chromeos::PowerManagerClient::GetScreenBrightnessPercent,
              base::Unretained(power_manager)));

          return current_brightness && (current_brightness.value() -
                                        kStartBrightnessPercent) > kEpsilon;
        }));
  }

  void ExpectBrightnessDown() {
    auto* power_manager = chromeos::PowerManagerClient::Get();
    // Check the brightness changes
    tester()->ExpectResult(
        true, base::BindLambdaForTesting([&]() {
          constexpr double kEpsilon = 1;
          auto current_brightness = tester()->SyncCall(base::BindOnce(
              &chromeos::PowerManagerClient::GetScreenBrightnessPercent,
              base::Unretained(power_manager)));

          return current_brightness && (kStartBrightnessPercent -
                                        current_brightness.value()) > kEpsilon;
        }));
  }

 private:
  AssistantTestMixin tester_{&mixin_host_, this, embedded_test_server(),
                             FakeS3Mode::kReplay};

  DISALLOW_COPY_AND_ASSIGN(AssistantBrowserTest);
};

IN_PROC_BROWSER_TEST_F(AssistantBrowserTest,
                       ShouldOpenAssistantUiWhenPressingAssistantKey) {
  tester()->StartAssistantAndWaitForReady();

  tester()->PressAssistantKey();

  EXPECT_TRUE(tester()->IsVisible());
}

IN_PROC_BROWSER_TEST_F(AssistantBrowserTest, ShouldDisplayTextResponse) {
  tester()->StartAssistantAndWaitForReady();

  ShowAssistantUi();

  tester()->SendTextQuery("test");
  tester()->ExpectAnyOfTheseTextResponses({
      "No one told me there would be a test",
      "You're coming in loud and clear",
      "debug OK",
      "I can assure you, this thing's on",
      "Is this thing on?",
  });
}

IN_PROC_BROWSER_TEST_F(AssistantBrowserTest, ShouldDisplayCardResponse) {
  tester()->StartAssistantAndWaitForReady();

  ShowAssistantUi();

  EXPECT_TRUE(tester()->IsVisible());

  tester()->SendTextQuery("What is the highest mountain in the world?");
  tester()->ExpectCardResponse("Mount Everest");
}

IN_PROC_BROWSER_TEST_F(AssistantBrowserTest, ShouldTurnUpVolume) {
  tester()->StartAssistantAndWaitForReady();

  ShowAssistantUi();

  EXPECT_TRUE(tester()->IsVisible());

  auto* cras = chromeos::CrasAudioHandler::Get();
  constexpr int kStartVolumePercent = 50;
  cras->SetOutputVolumePercent(kStartVolumePercent);
  EXPECT_EQ(kStartVolumePercent, cras->GetOutputVolumePercent());

  tester()->SendTextQuery("turn up volume");

  tester()->ExpectResult(true, base::BindRepeating(
                                   [](chromeos::CrasAudioHandler* cras) {
                                     return cras->GetOutputVolumePercent() >
                                            kStartVolumePercent;
                                   },
                                   cras));
}

IN_PROC_BROWSER_TEST_F(AssistantBrowserTest, ShouldTurnDownVolume) {
  tester()->StartAssistantAndWaitForReady();

  ShowAssistantUi();

  EXPECT_TRUE(tester()->IsVisible());

  auto* cras = chromeos::CrasAudioHandler::Get();
  constexpr int kStartVolumePercent = 50;
  cras->SetOutputVolumePercent(kStartVolumePercent);
  EXPECT_EQ(kStartVolumePercent, cras->GetOutputVolumePercent());

  tester()->SendTextQuery("turn down volume");

  tester()->ExpectResult(true, base::BindRepeating(
                                   [](chromeos::CrasAudioHandler* cras) {
                                     return cras->GetOutputVolumePercent() <
                                            kStartVolumePercent;
                                   },
                                   cras));
}

IN_PROC_BROWSER_TEST_F(AssistantBrowserTest, ShouldTurnUpBrightness) {
  tester()->StartAssistantAndWaitForReady();

  ShowAssistantUi();

  EXPECT_TRUE(tester()->IsVisible());

  InitializeBrightness();

  tester()->SendTextQuery("turn up brightness");

  ExpectBrightnessUp();
}

IN_PROC_BROWSER_TEST_F(AssistantBrowserTest, ShouldTurnDownBrightness) {
  tester()->StartAssistantAndWaitForReady();

  ShowAssistantUi();

  EXPECT_TRUE(tester()->IsVisible());

  InitializeBrightness();

  tester()->SendTextQuery("turn down brightness");

  ExpectBrightnessDown();
}

// TODO(b:152077326): See if we can get TaskEnvironment to work in
// AssistantBrowserTests so that we can use it instead of TestClock.
class TestClock {
 public:
  TestClock() {
    DCHECK_EQ(nullptr, instance_);
    instance_ = this;
  }

  TestClock(const TestClock&) = delete;
  TestClock& operator=(const TestClock&) = delete;

  ~TestClock() {
    DCHECK_EQ(this, instance_);
    instance_ = nullptr;
  }

  void Advance(base::TimeDelta delta) {
    DCHECK_GE(delta, base::TimeDelta());
    base::AutoLock lock(offset_lock_);
    offset_ += delta;
  }

 private:
  static base::Time TimeNow() {
    return base::subtle::TimeNowIgnoringOverride() +
           TestClock::instance_->GetOffset();
  }

  static base::TimeTicks TimeTicksNow() {
    return base::subtle::TimeTicksNowIgnoringOverride() +
           TestClock::instance_->GetOffset();
  }

  static base::ThreadTicks ThreadTicksNow() {
    return base::subtle::ThreadTicksNowIgnoringOverride() +
           TestClock::instance_->GetOffset();
  }

  base::TimeDelta GetOffset() {
    base::AutoLock lock(offset_lock_);
    return offset_;
  }

  static TestClock* instance_;

  base::subtle::ScopedTimeClockOverrides time_overrides_{
      &TestClock::TimeNow, &TestClock::TimeTicksNow,
      &TestClock::ThreadTicksNow};

  base::Lock offset_lock_;
  base::TimeDelta offset_ GUARDED_BY(offset_lock_);
};

// static
TestClock* TestClock::instance_ = nullptr;

class AssistantTimersV2BrowserTest : public AssistantBrowserTest {
 public:
  AssistantTimersV2BrowserTest() {
    feature_list_.InitAndEnableFeature(features::kAssistantTimersV2);
  }

  AssistantTimersV2BrowserTest(const AssistantTimersV2BrowserTest&) = delete;
  AssistantTimersV2BrowserTest& operator=(const AssistantTimersV2BrowserTest&) =
      delete;
  ~AssistantTimersV2BrowserTest() override = default;

  TestClock& clock() { return clock_; }

 private:
  TestClock clock_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(AssistantTimersV2BrowserTest,
                       ShouldDisplayTimersResponse) {
  tester()->StartAssistantAndWaitForReady();

  ShowAssistantUi();
  EXPECT_TRUE(tester()->IsVisible());

  tester()->SendTextQuery("Set a timer for 5 minutes");
  tester()->ExpectAnyOfTheseTextResponses({
      "Alright, 5 min. Starting… now.",
      "OK, 5 min. And we're starting… now.",
  });

  tester()->SendTextQuery("Set a timer for 10 minutes");
  tester()->ExpectAnyOfTheseTextResponses({
      "2nd timer, for 10 min. And that's starting… now.",
      "2nd timer, for 10 min. Starting… now.",
  });

  tester()->SendTextQuery("Show my timers");
  std::vector<base::TimeDelta> timers =
      tester()->ExpectAndReturnTimersResponse();
  EXPECT_EQ(2u, timers.size());

  // Five minute timer should be somewhere in the range of {0, 5} min.
  base::TimeDelta& five_min_timer = timers.at(0);
  EXPECT_WITHIN_RANGE(0, five_min_timer.InMinutes(), 5);

  // Ten minute timer should be somewhere in the range of {5, 10} min.
  base::TimeDelta& ten_min_timer = timers.at(1);
  EXPECT_WITHIN_RANGE(5, ten_min_timer.InMinutes(), 10);

  // Artificially advance the clock.
  clock().Advance(five_min_timer);
  base::RunLoop().RunUntilIdle();

  // Update our expectation for where our timers should be.
  ten_min_timer -= five_min_timer;
  five_min_timer = base::TimeDelta();

  // Assert that the UI has been updated to meet our expectations.
  tester()->ExpectTimersResponse(timers);
}

IN_PROC_BROWSER_TEST_F(AssistantTimersV2BrowserTest,
                       ShouldDismissTimerNotificationsWhenDisablingAssistant) {
  tester()->StartAssistantAndWaitForReady();

  ShowAssistantUi();
  EXPECT_TRUE(tester()->IsVisible());

  // Confirm no Assistant notifications are currently being shown.
  auto* message_center = message_center::MessageCenter::Get();
  EXPECT_TRUE(message_center->FindNotificationsByAppId("assistant").empty());

  // Start a timer for one minute.
  tester()->SendTextQuery("Set a timer for 1 minute.");
  tester()->ExpectTextResponse("Alright, 1 min. And that's starting… now.");

  // Confirm that an Assistant timer notification is now showing.
  auto notifications = message_center->FindNotificationsByAppId("assistant");
  EXPECT_EQ(1u, notifications.size());
  EXPECT_STARTS_WITH((*notifications.begin())->id(), "assistant/timer");

  // Disable Assistant.
  tester()->SetAssistantEnabled(false);
  base::RunLoop().RunUntilIdle();

  // Confirm that our Assistant timer notification has been dismissed.
  EXPECT_TRUE(message_center->FindNotificationsByAppId("assistant").empty());
}

}  // namespace assistant
}  // namespace chromeos
