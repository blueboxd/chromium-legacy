// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_notification_list_view.h"

#include "chrome/browser/ui/views/global_media_controls/media_notification_container_impl.h"
#include "ui/views/layout/box_layout.h"

MediaNotificationListView::MediaNotificationListView() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
}

MediaNotificationListView::~MediaNotificationListView() = default;

void MediaNotificationListView::ShowNotification(
    const std::string& id,
    std::unique_ptr<MediaNotificationContainerImpl> notification) {
  DCHECK(!base::Contains(notifications_, id));
  DCHECK_NE(nullptr, notification.get());

  notifications_[id] = AddChildView(std::move(notification));
  PreferredSizeChanged();
}

void MediaNotificationListView::HideNotification(const std::string& id) {
  if (!base::Contains(notifications_, id))
    return;

  RemoveChildView(notifications_[id]);
  notifications_.erase(id);
  PreferredSizeChanged();
}
