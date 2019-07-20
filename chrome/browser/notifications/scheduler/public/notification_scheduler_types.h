// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_NOTIFICATION_SCHEDULER_TYPES_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_NOTIFICATION_SCHEDULER_TYPES_H_

#include <string>

namespace notifications {

// Enum to describe the time to process scheduled notification data.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.chrome.browser.notifications.scheduler)
enum class SchedulerTaskTime {
  // The system is started from normal user launch or other background
  // tasks.
  kUnknown = 0,
  // Background task runs in the morning.
  kMorning = 1,
  // Background task runs in the evening.
  kEvening = 2,
};

// The type of a list of clients using the notification scheduler system.
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.chrome.browser.notifications.scheduler)
enum class SchedulerClientType {
  // Test only values.
  kTest1 = -1,
  kTest2 = -2,
  kTest3 = -3,

  // Default value of client type.
  kUnknown = 0,
  // Client used in chrome://notifications-internals for debugging.
  kWebUI = 1,
  kMaxValue = kWebUI
};

// The type of user feedback from a displayed notification.
enum class UserFeedback {
  // No user feedback yet.
  kNoFeedback = 0,
  // The user taps the helpful button, potentially a strong indicator of user's
  // positive preference on the notification.
  kHelpful = 1,
  // The user taps the unhelpful button, potentially a strong indicator of
  // user's negative preference on the notification.
  kNotHelpful = 2,
  // The user clicks the notification.
  kClick = 3,
  // The user clicks the body of the notification.
  kDismiss = 4,
  // The user has no interaction with the notification for a while.
  kIgnore = 5,
  kMaxValue = kIgnore
};

// The user impression of a particular notification, which may impact the
// notification display frenquency.
enum class ImpressionResult {
  // Invalid user impression.
  kInvalid = 0,
  // Positive user impression that the user may like the notification.
  kPositive = 1,
  // Positive user impression that the user may dislike the notification.
  kNegative = 2,
  // The feedback is neutral to the user.
  kNeutral = 3,
  kMaxValue = kNeutral
};

// Defines user actions type.
enum class UserActionType {
  // The user clicks on the notification body.
  kClick = 0,
  // The user clicks on the notification button.
  kButtonClick = 1,
  // The user dismisses the notification.
  kDismiss = 2,
};

// Categorizes type of notification buttons. Different type of button clicks
// may result in change of notification shown frequency.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.chrome.browser.notifications.scheduler)
enum class ActionButtonType {
  // The action button is not categorized.
  kUnknownAction = 0,

  // Helpful button indicates the user likes to interact with the notification.
  kHelpful = 1,

  // Unhelpful button indicates dislike of the notification.
  kUnhelpful = 2,
};

// Information about button clicks.
struct ButtonClickInfo {
  // Unique id of the button.
  std::string button_id;

  // Associate impression type for the button.
  ActionButtonType type = ActionButtonType::kUnknownAction;
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_NOTIFICATION_SCHEDULER_TYPES_H_
