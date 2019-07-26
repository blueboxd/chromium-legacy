// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_GET_ASSERTION_REQUEST_HANDLER_H_
#define DEVICE_FIDO_GET_ASSERTION_REQUEST_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_request_handler.h"
#include "device/fido/fido_transport_protocol.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace device {

class FidoAuthenticator;
class FidoDiscoveryFactory;
class AuthenticatorGetAssertionResponse;

class COMPONENT_EXPORT(DEVICE_FIDO) GetAssertionRequestHandler
    : public FidoRequestHandler<
          std::vector<AuthenticatorGetAssertionResponse>> {
 public:
  GetAssertionRequestHandler(
      service_manager::Connector* connector,
      FidoDiscoveryFactory* fido_discovery_factory,
      const base::flat_set<FidoTransportProtocol>& supported_transports,
      CtapGetAssertionRequest request_parameter,
      CompletionCallback completion_callback);
  ~GetAssertionRequestHandler() override;

 private:
  enum class State {
    kWaitingForTouch,
    kWaitingForSecondTouch,
    kGettingRetries,
    kWaitingForPIN,
    kGetEphemeralKey,
    kRequestWithPIN,
    kReadingMultipleResponses,
    kFinished,
  };

  // FidoRequestHandlerBase:
  void DispatchRequest(FidoAuthenticator* authenticator) override;
  void AuthenticatorAdded(FidoDiscoveryBase* discovery,
                          FidoAuthenticator* authenticator) override;
  void AuthenticatorRemoved(FidoDiscoveryBase* discovery,
                            FidoAuthenticator* authenticator) override;

  void HandleResponse(
      FidoAuthenticator* authenticator,
      CtapDeviceResponseCode response_code,
      base::Optional<AuthenticatorGetAssertionResponse> response);
  void HandleNextResponse(
      FidoAuthenticator* authenticator,
      CtapDeviceResponseCode response_code,
      base::Optional<AuthenticatorGetAssertionResponse> response);
  void HandleTouch(FidoAuthenticator* authenticator);
  void HandleInapplicableAuthenticator(FidoAuthenticator* authenticator);
  void OnRetriesResponse(CtapDeviceResponseCode status,
                         base::Optional<pin::RetriesResponse> response);
  void OnHavePIN(std::string pin);
  void OnHaveEphemeralKey(std::string pin,
                          CtapDeviceResponseCode status,
                          base::Optional<pin::KeyAgreementResponse> response);
  void OnHavePINToken(CtapDeviceResponseCode status,
                      base::Optional<pin::TokenResponse> response);

  State state_ = State::kWaitingForTouch;
  CtapGetAssertionRequest request_;
  // authenticator_ points to the authenticator that will be used for this
  // operation. It's only set after the user touches an authenticator to select
  // it, after which point that authenticator will be used exclusively through
  // requesting PIN etc. The object is owned by the underlying discovery object
  // and this pointer is cleared if it's removed during processing.
  FidoAuthenticator* authenticator_ = nullptr;
  // responses_ holds the set of responses while they are incrementally read
  // from the device. Only used when more than one response is returned.
  std::vector<AuthenticatorGetAssertionResponse> responses_;
  // remaining_responses_ contains the number of responses that remain to be
  // read when multiple responses are returned.
  size_t remaining_responses_ = 0;
  SEQUENCE_CHECKER(my_sequence_checker_);
  base::WeakPtrFactory<GetAssertionRequestHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GetAssertionRequestHandler);
};

}  // namespace device

#endif  // DEVICE_FIDO_GET_ASSERTION_REQUEST_HANDLER_H_
