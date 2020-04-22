// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_alarm_timer_controller.h"

#include <map>
#include <string>
#include <utility>

#include "ash/assistant/assistant_controller_impl.h"
#include "ash/assistant/assistant_notification_controller.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/public/mojom/assistant_controller.mojom.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/i18n/message_formatter.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "third_party/icu/source/i18n/unicode/measfmt.h"
#include "third_party/icu/source/i18n/unicode/measunit.h"
#include "third_party/icu/source/i18n/unicode/measure.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

using assistant::util::AlarmTimerAction;

// Grouping key and ID prefix for timer notifications.
constexpr char kTimerNotificationGroupingKey[] = "assistant/timer";
constexpr char kTimerNotificationIdPrefix[] = "assistant/timer";

constexpr base::TimeDelta kOneMin = base::TimeDelta::FromMinutes(1);

// Interval at which alarms/timers are ticked.
constexpr base::TimeDelta kTickInterval = base::TimeDelta::FromSeconds(1);

// Helpers ---------------------------------------------------------------------

// Creates a notification ID for the given |timer|. It is guaranteed that this
// method will always return the same notification ID given the same timer.
std::string CreateTimerNotificationId(const mojom::AssistantTimer& timer) {
  return std::string(kTimerNotificationIdPrefix) + timer.id;
}

// Creates a notification message for the given |timer|.
std::string CreateTimerNotificationMessage(const mojom::AssistantTimer& timer) {
  // Method aliases to prevent line-wrapping below.
  const auto createHour = icu::MeasureUnit::createHour;
  const auto createMinute = icu::MeasureUnit::createMinute;
  const auto createSecond = icu::MeasureUnit::createSecond;

  // Calculate hours/minutes/seconds remaining.
  const int64_t total_seconds = std::abs(timer.remaining_time.InSeconds());
  const int32_t hours = total_seconds / 3600;
  const int32_t minutes = (total_seconds - hours * 3600) / 60;
  const int32_t seconds = total_seconds % 60;

  // Success of the ICU APIs is tracked by |status|.
  UErrorCode status = U_ZERO_ERROR;

  // Create our distinct |measures| to be formatted. We only show |hours| if
  // necessary, otherwise they are omitted.
  std::vector<icu::Measure> measures;
  if (hours)
    measures.push_back(icu::Measure(hours, createHour(status), status));
  measures.push_back(icu::Measure(minutes, createMinute(status), status));
  measures.push_back(icu::Measure(seconds, createSecond(status), status));

  // Format our |measures| into a |unicode_message|.
  icu::UnicodeString unicode_message;
  icu::FieldPosition field_position = icu::FieldPosition::DONT_CARE;
  UMeasureFormatWidth width = UMEASFMT_WIDTH_NUMERIC;
  icu::MeasureFormat measure_format(icu::Locale::getDefault(), width, status);
  measure_format.formatMeasures(measures.data(), measures.size(),
                                unicode_message, field_position, status);

  std::string message;
  if (U_SUCCESS(status)) {
    // If formatting was successful, convert our |unicode_message| into UTF-8.
    unicode_message.toUTF8String(message);
  } else {
    // If something went wrong, we'll fall back to using "hh:mm:ss" instead.
    LOG(ERROR) << "Error formatting timer notification message: " << status;
    message = base::StringPrintf("%02d:%02d:%02d", hours, minutes, seconds);
  }

  // If time has elapsed since the |timer| has fired, we'll need to negate the
  // amount of time remaining.
  if (timer.remaining_time.InSeconds() < 0) {
    const auto format = l10n_util::GetStringUTF16(
        IDS_ASSISTANT_TIMER_NOTIFICATION_MESSAGE_EXPIRED);
    return base::UTF16ToUTF8(
        base::i18n::MessageFormatter::FormatWithNumberedArgs(format, message));
  }

  // Otherwise, all necessary formatting has been performed.
  return message;
}

// Creates a notification for the given |timer|.
chromeos::assistant::mojom::AssistantNotificationPtr CreateTimerNotification(
    const mojom::AssistantTimer& timer) {
  using chromeos::assistant::mojom::AssistantNotification;
  using chromeos::assistant::mojom::AssistantNotificationButton;
  using chromeos::assistant::mojom::AssistantNotificationPtr;
  using chromeos::assistant::mojom::AssistantNotificationType;

  const std::string title =
      l10n_util::GetStringUTF8(IDS_ASSISTANT_TIMER_NOTIFICATION_TITLE);
  const std::string message = CreateTimerNotificationMessage(timer);

  base::Optional<GURL> stop_alarm_timer_action_url =
      assistant::util::CreateAlarmTimerDeepLink(AlarmTimerAction::kRemove,
                                                timer.id);

  base::Optional<GURL> add_time_to_timer_action_url =
      assistant::util::CreateAlarmTimerDeepLink(
          AlarmTimerAction::kAddTimeToTimer, timer.id, kOneMin);

  AssistantNotificationPtr notification = AssistantNotification::New();
  notification->type = AssistantNotificationType::kSystem;
  notification->title = title;
  notification->message = message;
  notification->client_id = CreateTimerNotificationId(timer);
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
    AssistantControllerImpl* assistant_controller)
    : assistant_controller_(assistant_controller) {
  AddModelObserver(this);
  assistant_controller_observer_.Add(AssistantController::Get());
}

AssistantAlarmTimerController::~AssistantAlarmTimerController() {
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

void AssistantAlarmTimerController::SetAssistant(
    chromeos::assistant::mojom::Assistant* assistant) {
  assistant_ = assistant;
}

void AssistantAlarmTimerController::OnAssistantControllerConstructed() {
  assistant_controller_->state_controller()->AddObserver(this);
}

void AssistantAlarmTimerController::OnAssistantControllerDestroying() {
  assistant_controller_->state_controller()->RemoveObserver(this);
}

void AssistantAlarmTimerController::OnDeepLinkReceived(
    assistant::util::DeepLinkType type,
    const std::map<std::string, std::string>& params) {
  using assistant::util::DeepLinkParam;
  using assistant::util::DeepLinkType;

  if (type != DeepLinkType::kAlarmTimer)
    return;

  const base::Optional<AlarmTimerAction>& action =
      assistant::util::GetDeepLinkParamAsAlarmTimerAction(params);
  if (!action.has_value())
    return;

  const base::Optional<std::string>& alarm_timer_id =
      assistant::util::GetDeepLinkParam(params, DeepLinkParam::kId);
  if (!alarm_timer_id.has_value())
    return;

  // Duration is optional. Only used for adding time to timer.
  const base::Optional<base::TimeDelta>& duration =
      assistant::util::GetDeepLinkParamAsTimeDelta(params,
                                                   DeepLinkParam::kDurationMs);

  PerformAlarmTimerAction(action.value(), alarm_timer_id.value(), duration);
}

void AssistantAlarmTimerController::OnAssistantStatusChanged(
    mojom::AssistantState state) {
  // If LibAssistant is no longer running we need to clear our cache to
  // accurately reflect LibAssistant alarm/timer state.
  if (state == mojom::AssistantState::NOT_READY)
    model_.RemoveAllTimers();
}

void AssistantAlarmTimerController::OnTimerStateChanged(
    std::vector<mojom::AssistantTimerPtr> timers) {
  if (timers.empty()) {
    model_.RemoveAllTimers();
    return;
  }

  // First we remove all old timers that no longer exist.
  for (const auto* old_timer : model_.GetAllTimers()) {
    if (std::none_of(timers.begin(), timers.end(),
                     [&old_timer](const auto& new_or_updated_timer) {
                       return old_timer->id == new_or_updated_timer->id;
                     })) {
      model_.RemoveTimer(old_timer->id);
    }
  }

  // Then we add any new timers and update existing ones.
  for (auto& new_or_updated_timer : timers)
    model_.AddOrUpdateTimer(std::move(new_or_updated_timer));
}

void AssistantAlarmTimerController::OnTimerAdded(
    const mojom::AssistantTimer& timer) {
  // Schedule a repeating timer to tick the tracked timers.
  if (!ticker_.IsRunning()) {
    ticker_.Start(FROM_HERE, kTickInterval, &model_,
                  &AssistantAlarmTimerModel::Tick);
  }

  // Create a notification for the added alarm/timer.
  assistant_controller_->notification_controller()->AddOrUpdateNotification(
      CreateTimerNotification(timer));
}

void AssistantAlarmTimerController::OnTimerUpdated(
    const mojom::AssistantTimer& timer) {
  // When a |timer| is updated we need to update the corresponding notification
  // unless it has already been dismissed by the user.
  auto* notification_controller =
      assistant_controller_->notification_controller();
  if (notification_controller->model()->HasNotificationForId(
          CreateTimerNotificationId(timer))) {
    notification_controller->AddOrUpdateNotification(
        CreateTimerNotification(timer));
  }
}

void AssistantAlarmTimerController::OnTimerRemoved(
    const mojom::AssistantTimer& timer) {
  // If our model is empty, we no longer need tick updates.
  if (model_.empty())
    ticker_.Stop();

  // Remove any notification associated w/ |timer|.
  assistant_controller_->notification_controller()->RemoveNotificationById(
      CreateTimerNotificationId(timer), /*from_server=*/false);
}

void AssistantAlarmTimerController::OnAllTimersRemoved() {
  // We can stop our timer from ticking when all timers are removed.
  ticker_.Stop();

  // Remove any notifications associated w/ timers.
  assistant_controller_->notification_controller()
      ->RemoveNotificationByGroupingKey(kTimerNotificationGroupingKey,
                                        /*from_server=*/false);
}

void AssistantAlarmTimerController::PerformAlarmTimerAction(
    const AlarmTimerAction& action,
    const std::string& alarm_timer_id,
    const base::Optional<base::TimeDelta>& duration) {
  DCHECK(assistant_);

  switch (action) {
    case AlarmTimerAction::kAddTimeToTimer:
      if (!duration.has_value()) {
        LOG(ERROR) << "Ignoring add time to timer action duration.";
        return;
      }
      assistant_->AddTimeToTimer(alarm_timer_id, duration.value());
      break;
    case AlarmTimerAction::kRemove:
      assistant_->RemoveAlarmTimer(alarm_timer_id);
      break;
  }
}

}  // namespace ash
