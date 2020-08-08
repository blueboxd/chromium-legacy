// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/message_center_client_lacros.h"

#include <utility>

#include "base/check.h"
#include "base/notreached.h"

MessageCenterClientLacros::MessageCenterClientLacros(
    NotificationPlatformBridgeDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

MessageCenterClientLacros::~MessageCenterClientLacros() = default;

void MessageCenterClientLacros::Display(
    NotificationHandler::Type notification_type,
    Profile* profile,
    const message_center::Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata> metadata) {
  NOTIMPLEMENTED();
}

void MessageCenterClientLacros::Close(Profile* profile,
                                      const std::string& notification_id) {
  NOTIMPLEMENTED();
}

void MessageCenterClientLacros::GetDisplayed(
    Profile* profile,
    GetDisplayedNotificationsCallback callback) const {
  NOTIMPLEMENTED();
  std::move(callback).Run(/*notification_ids=*/{}, /*supports_sync=*/false);
}

void MessageCenterClientLacros::SetReadyCallback(
    NotificationBridgeReadyCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(true);
}

void MessageCenterClientLacros::DisplayServiceShutDown(Profile* profile) {
  NOTIMPLEMENTED();
}
