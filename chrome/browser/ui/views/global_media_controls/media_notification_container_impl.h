// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_CONTAINER_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_CONTAINER_IMPL_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "components/media_message_center/media_notification_container.h"
#include "components/media_message_center/media_notification_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace media_message_center {
class MediaNotificationItem;
}  // namespace media_message_center

namespace views {
class ImageButton;
}  // namespace views

class MediaDialogView;
class MediaToolbarButtonController;

// MediaNotificationContainerImpl holds a media notification for display within
// the MediaDialogView. The media notification shows metadata for a media
// session and can control playback.
class MediaNotificationContainerImpl
    : public views::View,
      public media_message_center::MediaNotificationContainer,
      public views::ButtonListener {
 public:
  MediaNotificationContainerImpl(
      MediaDialogView* parent,
      MediaToolbarButtonController* controller,
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item);
  ~MediaNotificationContainerImpl() override;

  // media_message_center::MediaNotificationContainer:
  void OnExpanded(bool expanded) override;
  void OnMediaSessionInfoChanged(
      const media_session::mojom::MediaSessionInfoPtr& session_info) override;
  void OnForegoundColorChanged(SkColor color) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  void UpdateDismissButtonIcon();

  // The MediaNotificationContainerImpl is owned by the
  // MediaNotificationListView which is owned by the MediaDialogView, so the raw
  // pointer is safe here.
  MediaDialogView* const parent_;

  // The MediaToolbarButtonController is owned by the MediaToolbarButton which
  // outlives the MediaDialogView (and therefore the
  // MediaNotificationContainerImpl).
  MediaToolbarButtonController* const controller_;

  const std::string id_;
  std::unique_ptr<views::ImageButton> dismiss_button_;
  std::unique_ptr<media_message_center::MediaNotificationView> view_;

  SkColor foreground_color_;

  DISALLOW_COPY_AND_ASSIGN(MediaNotificationContainerImpl);
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_CONTAINER_IMPL_H_
