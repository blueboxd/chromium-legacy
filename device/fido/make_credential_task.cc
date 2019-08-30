// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/make_credential_task.h"

#include <utility>

#include "base/bind.h"
#include "device/base/features.h"
#include "device/fido/ctap2_device_operation.h"
#include "device/fido/pin.h"
#include "device/fido/u2f_command_constructor.h"
#include "device/fido/u2f_register_operation.h"

namespace device {

namespace {

// CTAP 2.0 specifies[1] that once a PIN has been set on an authenticator, the
// PIN is required in order to make a credential. In some cases we don't want to
// prompt for a PIN and so use U2F to make the credential instead.
//
// [1]
// https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html#authenticatorMakeCredential,
// step 6
bool CtapDeviceShouldUseU2fBecauseClientPinIsSet(
    const FidoDevice* device,
    const CtapMakeCredentialRequest& request) {
  DCHECK_EQ(device->supported_protocol(), ProtocolVersion::kCtap2);
  // Don't use U2F for requests that require UV or PIN which U2F doesn't
  // support. Note that |pin_auth| may also be set by GetTouchRequest(), but we
  // don't want those requests to use U2F either if CTAP is supported.
  if (request.user_verification == UserVerificationRequirement::kRequired ||
      request.pin_auth) {
    return false;
  }

  DCHECK(device && device->device_info());
  bool client_pin_set =
      device->device_info()->options.client_pin_availability ==
      AuthenticatorSupportedOptions::ClientPinAvailability::kSupportedAndPinSet;
  bool supports_u2f =
      base::Contains(device->device_info()->versions, ProtocolVersion::kU2f);
  return client_pin_set && supports_u2f;
}

}  // namespace

MakeCredentialTask::MakeCredentialTask(FidoDevice* device,
                                       CtapMakeCredentialRequest request,
                                       MakeCredentialTaskCallback callback)
    : FidoTask(device),
      request_(std::move(request)),
      callback_(std::move(callback)) {
  // The UV parameter should have been made binary by this point because CTAP2
  // only takes a binary value.
  DCHECK_NE(request_.user_verification,
            UserVerificationRequirement::kPreferred);
}

MakeCredentialTask::~MakeCredentialTask() = default;

// static
CtapMakeCredentialRequest MakeCredentialTask::GetTouchRequest(
    const FidoDevice* device) {
  // We want to flash and wait for a touch. Newer versions of the CTAP2 spec
  // include a provision for blocking for a touch when an empty pinAuth is
  // specified, but devices exist that predate this part of the spec and also
  // the spec says that devices need only do that if they implement PIN support.
  // Therefore, in order to portably wait for a touch, a dummy credential is
  // created. This does assume that the device supports ECDSA P-256, however.
  PublicKeyCredentialUserEntity user({1} /* user ID */);
  // The user name is incorrectly marked as optional in the CTAP2 spec.
  user.name = "dummy";
  CtapMakeCredentialRequest req(
      "" /* client_data_json */, PublicKeyCredentialRpEntity(".dummy"),
      std::move(user),
      PublicKeyCredentialParams(
          {{CredentialType::kPublicKey,
            base::strict_cast<int>(CoseAlgorithmIdentifier::kCoseEs256)}}));
  req.exclude_list.reset();

  // If a device supports CTAP2 and has PIN support then setting an empty
  // pinAuth should trigger just a touch[1]. Our U2F code also understands
  // this convention.
  // [1]
  // https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html#using-pinToken-in-authenticatorGetAssertion
  if (device->supported_protocol() == ProtocolVersion::kU2f ||
      (device->device_info() &&
       device->device_info()->options.client_pin_availability !=
           AuthenticatorSupportedOptions::ClientPinAvailability::
               kNotSupported)) {
    req.pin_auth.emplace();
    req.pin_protocol = pin::kProtocolVersion;
  }

  DCHECK(IsConvertibleToU2fRegisterCommand(req));

  return req;
}

void MakeCredentialTask::Cancel() {
  canceled_ = true;

  if (register_operation_) {
    register_operation_->Cancel();
  }
  if (silent_sign_operation_) {
    silent_sign_operation_->Cancel();
  }
}

void MakeCredentialTask::StartTask() {
  if (device()->supported_protocol() == ProtocolVersion::kCtap2 &&
      !request_.is_u2f_only &&
      !CtapDeviceShouldUseU2fBecauseClientPinIsSet(device(), request_)) {
    MakeCredential();
  } else {
    // |device_info| should be present iff the device is CTAP2. This will be
    // used in |MaybeRevertU2fFallback| to restore the protocol of CTAP2 devices
    // once this task is complete.
    DCHECK_EQ(device()->supported_protocol() == ProtocolVersion::kCtap2,
              device()->device_info().has_value());
    device()->set_supported_protocol(ProtocolVersion::kU2f);
    U2fRegister();
  }
}

CtapGetAssertionRequest MakeCredentialTask::NextSilentSignRequest() {
  DCHECK(request_.exclude_list &&
         current_credential_ < request_.exclude_list->size());
  CtapGetAssertionRequest request(
      probing_alternative_rp_id_ ? *request_.app_id : request_.rp.id,
      /*client_data_json=*/"");
  request.allow_list = {{request_.exclude_list->at(current_credential_)}};
  request.user_presence_required = false;
  request.user_verification = UserVerificationRequirement::kDiscouraged;
  return request;
}

void MakeCredentialTask::MakeCredential() {
  // Silently probe each credential in the allow list to work around
  // authenticators rejecting lists over a certain size. Also do this if a
  // second RP ID needs to be tested because the site used the appidExclude
  // extension.
  if (request_.exclude_list &&
      (request_.exclude_list->size() > 1 ||
       (!request_.exclude_list->empty() && request_.app_id))) {
    silent_sign_operation_ = std::make_unique<Ctap2DeviceOperation<
        CtapGetAssertionRequest, AuthenticatorGetAssertionResponse>>(
        device(), NextSilentSignRequest(),
        base::BindOnce(&MakeCredentialTask::HandleResponseToSilentSignRequest,
                       weak_factory_.GetWeakPtr()),
        base::BindOnce(&ReadCTAPGetAssertionResponse),
        /*string_fixup_predicate=*/nullptr);
    silent_sign_operation_->Start();
    return;
  }

  register_operation_ = std::make_unique<Ctap2DeviceOperation<
      CtapMakeCredentialRequest, AuthenticatorMakeCredentialResponse>>(
      device(), std::move(request_), std::move(callback_),
      base::BindOnce(&ReadCTAPMakeCredentialResponse,
                     device()->DeviceTransport()),
      /*string_fixup_predicate=*/nullptr);
  register_operation_->Start();
}

void MakeCredentialTask::HandleResponseToSilentSignRequest(
    CtapDeviceResponseCode response_code,
    base::Optional<AuthenticatorGetAssertionResponse> response_data) {
  DCHECK(request_.exclude_list && request_.exclude_list->size() > 0);

  if (canceled_) {
    return;
  }

  // The authenticator recognized a credential from the exclude list. Send the
  // actual request with only that credential in the exclude list to collect a
  // touch and and the CTAP2_ERR_CREDENTIAL_EXCLUDED error code.
  if (response_code == CtapDeviceResponseCode::kSuccess) {
    CtapMakeCredentialRequest request = request_;
    request.exclude_list = {{request_.exclude_list->at(current_credential_)}};
    if (probing_alternative_rp_id_) {
      request.rp.id = *request_.app_id;
    }
    register_operation_ = std::make_unique<Ctap2DeviceOperation<
        CtapMakeCredentialRequest, AuthenticatorMakeCredentialResponse>>(
        device(), std::move(request), std::move(callback_),
        base::BindOnce(&ReadCTAPMakeCredentialResponse,
                       device()->DeviceTransport()),
        /*string_fixup_predicate=*/nullptr);
    register_operation_->Start();
    return;
  }

  // The authenticator returned an unexpected error. Collect a touch to take the
  // authenticator out of the set of active devices.
  if (response_code != CtapDeviceResponseCode::kCtap2ErrInvalidCredential &&
      response_code != CtapDeviceResponseCode::kCtap2ErrNoCredentials &&
      response_code != CtapDeviceResponseCode::kCtap2ErrLimitExceeded &&
      response_code != CtapDeviceResponseCode::kCtap2ErrRequestTooLarge) {
    register_operation_ = std::make_unique<Ctap2DeviceOperation<
        CtapMakeCredentialRequest, AuthenticatorMakeCredentialResponse>>(
        device(), GetTouchRequest(device()),
        base::BindOnce(&MakeCredentialTask::HandleResponseToDummyTouch,
                       weak_factory_.GetWeakPtr()),
        base::BindOnce(&ReadCTAPMakeCredentialResponse,
                       device()->DeviceTransport()),
        /*string_fixup_predicate=*/nullptr);
    register_operation_->Start();
    return;
  }

  // The authenticator doesn't recognize this particular credential from the
  // exclude list. Try the next one.
  current_credential_++;
  if (current_credential_ == request_.exclude_list->size() &&
      !probing_alternative_rp_id_ && request_.app_id) {
    // All elements of |request_.exclude_list| have been tested, but there's a
    // second RP ID so they need to be tested again.
    current_credential_ = 0;
    probing_alternative_rp_id_ = true;
  }

  if (current_credential_ < request_.exclude_list->size()) {
    silent_sign_operation_ = std::make_unique<Ctap2DeviceOperation<
        CtapGetAssertionRequest, AuthenticatorGetAssertionResponse>>(
        device(), NextSilentSignRequest(),
        base::BindOnce(&MakeCredentialTask::HandleResponseToSilentSignRequest,
                       weak_factory_.GetWeakPtr()),
        base::BindOnce(&ReadCTAPGetAssertionResponse),
        /*string_fixup_predicate=*/nullptr);
    silent_sign_operation_->Start();
    return;
  }

  // None of the credentials from the exclude list were recognized. The actual
  // register request may proceed but without the exclude list present in case
  // it exceeds the device's size limit.
  CtapMakeCredentialRequest request = request_;
  request.exclude_list.reset();
  register_operation_ = std::make_unique<Ctap2DeviceOperation<
      CtapMakeCredentialRequest, AuthenticatorMakeCredentialResponse>>(
      device(), std::move(request), std::move(callback_),
      base::BindOnce(&ReadCTAPMakeCredentialResponse,
                     device()->DeviceTransport()),
      /*string_fixup_predicate=*/nullptr);
  register_operation_->Start();
}

void MakeCredentialTask::HandleResponseToDummyTouch(
    CtapDeviceResponseCode response_code,
    base::Optional<AuthenticatorMakeCredentialResponse> response_data) {
  std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                           base::nullopt);
}

void MakeCredentialTask::U2fRegister() {
  if (!IsConvertibleToU2fRegisterCommand(request_)) {
    std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                             base::nullopt);
    return;
  }

  DCHECK_EQ(ProtocolVersion::kU2f, device()->supported_protocol());
  register_operation_ = std::make_unique<U2fRegisterOperation>(
      device(), std::move(request_),
      base::BindOnce(&MakeCredentialTask::MaybeRevertU2fFallback,
                     weak_factory_.GetWeakPtr()));
  register_operation_->Start();
}

void MakeCredentialTask::MaybeRevertU2fFallback(
    CtapDeviceResponseCode status,
    base::Optional<AuthenticatorMakeCredentialResponse> response) {
  DCHECK_EQ(ProtocolVersion::kU2f, device()->supported_protocol());
  if (device()->device_info()) {
    // This was actually a CTAP2 device, but the protocol version was set to U2F
    // because it had a PIN set and so, in order to make a credential, the U2F
    // interface was used.
    device()->set_supported_protocol(ProtocolVersion::kCtap2);
  }

  std::move(callback_).Run(status, std::move(response));
}

}  // namespace device
