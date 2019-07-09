// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_better_together_feature_types.h"

#include "base/logging.h"
#include "base/no_destructor.h"

namespace chromeos {

namespace device_sync {

const char kCryptAuthFeatureTypeBetterTogetherHostSupported[] =
    "BETTER_TOGETHER_HOST_SUPPORTED";
const char kCryptAuthFeatureTypeBetterTogetherClientSupported[] =
    "BETTER_TOGETHER_CLIENT_SUPPORTED";
const char kCryptAuthFeatureTypeEasyUnlockHostSupported[] =
    "EASY_UNLOCK_HOST_SUPPORTED";
const char kCryptAuthFeatureTypeEasyUnlockClientSupported[] =
    "EASY_UNLOCK_CLIENT_SUPPORTED";
const char kCryptAuthFeatureTypeMagicTetherHostSupported[] =
    "MAGIC_TETHER_HOST_SUPPORTED";
const char kCryptAuthFeatureTypeMagicTetherClientSupported[] =
    "MAGIC_TETHER_CLIENT_SUPPORTED";
const char kCryptAuthFeatureTypeSmsConnectHostSupported[] =
    "SMS_CONNECT_HOST_SUPPORTED";
const char kCryptAuthFeatureTypeSmsConnectClientSupported[] =
    "SMS_CONNECT_CLIENT_SUPPORTED";

const char kCryptAuthFeatureTypeBetterTogetherHostEnabled[] =
    "BETTER_TOGETHER_HOST";
const char kCryptAuthFeatureTypeBetterTogetherClientEnabled[] =
    "BETTER_TOGETHER_CLIENT";
const char kCryptAuthFeatureTypeEasyUnlockHostEnabled[] = "EASY_UNLOCK_HOST";
const char kCryptAuthFeatureTypeEasyUnlockClientEnabled[] =
    "EASY_UNLOCK_CLIENT";
const char kCryptAuthFeatureTypeMagicTetherHostEnabled[] = "MAGIC_TETHER_HOST";
const char kCryptAuthFeatureTypeMagicTetherClientEnabled[] =
    "MAGIC_TETHER_CLIENT";
const char kCryptAuthFeatureTypeSmsConnectHostEnabled[] = "SMS_CONNECT_HOST";
const char kCryptAuthFeatureTypeSmsConnectClientEnabled[] =
    "SMS_CONNECT_CLIENT";

const base::flat_set<std::string>& GetBetterTogetherFeatureTypes() {
  static const base::NoDestructor<base::flat_set<std::string>> feature_set([] {
    return base::flat_set<std::string>{
        kCryptAuthFeatureTypeBetterTogetherHostSupported,
        kCryptAuthFeatureTypeBetterTogetherClientSupported,
        kCryptAuthFeatureTypeEasyUnlockHostSupported,
        kCryptAuthFeatureTypeEasyUnlockClientSupported,
        kCryptAuthFeatureTypeMagicTetherHostSupported,
        kCryptAuthFeatureTypeMagicTetherClientSupported,
        kCryptAuthFeatureTypeSmsConnectHostSupported,
        kCryptAuthFeatureTypeSmsConnectClientSupported,
        kCryptAuthFeatureTypeBetterTogetherHostEnabled,
        kCryptAuthFeatureTypeBetterTogetherClientEnabled,
        kCryptAuthFeatureTypeEasyUnlockHostEnabled,
        kCryptAuthFeatureTypeEasyUnlockClientEnabled,
        kCryptAuthFeatureTypeMagicTetherHostEnabled,
        kCryptAuthFeatureTypeMagicTetherClientEnabled,
        kCryptAuthFeatureTypeSmsConnectHostEnabled,
        kCryptAuthFeatureTypeSmsConnectClientEnabled};
  }());
  return *feature_set;
}

const base::flat_set<std::string>& GetSupportedBetterTogetherFeatureTypes() {
  static const base::NoDestructor<base::flat_set<std::string>> supported_set(
      [] {
        return base::flat_set<std::string>{
            kCryptAuthFeatureTypeBetterTogetherHostSupported,
            kCryptAuthFeatureTypeBetterTogetherClientSupported,
            kCryptAuthFeatureTypeEasyUnlockHostSupported,
            kCryptAuthFeatureTypeEasyUnlockClientSupported,
            kCryptAuthFeatureTypeMagicTetherHostSupported,
            kCryptAuthFeatureTypeMagicTetherClientSupported,
            kCryptAuthFeatureTypeSmsConnectHostSupported,
            kCryptAuthFeatureTypeSmsConnectClientSupported};
      }());
  return *supported_set;
}

const base::flat_set<std::string>& GetEnabledBetterTogetherFeatureTypes() {
  static const base::NoDestructor<base::flat_set<std::string>> enabled_set([] {
    return base::flat_set<std::string>{
        kCryptAuthFeatureTypeBetterTogetherHostEnabled,
        kCryptAuthFeatureTypeBetterTogetherClientEnabled,
        kCryptAuthFeatureTypeEasyUnlockHostEnabled,
        kCryptAuthFeatureTypeEasyUnlockClientEnabled,
        kCryptAuthFeatureTypeMagicTetherHostEnabled,
        kCryptAuthFeatureTypeMagicTetherClientEnabled,
        kCryptAuthFeatureTypeSmsConnectHostEnabled,
        kCryptAuthFeatureTypeSmsConnectClientEnabled};
  }());
  return *enabled_set;
}

multidevice::SoftwareFeature BetterTogetherFeatureTypeStringToSoftwareFeature(
    const std::string& feature_type_string) {
  if (feature_type_string == kCryptAuthFeatureTypeBetterTogetherHostSupported ||
      feature_type_string == kCryptAuthFeatureTypeBetterTogetherHostEnabled) {
    return multidevice::SoftwareFeature::kBetterTogetherHost;
  }

  if (feature_type_string ==
          kCryptAuthFeatureTypeBetterTogetherClientSupported ||
      feature_type_string == kCryptAuthFeatureTypeBetterTogetherClientEnabled) {
    return multidevice::SoftwareFeature::kBetterTogetherClient;
  }

  if (feature_type_string == kCryptAuthFeatureTypeEasyUnlockHostSupported ||
      feature_type_string == kCryptAuthFeatureTypeEasyUnlockHostEnabled) {
    return multidevice::SoftwareFeature::kSmartLockHost;
  }

  if (feature_type_string == kCryptAuthFeatureTypeEasyUnlockClientSupported ||
      feature_type_string == kCryptAuthFeatureTypeEasyUnlockClientEnabled) {
    return multidevice::SoftwareFeature::kSmartLockClient;
  }

  if (feature_type_string == kCryptAuthFeatureTypeMagicTetherHostSupported ||
      feature_type_string == kCryptAuthFeatureTypeMagicTetherHostEnabled) {
    return multidevice::SoftwareFeature::kInstantTetheringHost;
  }

  if (feature_type_string == kCryptAuthFeatureTypeMagicTetherClientSupported ||
      feature_type_string == kCryptAuthFeatureTypeMagicTetherClientEnabled) {
    return multidevice::SoftwareFeature::kInstantTetheringClient;
  }

  if (feature_type_string == kCryptAuthFeatureTypeSmsConnectHostSupported ||
      feature_type_string == kCryptAuthFeatureTypeSmsConnectHostEnabled) {
    return multidevice::SoftwareFeature::kMessagesForWebHost;
  }

  DCHECK(feature_type_string ==
             kCryptAuthFeatureTypeSmsConnectClientSupported ||
         feature_type_string == kCryptAuthFeatureTypeSmsConnectClientEnabled);
  return multidevice::SoftwareFeature::kMessagesForWebClient;
}

}  // namespace device_sync

}  // namespace chromeos
