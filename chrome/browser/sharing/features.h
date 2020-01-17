// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_FEATURES_H_
#define CHROME_BROWSER_SHARING_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

// Feature flag to allow sharing infrastructure to register devices in
// DeviceInfo.
extern const base::Feature kSharingUseDeviceInfo;

// Feature flag to enable QR Code Generator (currently desktop-only).
extern const base::Feature kSharingQRCodeGenerator;

// Feature flag to enable deriving VAPID key from Sync.
extern const base::Feature kSharingDeriveVapidKey;

// Feature flag for configuring device expiration.
extern const base::Feature kSharingDeviceExpiration;

// The number of hours after which a device is considered expired.
extern const base::FeatureParam<int> kSharingDeviceExpirationHours;

// Feature flag for configuring the sharing message timeout. This sets both the
// FCM message TTL and the duration of the timer that waits for an ack.
extern const base::Feature kSharingMessageTTL;

// The duration in seconds for both the FCM message TTL and the timer that waits
// for an ack.
extern const base::FeatureParam<int> kSharingMessageTTLSeconds;

// Feature flag for configuring the FCM TTL for sharing ack messages.
extern const base::Feature kSharingAckMessageTTL;

// The FCM TTL in seconds for sharing ack messages.
extern const base::FeatureParam<int> kSharingAckMessageTTLSeconds;

#endif  // CHROME_BROWSER_SHARING_FEATURES_H_
