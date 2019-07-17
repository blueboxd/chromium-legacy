// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_FCM_CONSTANTS_H_
#define CHROME_BROWSER_SHARING_FCM_CONSTANTS_H_

#include "base/time/time.h"

// InstanceID scope for Google Cloud Messaging to get GCM enabled token.
extern const char kFCMScope[];

// Sender ID linked to GCM messages for Sharing.
extern const char kSharingFCMAppID[];

// Amount of time before FCM registration should happen again.
extern const base::TimeDelta kRegistrationExpiration;

#endif  // CHROME_BROWSER_SHARING_FCM_CONSTANTS_H_
