// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/get_assertion_task.h"

#include <utility>

#include "base/bind.h"
#include "device/base/features.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/ctap2_device_operation.h"
#include "device/fido/make_credential_task.h"
#include "device/fido/u2f_sign_operation.h"

namespace device {

namespace {

bool MayFallbackToU2fWithAppIdExtension(
    const FidoDevice& device,
    const CtapGetAssertionRequest& request) {
  bool ctap2_device_supports_u2f =
      device.device_info() &&
      base::Contains(device.device_info()->versions, ProtocolVersion::kU2f);
  return request.alternative_application_parameter &&
         ctap2_device_supports_u2f && !request.allow_list.empty();
}

}  // namespace

GetAssertionTask::GetAssertionTask(FidoDevice* device,
                                   CtapGetAssertionRequest request,
                                   GetAssertionTaskCallback callback)
    : FidoTask(device),
      request_(std::move(request)),
      callback_(std::move(callback)) {
  // This code assumes that user-presence is requested in order to implement
  // possible U2F-fallback.
  DCHECK(request_.user_presence_required);

  // The UV parameter should have been made binary by this point because CTAP2
  // only takes a binary value.
  DCHECK_NE(request_.user_verification,
            UserVerificationRequirement::kPreferred);
}

GetAssertionTask::~GetAssertionTask() = default;

void GetAssertionTask::Cancel() {
  canceled_ = true;

  if (sign_operation_) {
    sign_operation_->Cancel();
  }
  if (dummy_register_operation_) {
    dummy_register_operation_->Cancel();
  }
}

// static
bool GetAssertionTask::StringFixupPredicate(
    const std::vector<const cbor::Value*>& path) {
  if (path.size() != 2 || !path[0]->is_unsigned() ||
      path[0]->GetUnsigned() != 4 || !path[1]->is_string()) {
    return false;
  }

  const std::string& user_key = path[1]->GetString();
  return user_key == "name" || user_key == "displayName";
}

void GetAssertionTask::StartTask() {
  if (device()->supported_protocol() == ProtocolVersion::kCtap2) {
    GetAssertion();
  } else {
    U2fSign();
  }
}

CtapGetAssertionRequest GetAssertionTask::NextSilentRequest() {
  DCHECK(current_credential_ < request_.allow_list.size());
  CtapGetAssertionRequest request = request_;
  request.allow_list = {{request_.allow_list.at(current_credential_)}};
  request.user_presence_required = false;
  request.user_verification = UserVerificationRequirement::kDiscouraged;
  return request;
}

void GetAssertionTask::GetAssertion() {
  // Silently probe each credential in the allow list to work around
  // authenticators rejecting lists over a certain size. Also probe silently if
  // the request may fall back to U2F and the authenticator doesn't recognize
  // any of the provided credential IDs.
  if (((request_.allow_list.size() > 1 &&
        // If the device supports credProtect then it might have UV-required
        // credentials which it'll pretend don't exist for silent requests.
        // TODO(agl): should support batching of, and filtering over-long,
        // credentials based on GetInfo data. Also should support
        // PIN-authenticated silent requests.
        !device()->device_info()->options.supports_cred_protect) ||
       MayFallbackToU2fWithAppIdExtension(*device(), request_)) &&
      // caBLE devices might not support silent probing so don't do it with
      // them.
      device()->DeviceTransport() !=
          FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy) {
    sign_operation_ = std::make_unique<Ctap2DeviceOperation<
        CtapGetAssertionRequest, AuthenticatorGetAssertionResponse>>(
        device(), NextSilentRequest(),
        base::BindOnce(&GetAssertionTask::HandleResponseToSilentRequest,
                       weak_factory_.GetWeakPtr()),
        base::BindOnce(&ReadCTAPGetAssertionResponse),
        /*string_fixup_predicate=*/nullptr);
    sign_operation_->Start();
    return;
  }

  sign_operation_ =
      std::make_unique<Ctap2DeviceOperation<CtapGetAssertionRequest,
                                            AuthenticatorGetAssertionResponse>>(
          device(), request_,
          base::BindOnce(&GetAssertionTask::HandleResponse,
                         weak_factory_.GetWeakPtr()),
          base::BindOnce(&ReadCTAPGetAssertionResponse), StringFixupPredicate);
  sign_operation_->Start();
}

void GetAssertionTask::U2fSign() {
  DCHECK_EQ(ProtocolVersion::kU2f, device()->supported_protocol());

  sign_operation_ = std::make_unique<U2fSignOperation>(device(), request_,
                                                       std::move(callback_));
  sign_operation_->Start();
}

void GetAssertionTask::HandleResponse(
    CtapDeviceResponseCode response_code,
    base::Optional<AuthenticatorGetAssertionResponse> response_data) {
  if (canceled_) {
    return;
  }

  // Some authenticators will return this error before waiting for a touch if
  // they don't recognise a credential. In other cases the result can be
  // returned immediately.
  if (response_code != CtapDeviceResponseCode::kCtap2ErrInvalidCredential) {
    std::move(callback_).Run(response_code, std::move(response_data));
    return;
  }

  // The request failed in a way that didn't request a touch. Simulate it.
  dummy_register_operation_ = std::make_unique<Ctap2DeviceOperation<
      CtapMakeCredentialRequest, AuthenticatorMakeCredentialResponse>>(
      device(), MakeCredentialTask::GetTouchRequest(device()),
      base::BindOnce(&GetAssertionTask::HandleDummyMakeCredentialComplete,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&ReadCTAPMakeCredentialResponse,
                     device()->DeviceTransport()),
      /*string_fixup_predicate=*/nullptr);
  dummy_register_operation_->Start();
}

void GetAssertionTask::HandleResponseToSilentRequest(
    CtapDeviceResponseCode response_code,
    base::Optional<AuthenticatorGetAssertionResponse> response_data) {
  DCHECK(request_.allow_list.size() > 0);

  if (canceled_) {
    return;
  }

  // Credential was recognized by the device. As this authentication was a
  // silent authentication (i.e. user touch was not provided), try again with
  // only the matching credential, user presence enforced and with the original
  // user verification configuration.
  if (response_code == CtapDeviceResponseCode::kSuccess) {
    CtapGetAssertionRequest request = request_;
    request.allow_list = {{request_.allow_list.at(current_credential_)}};
    sign_operation_ = std::make_unique<Ctap2DeviceOperation<
        CtapGetAssertionRequest, AuthenticatorGetAssertionResponse>>(
        device(), std::move(request),
        base::BindOnce(&GetAssertionTask::HandleResponse,
                       weak_factory_.GetWeakPtr()),
        base::BindOnce(&ReadCTAPGetAssertionResponse),
        /*string_fixup_predicate=*/nullptr);
    sign_operation_->Start();
    return;
  }

  // Credential was not recognized or an error occurred. Probe the next
  // credential.
  if (++current_credential_ < request_.allow_list.size()) {
    sign_operation_ = std::make_unique<Ctap2DeviceOperation<
        CtapGetAssertionRequest, AuthenticatorGetAssertionResponse>>(
        device(), NextSilentRequest(),
        base::BindOnce(&GetAssertionTask::HandleResponseToSilentRequest,
                       weak_factory_.GetWeakPtr()),
        base::BindOnce(&ReadCTAPGetAssertionResponse),
        /*string_fixup_predicate=*/nullptr);
    sign_operation_->Start();
    return;
  }

  // None of the credentials were recognized. Fall back to U2F or collect a
  // dummy touch.
  if (MayFallbackToU2fWithAppIdExtension(*device(), request_)) {
    device()->set_supported_protocol(ProtocolVersion::kU2f);
    U2fSign();
    return;
  }
  dummy_register_operation_ = std::make_unique<Ctap2DeviceOperation<
      CtapMakeCredentialRequest, AuthenticatorMakeCredentialResponse>>(
      device(), MakeCredentialTask::GetTouchRequest(device()),
      base::BindOnce(&GetAssertionTask::HandleDummyMakeCredentialComplete,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&ReadCTAPMakeCredentialResponse,
                     device()->DeviceTransport()),
      /*string_fixup_predicate=*/nullptr);
  dummy_register_operation_->Start();
}

void GetAssertionTask::HandleDummyMakeCredentialComplete(
    CtapDeviceResponseCode response_code,
    base::Optional<AuthenticatorMakeCredentialResponse> response_data) {
  std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrNoCredentials,
                           base::nullopt);
}

}  // namespace device
