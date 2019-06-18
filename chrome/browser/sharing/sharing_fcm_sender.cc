// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_fcm_sender.h"

SharingFCMSender::SharingFCMSender() = default;

SharingFCMSender::~SharingFCMSender() = default;

bool SharingFCMSender::SendMessage(
    const std::string& device_guid,
    const chrome_browser_sharing::SharingMessage& message) {
  // TODO
  return true;
}
