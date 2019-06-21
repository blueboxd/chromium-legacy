// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/init_aware_scheduler.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/notifications/scheduler/public/notification_params.h"

namespace notifications {

InitAwareNotificationScheduler::InitAwareNotificationScheduler(
    std::unique_ptr<NotificationScheduler> impl)
    : impl_(std::move(impl)), weak_ptr_factory_(this) {}

InitAwareNotificationScheduler::~InitAwareNotificationScheduler() = default;

void InitAwareNotificationScheduler::Init(InitCallback init_callback) {
  DCHECK(!init_success_.has_value());
  impl_->Init(base::BindOnce(&InitAwareNotificationScheduler::OnInitialized,
                             weak_ptr_factory_.GetWeakPtr(),
                             std::move(init_callback)));
}

void InitAwareNotificationScheduler::Schedule(
    std::unique_ptr<NotificationParams> params) {
  if (IsReady()) {
    impl_->Schedule(std::move(params));
    return;
  }
  MaybeCacheClosure(base::BindOnce(&InitAwareNotificationScheduler::Schedule,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   std::move(params)));
}

void InitAwareNotificationScheduler::OnStartTask() {
  if (IsReady()) {
    impl_->OnStartTask();
    return;
  }
  MaybeCacheClosure(base::BindOnce(&InitAwareNotificationScheduler::OnStartTask,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void InitAwareNotificationScheduler::OnStopTask() {
  if (IsReady()) {
    impl_->OnStopTask();
    return;
  }
  MaybeCacheClosure(base::BindOnce(&InitAwareNotificationScheduler::OnStopTask,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void InitAwareNotificationScheduler::OnClick(
    const std::string& notification_id) {
  if (IsReady()) {
    impl_->OnClick(std::move(notification_id));
    return;
  }
  MaybeCacheClosure(base::BindOnce(&InitAwareNotificationScheduler::OnClick,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   std::move(notification_id)));
}

void InitAwareNotificationScheduler::OnActionClick(
    const std::string& notification_id,
    ActionButtonType button_type) {
  if (IsReady()) {
    impl_->OnActionClick(std::move(notification_id), button_type);
    return;
  }

  MaybeCacheClosure(base::BindOnce(
      &InitAwareNotificationScheduler::OnActionClick,
      weak_ptr_factory_.GetWeakPtr(), std::move(notification_id), button_type));
}

void InitAwareNotificationScheduler::OnDismiss(
    const std::string& notification_id) {
  if (IsReady()) {
    impl_->OnDismiss(std::move(notification_id));
    return;
  }

  MaybeCacheClosure(base::BindOnce(&InitAwareNotificationScheduler::OnDismiss,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   std::move(notification_id)));
}

void InitAwareNotificationScheduler::OnInitialized(InitCallback init_callback,
                                                   bool success) {
  init_success_ = success;
  if (!success) {
    cached_closures_.clear();
    std::move(init_callback).Run(false);
    return;
  }

  // Flush all cached calls.
  for (auto it = cached_closures_.begin(); it != cached_closures_.end(); ++it) {
    std::move(*it).Run();
  }
  cached_closures_.clear();
  std::move(init_callback).Run(true);
}

bool InitAwareNotificationScheduler::IsReady() const {
  return init_success_.has_value() && *init_success_;
}

void InitAwareNotificationScheduler::MaybeCacheClosure(
    base::OnceClosure closure) {
  DCHECK(closure);

  // Drop the call if initialization failed.
  if (init_success_.has_value() && !*init_success_)
    return;

  // Cache the closure to invoke later.
  cached_closures_.emplace_back(std::move(closure));
}

}  // namespace notifications
