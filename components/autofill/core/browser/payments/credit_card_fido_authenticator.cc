// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/credit_card_fido_authenticator.h"

#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/containers/flat_set.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "third_party/blink/public/mojom/webauthn/internal_authenticator.mojom.h"
#include "url/gurl.h"

namespace autofill {

namespace {
// Default timeout for user to respond to WebAuthn prompt.
constexpr int kWebAuthnTimeoutMs = 3 * 60 * 1000;  // 3 minutes
// Timeout to wait for synchronous version of IsUserVerifiable().
constexpr int kIsUserVerifiableTimeoutMs = 1000;
constexpr char kGooglePaymentsRpid[] = "google.com";
constexpr char kGooglePaymentsRpName[] = "Google Payments";

std::vector<uint8_t> Base64ToBytes(std::string base64) {
  std::string bytes;
  bool did_succeed = base::Base64Decode(base::StringPiece(base64), &bytes);
  if (did_succeed) {
    return std::vector<uint8_t>(bytes.begin(), bytes.end());
  }
  return std::vector<uint8_t>{};
}

base::Value BytesToBase64(const std::vector<uint8_t> bytes) {
  std::string base64;
  base::Base64Encode(std::string(bytes.begin(), bytes.end()), &base64);
  return base::Value(std::move(base64));
}
}  // namespace

CreditCardFIDOAuthenticator::CreditCardFIDOAuthenticator(AutofillDriver* driver,
                                                         AutofillClient* client)
    : autofill_driver_(driver),
      autofill_client_(client),
      payments_client_(client->GetPaymentsClient()),
      user_is_verifiable_callback_received_(
          base::WaitableEvent::ResetPolicy::AUTOMATIC,
          base::WaitableEvent::InitialState::NOT_SIGNALED) {}

CreditCardFIDOAuthenticator::~CreditCardFIDOAuthenticator() {}

void CreditCardFIDOAuthenticator::ShowWebauthnOfferDialog() {
  autofill_client_->ShowWebauthnOfferDialog(base::BindRepeating(
      &CreditCardFIDOAuthenticator::OnWebauthnOfferDialogUserResponse,
      weak_ptr_factory_.GetWeakPtr()));
}

void CreditCardFIDOAuthenticator::Authenticate(
    const CreditCard* card,
    base::WeakPtr<Requester> requester,
    base::TimeTicks form_parsed_timestamp,
    base::Value request_options) {
  card_ = card;
  requester_ = requester;
  form_parsed_timestamp_ = form_parsed_timestamp;

  if (card_ && IsValidRequestOptions(request_options.Clone())) {
    current_flow_ = AUTHENTICATION_FLOW;
    GetAssertion(ParseRequestOptions(std::move(request_options)));
  } else {
    requester_->OnFIDOAuthenticationComplete(/*did_succeed=*/false);
  }
}

void CreditCardFIDOAuthenticator::Register(base::Value creation_options) {
  // If |creation_options| is set, then must enroll a new credential. Otherwise
  // directly send request to payments for opting in.
  if (creation_options.is_dict()) {
    if (IsValidCreationOptions(creation_options)) {
      current_flow_ = OPT_IN_WITH_CHALLENGE_FLOW;
      MakeCredential(ParseCreationOptions(creation_options));
    }
  } else {
    current_flow_ = OPT_IN_WITHOUT_CHALLENGE_FLOW;
    OptChange(/*opt_in=*/true);
  }
}

void CreditCardFIDOAuthenticator::OptOut() {
  current_flow_ = OPT_OUT_FLOW;
  OptChange(/*opt_in=*/false);
}

void CreditCardFIDOAuthenticator::IsUserVerifiable(
    base::OnceCallback<void(bool)> callback) {
  if (base::FeatureList::IsEnabled(
          features::kAutofillCreditCardAuthentication)) {
    autofill_driver_->ConnectToAuthenticator(
        authenticator_.BindNewPipeAndPassReceiver());
    authenticator_->IsUserVerifyingPlatformAuthenticatorAvailable(
        std::move(callback));
  } else {
    std::move(callback).Run(false);
  }
}

bool CreditCardFIDOAuthenticator::IsUserVerifiable() {
  if (user_is_verifiable_.has_value())
    return user_is_verifiable_.value();

  IsUserVerifiable(
      base::BindOnce(&CreditCardFIDOAuthenticator::SetUserIsVerifiable,
                     weak_ptr_factory_.GetWeakPtr()));

  user_is_verifiable_callback_received_.declare_only_used_while_idle();
  user_is_verifiable_callback_received_.TimedWait(
      base::TimeDelta::FromMilliseconds(kIsUserVerifiableTimeoutMs));

  return user_is_verifiable_.value_or(false);
}

bool CreditCardFIDOAuthenticator::IsUserOptedIn() {
  return base::FeatureList::IsEnabled(
             features::kAutofillCreditCardAuthentication) &&
         ::autofill::prefs::IsCreditCardFIDOAuthEnabled(
             autofill_client_->GetPrefs());
}

void CreditCardFIDOAuthenticator::SyncUserOptIn(
    AutofillClient::UnmaskDetails& unmask_details) {
  bool is_user_opted_in = IsUserOptedIn();

  // If payments is offering to opt-in, then that means user is not opted in.
  if (unmask_details.offer_fido_opt_in) {
    is_user_opted_in = false;
  }

  // If payments is requesting a FIDO auth, then that means user is opted in.
  if (unmask_details.unmask_auth_method ==
      AutofillClient::UnmaskAuthMethod::FIDO) {
    is_user_opted_in = true;
  }

  // Update pref setting if needed.
  ::autofill::prefs::SetCreditCardFIDOAuthEnabled(autofill_client_->GetPrefs(),
                                                  is_user_opted_in);
}

void CreditCardFIDOAuthenticator::GetAssertion(
    PublicKeyCredentialRequestOptionsPtr request_options) {
  autofill_driver_->ConnectToAuthenticator(
      authenticator_.BindNewPipeAndPassReceiver());
  authenticator_->GetAssertion(
      std::move(request_options),
      base::BindOnce(&CreditCardFIDOAuthenticator::OnDidGetAssertion,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CreditCardFIDOAuthenticator::MakeCredential(
    PublicKeyCredentialCreationOptionsPtr creation_options) {
  autofill_driver_->ConnectToAuthenticator(
      authenticator_.BindNewPipeAndPassReceiver());
#if !defined(OS_ANDROID)
  // On desktop, close the WebAuthn offer dialog and get ready to show the OS
  // level authentication dialog. If dialog is already closed, then the offer
  // was declined during the fetching challenge process, and thus returned
  // early.
  if (!autofill_client_->CloseWebauthnOfferDialog()) {
    current_flow_ = NONE_FLOW;
    return;
  }
#endif
  authenticator_->MakeCredential(
      std::move(creation_options),
      base::BindOnce(&CreditCardFIDOAuthenticator::OnDidMakeCredential,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CreditCardFIDOAuthenticator::OptChange(bool opt_in,
                                            base::Value attestation_response) {
  payments::PaymentsClient::OptChangeRequestDetails request_details;
  request_details.app_locale =
      autofill_client_->GetPersonalDataManager()->app_locale();
  request_details.opt_in = opt_in;
  if (attestation_response.is_dict()) {
    request_details.fido_authenticator_response =
        std::move(attestation_response);
  }
  payments_client_->OptChange(
      request_details,
      base::BindOnce(&CreditCardFIDOAuthenticator::OnDidGetOptChangeResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CreditCardFIDOAuthenticator::OnDidGetAssertion(
    AuthenticatorStatus status,
    GetAssertionAuthenticatorResponsePtr assertion_response) {
  // End the flow if there was an authentication error.
  if (status != AuthenticatorStatus::SUCCESS) {
    current_flow_ = NONE_FLOW;
    requester_->OnFIDOAuthenticationComplete(/*did_succeed=*/false);
    return;
  }

  base::Value response = ParseAssertionResponse(std::move(assertion_response));
  full_card_request_.reset(new payments::FullCardRequest(
      autofill_client_, autofill_client_->GetPaymentsClient(),
      autofill_client_->GetPersonalDataManager(), form_parsed_timestamp_));
  full_card_request_->GetFullCardViaFIDO(
      *card_, AutofillClient::UNMASK_FOR_AUTOFILL,
      weak_ptr_factory_.GetWeakPtr(), std::move(response));
}

void CreditCardFIDOAuthenticator::OnDidMakeCredential(
    AuthenticatorStatus status,
    MakeCredentialAuthenticatorResponsePtr attestation_response) {
  // End the flow if there was an authentication error.
  if (status != AuthenticatorStatus::SUCCESS) {
    current_flow_ = NONE_FLOW;
    return;
  }

  OptChange(/*opt_in=*/true,
            ParseAttestationResponse(std::move(attestation_response)));
}

void CreditCardFIDOAuthenticator::OnDidGetOptChangeResult(
    AutofillClient::PaymentsRpcResult result,
    payments::PaymentsClient::OptChangeResponseDetails& response) {
  DCHECK(current_flow_ == OPT_IN_WITHOUT_CHALLENGE_FLOW ||
         current_flow_ == OPT_OUT_FLOW ||
         current_flow_ == OPT_IN_WITH_CHALLENGE_FLOW);
  // End the flow if the server responded with an error.
  if (result != AutofillClient::PaymentsRpcResult::SUCCESS) {
    current_flow_ = NONE_FLOW;
    autofill_client_->UpdateWebauthnOfferDialogWithError();
    return;
  }

  // Update user preference to keep in sync with server.
  if (response.user_is_opted_in.has_value()) {
    ::autofill::prefs::SetCreditCardFIDOAuthEnabled(
        autofill_client_->GetPrefs(), response.user_is_opted_in.value());
  }

  // If response contains |creation_options| and the last opt-in attempt did not
  // include a challenge, then invoke WebAuthn registration prompt. Otherwise
  // end the flow.
  if (response.fido_creation_options.has_value() &&
      current_flow_ == OPT_IN_WITHOUT_CHALLENGE_FLOW) {
    Register(std::move(response.fido_creation_options.value()));
  } else {
    current_flow_ = NONE_FLOW;
  }
}

void CreditCardFIDOAuthenticator::OnWebauthnOfferDialogUserResponse(
    bool did_accept) {
  if (did_accept) {
    Register();
  } else {
    payments_client_->CancelRequest();
    current_flow_ = NONE_FLOW;
  }
}

void CreditCardFIDOAuthenticator::OnFullCardRequestSucceeded(
    const payments::FullCardRequest& full_card_request,
    const CreditCard& card,
    const base::string16& cvc) {
  DCHECK_EQ(AUTHENTICATION_FLOW, current_flow_);
  current_flow_ = NONE_FLOW;
  requester_->OnFIDOAuthenticationComplete(/*did_succeed=*/true, &card);
}

void CreditCardFIDOAuthenticator::OnFullCardRequestFailed() {
  DCHECK_EQ(AUTHENTICATION_FLOW, current_flow_);
  current_flow_ = NONE_FLOW;
  requester_->OnFIDOAuthenticationComplete(/*did_succeed=*/false);
}

PublicKeyCredentialRequestOptionsPtr
CreditCardFIDOAuthenticator::ParseRequestOptions(
    const base::Value& request_options) {
  auto options = PublicKeyCredentialRequestOptions::New();

  const auto* rpid = request_options.FindStringKey("relying_party_id");
  options->relying_party_id = rpid ? *rpid : std::string(kGooglePaymentsRpid);

  const auto* challenge = request_options.FindStringKey("challenge");
  DCHECK(challenge);
  options->challenge = Base64ToBytes(*challenge);

  const auto* timeout = request_options.FindKeyOfType(
      "timeout_millis", base::Value::Type::INTEGER);
  options->adjusted_timeout = base::TimeDelta::FromMilliseconds(
      timeout ? timeout->GetInt() : kWebAuthnTimeoutMs);

  options->user_verification = UserVerificationRequirement::kRequired;

  const auto* key_info_list =
      request_options.FindKeyOfType("key_info", base::Value::Type::LIST);
  DCHECK(key_info_list);
  for (const base::Value& key_info : key_info_list->GetList()) {
    options->allow_credentials.push_back(ParseCredentialDescriptor(key_info));
  }

  return options;
}

PublicKeyCredentialCreationOptionsPtr
CreditCardFIDOAuthenticator::ParseCreationOptions(
    const base::Value& creation_options) {
  auto options = PublicKeyCredentialCreationOptions::New();

  const auto* rpid = creation_options.FindStringKey("relying_party_id");
  options->relying_party.id = rpid ? *rpid : kGooglePaymentsRpid;

  const auto* relying_party_name =
      creation_options.FindStringKey("relying_party_name");
  options->relying_party.name =
      relying_party_name ? *relying_party_name : kGooglePaymentsRpName;

  const auto* icon_url = creation_options.FindStringKey("icon_url");
  if (icon_url)
    options->relying_party.icon_url = GURL(*icon_url);

  const std::string gaia =
      autofill_client_->GetIdentityManager()->GetPrimaryAccountInfo().gaia;
  options->user.id = std::vector<uint8_t>(gaia.begin(), gaia.end());
  options->user.name =
      autofill_client_->GetIdentityManager()->GetPrimaryAccountInfo().email;

  base::Optional<AccountInfo> account_info =
      autofill_client_->GetIdentityManager()
          ->FindExtendedAccountInfoForAccountWithRefreshToken(
              autofill_client_->GetPersonalDataManager()
                  ->GetAccountInfoForPaymentsServer());
  if (account_info.has_value()) {
    options->user.display_name = account_info.value().given_name;
    options->user.icon_url = GURL(account_info.value().picture_url);
  } else {
    options->user.display_name = "";
  }

  const auto* challenge = creation_options.FindStringKey("challenge");
  DCHECK(challenge);
  options->challenge = Base64ToBytes(*challenge);

  const auto* identifier_list = creation_options.FindKeyOfType(
      "algorithm_identifier", base::Value::Type::LIST);
  if (identifier_list) {
    for (const base::Value& algorithm_identifier : identifier_list->GetList()) {
      device::PublicKeyCredentialParams::CredentialInfo parameter;
      parameter.type = device::CredentialType::kPublicKey;
      parameter.algorithm = algorithm_identifier.GetInt();
      options->public_key_parameters.push_back(parameter);
    }
  }

  const auto* timeout = creation_options.FindKeyOfType(
      "timeout_millis", base::Value::Type::INTEGER);
  options->adjusted_timeout = base::TimeDelta::FromMilliseconds(
      timeout ? timeout->GetInt() : kWebAuthnTimeoutMs);

  // List of keys that Payments already knows about, and so should not make a
  // new credential.
  const auto* excluded_keys_list =
      creation_options.FindKeyOfType("key_info", base::Value::Type::LIST);
  if (excluded_keys_list) {
    for (const base::Value& key_info : excluded_keys_list->GetList()) {
      options->exclude_credentials.push_back(
          ParseCredentialDescriptor(key_info));
    }
  }

  return options;
}

PublicKeyCredentialDescriptor
CreditCardFIDOAuthenticator::ParseCredentialDescriptor(
    const base::Value& key_info) {
  std::vector<uint8_t> credential_id;
  const auto* id = key_info.FindStringKey("credential_id");
  DCHECK(id);
  credential_id = Base64ToBytes(*id);

  base::flat_set<FidoTransportProtocol> authenticator_transports;
  const auto* transports = key_info.FindKeyOfType(
      "authenticator_transport_support", base::Value::Type::LIST);
  if (transports && !transports->GetList().empty()) {
    for (const base::Value& transport_type : transports->GetList()) {
      base::Optional<FidoTransportProtocol> protocol =
          device::ConvertToFidoTransportProtocol(
              base::ToLowerASCII(transport_type.GetString()));
      if (protocol.has_value())
        authenticator_transports.insert(*protocol);
    }
  }

  return PublicKeyCredentialDescriptor(CredentialType::kPublicKey,
                                       credential_id, authenticator_transports);
}

base::Value CreditCardFIDOAuthenticator::ParseAssertionResponse(
    GetAssertionAuthenticatorResponsePtr assertion_response) {
  base::Value response = base::Value(base::Value::Type::DICTIONARY);
  response.SetKey("credential_id", base::Value(assertion_response->info->id));
  response.SetKey("authenticator_data",
                  BytesToBase64(assertion_response->authenticator_data));
  response.SetKey("client_data",
                  BytesToBase64(assertion_response->info->client_data_json));
  response.SetKey("signature", BytesToBase64(assertion_response->signature));
  return response;
}

base::Value CreditCardFIDOAuthenticator::ParseAttestationResponse(
    MakeCredentialAuthenticatorResponsePtr attestation_response) {
  base::Value response = base::Value(base::Value::Type::DICTIONARY);

  base::Value fido_attestation_info =
      base::Value(base::Value::Type::DICTIONARY);
  fido_attestation_info.SetKey(
      "client_data",
      BytesToBase64(attestation_response->info->client_data_json));
  fido_attestation_info.SetKey(
      "attestation_object",
      BytesToBase64(attestation_response->attestation_object));

  base::Value authenticator_transport_list =
      base::Value(base::Value::Type::LIST);
  for (FidoTransportProtocol protocol : attestation_response->transports) {
    authenticator_transport_list.Append(
        base::Value(base::ToUpperASCII(device::ToString(protocol))));
  }

  response.SetKey("fido_attestation_info", std::move(fido_attestation_info));
  response.SetKey("authenticator_transport",
                  std::move(authenticator_transport_list));

  return response;
}

bool CreditCardFIDOAuthenticator::IsValidRequestOptions(
    const base::Value& request_options) {
  if (!request_options.is_dict() || request_options.DictEmpty() ||
      !request_options.FindStringKey("challenge") ||
      !request_options.FindKeyOfType("key_info", base::Value::Type::LIST)) {
    return false;
  }

  const auto* key_info_list =
      request_options.FindKeyOfType("key_info", base::Value::Type::LIST);

  if (key_info_list->GetList().empty())
    return false;

  for (const base::Value& key_info : key_info_list->GetList()) {
    if (!key_info.is_dict() || !key_info.FindStringKey("credential_id"))
      return false;
  }

  return true;
}

bool CreditCardFIDOAuthenticator::IsValidCreationOptions(
    const base::Value& creation_options) {
  return creation_options.is_dict() &&
         creation_options.FindStringKey("challenge");
}

void CreditCardFIDOAuthenticator::SetUserIsVerifiable(bool user_is_verifiable) {
  user_is_verifiable_ = user_is_verifiable;
  user_is_verifiable_callback_received_.Signal();
}

}  // namespace autofill
