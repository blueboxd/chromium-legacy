// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_CLIENT_LACROS_H_
#define CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_CLIENT_LACROS_H_

#include "chrome/browser/notifications/notification_platform_bridge.h"

class NotificationPlatformBridgeDelegate;

// Sends notifications to ash-chrome over mojo. Responds to user actions like
// clicks on notifications received over mojo. Works together with
// NotificationPlatformBridgeChromeOs because that class contains support for
// transient notifications and multiple profiles.
// TODO(jamescook): Derive from crosapi::mojom::MessageCenterClient once that
// mojo interface is introduced.
class MessageCenterClientLacros : public NotificationPlatformBridge {
 public:
  explicit MessageCenterClientLacros(
      NotificationPlatformBridgeDelegate* delegate);
  MessageCenterClientLacros(const MessageCenterClientLacros&) = delete;
  MessageCenterClientLacros& operator=(const MessageCenterClientLacros&) =
      delete;
  ~MessageCenterClientLacros() override;

  // NotificationPlatformBridge:
  void Display(NotificationHandler::Type notification_type,
               Profile* profile,
               const message_center::Notification& notification,
               std::unique_ptr<NotificationCommon::Metadata> metadata) override;
  void Close(Profile* profile, const std::string& notification_id) override;
  void GetDisplayed(Profile* profile,
                    GetDisplayedNotificationsCallback callback) const override;
  void SetReadyCallback(NotificationBridgeReadyCallback callback) override;
  void DisplayServiceShutDown(Profile* profile) override;

  // TODO(jamescook): Add "client" methods like OnNotificationClicked,
  // OnNotificationClosed, etc.

 private:
  NotificationPlatformBridgeDelegate* delegate_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_CLIENT_LACROS_H_
