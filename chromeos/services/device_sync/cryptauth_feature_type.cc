// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_feature_type.h"

#include "base/no_destructor.h"

namespace chromeos {

namespace device_sync {

namespace {

const char kBetterTogetherHostSupportedString[] =
    "BETTER_TOGETHER_HOST_SUPPORTED";
const char kBetterTogetherClientSupportedString[] =
    "BETTER_TOGETHER_CLIENT_SUPPORTED";
const char kEasyUnlockHostSupportedString[] = "EASY_UNLOCK_HOST_SUPPORTED";
const char kEasyUnlockClientSupportedString[] = "EASY_UNLOCK_CLIENT_SUPPORTED";
const char kMagicTetherHostSupportedString[] = "MAGIC_TETHER_HOST_SUPPORTED";
const char kMagicTetherClientSupportedString[] =
    "MAGIC_TETHER_CLIENT_SUPPORTED";
const char kSmsConnectHostSupportedString[] = "SMS_CONNECT_HOST_SUPPORTED";
const char kSmsConnectClientSupportedString[] = "SMS_CONNECT_CLIENT_SUPPORTED";

const char kBetterTogetherHostEnabledString[] = "BETTER_TOGETHER_HOST";
const char kBetterTogetherClientEnabledString[] = "BETTER_TOGETHER_CLIENT";
const char kEasyUnlockHostEnabledString[] = "EASY_UNLOCK_HOST";
const char kEasyUnlockClientEnabledString[] = "EASY_UNLOCK_CLIENT";
const char kMagicTetherHostEnabledString[] = "MAGIC_TETHER_HOST";
const char kMagicTetherClientEnabledString[] = "MAGIC_TETHER_CLIENT";
const char kSmsConnectHostEnabledString[] = "SMS_CONNECT_HOST";
const char kSmsConnectClientEnabledString[] = "SMS_CONNECT_CLIENT";

}  // namespace

const char* CryptAuthFeatureTypeToString(CryptAuthFeatureType feature_type) {
  switch (feature_type) {
    case CryptAuthFeatureType::kBetterTogetherHostSupported:
      return kBetterTogetherHostSupportedString;
    case CryptAuthFeatureType::kBetterTogetherHostEnabled:
      return kBetterTogetherHostEnabledString;
    case CryptAuthFeatureType::kBetterTogetherClientSupported:
      return kBetterTogetherClientSupportedString;
    case CryptAuthFeatureType::kBetterTogetherClientEnabled:
      return kBetterTogetherClientEnabledString;
    case CryptAuthFeatureType::kEasyUnlockHostSupported:
      return kEasyUnlockHostSupportedString;
    case CryptAuthFeatureType::kEasyUnlockHostEnabled:
      return kEasyUnlockHostEnabledString;
    case CryptAuthFeatureType::kEasyUnlockClientSupported:
      return kEasyUnlockClientSupportedString;
    case CryptAuthFeatureType::kEasyUnlockClientEnabled:
      return kEasyUnlockClientEnabledString;
    case CryptAuthFeatureType::kMagicTetherHostSupported:
      return kMagicTetherHostSupportedString;
    case CryptAuthFeatureType::kMagicTetherHostEnabled:
      return kMagicTetherHostEnabledString;
    case CryptAuthFeatureType::kMagicTetherClientSupported:
      return kMagicTetherClientSupportedString;
    case CryptAuthFeatureType::kMagicTetherClientEnabled:
      return kMagicTetherClientEnabledString;
    case CryptAuthFeatureType::kSmsConnectHostSupported:
      return kSmsConnectHostSupportedString;
    case CryptAuthFeatureType::kSmsConnectHostEnabled:
      return kSmsConnectHostEnabledString;
    case CryptAuthFeatureType::kSmsConnectClientSupported:
      return kSmsConnectClientSupportedString;
    case CryptAuthFeatureType::kSmsConnectClientEnabled:
      return kSmsConnectClientEnabledString;
  }
}

const char* SoftwareFeatureToEnabledCryptAuthFeatureTypeString(
    multidevice::SoftwareFeature software_feature) {
  switch (software_feature) {
    case multidevice::SoftwareFeature::kBetterTogetherHost:
      return kBetterTogetherHostEnabledString;
    case multidevice::SoftwareFeature::kBetterTogetherClient:
      return kBetterTogetherClientEnabledString;
    case multidevice::SoftwareFeature::kSmartLockHost:
      return kEasyUnlockHostEnabledString;
    case multidevice::SoftwareFeature::kSmartLockClient:
      return kEasyUnlockClientEnabledString;
    case multidevice::SoftwareFeature::kInstantTetheringHost:
      return kMagicTetherHostEnabledString;
    case multidevice::SoftwareFeature::kInstantTetheringClient:
      return kMagicTetherClientEnabledString;
    case multidevice::SoftwareFeature::kMessagesForWebHost:
      return kSmsConnectHostEnabledString;
    case multidevice::SoftwareFeature::kMessagesForWebClient:
      return kSmsConnectClientEnabledString;
  }
}

base::Optional<CryptAuthFeatureType> CryptAuthFeatureTypeFromString(
    const std::string& feature_type_string) {
  if (feature_type_string == kBetterTogetherHostSupportedString)
    return CryptAuthFeatureType::kBetterTogetherHostSupported;
  if (feature_type_string == kBetterTogetherHostEnabledString)
    return CryptAuthFeatureType::kBetterTogetherHostEnabled;
  if (feature_type_string == kBetterTogetherClientSupportedString)
    return CryptAuthFeatureType::kBetterTogetherClientSupported;
  if (feature_type_string == kBetterTogetherClientEnabledString)
    return CryptAuthFeatureType::kBetterTogetherClientEnabled;
  if (feature_type_string == kEasyUnlockHostSupportedString)
    return CryptAuthFeatureType::kEasyUnlockHostSupported;
  if (feature_type_string == kEasyUnlockHostEnabledString)
    return CryptAuthFeatureType::kEasyUnlockHostEnabled;
  if (feature_type_string == kEasyUnlockClientSupportedString)
    return CryptAuthFeatureType::kEasyUnlockClientSupported;
  if (feature_type_string == kEasyUnlockClientEnabledString)
    return CryptAuthFeatureType::kEasyUnlockClientEnabled;
  if (feature_type_string == kMagicTetherHostSupportedString)
    return CryptAuthFeatureType::kMagicTetherHostSupported;
  if (feature_type_string == kMagicTetherHostEnabledString)
    return CryptAuthFeatureType::kMagicTetherHostEnabled;
  if (feature_type_string == kMagicTetherClientSupportedString)
    return CryptAuthFeatureType::kMagicTetherClientSupported;
  if (feature_type_string == kMagicTetherClientEnabledString)
    return CryptAuthFeatureType::kMagicTetherClientEnabled;
  if (feature_type_string == kSmsConnectHostSupportedString)
    return CryptAuthFeatureType::kSmsConnectHostSupported;
  if (feature_type_string == kSmsConnectHostEnabledString)
    return CryptAuthFeatureType::kSmsConnectHostEnabled;
  if (feature_type_string == kSmsConnectClientSupportedString)
    return CryptAuthFeatureType::kSmsConnectClientSupported;
  if (feature_type_string == kSmsConnectClientEnabledString)
    return CryptAuthFeatureType::kSmsConnectClientEnabled;

  return base::nullopt;
}

const base::flat_set<std::string>& GetCryptAuthFeatureTypeStrings() {
  static const base::NoDestructor<base::flat_set<std::string>> feature_set([] {
    return base::flat_set<std::string>{kBetterTogetherHostSupportedString,
                                       kBetterTogetherClientSupportedString,
                                       kEasyUnlockHostSupportedString,
                                       kEasyUnlockClientSupportedString,
                                       kMagicTetherHostSupportedString,
                                       kMagicTetherClientSupportedString,
                                       kSmsConnectHostSupportedString,
                                       kSmsConnectClientSupportedString,
                                       kBetterTogetherHostEnabledString,
                                       kBetterTogetherClientEnabledString,
                                       kEasyUnlockHostEnabledString,
                                       kEasyUnlockClientEnabledString,
                                       kMagicTetherHostEnabledString,
                                       kMagicTetherClientEnabledString,
                                       kSmsConnectHostEnabledString,
                                       kSmsConnectClientEnabledString};
  }());
  return *feature_set;
}

const base::flat_set<std::string>& GetSupportedCryptAuthFeatureTypeStrings() {
  static const base::NoDestructor<base::flat_set<std::string>> supported_set(
      [] {
        return base::flat_set<std::string>{kBetterTogetherHostSupportedString,
                                           kBetterTogetherClientSupportedString,
                                           kEasyUnlockHostSupportedString,
                                           kEasyUnlockClientSupportedString,
                                           kMagicTetherHostSupportedString,
                                           kMagicTetherClientSupportedString,
                                           kSmsConnectHostSupportedString,
                                           kSmsConnectClientSupportedString};
      }());
  return *supported_set;
}

const base::flat_set<std::string>& GetEnabledCryptAuthFeatureTypeStrings() {
  static const base::NoDestructor<base::flat_set<std::string>> enabled_set([] {
    return base::flat_set<std::string>{
        kBetterTogetherHostEnabledString, kBetterTogetherClientEnabledString,
        kEasyUnlockHostEnabledString,     kEasyUnlockClientEnabledString,
        kMagicTetherHostEnabledString,    kMagicTetherClientEnabledString,
        kSmsConnectHostEnabledString,     kSmsConnectClientEnabledString};
  }());
  return *enabled_set;
}

multidevice::SoftwareFeature CryptAuthFeatureTypeStringToSoftwareFeature(
    const std::string& feature_type_string) {
  if (feature_type_string == kBetterTogetherHostSupportedString ||
      feature_type_string == kBetterTogetherHostEnabledString) {
    return multidevice::SoftwareFeature::kBetterTogetherHost;
  }

  if (feature_type_string == kBetterTogetherClientSupportedString ||
      feature_type_string == kBetterTogetherClientEnabledString) {
    return multidevice::SoftwareFeature::kBetterTogetherClient;
  }

  if (feature_type_string == kEasyUnlockHostSupportedString ||
      feature_type_string == kEasyUnlockHostEnabledString) {
    return multidevice::SoftwareFeature::kSmartLockHost;
  }

  if (feature_type_string == kEasyUnlockClientSupportedString ||
      feature_type_string == kEasyUnlockClientEnabledString) {
    return multidevice::SoftwareFeature::kSmartLockClient;
  }

  if (feature_type_string == kMagicTetherHostSupportedString ||
      feature_type_string == kMagicTetherHostEnabledString) {
    return multidevice::SoftwareFeature::kInstantTetheringHost;
  }

  if (feature_type_string == kMagicTetherClientSupportedString ||
      feature_type_string == kMagicTetherClientEnabledString) {
    return multidevice::SoftwareFeature::kInstantTetheringClient;
  }

  if (feature_type_string == kSmsConnectHostSupportedString ||
      feature_type_string == kSmsConnectHostEnabledString) {
    return multidevice::SoftwareFeature::kMessagesForWebHost;
  }

  DCHECK(feature_type_string == kSmsConnectClientSupportedString ||
         feature_type_string == kSmsConnectClientEnabledString);
  return multidevice::SoftwareFeature::kMessagesForWebClient;
}

std::ostream& operator<<(std::ostream& stream,
                         CryptAuthFeatureType feature_type) {
  stream << CryptAuthFeatureTypeToString(feature_type);
  return stream;
}

}  // namespace device_sync

}  // namespace chromeos
