// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_MEDIA_NOTIFICATION_PROVIDER_IMPL_H_
#define CHROME_BROWSER_UI_ASH_MEDIA_NOTIFICATION_PROVIDER_IMPL_H_

#include "ash/public/cpp/media_notification_provider.h"
#include "base/observer_list.h"

class MediaNotificationProviderImpl : public ash::MediaNotificationProvider {
 public:
  MediaNotificationProviderImpl();
  ~MediaNotificationProviderImpl() override;

  // MediaNotificationProvider implementations.
  void AddObserver(ash::MediaNotificationProviderObserver* observer) override;
  void RemoveObserver(
      ash::MediaNotificationProviderObserver* observer) override;
  bool HasActiveNotifications() override;
  bool HasFrozenNotifications() override;
  std::unique_ptr<views::View> GetMediaNotificationListView() override;
  std::unique_ptr<views::View> GetActiveMediaNotificationView() override;

 private:
  base::ObserverList<ash::MediaNotificationProviderObserver> observers_;
};

#endif  // CHROME_BROWSER_UI_ASH_MEDIA_NOTIFICATION_PROVIDER_IMPL_H_
