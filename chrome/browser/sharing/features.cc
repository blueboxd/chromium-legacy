// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/features.h"

const base::Feature kSharingUseDeviceInfo{"SharingUseDeviceInfo",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSharingQRCodeGenerator{"SharingQRCodeGenerator",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSharingDeriveVapidKey{"SharingDeriveVapidKey",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSharingDeviceExpiration{"SharingDeviceExpiration",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<int> kSharingDeviceExpirationHours = {
    &kSharingDeviceExpiration, "SharingDeviceExpirationHours", 48};

const base::Feature kSharingMessageTTL{"SharingMessageTTL",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<int> kSharingMessageTTLSeconds = {
    &kSharingMessageTTL, "SharingMessageTTLSeconds", 16};

const base::Feature kSharingAckMessageTTL{"SharingAckMessageTTL",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<int> kSharingAckMessageTTLSeconds = {
    &kSharingAckMessageTTL, "SharingAckMessageTTLSeconds", 8};
