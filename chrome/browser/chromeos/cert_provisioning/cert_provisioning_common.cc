// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_common.h"

#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"

namespace chromeos {
namespace cert_provisioning {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kRequiredClientCertificateForUser);
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kRequiredClientCertificateForDevice);
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

}  // namespace cert_provisioning
}  // namespace chromeos
