// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/sharing/mock_sharing_service.h"

#include "chrome/browser/sharing/sharing_fcm_handler.h"
#include "chrome/browser/sharing/sharing_fcm_sender.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sharing/vapid_key_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

MockSharingService::MockSharingService()
    : SharingService(
          /*sync_prefs=*/nullptr,
          /*vapid_key_manager=*/nullptr,
          std::make_unique<SharingDeviceRegistration>(
              /*pref_service=*/nullptr,
              /*sharing_sync_preference=*/nullptr,
              /*instance_id_driver=*/nullptr,
              /*vapid_key_manager=*/nullptr),
          /*fcm_sender=*/nullptr,
          std::make_unique<SharingFCMHandler>(/*gcm_driver=*/nullptr,
                                              /*sharing_fcm_sender=*/nullptr,
                                              /*sync_preference=*/nullptr),
          /*gcm_driver=*/nullptr,
          /*device_info_tracker=*/nullptr,
          /*local_device_info_provider=*/nullptr,
          /*sync_service*/ nullptr,
          /*notification_display_service=*/nullptr) {}

MockSharingService::~MockSharingService() = default;
