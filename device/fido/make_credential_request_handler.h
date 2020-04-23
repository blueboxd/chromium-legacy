// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAKE_CREDENTIAL_REQUEST_HANDLER_H_
#define DEVICE_FIDO_MAKE_CREDENTIAL_REQUEST_HANDLER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/authenticator_selection_criteria.h"
#include "device/fido/bio/enroller.h"
#include "device/fido/client_data.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/pin.h"

namespace device {

class FidoAuthenticator;
class FidoDiscoveryFactory;

namespace pin {
struct EmptyResponse;
struct RetriesResponse;
class TokenResponse;
}  // namespace pin

enum class MakeCredentialStatus {
  kSuccess,
  kAuthenticatorResponseInvalid,
  kUserConsentButCredentialExcluded,
  kUserConsentDenied,
  kAuthenticatorRemovedDuringPINEntry,
  kSoftPINBlock,
  kHardPINBlock,
  kAuthenticatorMissingResidentKeys,
  // TODO(agl): kAuthenticatorMissingUserVerification can
  // also be returned when the authenticator supports UV, but
  // there's no UI support for collecting a PIN. This could
  // be clearer.
  kAuthenticatorMissingUserVerification,
  kStorageFull,
  kWinInvalidStateError,
  kWinNotAllowedError,
};

class COMPONENT_EXPORT(DEVICE_FIDO) MakeCredentialRequestHandler
    : public FidoRequestHandlerBase,
      public BioEnroller::Delegate {
 public:
  using CompletionCallback = base::OnceCallback<void(
      MakeCredentialStatus,
      base::Optional<AuthenticatorMakeCredentialResponse>,
      const FidoAuthenticator*)>;

  // Options contains higher-level request parameters that aren't part of the
  // makeCredential request itself, or that need to be combined with knowledge
  // of the specific authenticator, thus don't live in
  // |CtapMakeCredentialRequest|.
  struct COMPONENT_EXPORT(DEVICE_FIDO) Options {
    Options();
    ~Options();
    Options(const Options&);

    bool allow_skipping_pin_touch = false;
    base::Optional<AndroidClientDataExtensionInput> android_client_data_ext;

    // cred_protect_request extends |CredProtect| to include information that
    // applies at request-routing time. The second element is true if the
    // indicated protection level must be provided by the target authenticator
    // for the MakeCredential request to be sent.
    base::Optional<std::pair<CredProtectRequest, bool>> cred_protect_request;
  };

  MakeCredentialRequestHandler(
      FidoDiscoveryFactory* fido_discovery_factory,
      const base::flat_set<FidoTransportProtocol>& supported_transports,
      CtapMakeCredentialRequest request_parameter,
      AuthenticatorSelectionCriteria authenticator_criteria,
      const Options& options,
      CompletionCallback completion_callback);
  ~MakeCredentialRequestHandler() override;

 private:
  enum class State {
    kWaitingForTouch,
    kWaitingForSecondTouch,
    kGettingRetries,
    kWaitingForPIN,
    kWaitingForNewPIN,
    kSettingPIN,
    kRequestWithPIN,
    kBioEnrollment,
    kBioEnrollmentDone,
    kFinished,
  };

  // FidoRequestHandlerBase:
  void DispatchRequest(FidoAuthenticator* authenticator) override;
  void AuthenticatorRemoved(FidoDiscoveryBase* discovery,
                            FidoAuthenticator* authenticator) override;

  // BioEnroller::Delegate:
  void OnSampleCollected(BioEnrollmentSampleStatus status,
                         int samples_remaining) override;
  void OnEnrollmentDone(
      base::Optional<std::vector<uint8_t>> template_id) override;
  void OnEnrollmentError(CtapDeviceResponseCode status) override;

  void HandleResponse(
      FidoAuthenticator* authenticator,
      CtapDeviceResponseCode response_code,
      base::Optional<AuthenticatorMakeCredentialResponse> response);
  void CollectPINThenSendRequest(FidoAuthenticator* authenticator);
  void StartPINFallbackForInternalUv(FidoAuthenticator* authenticator);
  void SetPINThenSendRequest(FidoAuthenticator* authenticator);
  void HandleInternalUvLocked(FidoAuthenticator* authenticator);
  void HandleInapplicableAuthenticator(FidoAuthenticator* authenticator);
  void OnHavePIN(std::string pin);
  void OnRetriesResponse(CtapDeviceResponseCode status,
                         base::Optional<pin::RetriesResponse> response);
  void OnHaveSetPIN(std::string pin,
                    CtapDeviceResponseCode status,
                    base::Optional<pin::EmptyResponse> response);
  void OnHavePINToken(CtapDeviceResponseCode status,
                      base::Optional<pin::TokenResponse> response);
  void OnEnrollmentDismissed();
  void OnUvRetriesResponse(CtapDeviceResponseCode status,
                           base::Optional<pin::RetriesResponse> response);
  void OnHaveUvToken(FidoAuthenticator* authenticator,
                     CtapDeviceResponseCode status,
                     base::Optional<pin::TokenResponse> response);
  void DispatchRequestWithToken(pin::TokenResponse token);

  void SpecializeRequestForAuthenticator(
      CtapMakeCredentialRequest* request,
      const FidoAuthenticator* authenticator);

  CompletionCallback completion_callback_;
  State state_ = State::kWaitingForTouch;
  CtapMakeCredentialRequest request_;
  AuthenticatorSelectionCriteria authenticator_selection_criteria_;
  const Options options_;

  // authenticator_ points to the authenticator that will be used for this
  // operation. It's only set after the user touches an authenticator to select
  // it, after which point that authenticator will be used exclusively through
  // requesting PIN etc. The object is owned by the underlying discovery object
  // and this pointer is cleared if it's removed during processing.
  FidoAuthenticator* authenticator_ = nullptr;
  base::Optional<pin::TokenResponse> token_;
  std::unique_ptr<BioEnroller> bio_enroller_;
  SEQUENCE_CHECKER(my_sequence_checker_);
  base::WeakPtrFactory<MakeCredentialRequestHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MakeCredentialRequestHandler);
};

}  // namespace device

#endif  // DEVICE_FIDO_MAKE_CREDENTIAL_REQUEST_HANDLER_H_
