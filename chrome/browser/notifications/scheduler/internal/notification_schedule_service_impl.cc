// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/notification_schedule_service_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/notifications/scheduler/internal/notification_scheduler.h"
#include "chrome/browser/notifications/scheduler/public/notification_params.h"

namespace notifications {

NotificationScheduleServiceImpl::NotificationScheduleServiceImpl(
    std::unique_ptr<NotificationScheduler> scheduler)
    : scheduler_(std::move(scheduler)) {
  scheduler_->Init(
      base::BindOnce(&NotificationScheduleServiceImpl::OnInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

NotificationScheduleServiceImpl::~NotificationScheduleServiceImpl() = default;

void NotificationScheduleServiceImpl::Schedule(
    std::unique_ptr<NotificationParams> notification_params) {
  scheduler_->Schedule(std::move(notification_params));
}

void NotificationScheduleServiceImpl::DeleteNotifications(
    SchedulerClientType type) {
  scheduler_->DeleteAllNotifications(type);
}

void NotificationScheduleServiceImpl::GetImpressionDetail(
    SchedulerClientType type,
    ImpressionDetail::ImpressionDetailCallback callback) {
  scheduler_->GetImpressionDetail(type, std::move(callback));
}

NotificationBackgroundTaskScheduler::Handler*
NotificationScheduleServiceImpl::GetBackgroundTaskSchedulerHandler() {
  return this;
}

UserActionHandler* NotificationScheduleServiceImpl::GetUserActionHandler() {
  return this;
}

void NotificationScheduleServiceImpl::OnStartTask(
    SchedulerTaskTime task_time,
    TaskFinishedCallback callback) {
  scheduler_->OnStartTask(task_time, std::move(callback));
}

void NotificationScheduleServiceImpl::OnStopTask(SchedulerTaskTime task_time) {
  scheduler_->OnStopTask(task_time);
}

void NotificationScheduleServiceImpl::OnClick(SchedulerClientType type,
                                              const std::string& guid) {
  scheduler_->OnClick(type, guid);
}

void NotificationScheduleServiceImpl::OnActionClick(
    SchedulerClientType type,
    const std::string& guid,
    ActionButtonType button_type) {
  scheduler_->OnActionClick(type, guid, button_type);
}

void NotificationScheduleServiceImpl::OnDismiss(SchedulerClientType type,
                                                const std::string& guid) {
  scheduler_->OnDismiss(type, guid);
}

void NotificationScheduleServiceImpl::OnInitialized(bool success) {
  // TODO(xingliu): Track metric here.
  NOTIMPLEMENTED();
}

}  // namespace notifications
