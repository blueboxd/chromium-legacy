// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/media/media_notification_controller_impl.h"

#include "ash/media/media_notification_constants.h"
#include "ash/media/media_notification_container_impl.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/session_observer.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "components/media_message_center/media_notification_item.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_blocker.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "ui/message_center/views/message_view_factory.h"

namespace ash {

namespace {

std::unique_ptr<message_center::MessageView> CreateCustomMediaNotificationView(
    const message_center::Notification& notification) {
  DCHECK_EQ(kMediaSessionNotificationCustomViewType,
            notification.custom_view_type());
  auto* controller = Shell::Get()->media_notification_controller();
  if (controller)
    return controller->CreateMediaNotification(notification);
  return nullptr;
}

// The maximum number of media notifications to count when recording the
// Media.Notification.Count histogram. 20 was chosen because it would be very
// unlikely to see a user with 20+ things playing at once.
const int kMediaNotificationCountHistogramMax = 20;

// MediaNotificationBlocker will block media notifications if the screen is
// locked.
class MediaNotificationBlocker : public message_center::NotificationBlocker,
                                 public SessionObserver {
 public:
  MediaNotificationBlocker(message_center::MessageCenter* message_center,
                           SessionControllerImpl* session_controller)
      : message_center::NotificationBlocker(message_center),
        locked_(session_controller->IsScreenLocked()),
        session_controller_(session_controller) {
    message_center->AddNotificationBlocker(this);
    session_controller->AddObserver(this);

    NotifyBlockingStateChanged();
  }

  ~MediaNotificationBlocker() override {
    message_center()->RemoveNotificationBlocker(this);
    session_controller_->RemoveObserver(this);
  }

  void CheckState() override {
    OnLockStateChanged(session_controller_->IsScreenLocked());
  }

  bool ShouldShowNotification(
      const message_center::Notification& notification) const override {
    if (notification.notifier_id() ==
        message_center::NotifierId(
            message_center::NotifierType::SYSTEM_COMPONENT,
            kMediaSessionNotifierId)) {
      return !locked_;
    }

    return true;
  }

  bool ShouldShowNotificationAsPopup(
      const message_center::Notification& notification) const override {
    return ShouldShowNotification(notification);
  }

  void OnLockStateChanged(bool locked) override {
    if (locked == locked_)
      return;

    locked_ = locked;
    NotifyBlockingStateChanged();
  }

 private:
  bool locked_;

  SessionControllerImpl* const session_controller_;

  DISALLOW_COPY_AND_ASSIGN(MediaNotificationBlocker);
};

}  // namespace

// static
const char MediaNotificationControllerImpl::kCountHistogramName[] =
    "Media.Notification.Count";

MediaNotificationControllerImpl::MediaNotificationControllerImpl(
    service_manager::Connector* connector)
    : blocker_(std::make_unique<MediaNotificationBlocker>(
          message_center::MessageCenter::Get(),
          Shell::Get()->session_controller())) {
  if (!message_center::MessageViewFactory::HasCustomNotificationViewFactory(
          kMediaSessionNotificationCustomViewType)) {
    message_center::MessageViewFactory::SetCustomNotificationViewFactory(
        kMediaSessionNotificationCustomViewType,
        base::BindRepeating(&CreateCustomMediaNotificationView));
  }

  // |connector| can be null in tests.
  if (!connector)
    return;

  media_session::mojom::AudioFocusManagerPtr audio_focus_ptr;
  connector->BindInterface(media_session::mojom::kServiceName,
                           mojo::MakeRequest(&audio_focus_ptr));
  connector->BindInterface(media_session::mojom::kServiceName,
                           mojo::MakeRequest(&controller_manager_ptr_));

  audio_focus_ptr->AddObserver(
      audio_focus_observer_receiver_.BindNewPipeAndPassRemote());
}

MediaNotificationControllerImpl::~MediaNotificationControllerImpl() = default;

void MediaNotificationControllerImpl::OnFocusGained(
    media_session::mojom::AudioFocusRequestStatePtr session) {
  const std::string id = session->request_id->ToString();

  if (base::Contains(notifications_, id))
    return;

  media_session::mojom::MediaControllerPtr controller;

  // |controller_manager_ptr_| may be null in tests where connector is
  // unavailable.
  if (controller_manager_ptr_) {
    controller_manager_ptr_->CreateMediaControllerForSession(
        mojo::MakeRequest(&controller), *session->request_id);
  }

  notifications_.emplace(
      std::piecewise_construct, std::forward_as_tuple(id),
      std::forward_as_tuple(
          this, id, session->source_name.value_or(std::string()),
          std::move(controller), std::move(session->session_info)));
}

void MediaNotificationControllerImpl::OnFocusLost(
    media_session::mojom::AudioFocusRequestStatePtr session) {
  notifications_.erase(session->request_id->ToString());
}

void MediaNotificationControllerImpl::ShowNotification(const std::string& id) {
  // If a notification already exists, do nothing.
  if (message_center::MessageCenter::Get()->FindVisibleNotificationById(id))
    return;

  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotification(
          message_center::NotificationType::NOTIFICATION_TYPE_CUSTOM, id,
          base::string16(), base::string16(), base::string16(), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kMediaSessionNotifierId),
          message_center::RichNotificationData(), nullptr, gfx::VectorIcon(),
          message_center::SystemNotificationWarningLevel::NORMAL);

  // Set the priority to low to prevent the notification showing as a popup and
  // keep it at the bottom of the list.
  notification->set_priority(message_center::LOW_PRIORITY);

  notification->set_custom_view_type(kMediaSessionNotificationCustomViewType);

  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));

  RecordConcurrentNotificationCount();
}

void MediaNotificationControllerImpl::HideNotification(const std::string& id) {
  message_center::MessageCenter::Get()->RemoveNotification(id, false);
}

std::unique_ptr<MediaNotificationContainerImpl>
MediaNotificationControllerImpl::CreateMediaNotification(
    const message_center::Notification& notification) {
  base::WeakPtr<media_message_center::MediaNotificationItem> item;

  auto it = notifications_.find(notification.id());
  if (it != notifications_.end())
    item = it->second.GetWeakPtr();

  return std::make_unique<MediaNotificationContainerImpl>(notification,
                                                          std::move(item));
}

void MediaNotificationControllerImpl::RecordConcurrentNotificationCount() {
  UMA_HISTOGRAM_EXACT_LINEAR(
      kCountHistogramName,
      message_center::MessageCenter::Get()
          ->FindNotificationsByAppId(kMediaSessionNotifierId)
          .size(),
      kMediaNotificationCountHistogramMax);
}

}  // namespace ash
