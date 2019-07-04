// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/bio/enrollment_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"

namespace device {

BioEnrollmentHandler::BioEnrollmentHandler(
    service_manager::Connector* connector,
    const base::flat_set<FidoTransportProtocol>& supported_transports,
    base::OnceClosure ready_callback,
    ErrorCallback error_callback,
    GetPINCallback get_pin_callback,
    FidoDiscoveryFactory* factory)
    : FidoRequestHandlerBase(connector, factory, supported_transports),
      ready_callback_(std::move(ready_callback)),
      error_callback_(std::move(error_callback)),
      get_pin_callback_(std::move(get_pin_callback)),
      weak_factory_(this) {
  Start();
}

BioEnrollmentHandler::~BioEnrollmentHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BioEnrollmentHandler::GetModality(ResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(authenticator_);
  authenticator_->GetModality(std::move(callback));
}

void BioEnrollmentHandler::GetSensorInfo(ResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(authenticator_);
  authenticator_->GetSensorInfo(std::move(callback));
}

void BioEnrollmentHandler::EnrollTemplate(SampleCallback sample_callback,
                                          StatusCallback completion_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  authenticator_->BioEnrollFingerprint(
      *pin_token_response_, std::move(sample_callback),
      base::BindOnce(&BioEnrollmentHandler::OnEnrollTemplateFinished,
                     weak_factory_.GetWeakPtr(),
                     std::move(completion_callback)));
}

void BioEnrollmentHandler::Cancel(StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  authenticator_->BioEnrollCancel(
      base::BindOnce(&BioEnrollmentHandler::OnCancel,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BioEnrollmentHandler::EnumerateTemplates(EnumerationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  authenticator_->BioEnrollEnumerate(
      *pin_token_response_,
      base::BindOnce(&BioEnrollmentHandler::OnEnumerateTemplates,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BioEnrollmentHandler::RenameTemplate(std::vector<uint8_t> template_id,
                                          std::string name,
                                          StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  authenticator_->BioEnrollRename(
      *pin_token_response_, std::move(template_id), std::move(name),
      base::BindOnce(&BioEnrollmentHandler::OnRenameTemplate,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BioEnrollmentHandler::DeleteTemplate(std::vector<uint8_t> template_id,
                                          StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  authenticator_->BioEnrollDelete(
      *pin_token_response_, std::move(template_id),
      base::BindOnce(&BioEnrollmentHandler::OnDeleteTemplate,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BioEnrollmentHandler::DispatchRequest(FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  authenticator->GetTouch(base::BindOnce(&BioEnrollmentHandler::OnTouch,
                                         weak_factory_.GetWeakPtr(),
                                         authenticator));
}

void BioEnrollmentHandler::AuthenticatorRemoved(
    FidoDiscoveryBase* discovery,
    FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FidoRequestHandlerBase::AuthenticatorRemoved(discovery, authenticator);
  if (authenticator_ != authenticator) {
    return;
  }

  authenticator_ = nullptr;

  std::move(error_callback_)
      .Run(pin_token_response_
               ? FidoReturnCode::kAuthenticatorRemovedDuringPINEntry
               : FidoReturnCode::kSuccess);
}

void BioEnrollmentHandler::OnTouch(FidoAuthenticator* authenticator) {
  CancelActiveAuthenticators(authenticator->GetId());

  if (!authenticator->Options() ||
      (authenticator->Options()->bio_enrollment_availability ==
           AuthenticatorSupportedOptions::BioEnrollmentAvailability::
               kNotSupported &&
       authenticator->Options()->bio_enrollment_availability_preview ==
           AuthenticatorSupportedOptions::BioEnrollmentAvailability::
               kNotSupported)) {
    std::move(error_callback_)
        .Run(FidoReturnCode::kAuthenticatorMissingBioEnrollment);
    return;
  }

  if (authenticator->Options()->client_pin_availability !=
      AuthenticatorSupportedOptions::ClientPinAvailability::
          kSupportedAndPinSet) {
    std::move(error_callback_)
        .Run(FidoReturnCode::kAuthenticatorMissingUserVerification);
    return;
  }

  authenticator_ = authenticator;
  authenticator_->GetRetries(base::BindOnce(
      &BioEnrollmentHandler::OnRetriesResponse, weak_factory_.GetWeakPtr()));
}

void BioEnrollmentHandler::OnRetriesResponse(
    CtapDeviceResponseCode code,
    base::Optional<pin::RetriesResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!response || code != CtapDeviceResponseCode::kSuccess) {
    FIDO_LOG(DEBUG) << "OnRetriesResponse failed: " << static_cast<int>(code);
    std::move(error_callback_)
        .Run(FidoReturnCode::kAuthenticatorResponseInvalid);
    return;
  }

  if (response->retries == 0) {
    std::move(error_callback_).Run(FidoReturnCode::kHardPINBlock);
    return;
  }

  get_pin_callback_.Run(response->retries,
                        base::BindOnce(&BioEnrollmentHandler::OnHavePIN,
                                       weak_factory_.GetWeakPtr()));
}

void BioEnrollmentHandler::OnHavePIN(std::string pin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  authenticator_->GetEphemeralKey(
      base::BindOnce(&BioEnrollmentHandler::OnHaveEphemeralKey,
                     weak_factory_.GetWeakPtr(), std::move(pin)));
}

void BioEnrollmentHandler::OnHaveEphemeralKey(
    std::string pin,
    CtapDeviceResponseCode code,
    base::Optional<pin::KeyAgreementResponse> response) {
  if (code != CtapDeviceResponseCode::kSuccess) {
    FIDO_LOG(DEBUG) << "OnHaveEphemeralKey failed: " << static_cast<int>(code);
    std::move(error_callback_)
        .Run(FidoReturnCode::kAuthenticatorResponseInvalid);
    return;
  }

  authenticator_->GetPINToken(
      std::move(pin), *response,
      base::BindOnce(&BioEnrollmentHandler::OnHavePINToken,
                     weak_factory_.GetWeakPtr()));
}

void BioEnrollmentHandler::OnHavePINToken(
    CtapDeviceResponseCode code,
    base::Optional<pin::TokenResponse> response) {
  if (code == CtapDeviceResponseCode::kCtap2ErrPinInvalid) {
    authenticator_->GetRetries(base::BindOnce(
        &BioEnrollmentHandler::OnRetriesResponse, weak_factory_.GetWeakPtr()));
    return;
  }

  switch (code) {
    case CtapDeviceResponseCode::kCtap2ErrPinAuthBlocked:
      std::move(error_callback_).Run(FidoReturnCode::kSoftPINBlock);
      return;
    case CtapDeviceResponseCode::kCtap2ErrPinBlocked:
      std::move(error_callback_).Run(FidoReturnCode::kHardPINBlock);
      return;
    default:
      std::move(error_callback_)
          .Run(FidoReturnCode::kAuthenticatorResponseInvalid);
      return;
    case CtapDeviceResponseCode::kSuccess:
      // fall through on success
      break;
  }

  pin_token_response_ = *response;
  std::move(ready_callback_).Run();
}

void BioEnrollmentHandler::OnEnrollTemplateFinished(
    StatusCallback callback,
    CtapDeviceResponseCode code,
    base::Optional<BioEnrollmentResponse> response) {
  if (code == CtapDeviceResponseCode::kSuccess &&
      (!response || !response->last_status || !response->remaining_samples)) {
    // Response is incomplete or invalid.
    std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrOther);
    return;
  }
  FIDO_LOG(DEBUG) << "Finished bio enrollment with code "
                  << static_cast<int>(code);
  std::move(callback).Run(code);
}

void BioEnrollmentHandler::OnCancel(StatusCallback callback,
                                    CtapDeviceResponseCode code,
                                    base::Optional<BioEnrollmentResponse>) {
  std::move(callback).Run(code);
}

void BioEnrollmentHandler::OnEnumerateTemplates(
    EnumerationCallback callback,
    CtapDeviceResponseCode code,
    base::Optional<BioEnrollmentResponse> response) {
  if (code != CtapDeviceResponseCode::kSuccess) {
    // Response is not valid if operation was not successful.
    // Note that an empty enumeration returns kCtap2ErrInvalidOption.
    std::move(callback).Run(code, base::nullopt);
    return;
  }
  if (!response || !response->template_infos) {
    // Response must have template_infos.
    std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                            base::nullopt);
    return;
  }
  std::move(callback).Run(code, std::move(*response->template_infos));
}

void BioEnrollmentHandler::OnRenameTemplate(
    StatusCallback callback,
    CtapDeviceResponseCode code,
    base::Optional<BioEnrollmentResponse> response) {
  std::move(callback).Run(code);
}

void BioEnrollmentHandler::OnDeleteTemplate(
    StatusCallback callback,
    CtapDeviceResponseCode code,
    base::Optional<BioEnrollmentResponse> response) {
  std::move(callback).Run(code);
}

}  // namespace device
