// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/notification_schedule_service_impl.h"

#include <utility>

#include "base/logging.h"
#include "chrome/browser/notifications/scheduler/internal/notification_scheduler.h"
#include "chrome/browser/notifications/scheduler/public/notification_params.h"

namespace notifications {

NotificationScheduleServiceImpl::NotificationScheduleServiceImpl(
    std::unique_ptr<NotificationScheduler> scheduler)
    : scheduler_(std::move(scheduler)) {}

NotificationScheduleServiceImpl::~NotificationScheduleServiceImpl() = default;

void NotificationScheduleServiceImpl::Schedule(
    std::unique_ptr<NotificationParams> notification_params) {
  scheduler_->Schedule(std::move(notification_params));
}

NotificationBackgroundTaskScheduler::Handler*
NotificationScheduleServiceImpl::GetBackgroundTaskSchedulerHandler() {
  return this;
}

UserActionHandler* NotificationScheduleServiceImpl::GetUserActionHandler() {
  return this;
}

void NotificationScheduleServiceImpl::OnStartTask() {
  scheduler_->OnStartTask();
}

void NotificationScheduleServiceImpl::OnStopTask() {
  scheduler_->OnStopTask();
}

void NotificationScheduleServiceImpl::OnClick(
    const std::string& notification_id) {
  scheduler_->OnClick(notification_id);
}

void NotificationScheduleServiceImpl::OnActionClick(
    const std::string& notification_id,
    ActionButtonType button_type) {
  scheduler_->OnActionClick(notification_id, button_type);
}

void NotificationScheduleServiceImpl::OnDismiss(
    const std::string& notification_id) {
  scheduler_->OnDismiss(notification_id);
}

}  // namespace notifications
