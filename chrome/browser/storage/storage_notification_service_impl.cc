// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage/storage_notification_service_impl.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/storage_pressure_bubble.h"
#endif

base::RepeatingCallback<void(const url::Origin)>
StorageNotificationServiceImpl::GetStoragePressureNotificationClosure() {
  return base::BindRepeating(&chrome::ShowStoragePressureBubble);
}

StorageNotificationServiceImpl::StorageNotificationServiceImpl() = default;

StorageNotificationServiceImpl::~StorageNotificationServiceImpl() = default;
