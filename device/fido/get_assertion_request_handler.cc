// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/get_assertion_request_handler.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/cable/fido_cable_discovery.h"
#include "device/fido/features.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/get_assertion_task.h"
#include "device/fido/pin.h"

#if defined(OS_MACOSX)
#include "device/fido/mac/authenticator.h"
#endif  // defined(OS_MACOSX)

namespace device {

namespace {

bool ResponseValid(const FidoAuthenticator& authenticator,
                   const CtapGetAssertionRequest& request,
                   const AuthenticatorGetAssertionResponse& response) {
  if (response.GetRpIdHash() !=
          fido_parsing_utils::CreateSHA256Hash(request.rp_id) &&
      (!request.app_id ||
       response.GetRpIdHash() != request.alternative_application_parameter)) {
    return false;
  }

  // PublicKeyUserEntity field in GetAssertion response is optional with the
  // following constraints:
  // - If assertion has been made without user verification, user identifiable
  //   information must not be included.
  // - For resident key credentials, user id of the user entity is mandatory.
  // - When multiple accounts exist for specified RP ID, user entity is
  //   mandatory.
  // TODO(hongjunchoi) : Add link to section of the CTAP spec once it is
  // published.
  const auto& user_entity = response.user_entity();
  const bool has_user_identifying_info =
      user_entity &&
      (user_entity->display_name || user_entity->name || user_entity->icon_url);
  if (!response.auth_data().obtained_user_verification() &&
      has_user_identifying_info) {
    return false;
  }

  if (request.allow_list.empty() && !user_entity) {
    return false;
  }

  if (response.num_credentials().value_or(0u) > 1 && !user_entity) {
    return false;
  }

  // Check whether credential ID returned from the authenticator and transport
  // type used matches the transport type and credential ID defined in
  // PublicKeyCredentialDescriptor of the allowed list. If the device has
  // resident key support, returned credential ID may be resident credential.
  // Thus, returned credential ID need not be in allowed list.
  // TODO(hongjunchoi) : Add link to section of the CTAP spec once it is
  // published.
  const auto& allow_list = request.allow_list;
  if (allow_list.empty()) {
    if (authenticator.Options() &&
        !authenticator.Options()->supports_resident_key) {
      // Allow list can't be empty for authenticators w/o resident key support.
      return false;
    }
  } else {
    // Non-empty allow list. Credential ID on the response may be omitted if
    // allow list has size 1. Otherwise, it needs to match an entry from the
    // allow list
    const auto opt_transport_used = authenticator.AuthenticatorTransport();
    if ((!response.credential() && allow_list.size() != 1) ||
        (response.credential() &&
         !std::any_of(allow_list.cbegin(), allow_list.cend(),
                      [&response, opt_transport_used](const auto& credential) {
                        return credential.id() ==
                                   response.raw_credential_id() &&
                               (!opt_transport_used ||
                                base::Contains(credential.transports(),
                                               *opt_transport_used));
                      }))) {
      return false;
    }
  }

  // The authenticatorData on an GetAssertionResponse must not have
  // attestedCredentialData set.
  if (response.auth_data().attested_data().has_value()) {
    return false;
  }

  return true;
}

// When the response from the authenticator does not contain a credential and
// the allow list from the GetAssertion request only contains a single
// credential id, manually set credential id in the returned response.
void SetCredentialIdForResponseWithEmptyCredential(
    const CtapGetAssertionRequest& request,
    AuthenticatorGetAssertionResponse& response) {
  if (request.allow_list.size() == 1 && !response.credential()) {
    response.SetCredential(request.allow_list.at(0));
  }
}

// Checks UserVerificationRequirement enum passed from the relying party is
// compatible with the authenticator.
bool CheckUserVerificationCompatible(FidoAuthenticator* authenticator,
                                     const CtapGetAssertionRequest& request,
                                     bool have_observer) {
  const auto& opt_options = authenticator->Options();
  if (!opt_options) {
    // This authenticator doesn't know its capabilities yet, so we need
    // to assume it can handle the request. This is the case for Windows,
    // where we proxy the request to the native API.
    return true;
  }

  const bool pin_support =
      base::FeatureList::IsEnabled(device::kWebAuthPINSupport) && have_observer;
  return request.user_verification != UserVerificationRequirement::kRequired ||
         opt_options->user_verification_availability ==
             AuthenticatorSupportedOptions::UserVerificationAvailability::
                 kSupportedAndConfigured ||
         (pin_support && opt_options->client_pin_availability ==
                             AuthenticatorSupportedOptions::
                                 ClientPinAvailability::kSupportedAndPinSet);
}

base::flat_set<FidoTransportProtocol> GetTransportsAllowedByRP(
    const CtapGetAssertionRequest& request) {
  const base::flat_set<FidoTransportProtocol> kAllTransports = {
      FidoTransportProtocol::kInternal,
      FidoTransportProtocol::kNearFieldCommunication,
      FidoTransportProtocol::kUsbHumanInterfaceDevice,
      FidoTransportProtocol::kBluetoothLowEnergy,
      FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy};

  const auto& allowed_list = request.allow_list;
  if (allowed_list.empty()) {
    return kAllTransports;
  }

  base::flat_set<FidoTransportProtocol> transports;
  for (const auto credential : allowed_list) {
    if (credential.transports().empty())
      return kAllTransports;
    transports.insert(credential.transports().begin(),
                      credential.transports().end());
  }

  return transports;
}

base::flat_set<FidoTransportProtocol> GetTransportsAllowedAndConfiguredByRP(
    const CtapGetAssertionRequest& request) {
  auto transports = GetTransportsAllowedByRP(request);
  if (!request.cable_extension)
    transports.erase(FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy);
  return transports;
}

void ReportGetAssertionRequestTransport(FidoAuthenticator* authenticator) {
  if (authenticator->AuthenticatorTransport()) {
    base::UmaHistogramEnumeration(
        "WebAuthentication.GetAssertionRequestTransport",
        *authenticator->AuthenticatorTransport());
  }
}

void ReportGetAssertionResponseTransport(FidoAuthenticator* authenticator) {
  if (authenticator->AuthenticatorTransport()) {
    base::UmaHistogramEnumeration(
        "WebAuthentication.GetAssertionResponseTransport",
        *authenticator->AuthenticatorTransport());
  }
}

}  // namespace

GetAssertionRequestHandler::GetAssertionRequestHandler(
    service_manager::Connector* connector,
    FidoDiscoveryFactory* fido_discovery_factory,
    const base::flat_set<FidoTransportProtocol>& supported_transports,
    CtapGetAssertionRequest request,
    CompletionCallback completion_callback)
    : FidoRequestHandler(
          connector,
          fido_discovery_factory,
          base::STLSetIntersection<base::flat_set<FidoTransportProtocol>>(
              supported_transports,
              GetTransportsAllowedAndConfiguredByRP(request)),
          std::move(completion_callback)),
      request_(std::move(request)),
      weak_factory_(this) {
  transport_availability_info().request_type =
      FidoRequestHandlerBase::RequestType::kGetAssertion;
  transport_availability_info().has_empty_allow_list =
      request_.allow_list.empty();

  if (base::Contains(transport_availability_info().available_transports,
                     FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy)) {
    DCHECK(request_.cable_extension);
    auto discovery =
        fido_discovery_factory_->CreateCable(*request_.cable_extension);
    discovery->set_observer(this);
    discoveries().push_back(std::move(discovery));
  }

  if (request_.allow_list.empty()) {
    // Resident credential requests always involve user verification.
    request_.user_verification = UserVerificationRequirement::kRequired;
  }

  FIDO_LOG(EVENT) << "Starting GetAssertion flow";
  Start();
}

GetAssertionRequestHandler::~GetAssertionRequestHandler() = default;

void GetAssertionRequestHandler::DispatchRequest(
    FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  if (state_ != State::kWaitingForTouch) {
    FIDO_LOG(DEBUG) << "Not dispatching request to "
                    << authenticator->GetDisplayName()
                    << " because no longer waiting for touch";
    return;
  }

  if (!base::FeatureList::IsEnabled(device::kWebAuthPINSupport) &&
      !CheckUserVerificationCompatible(authenticator, request_, observer())) {
    // Don't flash authenticator without PIN support. This maintains
    // previous behaviour and avoids adding UI unprotected by a feature
    // flag without increasing the number of feature flags.
    FIDO_LOG(DEBUG) << "Not dispatching request to "
                    << authenticator->GetDisplayName()
                    << " because authenticator is not UV-compatible";
    return;
  }

  if (base::FeatureList::IsEnabled(device::kWebAuthPINSupport)) {
    switch (authenticator->WillNeedPINToGetAssertion(request_, observer())) {
      case FidoAuthenticator::GetAssertionPINDisposition::kUsePIN:
        // A PIN will be needed. Just request a touch to let the user select
        // this authenticator if they wish.
        FIDO_LOG(DEBUG) << "Asking for touch from "
                        << authenticator->GetDisplayName()
                        << " because a PIN will be required";
        authenticator->GetTouch(
            base::BindOnce(&GetAssertionRequestHandler::HandleTouch,
                           weak_factory_.GetWeakPtr(), authenticator));
        return;

      case FidoAuthenticator::GetAssertionPINDisposition::kUnsatisfiable:
        FIDO_LOG(DEBUG) << authenticator->GetDisplayName()
                        << " cannot satisfy assertion request. Requesting "
                           "touch in order to handle error case.";
        authenticator->GetTouch(base::BindOnce(
            &GetAssertionRequestHandler::HandleInapplicableAuthenticator,
            weak_factory_.GetWeakPtr(), authenticator));
        return;

      case FidoAuthenticator::GetAssertionPINDisposition::kNoPIN:
        break;
    }
  }

  CtapGetAssertionRequest request(request_);
  if (authenticator->Options()) {
    if (authenticator->Options()->user_verification_availability ==
            AuthenticatorSupportedOptions::UserVerificationAvailability::
                kSupportedAndConfigured &&
        request_.user_verification !=
            UserVerificationRequirement::kDiscouraged) {
      request.user_verification = UserVerificationRequirement::kRequired;
    } else {
      request.user_verification = UserVerificationRequirement::kDiscouraged;
    }
  }

  ReportGetAssertionRequestTransport(authenticator);

  FIDO_LOG(DEBUG) << "Asking for assertion from "
                  << authenticator->GetDisplayName();
  authenticator->GetAssertion(
      std::move(request),
      base::BindOnce(&GetAssertionRequestHandler::HandleResponse,
                     weak_factory_.GetWeakPtr(), authenticator));
}

void GetAssertionRequestHandler::AuthenticatorAdded(
    FidoDiscoveryBase* discovery,
    FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

#if defined(OS_MACOSX)
  if (authenticator->AuthenticatorTransport() ==
      FidoTransportProtocol::kInternal) {
    transport_availability_info().has_recognized_mac_touch_id_credential =
        static_cast<fido::mac::TouchIdAuthenticator*>(authenticator)
            ->HasCredentialForGetAssertionRequest(request_);
  }
#endif  // defined(OS_MACOSX)

  FidoRequestHandlerBase::AuthenticatorAdded(discovery, authenticator);
}

void GetAssertionRequestHandler::AuthenticatorRemoved(
    FidoDiscoveryBase* discovery,
    FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  FidoRequestHandlerBase::AuthenticatorRemoved(discovery, authenticator);

  if (authenticator == authenticator_) {
    authenticator_ = nullptr;
    if (state_ == State::kWaitingForPIN ||
        state_ == State::kWaitingForSecondTouch) {
      state_ = State::kFinished;
      std::move(completion_callback_)
          .Run(FidoReturnCode::kAuthenticatorRemovedDuringPINEntry,
               base::nullopt, nullptr);
    }
  }
}

void GetAssertionRequestHandler::HandleResponse(
    FidoAuthenticator* authenticator,
    CtapDeviceResponseCode status,
    base::Optional<AuthenticatorGetAssertionResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  if (state_ != State::kWaitingForTouch &&
      state_ != State::kWaitingForSecondTouch) {
    FIDO_LOG(DEBUG) << "Ignoring response from "
                    << authenticator->GetDisplayName()
                    << " because no longer waiting for touch";
    return;
  }

  // Requests that require a PIN should follow the |GetTouch| path initially.
  DCHECK(state_ == State::kWaitingForSecondTouch ||
         !base::FeatureList::IsEnabled(device::kWebAuthPINSupport) ||
         authenticator->WillNeedPINToGetAssertion(request_, observer()) ==
             FidoAuthenticator::GetAssertionPINDisposition::kNoPIN);

  const base::Optional<FidoReturnCode> maybe_result =
      ConvertDeviceResponseCodeToFidoReturnCode(status);
  if (!maybe_result) {
    if (state_ == State::kWaitingForSecondTouch) {
      OnAuthenticatorResponse(authenticator,
                              FidoReturnCode::kAuthenticatorResponseInvalid,
                              base::nullopt);
    } else {
      FIDO_LOG(ERROR) << "Ignoring status " << static_cast<int>(status)
                      << " from " << authenticator->GetDisplayName();
    }
    return;
  }

  state_ = State::kFinished;
  CancelActiveAuthenticators(authenticator->GetId());

  if (status != CtapDeviceResponseCode::kSuccess) {
    FIDO_LOG(ERROR) << "Failing assertion request due to status "
                    << static_cast<int>(status) << " from "
                    << authenticator->GetDisplayName();
    OnAuthenticatorResponse(authenticator, *maybe_result, base::nullopt);
    return;
  }

  if (!response || !ResponseValid(*authenticator, request_, *response)) {
    FIDO_LOG(ERROR) << "Failing assertion request due to bad response from "
                    << authenticator->GetDisplayName();
    OnAuthenticatorResponse(authenticator,
                            FidoReturnCode::kAuthenticatorResponseInvalid,
                            base::nullopt);
    return;
  }

  SetCredentialIdForResponseWithEmptyCredential(request_, *response);
  const size_t num_responses = response->num_credentials().value_or(1);
  if (num_responses == 0 ||
      (num_responses > 1 && !request_.allow_list.empty())) {
    OnAuthenticatorResponse(authenticator,
                            FidoReturnCode::kAuthenticatorResponseInvalid,
                            base::nullopt);
    return;
  }

  DCHECK(responses_.empty());
  responses_.emplace_back(std::move(*response));
  if (num_responses > 1) {
    // Multiple responses. Need to read them all.
    state_ = State::kReadingMultipleResponses;
    remaining_responses_ = num_responses - 1;
    authenticator->GetNextAssertion(
        base::BindOnce(&GetAssertionRequestHandler::HandleNextResponse,
                       weak_factory_.GetWeakPtr(), authenticator));
    return;
  }

  ReportGetAssertionResponseTransport(authenticator);

  OnAuthenticatorResponse(authenticator, FidoReturnCode::kSuccess,
                          std::move(responses_));
}

void GetAssertionRequestHandler::HandleNextResponse(
    FidoAuthenticator* authenticator,
    CtapDeviceResponseCode status,
    base::Optional<AuthenticatorGetAssertionResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK_EQ(State::kReadingMultipleResponses, state_);
  DCHECK_LT(0u, remaining_responses_);

  state_ = State::kFinished;
  if (status != CtapDeviceResponseCode::kSuccess) {
    FIDO_LOG(ERROR) << "Failing assertion request due to status "
                    << static_cast<int>(status) << " from "
                    << authenticator->GetDisplayName();
    OnAuthenticatorResponse(authenticator,
                            FidoReturnCode::kAuthenticatorResponseInvalid,
                            base::nullopt);
    return;
  }

  if (!ResponseValid(*authenticator, request_, *response)) {
    FIDO_LOG(ERROR) << "Failing assertion request due to bad response from "
                    << authenticator->GetDisplayName();
    OnAuthenticatorResponse(authenticator,
                            FidoReturnCode::kAuthenticatorResponseInvalid,
                            base::nullopt);
    return;
  }

  DCHECK(!responses_.empty());
  responses_.emplace_back(std::move(*response));
  remaining_responses_--;
  if (remaining_responses_ > 0) {
    state_ = State::kReadingMultipleResponses;
    authenticator->GetNextAssertion(
        base::BindOnce(&GetAssertionRequestHandler::HandleNextResponse,
                       weak_factory_.GetWeakPtr(), authenticator));
    return;
  }

  ReportGetAssertionResponseTransport(authenticator);

  OnAuthenticatorResponse(authenticator, FidoReturnCode::kSuccess,
                          std::move(responses_));
}

void GetAssertionRequestHandler::HandleTouch(FidoAuthenticator* authenticator) {
  if (state_ != State::kWaitingForTouch) {
    return;
  }

  DCHECK(base::FeatureList::IsEnabled(device::kWebAuthPINSupport) &&
         authenticator->WillNeedPINToGetAssertion(request_, observer()) !=
             FidoAuthenticator::GetAssertionPINDisposition::kNoPIN);

  DCHECK(observer());
  state_ = State::kGettingRetries;
  CancelActiveAuthenticators(authenticator->GetId());
  authenticator_ = authenticator;
  authenticator_->GetRetries(
      base::BindOnce(&GetAssertionRequestHandler::OnRetriesResponse,
                     weak_factory_.GetWeakPtr()));
}

void GetAssertionRequestHandler::HandleInapplicableAuthenticator(
    FidoAuthenticator* authenticator) {
  // User touched an authenticator that cannot handle this request.
  state_ = State::kFinished;
  CancelActiveAuthenticators(authenticator->GetId());
  std::move(completion_callback_)
      .Run(FidoReturnCode::kUserConsentButCredentialNotRecognized,
           base::nullopt, nullptr);
}

void GetAssertionRequestHandler::OnRetriesResponse(
    CtapDeviceResponseCode status,
    base::Optional<pin::RetriesResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK_EQ(state_, State::kGettingRetries);
  if (status != CtapDeviceResponseCode::kSuccess) {
    state_ = State::kFinished;
    std::move(completion_callback_)
        .Run(FidoReturnCode::kAuthenticatorResponseInvalid, base::nullopt,
             nullptr);
    return;
  }
  if (response->retries == 0) {
    state_ = State::kFinished;
    std::move(completion_callback_)
        .Run(FidoReturnCode::kHardPINBlock, base::nullopt, nullptr);
    return;
  }
  state_ = State::kWaitingForPIN;
  observer()->CollectPIN(response->retries,
                         base::BindOnce(&GetAssertionRequestHandler::OnHavePIN,
                                        weak_factory_.GetWeakPtr()));
}

void GetAssertionRequestHandler::OnHavePIN(std::string pin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK_EQ(State::kWaitingForPIN, state_);
  DCHECK(pin::IsValid(pin));

  if (authenticator_ == nullptr) {
    // Authenticator was detached. The request will already have been canceled
    // but this callback may have been waiting in a queue.
    DCHECK(!completion_callback_);
    return;
  }

  state_ = State::kGetEphemeralKey;
  authenticator_->GetEphemeralKey(
      base::BindOnce(&GetAssertionRequestHandler::OnHaveEphemeralKey,
                     weak_factory_.GetWeakPtr(), std::move(pin)));
}

void GetAssertionRequestHandler::OnHaveEphemeralKey(
    std::string pin,
    CtapDeviceResponseCode status,
    base::Optional<pin::KeyAgreementResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK_EQ(State::kGetEphemeralKey, state_);

  if (status != CtapDeviceResponseCode::kSuccess) {
    state_ = State::kFinished;
    std::move(completion_callback_)
        .Run(FidoReturnCode::kAuthenticatorResponseInvalid, base::nullopt,
             nullptr);
    return;
  }

  state_ = State::kRequestWithPIN;
  authenticator_->GetPINToken(
      std::move(pin), *response,
      base::BindOnce(&GetAssertionRequestHandler::OnHavePINToken,
                     weak_factory_.GetWeakPtr()));
}

void GetAssertionRequestHandler::OnHavePINToken(
    CtapDeviceResponseCode status,
    base::Optional<pin::TokenResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK_EQ(state_, State::kRequestWithPIN);

  if (status == CtapDeviceResponseCode::kCtap2ErrPinInvalid) {
    state_ = State::kGettingRetries;
    authenticator_->GetRetries(
        base::BindOnce(&GetAssertionRequestHandler::OnRetriesResponse,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  if (status != CtapDeviceResponseCode::kSuccess) {
    state_ = State::kFinished;
    FidoReturnCode ret;
    switch (status) {
      case CtapDeviceResponseCode::kCtap2ErrPinAuthBlocked:
        ret = FidoReturnCode::kSoftPINBlock;
        break;
      case CtapDeviceResponseCode::kCtap2ErrPinBlocked:
        ret = FidoReturnCode::kHardPINBlock;
        break;
      default:
        ret = FidoReturnCode::kAuthenticatorResponseInvalid;
        break;
    }
    std::move(completion_callback_).Run(ret, base::nullopt, nullptr);
    return;
  }

  observer()->FinishCollectPIN();
  state_ = State::kWaitingForSecondTouch;
  CtapGetAssertionRequest request(request_);
  request.pin_auth = response->PinAuth(request.client_data_hash);
  request.pin_protocol = pin::kProtocolVersion;
  // If doing a PIN operation then we don't ask the authenticator to also do
  // internal UV.
  request.user_verification = UserVerificationRequirement::kDiscouraged;

  ReportGetAssertionRequestTransport(authenticator_);

  authenticator_->GetAssertion(
      std::move(request),
      base::BindOnce(&GetAssertionRequestHandler::HandleResponse,
                     weak_factory_.GetWeakPtr(), authenticator_));
}

}  // namespace device
