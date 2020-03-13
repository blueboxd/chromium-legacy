// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage/storage_notification_service_impl.h"

#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/storage_pressure_bubble.h"
#endif

// Minimum interval between consecutive storage pressure notifications.
const base::TimeDelta kDiskPressureNotificationInterval =
    base::TimeDelta::FromDays(1);

void StorageNotificationServiceImpl::MaybeShowStoragePressureNotification(
    const url::Origin origin) {
  if (base::TimeTicks::Now() - disk_pressure_notification_last_sent_at_ <
      kDiskPressureNotificationInterval)
    return;

  chrome::ShowStoragePressureBubble(origin);
  disk_pressure_notification_last_sent_at_ = base::TimeTicks::Now();
}

StorageNotificationServiceImpl::StorageNotificationServiceImpl() = default;

StorageNotificationServiceImpl::~StorageNotificationServiceImpl() = default;
