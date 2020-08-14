// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/message_center_ash.h"

#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "chromeos/crosapi/mojom/message_center.mojom.h"
#include "chromeos/crosapi/mojom/notification.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "url/gurl.h"

namespace mc = message_center;

namespace crosapi {
namespace {

mc::NotificationType FromMojo(mojom::NotificationType type) {
  switch (type) {
    case mojom::NotificationType::kSimple:
      return mc::NOTIFICATION_TYPE_SIMPLE;
    case mojom::NotificationType::kImage:
      return mc::NOTIFICATION_TYPE_IMAGE;
    case mojom::NotificationType::kList:
      return mc::NOTIFICATION_TYPE_MULTIPLE;
    case mojom::NotificationType::kProgress:
      return mc::NOTIFICATION_TYPE_PROGRESS;
  }
}

// Forwards NotificationDelegate methods to a remote delegate over mojo. If the
// remote delegate disconnects (e.g. lacros-chrome crashes) the corresponding
// notification will be removed.
class ForwardingDelegate : public message_center::NotificationDelegate {
 public:
  ForwardingDelegate(const std::string& notification_id,
                     mojo::PendingRemote<mojom::NotificationDelegate> delegate)
      : notification_id_(notification_id),
        remote_delegate_(std::move(delegate)) {
    DCHECK(!notification_id_.empty());
    DCHECK(remote_delegate_);
  }
  ForwardingDelegate(const ForwardingDelegate&) = delete;
  ForwardingDelegate& operator=(const ForwardingDelegate&) = delete;

  void Init() {
    // Cannot be done in constructor because base::BindOnce() requires a
    // non-zero reference count.
    remote_delegate_.set_disconnect_handler(
        base::BindOnce(&ForwardingDelegate::OnDisconnect, this));
  }

 private:
  // Private due to ref-counting.
  ~ForwardingDelegate() override = default;

  void OnDisconnect() {
    // NOTE: Triggers a call to Close() if the notification is still showing.
    mc::MessageCenter::Get()->RemoveNotification(notification_id_,
                                                 /*by_user=*/false);
  }

  // message_center::NotificationDelegate:
  void Close(bool by_user) override {
    // Can be called after |remote_delegate_| is disconnected.
    if (remote_delegate_)
      remote_delegate_->OnNotificationClosed(by_user);
  }

  void Click(const base::Optional<int>& button_index,
             const base::Optional<base::string16>& reply) override {
    if (button_index) {
      // Chrome OS does not support inline reply. The button index comes out of
      // trusted ash-side message center UI code and is guaranteed not to be
      // negative.
      remote_delegate_->OnNotificationButtonClicked(
          base::checked_cast<uint32_t>(*button_index));
    } else {
      remote_delegate_->OnNotificationClicked();
    }
  }

  void SettingsClick() override {
    remote_delegate_->OnNotificationSettingsButtonClicked();
  }

  void DisableNotification() override {
    remote_delegate_->OnNotificationDisabled();
  }

  const std::string notification_id_;
  mojo::Remote<mojom::NotificationDelegate> remote_delegate_;
};

}  // namespace

MessageCenterAsh::MessageCenterAsh(
    mojo::PendingReceiver<mojom::MessageCenter> receiver)
    : receiver_(this, std::move(receiver)) {}

MessageCenterAsh::~MessageCenterAsh() = default;

void MessageCenterAsh::DisplayNotification(
    mojom::NotificationPtr notification,
    mojo::PendingRemote<mojom::NotificationDelegate> delegate) {
  auto forwarding_delegate = base::MakeRefCounted<ForwardingDelegate>(
      notification->id, std::move(delegate));
  forwarding_delegate->Init();

  // TODO(crbug.com/1113889): Icon support.
  // TODO(crbug.com/1113889): NotifierId support.
  // TODO(crbug.com/1113889): RichNotificationData support.
  mc::MessageCenter::Get()->AddNotification(std::make_unique<mc::Notification>(
      FromMojo(notification->type), notification->id, notification->title,
      notification->message, gfx::Image(), notification->display_source,
      notification->origin_url.value_or(GURL()), mc::NotifierId(),
      mc::RichNotificationData(), std::move(forwarding_delegate)));
}

void MessageCenterAsh::CloseNotification(const std::string& id) {
  mc::MessageCenter::Get()->RemoveNotification(id, /*by_user=*/false);
}

}  // namespace crosapi
