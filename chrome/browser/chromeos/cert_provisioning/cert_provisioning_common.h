// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_CERT_PROVISIONING_COMMON_H_
#define CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_CERT_PROVISIONING_COMMON_H_

#include <string>

#include "base/optional.h"
#include "base/values.h"
#include "chromeos/dbus/constants/attestation_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "net/cert/x509_certificate.h"

class PrefRegistrySimple;

namespace chromeos {
namespace cert_provisioning {

const char kKeyNamePrefix[] = "cert-provis-";

// The type for variables containing an error from DM Server response.
using CertProvisioningResponseErrorType =
    enterprise_management::ClientCertificateProvisioningResponse::Error;
// The namespace that contains convenient aliases for error values, e.g.
// UNDEFINED, TIMED_OUT, IDENTITY_VERIFICATION_ERROR, CA_ERROR.
using CertProvisioningResponseError =
    enterprise_management::ClientCertificateProvisioningResponse;

// Numeric values are used in serialization and should not be remapped.
enum class CertScope { kUser = 0, kDevice = 1, kMaxValue = kDevice };

// These values are used in serialization and should be changed carefully.
enum class CertProvisioningWorkerState {
  kInitState = 0,
  kKeypairGenerated = 1,
  kStartCsrResponseReceived = 2,
  kVaChallengeFinished = 3,
  kKeyRegistered = 4,
  kKeypairMarked = 5,
  kSignCsrFinished = 6,
  kFinishCsrResponseReceived = 7,
  kSucceed = 8,
  kInconsistentDataError = 9,
  kFailed = 10,
  kMaxValue = kFailed,
};

using CertProfileId = std::string;

// Names of CertProfile fields in a base::Value representation. Must be in sync
// with definitions of RequiredClientCertificateForDevice and
// RequiredClientCertificateForUser policies in policy_templates.json file.
const char kCertProfileIdKey[] = "cert_profile_id";
const char kCertProfilePolicyVersionKey[] = "policy_version";

struct CertProfile {
  CertProfileId profile_id;
  std::string policy_version;

  // IMPORTANT:
  // Increment this when you add/change any member in CertProfile (and update
  // all functions that fail to compile because of it).
  static constexpr int kVersion = 2;

  static base::Optional<CertProfile> MakeFromValue(const base::Value& value);
  bool operator==(const CertProfile& other) const;
  bool operator!=(const CertProfile& other) const;
};

void RegisterProfilePrefs(PrefRegistrySimple* registry);
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
const char* GetPrefNameForSerialization(CertScope scope);

// Returns the nickname (CKA_LABEL) for keys created for the |profile_id|.
std::string GetKeyName(CertProfileId profile_id);
// Returns the key type for VA API calls for |scope|.
attestation::AttestationKeyType GetVaKeyType(CertScope scope);
const char* GetPlatformKeysTokenId(CertScope scope);

// The Verified Access APIs are used to generate key pairs. For user-specific
// key pairs, it is possible to reuse the key pair that is used for Verified
// Access challenge response generation and name it with a custom name. For
// device-wide key pairs, the key pair used for Verified Access challenge
// response generation must be stable, but an additional key pair can be
// embedded (key_name_for_spkac). See
// http://go/chromeos-va-registering-device-wide-keys-support for details. For
// these reasons, the name of key that should be registered and will be used for
// certificate provisioning is passed as key_name for user-specific keys and
// key_name_for_spkac for device-wide keys.
std::string GetVaKeyName(CertScope scope, CertProfileId profile_id);
std::string GetVaKeyNameForSpkac(CertScope scope, CertProfileId profile_id);

// Parses |data| using net::X509Certificate::FORMAT_AUTO as format specifier.
// Expects exactly one certificate to be the result and returns it.  If parsing
// fails or if more than one certificates were in |data|, returns an null ptr.
scoped_refptr<net::X509Certificate> CreateSingleCertificateFromBytes(
    const char* data,
    size_t length);

}  // namespace cert_provisioning
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_CERT_PROVISIONING_COMMON_H_
