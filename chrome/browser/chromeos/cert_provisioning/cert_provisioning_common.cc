// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_common.h"

#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/common/pref_names.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"

// MIERSH
// #include "chromeos/dbus/dbus_method_call_status.h"

namespace chromeos {
namespace cert_provisioning {

namespace {
base::Optional<AccountId> GetAccountId(CertScope scope, Profile* profile) {
  switch (scope) {
    case CertScope::kDevice: {
      return EmptyAccountId();
    }
    case CertScope::kUser: {
      user_manager::User* user =
          ProfileHelper::Get()->GetUserByProfile(profile);
      if (!user) {
        return base::nullopt;
      }

      return user->GetAccountId();
    }
  }

  NOTREACHED();
}
}  // namespace

bool IsFinalState(CertProvisioningWorkerState state) {
  switch (state) {
    case CertProvisioningWorkerState::kSucceeded:
    case CertProvisioningWorkerState::kInconsistentDataError:
    case CertProvisioningWorkerState::kFailed:
    case CertProvisioningWorkerState::kCanceled:
      return true;
    default:
      return false;
  }
}

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

void DeleteVaKey(CertScope scope,
                 Profile* profile,
                 const std::string& key_name,
                 DeleteVaKeyCallback callback) {
  auto account_id = GetAccountId(scope, profile);
  if (!account_id.has_value()) {
    return;
  }

  CryptohomeClient::Get()->TpmAttestationDeleteKey(
      GetVaKeyType(scope),
      cryptohome::CreateAccountIdentifierFromAccountId(account_id.value()),
      key_name, std::move(callback));
}

void DeleteVaKeysByPrefix(CertScope scope,
                          Profile* profile,
                          const std::string& key_prefix,
                          DeleteVaKeyCallback callback) {
  auto account_id = GetAccountId(scope, profile);
  if (!account_id.has_value()) {
    return;
  }

  CryptohomeClient::Get()->TpmAttestationDeleteKeysByPrefix(
      GetVaKeyType(scope),
      cryptohome::CreateAccountIdentifierFromAccountId(account_id.value()),
      key_prefix, std::move(callback));
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
