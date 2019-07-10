// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_LIST_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_LIST_VIEW_H_

#include <map>
#include <memory>

#include "ui/views/view.h"

class MediaNotificationContainerImpl;

// MediaNotificationListView is a container that holds a list of active media
// sessions.
class MediaNotificationListView : public views::View {
 public:
  MediaNotificationListView();
  ~MediaNotificationListView() override;

  void ShowNotification(
      const std::string& id,
      std::unique_ptr<MediaNotificationContainerImpl> notification);
  void HideNotification(const std::string& id);
  bool empty() { return notifications_.empty(); }

 private:
  std::map<const std::string, MediaNotificationContainerImpl*> notifications_;

  DISALLOW_COPY_AND_ASSIGN(MediaNotificationListView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_LIST_VIEW_H_
