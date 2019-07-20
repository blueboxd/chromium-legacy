// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_REQUEST_HANDLER_H_
#define DEVICE_FIDO_FIDO_REQUEST_HANDLER_H_

#include "device/fido/fido_request_handler_base.h"

#include <utility>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/optional.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_device.h"
#include "device/fido/fido_transport_protocol.h"

namespace device {

class FidoDiscoveryFactory;

// Handles receiving response form potentially multiple connected authenticators
// and relaying response to the relying party.
//
// TODO: this class should be dropped; it's not pulling its weight.
template <class Response>
class FidoRequestHandler : public FidoRequestHandlerBase {
 public:
  using CompletionCallback =
      base::OnceCallback<void(FidoReturnCode status_code,
                              base::Optional<Response> response_data,
                              const FidoAuthenticator* authenticator)>;

  // The |available_transports| should be the intersection of transports
  // supported by the client and allowed by the relying party.
  FidoRequestHandler(
      service_manager::Connector* connector,
      FidoDiscoveryFactory* fido_discovery_factory,
      const base::flat_set<FidoTransportProtocol>& available_transports,
      CompletionCallback completion_callback)
      : FidoRequestHandlerBase(connector,
                               fido_discovery_factory,
                               available_transports),
        completion_callback_(std::move(completion_callback)) {}

  ~FidoRequestHandler() override = default;

  bool is_complete() const { return completion_callback_.is_null(); }

 protected:
  // Converts authenticator response code received from CTAP1/CTAP2 device into
  // FidoReturnCode and passes response data to webauth::mojom::Authenticator.
  void OnAuthenticatorResponse(FidoAuthenticator* authenticator,
                               FidoReturnCode result,
                               base::Optional<Response> response_data) {
    if (is_complete()) {
      // TODO: this should be handled at a higher level and so there should be
      // a NOTREACHED() here. But some unittests are exercising this directly.
      return;
    }

    std::move(completion_callback_)
        .Run(result, std::move(response_data), authenticator);
  }

  CompletionCallback completion_callback_;

  static base::Optional<FidoReturnCode>
  ConvertDeviceResponseCodeToFidoReturnCode(
      CtapDeviceResponseCode device_response_code) {
    switch (device_response_code) {
      case CtapDeviceResponseCode::kSuccess:
        return FidoReturnCode::kSuccess;

      // These errors are only returned after the user interacted with the
      // authenticator.
      case CtapDeviceResponseCode::kCtap2ErrCredentialExcluded:
        return FidoReturnCode::kUserConsentButCredentialExcluded;
      case CtapDeviceResponseCode::kCtap2ErrNoCredentials:
        return FidoReturnCode::kUserConsentButCredentialNotRecognized;

      // The user explicitly denied the operation. Touch ID returns this error
      // when the user cancels the macOS prompt. External authenticators may
      // return it e.g. after the user fails fingerprint verification.
      case CtapDeviceResponseCode::kCtap2ErrOperationDenied:
        return FidoReturnCode::kUserConsentDenied;

      // External authenticators may return this error if internal user
      // verification fails for a make credential request or if the pin token is
      // not valid.
      case CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid:
        return FidoReturnCode::kUserConsentDenied;

      case CtapDeviceResponseCode::kCtap2ErrKeyStoreFull:
        return FidoReturnCode::kStorageFull;

      // This error is returned by some authenticators (e.g. the "Yubico FIDO
      // 2" CTAP2 USB keys) during GetAssertion **before the user interacted
      // with the device**. The authenticator does this to avoid blinking (and
      // possibly asking the user for their PIN) for requests it knows
      // beforehand it cannot handle.
      //
      // Ignore this error to avoid canceling the request without user
      // interaction.
      case CtapDeviceResponseCode::kCtap2ErrInvalidCredential:
        return base::nullopt;

      // For all other errors, the authenticator will be dropped, and other
      // authenticators may continue.
      default:
        return base::nullopt;
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FidoRequestHandler);
};

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_REQUEST_HANDLER_H_
