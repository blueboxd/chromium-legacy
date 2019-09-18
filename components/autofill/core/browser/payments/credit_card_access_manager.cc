// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/credit_card_access_manager.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/guid.h"
#include "base/strings/string16.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/credit_card_form_event_logger.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {
// Timeout to wait for unmask details from Google Payments in milliseconds.
constexpr int64_t kUnmaskDetailsResponseTimeout = 1000;
// Time to wait between multiple calls to GetUnmaskDetails().
constexpr int64_t kDelayForGetUnmaskDetails = 3 * 60 * 1000;  // 3 min

// Used for asynchronously waiting for |event| to be signaled.
bool WaitForEvent(base::WaitableEvent* event) {
  event->declare_only_used_while_idle();
  return event->TimedWait(
      base::TimeDelta::FromMilliseconds(kUnmaskDetailsResponseTimeout));
}

// Used with PostTaskWithDelay() to signal event after a timeout.
void SignalEvent(base::WaitableEvent* event) {
  event->Signal();
}
}  // namespace

CreditCardAccessManager::CreditCardAccessManager(
    AutofillDriver* driver,
    AutofillManager* autofill_manager)
    : CreditCardAccessManager(
          driver,
          autofill_manager->client(),
          autofill_manager->client()->GetPersonalDataManager()) {}

CreditCardAccessManager::CreditCardAccessManager(
    AutofillDriver* driver,
    AutofillClient* client,
    PersonalDataManager* personal_data_manager,
    CreditCardFormEventLogger* form_event_logger)
    : driver_(driver),
      client_(client),
      payments_client_(client_->GetPaymentsClient()),
      personal_data_manager_(personal_data_manager),
      form_event_logger_(form_event_logger),
      ready_to_start_authentication_(
          base::WaitableEvent::ResetPolicy::AUTOMATIC,
          base::WaitableEvent::InitialState::NOT_SIGNALED),
      can_fetch_unmask_details_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                                base::WaitableEvent::InitialState::SIGNALED) {}

CreditCardAccessManager::~CreditCardAccessManager() {}

void CreditCardAccessManager::UpdateCreditCardFormEventLogger() {
  std::vector<CreditCard*> credit_cards = GetCreditCards();

  if (form_event_logger_) {
    size_t server_record_type_count = 0;
    size_t local_record_type_count = 0;
    for (CreditCard* credit_card : credit_cards) {
      if (credit_card->record_type() == CreditCard::LOCAL_CARD)
        local_record_type_count++;
      else
        server_record_type_count++;
    }
    form_event_logger_->set_server_record_type_count(server_record_type_count);
    form_event_logger_->set_local_record_type_count(local_record_type_count);
    form_event_logger_->set_is_context_secure(client_->IsContextSecure());
  }
}

std::vector<CreditCard*> CreditCardAccessManager::GetCreditCards() {
  return personal_data_manager_->GetCreditCards();
}

std::vector<CreditCard*> CreditCardAccessManager::GetCreditCardsToSuggest() {
  const std::vector<CreditCard*> cards_to_suggest =
      personal_data_manager_->GetCreditCardsToSuggest(
          client_->AreServerCardsSupported());

  for (const CreditCard* credit_card : cards_to_suggest) {
    if (form_event_logger_ && !credit_card->bank_name().empty()) {
      form_event_logger_->SetBankNameAvailable();
      break;
    }
  }

  return cards_to_suggest;
}

bool CreditCardAccessManager::ShouldDisplayGPayLogo() {
  for (const CreditCard* credit_card : GetCreditCardsToSuggest()) {
    if (IsLocalCard(credit_card))
      return false;
  }
  return true;
}

bool CreditCardAccessManager::ServerCardsAvailable() {
  for (const CreditCard* credit_card : GetCreditCardsToSuggest()) {
    if (!IsLocalCard(credit_card))
      return true;
  }
  return false;
}

bool CreditCardAccessManager::DeleteCard(const CreditCard* card) {
  // Server cards cannot be deleted from within Chrome.
  bool allowed_to_delete = IsLocalCard(card);

  if (allowed_to_delete)
    personal_data_manager_->DeleteLocalCreditCards({*card});

  return allowed_to_delete;
}

bool CreditCardAccessManager::GetDeletionConfirmationText(
    const CreditCard* card,
    base::string16* title,
    base::string16* body) {
  if (!IsLocalCard(card))
    return false;

  if (title)
    title->assign(card->NetworkOrBankNameAndLastFourDigits());
  if (body) {
    body->assign(l10n_util::GetStringUTF16(
        IDS_AUTOFILL_DELETE_CREDIT_CARD_SUGGESTION_CONFIRMATION_BODY));
  }

  return true;
}

bool CreditCardAccessManager::ShouldClearPreviewedForm() {
  return !is_authentication_in_progress_;
}

CreditCard* CreditCardAccessManager::GetCreditCard(std::string guid) {
  if (base::IsValidGUID(guid)) {
    return personal_data_manager_->GetCreditCardByGUID(guid);
  }
  return nullptr;
}

void CreditCardAccessManager::PrepareToFetchCreditCard() {
#if !defined(OS_IOS)
  // No need to fetch details if there are no server cards.
  if (!ServerCardsAvailable())
    return;

  // Do not make an unnecessary preflight call unless signaled.
  if (!can_fetch_unmask_details_.IsSignaled())
    return;

  // Reset in case a late response was ignored.
  ready_to_start_authentication_.Reset();

  // If |is_user_verifiable_| is set, then directly call
  // GetUnmaskDetailsIfUserIsVerifiable(), otherwise fetch value for
  // |is_user_verifiable_|.
  if (is_user_verifiable_.has_value()) {
    GetUnmaskDetailsIfUserIsVerifiable(is_user_verifiable_.value());
  } else {
    GetOrCreateFIDOAuthenticator()->IsUserVerifiable(base::BindOnce(
        &CreditCardAccessManager::GetUnmaskDetailsIfUserIsVerifiable,
        weak_ptr_factory_.GetWeakPtr()));
  }
#endif
}

void CreditCardAccessManager::GetUnmaskDetailsIfUserIsVerifiable(
    bool is_user_verifiable) {
  is_user_verifiable_ = is_user_verifiable;

  // If user is verifiable, then make preflight call to payments to fetch unmask
  // details, otherwise the only option is to perform CVC Auth, which does not
  // require any. Do nothing if request is already in progress.
  if (is_user_verifiable_ && !unmask_details_request_in_progress_) {
    unmask_details_request_in_progress_ = true;
    payments_client_->GetUnmaskDetails(
        base::BindOnce(&CreditCardAccessManager::OnDidGetUnmaskDetails,
                       weak_ptr_factory_.GetWeakPtr()),
        personal_data_manager_->app_locale());
  }
}

void CreditCardAccessManager::OnDidGetUnmaskDetails(
    AutofillClient::PaymentsRpcResult result,
    AutofillClient::UnmaskDetails& unmask_details) {
  unmask_details_request_in_progress_ = false;
  unmask_details_.offer_fido_opt_in = unmask_details.offer_fido_opt_in;
  unmask_details_.unmask_auth_method = unmask_details.unmask_auth_method;
  unmask_details_.fido_request_options =
      std::move(unmask_details.fido_request_options);
  unmask_details_.fido_eligible_card_ids =
      unmask_details.fido_eligible_card_ids;

  // Set delay as fido request timeout if available, otherwise set to default.
  int delay_ms = kDelayForGetUnmaskDetails;
  if (unmask_details_.fido_request_options.is_dict()) {
    const auto* request_timeout =
        unmask_details_.fido_request_options.FindKeyOfType(
            "timeout_millis", base::Value::Type::INTEGER);
    if (request_timeout)
      delay_ms = request_timeout->GetInt();
  }

#if !defined(OS_IOS)
  GetOrCreateFIDOAuthenticator()->SyncUserOptIn(unmask_details);
#endif
  ready_to_start_authentication_.Signal();

  base::PostDelayedTask(
      FROM_HERE, base::BindOnce(&SignalEvent, &can_fetch_unmask_details_),
      base::TimeDelta::FromMilliseconds(delay_ms));
}

void CreditCardAccessManager::FetchCreditCard(
    const CreditCard* card,
    base::WeakPtr<Accessor> accessor,
    const base::TimeTicks& form_parsed_timestamp) {
  if (is_authentication_in_progress_ || !card) {
    accessor->OnCreditCardFetched(/*did_succeed=*/false, nullptr);
    return;
  }

  if (card->record_type() != CreditCard::MASKED_SERVER_CARD) {
    accessor->OnCreditCardFetched(/*did_succeed=*/true, card);
    return;
  }

  card_ = card;
  accessor_ = accessor;
  form_parsed_timestamp_ = form_parsed_timestamp;
  is_authentication_in_progress_ = true;

  if (AuthenticationRequiresUnmaskDetails()) {
    // Wait for |ready_to_start_authentication_| to be signaled by
    // OnDidGetUnmaskDetails() or until timeout before calling Authenticate().
    base::PostTaskAndReplyWithResult(
        FROM_HERE, {base::ThreadPool(), base::MayBlock()},
        base::BindOnce(&WaitForEvent, &ready_to_start_authentication_),
        base::BindOnce(&CreditCardAccessManager::Authenticate,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    Authenticate();
  }
}

void CreditCardAccessManager::FIDOAuthOptChange(bool opt_in,
                                                base::Value creation_options) {
#if defined(OS_IOS)
  return;
#else
  if (opt_in) {
    GetOrCreateFIDOAuthenticator()->Register(
        /*card_authorization_token=*/std::string(),
        std::move(creation_options));
  } else {
    GetOrCreateFIDOAuthenticator()->OptOut();
  }
#endif
}

void CreditCardAccessManager::Authenticate(bool did_get_unmask_details) {
  // Reset now that we have started authentication.
  ready_to_start_authentication_.Reset();
  unmask_details_request_in_progress_ = false;

  // Do not use FIDO if card is not listed in unmask details, as each Card must
  // be CVC authed at least once per device.
  bool card_is_eligible_for_fido =
      did_get_unmask_details &&
      unmask_details_.unmask_auth_method ==
          AutofillClient::UnmaskAuthMethod::FIDO &&
      unmask_details_.fido_eligible_card_ids.find(card_->server_id()) !=
          unmask_details_.fido_eligible_card_ids.end();

  if (card_is_eligible_for_fido) {
#if defined(OS_IOS)
    NOTREACHED();
#else
    DCHECK(unmask_details_.fido_request_options.is_dict());
    GetOrCreateFIDOAuthenticator()->Authenticate(
        card_, weak_ptr_factory_.GetWeakPtr(), form_parsed_timestamp_,
        std::move(unmask_details_.fido_request_options));
#endif
  } else {
    GetOrCreateCVCAuthenticator()->Authenticate(
        card_, weak_ptr_factory_.GetWeakPtr(), personal_data_manager_,
        form_parsed_timestamp_);
  }
}

CreditCardCVCAuthenticator*
CreditCardAccessManager::GetOrCreateCVCAuthenticator() {
  if (!cvc_authenticator_)
    cvc_authenticator_ = std::make_unique<CreditCardCVCAuthenticator>(client_);
  return cvc_authenticator_.get();
}

#if !defined(OS_IOS)
CreditCardFIDOAuthenticator*
CreditCardAccessManager::GetOrCreateFIDOAuthenticator() {
  if (!fido_authenticator_)
    fido_authenticator_ =
        std::make_unique<CreditCardFIDOAuthenticator>(driver_, client_);
  return fido_authenticator_.get();
}
#endif

void CreditCardAccessManager::OnCVCAuthenticationComplete(
    const CreditCardCVCAuthenticator::CVCAuthenticationResponse& response) {
  is_authentication_in_progress_ = false;
  accessor_->OnCreditCardFetched(response.did_succeed, response.card,
                                 response.cvc);
  can_fetch_unmask_details_.Signal();

  if (!response.did_succeed || response.card_authorization_token.empty())
    return;

#if defined(OS_ANDROID)
  // Opt-in was already offered at this point for Android.
  bool should_offer_fido_auth = false;
#elif !defined(OS_IOS)
  bool should_offer_fido_auth = unmask_details_.offer_fido_opt_in;
#endif

#if !defined(OS_IOS)
  // Now that unmask flow is complete and form is filled, the remaining flows
  // will be completely handed over to CreditCardFIDOAuthenticator.
  // If the GetRealPan response includes |creation_options| or
  // |request_options|, that means the user showed intention to opt-in while
  // unmasking (this can only happen on Android) and must complete the challenge
  // before successfully opting-in. If UnmaskDetails contained
  // |request_options|, that means the user is already opted-into FIDO auth, and
  // this is the first time use of a new card, and must complete the challenge
  // to successfully authorize the card. Otherwise, if on desktop and eligible,
  // show the dialog that offers opting-in to FIDO authentication in the future.
  if (response.creation_options.has_value()) {
    DCHECK(response.creation_options->is_dict());
    GetOrCreateFIDOAuthenticator()->Register(
        response.card_authorization_token, response.creation_options->Clone());
  } else if (response.request_options.has_value()) {
    DCHECK(response.request_options->is_dict());
    GetOrCreateFIDOAuthenticator()->Authorize(
        response.card_authorization_token, response.request_options->Clone());
  } else if (should_offer_fido_auth) {
    GetOrCreateFIDOAuthenticator()->ShowWebauthnOfferDialog(
        response.card_authorization_token);
  } else if (unmask_details_.fido_request_options.is_dict()) {
    GetOrCreateFIDOAuthenticator()->Authorize(
        response.card_authorization_token,
        std::move(unmask_details_.fido_request_options));
  }
#endif
}

#if !defined(OS_IOS)
void CreditCardAccessManager::OnFIDOAuthenticationComplete(
    bool did_succeed,
    const CreditCard* card) {
  if (did_succeed) {
    is_authentication_in_progress_ = false;
    accessor_->OnCreditCardFetched(did_succeed, card);
    can_fetch_unmask_details_.Signal();
  } else {
    // Fall back to CVC if WebAuthn failed.
    // TODO(crbug/949269): Add metrics to log fallback CVC auths.
    GetOrCreateCVCAuthenticator()->Authenticate(
        card_, weak_ptr_factory_.GetWeakPtr(), personal_data_manager_,
        form_parsed_timestamp_);
  }
}
#endif

bool CreditCardAccessManager::AuthenticationRequiresUnmaskDetails() {
#if defined(OS_IOS)
  return false;
#else
  return is_user_verifiable_.value_or(false) &&
         GetOrCreateFIDOAuthenticator()->IsUserOptedIn();
#endif
}

bool CreditCardAccessManager::IsLocalCard(const CreditCard* card) {
  return card && card->record_type() == CreditCard::LOCAL_CARD;
}

}  // namespace autofill
