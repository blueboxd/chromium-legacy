// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_alarm_timer_controller.h"

#include <map>
#include <string>
#include <utility>

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_notification_controller.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/public/mojom/assistant_controller.mojom.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/i18n/message_formatter.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/services/assistant/public/features.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

// Grouping key and ID prefix for timer notifications.
constexpr char kTimerNotificationGroupingKey[] = "assistant/timer";
constexpr char kTimerNotificationIdPrefix[] = "assistant/timer";

constexpr base::TimeDelta kOneMin = base::TimeDelta::FromMinutes(1);

// Interval at which alarms/timers are ticked.
constexpr base::TimeDelta kTickInterval = base::TimeDelta::FromSeconds(1);

// Helpers ---------------------------------------------------------------------

// Creates a notification ID for the given |alarm_timer_id|. It is guaranteed
// that this method will always return the same notification ID given the same
// alarm/timer ID.
std::string CreateTimerNotificationId(const std::string& alarm_timer_id) {
  return std::string(kTimerNotificationIdPrefix) + alarm_timer_id;
}

std::string CreateTimerNotificationMessage(const AlarmTimer& alarm_timer,
                                           base::TimeDelta time_remaining) {
  const int minutes_remaining = time_remaining.InMinutes();
  const int seconds_remaining =
      (time_remaining - base::TimeDelta::FromMinutes(minutes_remaining))
          .InSeconds();
  return base::UTF16ToUTF8(base::i18n::MessageFormatter::FormatWithNumberedArgs(
      l10n_util::GetStringUTF16(IDS_ASSISTANT_TIMER_NOTIFICATION_MESSAGE),
      alarm_timer.expired() ? "-" : "", minutes_remaining, seconds_remaining));
}

// TODO(llin): Migrate to use the AlarmManager API to better support multiple
// timers when the API is available.
chromeos::assistant::mojom::AssistantNotificationPtr CreateTimerNotification(
    const AlarmTimer& alarm_timer,
    base::TimeDelta time_remaining) {
  using chromeos::assistant::mojom::AssistantNotification;
  using chromeos::assistant::mojom::AssistantNotificationButton;
  using chromeos::assistant::mojom::AssistantNotificationPtr;
  using chromeos::assistant::mojom::AssistantNotificationType;

  const std::string title =
      l10n_util::GetStringUTF8(IDS_ASSISTANT_TIMER_NOTIFICATION_TITLE);
  const std::string message =
      CreateTimerNotificationMessage(alarm_timer, time_remaining);

  base::Optional<GURL> stop_alarm_timer_action_url =
      assistant::util::CreateAlarmTimerDeepLink(
          assistant::util::AlarmTimerAction::kStopRinging,
          /*alarm_timer_id=*/base::nullopt,
          /*duration=*/base::nullopt);

  base::Optional<GURL> add_time_to_timer_action_url =
      assistant::util::CreateAlarmTimerDeepLink(
          assistant::util::AlarmTimerAction::kAddTimeToTimer, alarm_timer.id,
          kOneMin);

  AssistantNotificationPtr notification = AssistantNotification::New();

  // If in-Assistant notifications are supported, we'll allow alarm/timer
  // notifications to show in either Assistant UI or the Message Center.
  // Otherwise, we'll only allow the notification to show in the Message Center.
  notification->type =
      chromeos::assistant::features::IsInAssistantNotificationsEnabled()
          ? AssistantNotificationType::kPreferInAssistant
          : AssistantNotificationType::kSystem;

  notification->title = title;
  notification->message = message;
  notification->client_id = CreateTimerNotificationId(alarm_timer.id);
  notification->grouping_key = kTimerNotificationGroupingKey;

  // This notification should be able to wake up the display if it was off.
  notification->is_high_priority = true;

  if (!stop_alarm_timer_action_url.has_value()) {
    LOG(ERROR) << "Can't create stop alarm timer action URL";
    return notification;
  }

  notification->action_url = stop_alarm_timer_action_url.value();

  // "STOP" button.
  notification->buttons.push_back(AssistantNotificationButton::New(
      l10n_util::GetStringUTF8(IDS_ASSISTANT_TIMER_NOTIFICATION_STOP_BUTTON),
      stop_alarm_timer_action_url.value()));

  if (!add_time_to_timer_action_url.has_value()) {
    LOG(ERROR) << "Can't create add time to timer action URL";
    return notification;
  }

  // "ADD 1 MIN" button.
  notification->buttons.push_back(
      chromeos::assistant::mojom::AssistantNotificationButton::New(
          l10n_util::GetStringUTF8(
              IDS_ASSISTANT_TIMER_NOTIFICATION_ADD_1_MIN_BUTTON),
          add_time_to_timer_action_url.value()));

  return notification;
}

}  // namespace

// AssistantAlarmTimerController -----------------------------------------------

AssistantAlarmTimerController::AssistantAlarmTimerController(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller) {
  AddModelObserver(this);
  assistant_controller_->AddObserver(this);
}

AssistantAlarmTimerController::~AssistantAlarmTimerController() {
  assistant_controller_->RemoveObserver(this);
  RemoveModelObserver(this);
}

void AssistantAlarmTimerController::BindReceiver(
    mojo::PendingReceiver<mojom::AssistantAlarmTimerController> receiver) {
  receiver_.Bind(std::move(receiver));
}

void AssistantAlarmTimerController::AddModelObserver(
    AssistantAlarmTimerModelObserver* observer) {
  model_.AddObserver(observer);
}

void AssistantAlarmTimerController::RemoveModelObserver(
    AssistantAlarmTimerModelObserver* observer) {
  model_.RemoveObserver(observer);
}

void AssistantAlarmTimerController::OnAlarmTimerStateChanged(
    mojom::AssistantAlarmTimerEventPtr event) {
  if (!event) {
    // Nothing is ringing. Remove all alarms and timers.
    model_.RemoveAllAlarmsTimers();
    return;
  }

  switch (event->type) {
    case mojom::AssistantAlarmTimerEventType::kTimer:
      if (event->data->get_timer_data()->state ==
          mojom::AssistantTimerState::kFired) {
        // Remove all timers/alarms since there will be only one timer/alarm
        // firing.
        // TODO(llin): Handle multiple timers firing when the API is supported.
        model_.RemoveAllAlarmsTimers();

        AlarmTimer timer;
        timer.id = event->data->get_timer_data()->timer_id;
        timer.type = AlarmTimerType::kTimer;
        timer.end_time = base::TimeTicks::Now();
        model_.AddAlarmTimer(timer);
      }
      break;
      // TODO(llin): Handle alarm event.
  }
}

void AssistantAlarmTimerController::OnAlarmTimerAdded(
    const AlarmTimer& alarm_timer,
    const base::TimeDelta& time_remaining) {
  // Schedule a repeating timer to tick the tracked alarms/timers.
  if (!timer_.IsRunning()) {
    timer_.Start(FROM_HERE, kTickInterval, &model_,
                 &AssistantAlarmTimerModel::Tick);
  }

  // Create a notification for the added alarm/timer.
  assistant_controller_->notification_controller()->AddOrUpdateNotification(
      CreateTimerNotification(alarm_timer, time_remaining));
}

void AssistantAlarmTimerController::OnAlarmsTimersTicked(
    const std::map<std::string, base::TimeDelta>& times_remaining) {
  // Update any existing notifications associated w/ our alarms/timers.
  for (auto& pair : times_remaining) {
    auto* notification_controller =
        assistant_controller_->notification_controller();
    if (notification_controller->model()->HasNotificationForId(
            CreateTimerNotificationId(/*alarm_timer_id=*/pair.first))) {
      notification_controller->AddOrUpdateNotification(CreateTimerNotification(
          *model_.GetAlarmTimerById(pair.first), pair.second));
    }
  }
}

void AssistantAlarmTimerController::OnAllAlarmsTimersRemoved() {
  // We can stop our timer from ticking when all alarms/timers are removed.
  timer_.Stop();

  // Remove any notifications associated w/ alarms/timers.
  assistant_controller_->notification_controller()
      ->RemoveNotificationByGroupingKey(kTimerNotificationGroupingKey,
                                        /*from_server=*/false);
}

void AssistantAlarmTimerController::SetAssistant(
    chromeos::assistant::mojom::Assistant* assistant) {
  assistant_ = assistant;
}

void AssistantAlarmTimerController::OnAssistantControllerConstructed() {
  assistant_controller_->ui_controller()->AddModelObserver(this);
}

void AssistantAlarmTimerController::OnAssistantControllerDestroying() {
  assistant_controller_->ui_controller()->RemoveModelObserver(this);
}

void AssistantAlarmTimerController::OnDeepLinkReceived(
    assistant::util::DeepLinkType type,
    const std::map<std::string, std::string>& params) {
  using assistant::util::DeepLinkParam;
  using assistant::util::DeepLinkType;

  if (type != DeepLinkType::kAlarmTimer)
    return;

  const base::Optional<assistant::util::AlarmTimerAction>& action =
      assistant::util::GetDeepLinkParamAsAlarmTimerAction(params);
  if (!action.has_value())
    return;

  // Timer ID is optional. Only used for adding time to timer.
  const base::Optional<std::string>& alarm_timer_id =
      assistant::util::GetDeepLinkParam(params, DeepLinkParam::kId);

  // Duration is optional. Only used for adding time to timer.
  const base::Optional<base::TimeDelta>& duration =
      assistant::util::GetDeepLinkParamAsTimeDelta(params,
                                                   DeepLinkParam::kDurationMs);

  PerformAlarmTimerAction(action.value(), alarm_timer_id, duration);
}

void AssistantAlarmTimerController::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    base::Optional<AssistantEntryPoint> entry_point,
    base::Optional<AssistantExitPoint> exit_point) {
  // When the Assistant UI transitions from a visible state, we'll dismiss any
  // ringing alarms or timers (assuming certain conditions have been met).
  if (old_visibility != AssistantVisibility::kVisible)
    return;

  // We only do this if in-Assistant notifications are enabled, as in-Assistant
  // alarm/timer notifications only live as long the Assistant UI. Per UX
  // requirement, when Assistant UI dismisses with an in-Assistant timer
  // notification showing, any ringing alarms/timers should be stopped.
  if (!chromeos::assistant::features::IsInAssistantNotificationsEnabled())
    return;

  assistant_->StopAlarmTimerRinging();
}

void AssistantAlarmTimerController::PerformAlarmTimerAction(
    const assistant::util::AlarmTimerAction& action,
    const base::Optional<std::string>& alarm_timer_id,
    const base::Optional<base::TimeDelta>& duration) {
  DCHECK(assistant_);

  switch (action) {
    case assistant::util::AlarmTimerAction::kAddTimeToTimer:
      if (!alarm_timer_id.has_value() || !duration.has_value()) {
        LOG(ERROR) << "Ignore add time to timer action without timer ID or "
                   << "duration.";
        return;
      }
      // Verify the timer is ringing.
      DCHECK(model_.GetAlarmTimerById(alarm_timer_id.value()));
      // LibAssistant doesn't currently support adding time to an ringing timer.
      // We'll create a new one with the duration specified. Note that we
      // currently only support this deep link for an alarm/timer that is
      // ringing.
      assistant_->StopAlarmTimerRinging();
      assistant_->CreateTimer(duration.value());
      break;
    case assistant::util::AlarmTimerAction::kStopRinging:
      assistant_->StopAlarmTimerRinging();
      break;
  }
}
}  // namespace ash
