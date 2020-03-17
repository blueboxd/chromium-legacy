// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_CERT_PROVISIONING_COMMON_H_
#define CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_CERT_PROVISIONING_COMMON_H_

#include <string>

#include "chromeos/dbus/constants/attestation_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"

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

using CertProfileId = std::string;

struct CertProfile {
  CertProfileId profile_id;
};

void RegisterProfilePrefs(PrefRegistrySimple* registry);
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

// Returns the nickname (CKA_LABEL) for keys created for the |profile_id|.
std::string GetKeyName(CertProfileId profile_id);
// Returns the key type for VA API calls for |scope|.
attestation::AttestationKeyType GetVaKeyType(CertScope scope);
const char* GetPlatformKeysTokenId(CertScope scope);

// The Verified Access APIs are used to generate keypairs. For user-specific
// keypairs, it is possible to reuse the keypair that is used for Verified
// Access challenge response generation and name it with a custom name. For
// device-wide keypairs, the keypair used for Verified Access challenge response
// generation must be stable, but an additional keypair can be embedded
// (key_name_for_spkac). See
// http://go/chromeos-va-registering-device-wide-keys-support for details. For
// these reasons, the name of key that should be registered and will be used for
// certificate provisioning is passed as key_name for user-specific keys and
// key_name_for_spkac for device-wide keys.
std::string GetVaKeyName(CertScope scope, CertProfileId profile_id);
std::string GetVaKeyNameForSpkac(CertScope scope, CertProfileId profile_id);

}  // namespace cert_provisioning
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_CERT_PROVISIONING_COMMON_H_
