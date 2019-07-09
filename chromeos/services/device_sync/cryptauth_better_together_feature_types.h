// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_BETTER_TOGETHER_FEATURE_TYPES_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_BETTER_TOGETHER_FEATURE_TYPES_H_

#include <string>

#include "base/containers/flat_set.h"
#include "chromeos/components/multidevice/software_feature.h"

namespace chromeos {

namespace device_sync {

// Strings used by CryptAuth to identify the supported and enabled states of
// BetterTogether features. They are used in the "feature_type(s)" fields of the
// DeviceSync v2 RPCs BatchNotifyGroupDevices, BatchGetFeatureStatuses, and
// BatchSetFeatureStatuses.
extern const char kCryptAuthFeatureTypeBetterTogetherHostSupported[];
extern const char kCryptAuthFeatureTypeBetterTogetherClientSupported[];
extern const char kCryptAuthFeatureTypeEasyUnlockHostSupported[];
extern const char kCryptAuthFeatureTypeEasyUnlockClientSupported[];
extern const char kCryptAuthFeatureTypeMagicTetherHostSupported[];
extern const char kCryptAuthFeatureTypeMagicTetherClientSupported[];
extern const char kCryptAuthFeatureTypeSmsConnectHostSupported[];
extern const char kCryptAuthFeatureTypeSmsConnectClientSupported[];
extern const char kCryptAuthFeatureTypeBetterTogetherHostEnabled[];
extern const char kCryptAuthFeatureTypeBetterTogetherClientEnabled[];
extern const char kCryptAuthFeatureTypeEasyUnlockHostEnabled[];
extern const char kCryptAuthFeatureTypeEasyUnlockClientEnabled[];
extern const char kCryptAuthFeatureTypeMagicTetherHostEnabled[];
extern const char kCryptAuthFeatureTypeMagicTetherClientEnabled[];
extern const char kCryptAuthFeatureTypeSmsConnectHostEnabled[];
extern const char kCryptAuthFeatureTypeSmsConnectClientEnabled[];

const base::flat_set<std::string>& GetBetterTogetherFeatureTypes();
const base::flat_set<std::string>& GetSupportedBetterTogetherFeatureTypes();
const base::flat_set<std::string>& GetEnabledBetterTogetherFeatureTypes();

multidevice::SoftwareFeature BetterTogetherFeatureTypeStringToSoftwareFeature(
    const std::string& feature_type_string);

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_BETTER_TOGETHER_FEATURE_TYPES_H_
