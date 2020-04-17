// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_NOTIFICATION_CONTROLLER_H_
#define ASH_ASSISTANT_ASSISTANT_NOTIFICATION_CONTROLLER_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/assistant/assistant_notification_expiry_monitor.h"
#include "ash/assistant/model/assistant_notification_model.h"
#include "ash/assistant/model/assistant_notification_model_observer.h"
#include "ash/public/mojom/assistant_controller.mojom.h"
#include "base/macros.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace ash {

// The class to manage Assistant notifications.
class ASH_EXPORT AssistantNotificationController
    : public mojom::AssistantNotificationController,
      public AssistantNotificationModelObserver,
      public message_center::MessageCenterObserver {
 public:
  using AssistantNotification =
      chromeos::assistant::mojom::AssistantNotification;
  using AssistantNotificationPtr =
      chromeos::assistant::mojom::AssistantNotificationPtr;
  using AssistantNotificationType =
      chromeos::assistant::mojom::AssistantNotificationType;

  AssistantNotificationController();
  ~AssistantNotificationController() override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::AssistantNotificationController> receiver);

  // Returns the underlying model.
  const AssistantNotificationModel* model() const { return &model_; }

  // Adds/removes the specified model |observer|.
  void AddModelObserver(AssistantNotificationModelObserver* observer);
  void RemoveModelObserver(AssistantNotificationModelObserver* observer);

  // Provides a pointer to the |assistant| owned by AssistantController.
  void SetAssistant(chromeos::assistant::mojom::Assistant* assistant);

  // mojom::AssistantNotificationController:
  void AddOrUpdateNotification(AssistantNotificationPtr notification) override;
  void RemoveNotificationById(const std::string& id, bool from_server) override;
  void RemoveNotificationByGroupingKey(const std::string& grouping_id,
                                       bool from_server) override;
  void RemoveAllNotifications(bool from_server) override;
  void SetQuietMode(bool enabled) override;

  // AssistantNotificationModelObserver:
  void OnNotificationAdded(const AssistantNotification* notification) override;
  void OnNotificationUpdated(
      const AssistantNotification* notification) override;
  void OnNotificationRemoved(const AssistantNotification* notification,
                             bool from_server) override;
  void OnAllNotificationsRemoved(bool from_server) override;

  // message_center::MessageCenterObserver:
  void OnNotificationAdded(const std::string& id) override {}
  void OnNotificationClicked(
      const std::string& id,
      const base::Optional<int>& button_index,
      const base::Optional<base::string16>& reply) override;
  void OnNotificationUpdated(const std::string& notification) override {}
  void OnNotificationRemoved(const std::string& notification_id,
                             bool by_user) override;

 private:
  mojo::Receiver<mojom::AssistantNotificationController> receiver_{this};

  AssistantNotificationModel model_;
  AssistantNotificationExpiryMonitor expiry_monitor_;

  // Owned by AssistantController.
  chromeos::assistant::mojom::Assistant* assistant_ = nullptr;

  const message_center::NotifierId notifier_id_;

  DISALLOW_COPY_AND_ASSIGN(AssistantNotificationController);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_NOTIFICATION_CONTROLLER_H_
