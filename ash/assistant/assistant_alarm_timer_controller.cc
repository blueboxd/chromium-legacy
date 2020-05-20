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
using chromeos::assistant::features::IsTimersV2Enabled;
using chromeos::assistant::mojom::AssistantNotification;
using chromeos::assistant::mojom::AssistantNotificationButton;
using chromeos::assistant::mojom::AssistantNotificationButtonPtr;
using chromeos::assistant::mojom::AssistantNotificationPtr;

// Grouping key and ID prefix for timer notifications.
constexpr char kTimerNotificationGroupingKey[] = "assistant/timer";
constexpr char kTimerNotificationIdPrefix[] = "assistant/timer";

// Interval at which alarms/timers are ticked.
constexpr base::TimeDelta kTickInterval = base::TimeDelta::FromSeconds(1);

// Helpers ---------------------------------------------------------------------

// Creates a notification ID for the given |timer|. It is guaranteed that this
// method will always return the same notification ID given the same timer.
std::string CreateTimerNotificationId(const AssistantTimer& timer) {
  return std::string(kTimerNotificationIdPrefix) + timer.id;
}

// Creates a notification title.
std::string CreateTimerNotificationTitle() {
  return l10n_util::GetStringUTF8(IDS_ASSISTANT_TIMER_NOTIFICATION_TITLE);
}

// Creates a notification message for the given |timer|.
std::string CreateTimerNotificationMessage(const AssistantTimer& timer) {
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

// Creates notification action URL for the given |timer|.
GURL CreateTimerNotificationActionUrl(const AssistantTimer& timer) {
  return assistant::util::CreateAlarmTimerDeepLink(
             AlarmTimerAction::kRemoveAlarmOrTimer, timer.id)
      .value();
}

// Creates notification buttons for the given |timer|.
std::vector<AssistantNotificationButtonPtr> CreateTimerNotificationButtons(
    const AssistantTimer& timer) {
  std::vector<AssistantNotificationButtonPtr> buttons;

  if (!IsTimersV2Enabled()) {
    // "STOP" button.
    buttons.push_back(AssistantNotificationButton::New(
        l10n_util::GetStringUTF8(IDS_ASSISTANT_TIMER_NOTIFICATION_STOP_BUTTON),
        assistant::util::CreateAlarmTimerDeepLink(
            AlarmTimerAction::kRemoveAlarmOrTimer, timer.id)
            .value()));

    // "ADD 1 MIN" button.
    buttons.push_back(AssistantNotificationButton::New(
        l10n_util::GetStringUTF8(
            IDS_ASSISTANT_TIMER_NOTIFICATION_ADD_1_MIN_BUTTON),
        assistant::util::CreateAlarmTimerDeepLink(
            AlarmTimerAction::kAddTimeToTimer, timer.id,
            base::TimeDelta::FromMinutes(1))
            .value()));

    return buttons;
  }

  DCHECK(IsTimersV2Enabled());

  if (timer.state != AssistantTimerState::kFired) {
    if (timer.state == AssistantTimerState::kPaused) {
      // "RESUME" button.
      buttons.push_back(AssistantNotificationButton::New(
          l10n_util::GetStringUTF8(
              IDS_ASSISTANT_TIMER_NOTIFICATION_RESUME_BUTTON),
          assistant::util::CreateAlarmTimerDeepLink(
              AlarmTimerAction::kResumeTimer, timer.id)
              .value()));
    } else {
      // "PAUSE" button.
      buttons.push_back(AssistantNotificationButton::New(
          l10n_util::GetStringUTF8(
              IDS_ASSISTANT_TIMER_NOTIFICATION_PAUSE_BUTTON),
          assistant::util::CreateAlarmTimerDeepLink(
              AlarmTimerAction::kPauseTimer, timer.id)
              .value()));
    }
  }

  // "CANCEL" button.
  buttons.push_back(AssistantNotificationButton::New(
      l10n_util::GetStringUTF8(IDS_ASSISTANT_TIMER_NOTIFICATION_CANCEL_BUTTON),
      assistant::util::CreateAlarmTimerDeepLink(
          AlarmTimerAction::kRemoveAlarmOrTimer, timer.id)
          .value()));

  if (timer.state == AssistantTimerState::kFired) {
    // "ADD 1 MIN" button.
    buttons.push_back(AssistantNotificationButton::New(
        l10n_util::GetStringUTF8(
            IDS_ASSISTANT_TIMER_NOTIFICATION_ADD_1_MIN_BUTTON),
        assistant::util::CreateAlarmTimerDeepLink(
            AlarmTimerAction::kAddTimeToTimer, timer.id,
            base::TimeDelta::FromMinutes(1))
            .value()));
  }

  return buttons;
}

// Creates a notification for the given |timer|.
AssistantNotificationPtr CreateTimerNotification(const AssistantTimer& timer) {
  AssistantNotificationPtr notification = AssistantNotification::New();
  notification->title = CreateTimerNotificationTitle();
  notification->message = CreateTimerNotificationMessage(timer);
  notification->action_url = CreateTimerNotificationActionUrl(timer);
  notification->buttons = CreateTimerNotificationButtons(timer);
  notification->client_id = CreateTimerNotificationId(timer);
  notification->grouping_key = kTimerNotificationGroupingKey;

  // This notification should be able to wake up the display if it was off.
  notification->is_high_priority = true;

  return notification;
}

}  // namespace

// AssistantAlarmTimerControllerImpl ------------------------------------------

AssistantAlarmTimerControllerImpl::AssistantAlarmTimerControllerImpl(
    AssistantControllerImpl* assistant_controller)
    : assistant_controller_(assistant_controller) {
  model_.AddObserver(this);
  assistant_controller_observer_.Add(AssistantController::Get());
}

AssistantAlarmTimerControllerImpl::~AssistantAlarmTimerControllerImpl() {
  model_.RemoveObserver(this);
}

void AssistantAlarmTimerControllerImpl::SetAssistant(
    chromeos::assistant::mojom::Assistant* assistant) {
  assistant_ = assistant;
}

void AssistantAlarmTimerControllerImpl::OnAssistantControllerConstructed() {
  AssistantState::Get()->AddObserver(this);
}

void AssistantAlarmTimerControllerImpl::OnAssistantControllerDestroying() {
  AssistantState::Get()->RemoveObserver(this);
}

void AssistantAlarmTimerControllerImpl::OnDeepLinkReceived(
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

void AssistantAlarmTimerControllerImpl::OnAssistantStatusChanged(
    chromeos::assistant::AssistantStatus status) {
  // If LibAssistant is no longer running we need to clear our cache to
  // accurately reflect LibAssistant alarm/timer state.
  if (status == chromeos::assistant::AssistantStatus::NOT_READY)
    model_.RemoveAllTimers();
}

void AssistantAlarmTimerControllerImpl::OnTimerStateChanged(
    std::vector<AssistantTimerPtr> timers) {
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

void AssistantAlarmTimerControllerImpl::OnTimerAdded(
    const AssistantTimer& timer) {
  // Schedule a repeating timer to tick the tracked timers.
  if (!ticker_.IsRunning()) {
    ticker_.Start(FROM_HERE, kTickInterval, &model_,
                  &AssistantAlarmTimerModel::Tick);
  }

  // Create a notification for the added alarm/timer.
  assistant_controller_->notification_controller()->AddOrUpdateNotification(
      CreateTimerNotification(timer));
}

void AssistantAlarmTimerControllerImpl::OnTimerUpdated(
    const AssistantTimer& timer) {
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

void AssistantAlarmTimerControllerImpl::OnTimerRemoved(
    const AssistantTimer& timer) {
  // If our model is empty, we no longer need tick updates.
  if (model_.empty())
    ticker_.Stop();

  // Remove any notification associated w/ |timer|.
  assistant_controller_->notification_controller()->RemoveNotificationById(
      CreateTimerNotificationId(timer), /*from_server=*/false);
}

void AssistantAlarmTimerControllerImpl::OnAllTimersRemoved() {
  // We can stop our timer from ticking when all timers are removed.
  ticker_.Stop();

  // Remove any notifications associated w/ timers.
  assistant_controller_->notification_controller()
      ->RemoveNotificationByGroupingKey(kTimerNotificationGroupingKey,
                                        /*from_server=*/false);
}

void AssistantAlarmTimerControllerImpl::PerformAlarmTimerAction(
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
    case AlarmTimerAction::kPauseTimer:
      DCHECK(!duration.has_value());
      assistant_->PauseTimer(alarm_timer_id);
      break;
    case AlarmTimerAction::kRemoveAlarmOrTimer:
      DCHECK(!duration.has_value());
      assistant_->RemoveAlarmOrTimer(alarm_timer_id);
      break;
    case AlarmTimerAction::kResumeTimer:
      DCHECK(!duration.has_value());
      assistant_->ResumeTimer(alarm_timer_id);
      break;
  }
}

}  // namespace ash
