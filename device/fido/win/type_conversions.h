// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_WIN_TYPE_CONVERSIONS_H_
#define DEVICE_FIDO_WIN_TYPE_CONVERSIONS_H_

#include <windows.h>

#include "base/component_export.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/fido_constants.h"
#include "third_party/microsoft_webauthn/webauthn.h"

namespace device {

COMPONENT_EXPORT(DEVICE_FIDO)
base::Optional<AuthenticatorMakeCredentialResponse>
ToAuthenticatorMakeCredentialResponse(
    const WEBAUTHN_CREDENTIAL_ATTESTATION& credential_attestation);

COMPONENT_EXPORT(DEVICE_FIDO)
base::Optional<AuthenticatorGetAssertionResponse>
ToAuthenticatorGetAssertionResponse(
    const WEBAUTHN_ASSERTION& credential_attestation);

COMPONENT_EXPORT(DEVICE_FIDO)
uint32_t ToWinUserVerificationRequirement(
    UserVerificationRequirement user_verification_requirement);

COMPONENT_EXPORT(DEVICE_FIDO)
uint32_t ToWinAuthenticatorAttachment(
    AuthenticatorAttachment authenticator_attachment);

COMPONENT_EXPORT(DEVICE_FIDO)
std::vector<WEBAUTHN_CREDENTIAL> ToWinCredentialVector(
    const std::vector<PublicKeyCredentialDescriptor>* credentials);

COMPONENT_EXPORT(DEVICE_FIDO)
std::vector<WEBAUTHN_CREDENTIAL_EX> ToWinCredentialExVector(
    const std::vector<PublicKeyCredentialDescriptor>* credentials);

COMPONENT_EXPORT(DEVICE_FIDO)
CtapDeviceResponseCode WinErrorNameToCtapDeviceResponseCode(
    const base::string16& error_name);

COMPONENT_EXPORT(DEVICE_FIDO)
uint32_t ToWinAttestationConveyancePreference(
    const AttestationConveyancePreference&);

}  // namespace device

#endif  // DEVICE_FIDO_WIN_TYPE_CONVERSIONS_H_
