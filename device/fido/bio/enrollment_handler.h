// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_BIO_ENROLLMENT_HANDLER_H_
#define DEVICE_FIDO_BIO_ENROLLMENT_HANDLER_H_

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "device/fido/bio/enrollment.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/pin.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace device {

class COMPONENT_EXPORT(DEVICE_FIDO) BioEnrollmentHandler
    : public FidoRequestHandlerBase {
 public:
  using ErrorCallback = base::OnceCallback<void(FidoReturnCode)>;
  using GetPINCallback =
      base::RepeatingCallback<void(int64_t retries,
                                   base::OnceCallback<void(std::string)>)>;
  using ResponseCallback =
      base::OnceCallback<void(CtapDeviceResponseCode,
                              base::Optional<BioEnrollmentResponse>)>;

  BioEnrollmentHandler(
      service_manager::Connector* connector,
      const base::flat_set<FidoTransportProtocol>& supported_transports,
      base::OnceClosure ready_callback,
      ErrorCallback error_callback,
      GetPINCallback get_pin_callback,
      FidoDiscoveryFactory* factory =
          std::make_unique<FidoDiscoveryFactory>().get());
  ~BioEnrollmentHandler() override;

  void GetModality(ResponseCallback);
  void GetSensorInfo(ResponseCallback);
  void EnrollTemplate(ResponseCallback);
  void Cancel(base::OnceClosure);
  void EnumerateTemplates(ResponseCallback);

 private:
  // FidoRequestHandlerBase:
  void DispatchRequest(FidoAuthenticator*) override;
  void AuthenticatorRemoved(FidoDiscoveryBase*, FidoAuthenticator*) override;

  void OnTouch(FidoAuthenticator* authenticator);
  void OnRetriesResponse(CtapDeviceResponseCode,
                         base::Optional<pin::RetriesResponse>);
  void OnHavePIN(std::string pin);
  void OnHaveEphemeralKey(std::string,
                          CtapDeviceResponseCode,
                          base::Optional<pin::KeyAgreementResponse>);
  void OnHavePINToken(CtapDeviceResponseCode,
                      base::Optional<pin::TokenResponse>);
  void OnEnrollTemplate(CtapDeviceResponseCode,
                        base::Optional<BioEnrollmentResponse>);
  void OnCancel(CtapDeviceResponseCode, base::Optional<BioEnrollmentResponse>);
  void OnEnumerateTemplates(CtapDeviceResponseCode,
                            base::Optional<BioEnrollmentResponse>);

  SEQUENCE_CHECKER(sequence_checker);

  FidoAuthenticator* authenticator_ = nullptr;
  base::OnceClosure ready_callback_;
  ErrorCallback error_callback_;
  GetPINCallback get_pin_callback_;
  ResponseCallback response_callback_;
  base::OnceClosure status_callback_;
  base::Optional<pin::TokenResponse> pin_token_response_;
  base::WeakPtrFactory<BioEnrollmentHandler> weak_factory_;

  BioEnrollmentHandler(const BioEnrollmentHandler&) = delete;
  BioEnrollmentHandler(BioEnrollmentHandler&&) = delete;
};

}  // namespace device

#endif  // DEVICE_FIDO_BIO_ENROLLMENT_HANDLER_H_
