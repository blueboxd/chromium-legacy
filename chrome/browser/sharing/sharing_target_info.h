// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_TARGET_INFO_H_
#define CHROME_BROWSER_SHARING_SHARING_TARGET_INFO_H_

#include <string>

struct SharingTargetInfo {
  // FCM registration token of device.
  std::string fcm_token;

  // Subscription public key required for Sharing message encryption[RFC8291].
  std::string p256dh;

  // Auth secret key required for Sharing message encryption[RFC8291].
  std::string auth_secret;
};

#endif  // CHROME_BROWSER_SHARING_SHARING_TARGET_INFO_H_
