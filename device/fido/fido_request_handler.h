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
template <typename Status, typename Response>
class FidoRequestHandler : public FidoRequestHandlerBase {
 public:
  using CompletionCallback =
      base::OnceCallback<void(Status status,
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
                               Status status,
                               base::Optional<Response> response_data) {
    if (is_complete()) {
      // TODO: this should be handled at a higher level and so there should be
      // a NOTREACHED() here. But some unittests are exercising this directly.
      return;
    }

    std::move(completion_callback_)
        .Run(status, std::move(response_data), authenticator);
  }

  CompletionCallback completion_callback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FidoRequestHandler);
};

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_REQUEST_HANDLER_H_
