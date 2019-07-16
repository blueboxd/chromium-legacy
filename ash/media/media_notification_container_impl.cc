// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/media/media_notification_container_impl.h"

#include "ui/message_center/message_center.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/native_theme/native_theme_dark_aura.h"
#include "ui/views/background.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

MediaNotificationContainerImpl::MediaNotificationContainerImpl(
    const message_center::Notification& notification,
    base::WeakPtr<media_message_center::MediaNotificationItem> item)
    : message_center::MessageView(notification),
      control_buttons_view_(
          std::make_unique<message_center::NotificationControlButtonsView>(
              this)),
      view_(this,
            std::move(item),
            control_buttons_view_.get(),
            message_center::MessageCenter::Get()
                ->GetSystemNotificationAppName()) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Since we own these, we don't want Views to destroy them when their parent
  // is destroyed.
  control_buttons_view_->set_owned_by_client();
  view_.set_owned_by_client();

  AddChildView(&view_);

  SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));
}

MediaNotificationContainerImpl::~MediaNotificationContainerImpl() = default;

void MediaNotificationContainerImpl::UpdateWithNotification(
    const message_center::Notification& notification) {
  MessageView::UpdateWithNotification(notification);

  UpdateControlButtonsVisibilityWithNotification(notification);

  PreferredSizeChanged();
  Layout();
  SchedulePaint();
}

message_center::NotificationControlButtonsView*
MediaNotificationContainerImpl::GetControlButtonsView() const {
  return control_buttons_view_.get();
}

void MediaNotificationContainerImpl::SetExpanded(bool expanded) {
  view_.SetExpanded(expanded);
}

void MediaNotificationContainerImpl::UpdateCornerRadius(int top_radius,
                                                        int bottom_radius) {
  MessageView::SetCornerRadius(top_radius, bottom_radius);
  view_.UpdateCornerRadius(top_radius, bottom_radius);
}

void MediaNotificationContainerImpl::OnExpanded(bool expanded) {
  PreferredSizeChanged();
}

void MediaNotificationContainerImpl::OnMouseEvent(ui::MouseEvent* event) {
  switch (event->type()) {
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED:
      UpdateControlButtonsVisibility();
      break;
    default:
      break;
  }

  View::OnMouseEvent(event);
}

void MediaNotificationContainerImpl::
    UpdateControlButtonsVisibilityWithNotification(
        const message_center::Notification& notification) {
  // Media notifications do not use the settings and snooze buttons.
  DCHECK(!notification.should_show_settings_button());
  DCHECK(!notification.should_show_snooze_button());

  control_buttons_view_->ShowCloseButton(!notification.pinned());
  UpdateControlButtonsVisibility();
}

}  // namespace ash
