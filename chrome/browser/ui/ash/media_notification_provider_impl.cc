// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/media_notification_provider_impl.h"

#include "ash/public/cpp/media_notification_provider_observer.h"
#include "ui/views/view.h"

MediaNotificationProviderImpl::MediaNotificationProviderImpl() = default;

MediaNotificationProviderImpl::~MediaNotificationProviderImpl() = default;

void MediaNotificationProviderImpl::AddObserver(
    ash::MediaNotificationProviderObserver* observer) {
  observers_.AddObserver(observer);
}

void MediaNotificationProviderImpl::RemoveObserver(
    ash::MediaNotificationProviderObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool MediaNotificationProviderImpl::HasActiveNotifications() {
  return false;
}

bool MediaNotificationProviderImpl::HasFrozenNotifications() {
  return false;
}

std::unique_ptr<views::View>
MediaNotificationProviderImpl::GetMediaNotificationListView() {
  return std::make_unique<views::View>();
}

std::unique_ptr<views::View>
MediaNotificationProviderImpl::GetActiveMediaNotificationView() {
  return std::make_unique<views::View>();
}
