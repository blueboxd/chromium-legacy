// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_CHROME_ASH_MESSAGE_CENTER_CLIENT_H_
#define CHROME_BROWSER_NOTIFICATIONS_CHROME_ASH_MESSAGE_CENTER_CLIENT_H_

#include "ash/public/interfaces/ash_message_center_controller.mojom.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"
#include "chrome/browser/notifications/notification_platform_bridge_chromeos.h"
#include "chrome/browser/notifications/notifier_controller.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "ui/message_center/public/cpp/notifier_id.h"

// This class serves as Chrome's AshMessageCenterClient, as well as the
// NotificationPlatformBridge for ChromeOS. It dispatches notifications to Ash
// and handles interactions with those notifications, plus it keeps track of
// NotifierControllers to provide notifier settings information to Ash (visible
// in NotifierSettingsView).
class ChromeAshMessageCenterClient : public ash::mojom::AshMessageCenterClient,
                                     public NotifierController::Observer,
                                     public content::NotificationObserver {
 public:
  explicit ChromeAshMessageCenterClient(
      NotificationPlatformBridgeDelegate* delegate);

  ~ChromeAshMessageCenterClient() override;

  void Display(const message_center::Notification& notification);
  void Close(const std::string& notification_id);

  // ash::mojom::AshMessageCenterClient:
  void SetNotifierEnabled(const message_center::NotifierId& notifier_id,
                          bool enabled) override;
  void GetNotifierList(GetNotifierListCallback callback) override;
  void GetArcAppIdByPackageName(
      const std::string& package_name,
      GetArcAppIdByPackageNameCallback callback) override;

  // NotifierController::Observer:
  void OnIconImageUpdated(const message_center::NotifierId& notifier_id,
                          const gfx::ImageSkia& icon) override;
  void OnNotifierEnabledChanged(const message_center::NotifierId& notifier_id,
                                bool enabled) override;

  // Flushs |binding_|.
  static void FlushForTesting();

 private:
  void RespondWithNotifierList(Profile* profile,
                               GetNotifierListCallback callback) const;

  // content::NotificationObserver override.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  NotificationPlatformBridgeDelegate* delegate_;

  // Notifier source for each notifier type.
  std::map<message_center::NotifierType, std::unique_ptr<NotifierController>>
      sources_;

  ash::mojom::AshMessageCenterControllerPtr controller_;
  mojo::AssociatedBinding<ash::mojom::AshMessageCenterClient> binding_;
  content::NotificationRegistrar registrar_;
  GetNotifierListCallback deferred_notifier_list_callback_;

  DISALLOW_COPY_AND_ASSIGN(ChromeAshMessageCenterClient);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_CHROME_ASH_MESSAGE_CENTER_CLIENT_H_
