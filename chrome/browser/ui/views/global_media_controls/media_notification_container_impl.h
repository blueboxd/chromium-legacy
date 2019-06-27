// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_CONTAINER_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_CONTAINER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "components/media_message_center/media_notification_container.h"
#include "components/media_message_center/media_notification_view.h"
#include "ui/views/view.h"

namespace media_message_center {
class MediaNotificationItem;
}  // namespace media_message_center

class MediaDialogView;

// MediaNotificationContainerImpl holds a media notification for display within
// the MediaDialogView. The media notification shows metadata for a media
// session and can control playback.
class MediaNotificationContainerImpl
    : public views::View,
      public media_message_center::MediaNotificationContainer {
 public:
  explicit MediaNotificationContainerImpl(
      MediaDialogView* parent,
      base::WeakPtr<media_message_center::MediaNotificationItem> item);
  ~MediaNotificationContainerImpl() override;

  // media_message_center::MediaNotificationContainer:
  void OnExpanded(bool expanded) override;

 private:
  MediaDialogView* parent_;
  media_message_center::MediaNotificationView view_;

  DISALLOW_COPY_AND_ASSIGN(MediaNotificationContainerImpl);
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_CONTAINER_IMPL_H_
