// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/message_center/unified_message_center_bubble.h"
#include "ash/system/message_center/unified_message_center_view.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "base/strings/string_util.h"
#include "base/test/icu_test_util.h"
#include "chrome/browser/ui/ash/assistant/assistant_test_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/views/notification_view_md.h"

namespace chromeos {
namespace assistant {

namespace {

// Please remember to set auth token when running in |kProxy| mode.
constexpr auto kMode = FakeS3Mode::kReplay;
// Update this when you introduce breaking changes to existing tests.
constexpr int kVersion = 1;

// TODO(b:153496343): Move generic helpers to a more generic location for reuse.
// Helpers ---------------------------------------------------------------------

// Finds any descendents of |parent| with the desired |class_name| and pushes
// them onto the strongly typed |result| vector.
// NOTE: Callers are expected to ensure that casting to <T> makes sense.
template <typename T>
void FindDescendentsOfClass(views::View* parent,
                            std::string class_name,
                            std::vector<T*>* result) {
  for (auto* child : parent->children()) {
    if (child->GetClassName() == class_name)
      result->push_back(static_cast<T*>(child));
    FindDescendentsOfClass(child, class_name, result);
  }
}

// Finds any descendents of |parent| with class name equal to the static class
// variable |kViewClassName| and pushes them onto the strongly typed |result|
// vector.
// NOTE: This variant of FindDescendentsOfClass is safer than the three argument
// variant and its usage should be preferred where possible.
template <typename T>
void FindDescendentsOfClass(views::View* parent, std::vector<T*>* result) {
  FindDescendentsOfClass(parent, T::kViewClassName, result);
}

// Returns the status area widget.
ash::StatusAreaWidget* FindStatusAreaWidget() {
  return ash::Shelf::ForWindow(ash::Shell::GetRootWindowForNewWindows())
      ->shelf_widget()
      ->status_area_widget();
}

// Returns the set of Assistant notifications (as indicated by application id).
message_center::NotificationList::Notifications FindAssistantNotifications() {
  return message_center::MessageCenter::Get()->FindNotificationsByAppId(
      "assistant");
}

// Returns visible notifications having id starting with |prefix|.
std::vector<message_center::Notification*> FindVisibleNotificationsByPrefixedId(
    const std::string& prefix) {
  std::vector<message_center::Notification*> notifications;
  for (auto* notification :
       message_center::MessageCenter::Get()->GetVisibleNotifications()) {
    if (base::StartsWith(notification->id(), prefix,
                         base::CompareCase::SENSITIVE)) {
      notifications.push_back(notification);
    }
  }
  return notifications;
}

// Returns the view for the specified |notification|.
message_center::MessageView* FindViewForNotification(
    const message_center::Notification* notification) {
  ash::UnifiedMessageCenterView* unified_message_center_view =
      FindStatusAreaWidget()
          ->unified_system_tray()
          ->message_center_bubble()
          ->message_center_view();

  std::vector<message_center::MessageView*> message_views;
  FindDescendentsOfClass(unified_message_center_view, &message_views);

  for (message_center::MessageView* message_view : message_views) {
    if (message_view->notification_id() == notification->id())
      return message_view;
  }

  return nullptr;
}

// Returns the action buttons for the specified |notification|.
std::vector<message_center::NotificationButtonMD*>
FindActionButtonsForNotification(
    const message_center::Notification* notification) {
  auto* notification_view = FindViewForNotification(notification);

  std::vector<message_center::NotificationButtonMD*> action_buttons;
  FindDescendentsOfClass(notification_view, "NotificationButtonMD",
                         &action_buttons);

  return action_buttons;
}

// Performs a tap of the specified |view| and waits until the RunLoop idles.
void TapOnAndWait(const views::View* view) {
  auto* root_window = view->GetWidget()->GetNativeWindow()->GetRootWindow();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.MoveTouch(view->GetBoundsInScreen().CenterPoint());
  event_generator.PressTouch();
  event_generator.ReleaseTouch();
  base::RunLoop().RunUntilIdle();
}

// Performs a tap of the specified |widget| and waits until the RunLoop idles.
void TapOnAndWait(const views::Widget* widget) {
  aura::Window* root_window = widget->GetNativeWindow()->GetRootWindow();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.MoveTouch(widget->GetWindowBoundsInScreen().CenterPoint());
  event_generator.PressTouch();
  event_generator.ReleaseTouch();
  base::RunLoop().RunUntilIdle();
}

}  // namespace

// AssistantTimersBrowserTest --------------------------------------------------

class AssistantTimersBrowserTest : public MixinBasedInProcessBrowserTest {
 public:
  AssistantTimersBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kAssistantTimersV2);
  }

  AssistantTimersBrowserTest(const AssistantTimersBrowserTest&) = delete;
  AssistantTimersBrowserTest& operator=(const AssistantTimersBrowserTest&) =
      delete;

  ~AssistantTimersBrowserTest() override = default;

  void ShowAssistantUi() {
    if (!tester()->IsVisible())
      tester()->PressAssistantKey();
  }

  AssistantTestMixin* tester() { return &tester_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedRestoreICUDefaultLocale locale_{"en_US"};
  AssistantTestMixin tester_{&mixin_host_, this, embedded_test_server(), kMode,
                             kVersion};
};

// Tests -----------------------------------------------------------------------

// Pressing the "STOP" action button in a timer notification should result in
// the timer being removed.
IN_PROC_BROWSER_TEST_F(AssistantTimersBrowserTest,
                       ShouldRemoveTimerWhenStoppingViaNotification) {
  tester()->StartAssistantAndWaitForReady();

  ShowAssistantUi();
  EXPECT_TRUE(tester()->IsVisible());

  // Confirm no Assistant notifications are currently being shown.
  EXPECT_TRUE(FindAssistantNotifications().empty());

  // Start a timer for five minutes.
  tester()->SendTextQuery("Set a timer for 5 minutes");
  tester()->ExpectAnyOfTheseTextResponses({
      "Alright, 5 min. Starting… now.",
      "OK, 5 min. And we're starting… now.",
      "OK, 5 min. Starting… now.",
      "Sure, 5 min. And that's starting… now.",
      "Sure, 5 min. Starting now.",
  });

  // Tap status area widget (to show notifications in the Message Center).
  TapOnAndWait(FindStatusAreaWidget());

  // Confirm that an Assistant timer notification is now showing.
  auto notifications = FindVisibleNotificationsByPrefixedId("assistant/timer");
  ASSERT_EQ(1u, notifications.size());

  // Find the action buttons for our notification.
  // NOTE: We expect action buttons for "STOP" and "ADD 1 MIN".
  auto action_buttons = FindActionButtonsForNotification(notifications.at(0));
  EXPECT_EQ(2u, action_buttons.size());

  // Tap the "STOP" action button in the notification.
  EXPECT_EQ(base::UTF8ToUTF16("STOP"), action_buttons.at(0)->GetText());
  TapOnAndWait(action_buttons.at(0));

  ShowAssistantUi();
  EXPECT_TRUE(tester()->IsVisible());

  // Confirm that no timers exist anymore.
  tester()->SendTextQuery("Show my timers");
  tester()->ExpectAnyOfTheseTextResponses({
      "It looks like you don't have any timers set at the moment.",
  });
}

}  // namespace assistant
}  // namespace chromeos
