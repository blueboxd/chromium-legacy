// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/prefetch/notifications/prefetch_notification_service_impl.h"

namespace offline_pages {

PrefetchNotificationServiceImpl::PrefetchNotificationServiceImpl(
    notifications::NotificationScheduleService* schedule_service)
    : schedule_service_(schedule_service) {
  DCHECK(schedule_service_);
}

PrefetchNotificationServiceImpl::~PrefetchNotificationServiceImpl() = default;

}  // namespace offline_pages
