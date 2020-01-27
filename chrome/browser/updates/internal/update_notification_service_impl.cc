// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updates/internal/update_notification_service_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/time/default_clock.h"
#include "chrome/browser/notifications/scheduler/public/client_overview.h"
#include "chrome/browser/notifications/scheduler/public/notification_params.h"
#include "chrome/browser/notifications/scheduler/public/notification_schedule_service.h"
#include "chrome/browser/notifications/scheduler/public/schedule_service_utils.h"
#include "chrome/browser/updates/update_notification_config.h"
#include "chrome/browser/updates/update_notification_info.h"
#include "chrome/browser/updates/update_notification_service_bridge.h"

namespace updates {
namespace {

void BuildNotificationData(const updates::UpdateNotificationInfo& data,
                           notifications::NotificationData* out) {
  DCHECK(out);
  out->title = data.title;
  out->message = data.message;
}

}  // namespace

// Maximum number of update notification should be cached in scheduler.
constexpr int kNumMaxNotificationsLimit = 1;

// Maxmium number of consecutive dismiss actions from user that should be
// considered as negative feedback.
constexpr int kNumConsecutiveDismissCountCap = 2;

UpdateNotificationServiceImpl::UpdateNotificationServiceImpl(
    notifications::NotificationScheduleService* schedule_service,
    std::unique_ptr<UpdateNotificationConfig> config,
    std::unique_ptr<UpdateNotificationServiceBridge> bridge)
    : schedule_service_(schedule_service),
      config_(std::move(config)),
      bridge_(std::move(bridge)) {}

UpdateNotificationServiceImpl::~UpdateNotificationServiceImpl() = default;

void UpdateNotificationServiceImpl::Schedule(UpdateNotificationInfo data) {
  schedule_service_->GetClientOverview(
      notifications::SchedulerClientType::kChromeUpdate,
      base::BindOnce(&UpdateNotificationServiceImpl::OnClientOverviewQueried,
                     weak_ptr_factory_.GetWeakPtr(), std::move(data)));
}

bool UpdateNotificationServiceImpl::IsReadyToDisplay() const {
  auto last_shown_timestamp = bridge_->GetLastShownTimeStamp();
  bool wait_next_trottle_event =
      last_shown_timestamp.has_value() &&
      GetThrottleInterval() >= base::Time::Now() - last_shown_timestamp.value();
  if (!config_->is_enabled || wait_next_trottle_event)
    return false;
  bridge_->UpdateLastShownTimeStamp(base::Time::Now());
  return true;
}

base::TimeDelta UpdateNotificationServiceImpl::GetThrottleInterval() const {
  auto throttle_interval = bridge_->GetThrottleInterval();
  return throttle_interval.has_value() ? throttle_interval.value()
                                       : config_->default_interval;
}

void UpdateNotificationServiceImpl::OnClientOverviewQueried(
    UpdateNotificationInfo data,
    notifications::ClientOverview overview) {
  int num_scheduled_notifs = overview.num_scheduled_notifications;

  if (num_scheduled_notifs == kNumMaxNotificationsLimit) {
    return;
  }

  if (num_scheduled_notifs > kNumMaxNotificationsLimit) {
    schedule_service_->DeleteNotifications(
        notifications::SchedulerClientType::kChromeUpdate);
  }

  notifications::NotificationData notification_data;
  BuildNotificationData(data, &notification_data);
  auto params = std::make_unique<notifications::NotificationParams>(
      notifications::SchedulerClientType::kChromeUpdate,
      std::move(notification_data), BuildScheduleParams());
  schedule_service_->Schedule(std::move(params));
}

notifications::ScheduleParams
UpdateNotificationServiceImpl::BuildScheduleParams() {
  DCHECK(config_);

  notifications::ScheduleParams schedule_params;
  notifications::TimePair actual_window;
  notifications::NextTimeWindow(
      base::DefaultClock::GetInstance(), config_->deliver_window_morning,
      config_->deliver_window_evening, &actual_window);
  schedule_params.deliver_time_start =
      base::make_optional(std::move(actual_window.first));
  schedule_params.deliver_time_end =
      base::make_optional(std::move(actual_window.second));
  return schedule_params;
}

void UpdateNotificationServiceImpl::OnUserDismiss() {
  int count = bridge_->GetUserDismissCount() + 1;
  if (count >= kNumConsecutiveDismissCountCap) {
    ApplyLinearThrottle();
    count = 0;
  }
  bridge_->UpdateUserDismissCount(count);
}

void UpdateNotificationServiceImpl::ApplyLinearThrottle() {
  auto scale = config_->throttle_interval_linear_co_scale;
  auto offset =
      base::TimeDelta::FromDays(config_->throttle_interval_linear_co_offset);
  auto interval = GetThrottleInterval();
  bridge_->UpdateThrottleInterval(scale * interval + offset);
}

void UpdateNotificationServiceImpl::OnUserClick() {
  bridge_->LaunchChromeActivity();
}

}  // namespace updates
