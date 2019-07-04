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
  using StatusCallback = base::OnceCallback<void(CtapDeviceResponseCode)>;
  using EnumerationCallback = base::OnceCallback<void(
      CtapDeviceResponseCode,
      base::Optional<std::map<std::vector<uint8_t>, std::string>>)>;
  using SampleCallback =
      base::RepeatingCallback<void(BioEnrollmentSampleStatus, uint8_t)>;

  BioEnrollmentHandler(
      service_manager::Connector* connector,
      const base::flat_set<FidoTransportProtocol>& supported_transports,
      base::OnceClosure ready_callback,
      ErrorCallback error_callback,
      GetPINCallback get_pin_callback,
      FidoDiscoveryFactory* factory =
          std::make_unique<FidoDiscoveryFactory>().get());
  ~BioEnrollmentHandler() override;

  // Returns the modality of the authenticator's user verification.
  // Currently, the only valid modality is fingerprint.
  void GetModality(ResponseCallback);

  // Returns fingerprint sensor info for the authenticator.
  // This includes number of required samples to enroll and sensor type.
  void GetSensorInfo(ResponseCallback);

  // Enrolls a new fingerprint template. The user must provide the required
  // number of samples by touching the authenticator's sensor repeatedly.
  // After each sample, or a timeout, |sample_callback| is invoked with the
  // remaining number of samples. Once all samples have been collected or
  // the operation has been cancelled, |completion_callback| is invoked
  // with the operation status.
  void EnrollTemplate(SampleCallback sample_callback,
                      StatusCallback completion_callback);

  // Cancels an ongoing enrollment, if any, and invokes the callback with
  // |CtapDeviceResponseCode::kSuccess|.
  void Cancel(StatusCallback);

  // Requests a map of current enrollments from the authenticator. On success,
  // the callback is invoked with a map from template IDs to human-readable
  // names. On failure, the callback is invoked with base::nullopt.
  void EnumerateTemplates(EnumerationCallback);

  // Renames the enrollment identified by |template_id| to |name|.
  void RenameTemplate(std::vector<uint8_t> template_id,
                      std::string name,
                      StatusCallback);

  // Deletes the enrollment identified by |template_id|.
  void DeleteTemplate(std::vector<uint8_t> template_id, StatusCallback);

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
  void OnEnrollTemplateFinished(StatusCallback,
                                CtapDeviceResponseCode,
                                base::Optional<BioEnrollmentResponse>);
  void OnCancel(StatusCallback,
                CtapDeviceResponseCode,
                base::Optional<BioEnrollmentResponse>);
  void OnEnumerateTemplates(EnumerationCallback,
                            CtapDeviceResponseCode,
                            base::Optional<BioEnrollmentResponse>);
  void OnRenameTemplate(StatusCallback,
                        CtapDeviceResponseCode,
                        base::Optional<BioEnrollmentResponse>);
  void OnDeleteTemplate(StatusCallback,
                        CtapDeviceResponseCode,
                        base::Optional<BioEnrollmentResponse>);

  SEQUENCE_CHECKER(sequence_checker_);

  FidoAuthenticator* authenticator_ = nullptr;
  base::OnceClosure ready_callback_;
  ErrorCallback error_callback_;
  GetPINCallback get_pin_callback_;
  base::Optional<pin::TokenResponse> pin_token_response_;
  base::WeakPtrFactory<BioEnrollmentHandler> weak_factory_;

  BioEnrollmentHandler(const BioEnrollmentHandler&) = delete;
  BioEnrollmentHandler(BioEnrollmentHandler&&) = delete;
};

}  // namespace device

#endif  // DEVICE_FIDO_BIO_ENROLLMENT_HANDLER_H_
