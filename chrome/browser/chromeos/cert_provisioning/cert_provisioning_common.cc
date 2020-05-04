// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_common.h"

#include "base/optional.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"

namespace chromeos {
namespace cert_provisioning {

//===================== CertProfile ============================================

base::Optional<CertProfile> CertProfile::MakeFromValue(
    const base::Value& value) {
  const std::string* id = value.FindStringKey(kCertProfileIdKey);
  const std::string* policy_version =
      value.FindStringKey(kCertProfilePolicyVersionKey);
  if (!id || !policy_version) {
    return base::nullopt;
  }

  CertProfile result;
  result.profile_id = *id;
  result.policy_version = *policy_version;

  return result;
}

bool CertProfile::operator==(const CertProfile& other) const {
  static_assert(kVersion == 2, "This function should be updated");
  return ((profile_id == other.profile_id) &&
          (policy_version == other.policy_version));
}

bool CertProfile::operator!=(const CertProfile& other) const {
  return (*this == other);
}

//==============================================================================

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kRequiredClientCertificateForUser);
  registry->RegisterDictionaryPref(prefs::kCertificateProvisioningStateForUser);
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kRequiredClientCertificateForDevice);
  registry->RegisterDictionaryPref(
      prefs::kCertificateProvisioningStateForDevice);
}

const char* GetPrefNameForSerialization(CertScope scope) {
  switch (scope) {
    case CertScope::kUser:
      return prefs::kCertificateProvisioningStateForUser;
    case CertScope::kDevice:
      return prefs::kCertificateProvisioningStateForDevice;
  }
}

std::string GetKeyName(CertProfileId profile_id) {
  return kKeyNamePrefix + profile_id;
}

attestation::AttestationKeyType GetVaKeyType(CertScope scope) {
  switch (scope) {
    case CertScope::kUser:
      return attestation::AttestationKeyType::KEY_USER;
    case CertScope::kDevice:
      return attestation::AttestationKeyType::KEY_DEVICE;
  }
}

std::string GetVaKeyName(CertScope scope, CertProfileId profile_id) {
  switch (scope) {
    case CertScope::kUser:
      return GetKeyName(profile_id);
    case CertScope::kDevice:
      return std::string();
  }
}

std::string GetVaKeyNameForSpkac(CertScope scope, CertProfileId profile_id) {
  switch (scope) {
    case CertScope::kUser:
      return std::string();
    case CertScope::kDevice:
      return GetKeyName(profile_id);
  }
}

const char* GetPlatformKeysTokenId(CertScope scope) {
  switch (scope) {
    case CertScope::kUser:
      return platform_keys::kTokenIdUser;
    case CertScope::kDevice:
      return platform_keys::kTokenIdSystem;
  }
}

scoped_refptr<net::X509Certificate> CreateSingleCertificateFromBytes(
    const char* data,
    size_t length) {
  net::CertificateList cert_list =
      net::X509Certificate::CreateCertificateListFromBytes(
          data, length, net::X509Certificate::FORMAT_AUTO);

  if (cert_list.size() != 1) {
    return {};
  }

  return cert_list[0];
}

}  // namespace cert_provisioning
}  // namespace chromeos
