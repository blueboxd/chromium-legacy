// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updates/update_notification_service_impl.h"

#include "base/logging.h"

namespace updates {

UpdateNotificationServiceImpl::UpdateNotificationServiceImpl() = default;

UpdateNotificationServiceImpl::~UpdateNotificationServiceImpl() = default;

void UpdateNotificationServiceImpl::Schedule(
    notifications::NotificationData data) {
  NOTIMPLEMENTED();
}

void UpdateNotificationServiceImpl::OnClientOverviewQueried(
    notifications::ClientOverview overview) {
  NOTIMPLEMENTED();
}

}  // namespace updates
