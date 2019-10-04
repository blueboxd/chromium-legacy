// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_notification_list_view.h"

#include "chrome/browser/ui/views/global_media_controls/media_notification_container_impl.h"
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"
#include "ui/views/layout/box_layout.h"

namespace {

constexpr int kMediaListMaxHeight = 478;

}  // anonymous namespace

MediaNotificationListView::MediaNotificationListView() {
  SetBackgroundColor(SK_ColorTRANSPARENT);
  SetContents(std::make_unique<views::View>());
  contents()->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  ClipHeightTo(0, kMediaListMaxHeight);

  SetVerticalScrollBar(
      std::make_unique<views::OverlayScrollBar>(/*horizontal=*/false));
  SetHorizontalScrollBar(
      std::make_unique<views::OverlayScrollBar>(/*horizontal=*/true));
}

MediaNotificationListView::~MediaNotificationListView() = default;

void MediaNotificationListView::ShowNotification(
    const std::string& id,
    std::unique_ptr<MediaNotificationContainerImpl> notification) {
  DCHECK(!base::Contains(notifications_, id));
  DCHECK_NE(nullptr, notification.get());

  notifications_[id] = contents()->AddChildView(std::move(notification));

  contents()->InvalidateLayout();
  PreferredSizeChanged();
}

void MediaNotificationListView::HideNotification(const std::string& id) {
  if (!base::Contains(notifications_, id))
    return;

  contents()->RemoveChildView(notifications_[id]);
  notifications_.erase(id);

  contents()->InvalidateLayout();
  PreferredSizeChanged();
}
