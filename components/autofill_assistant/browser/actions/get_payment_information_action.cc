// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/get_payment_information_action.h"

#include <algorithm>
#include <utility>

#include "base/android/locale_utils.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/geo/address_i18n.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_memory.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/website_login_fetcher_impl.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "third_party/libaddressinput/chromium/addressinput_util.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_assistant {

GetPaymentInformationAction::LoginDetails::LoginDetails(
    bool _choose_automatically_if_no_other_options,
    const std::string& _payload,
    const WebsiteLoginFetcher::Login& _login)
    : choose_automatically_if_no_other_options(
          _choose_automatically_if_no_other_options),
      payload(_payload),
      login(_login) {}

GetPaymentInformationAction::LoginDetails::LoginDetails(
    bool _choose_automatically_if_no_other_options,
    const std::string& _payload)
    : choose_automatically_if_no_other_options(
          _choose_automatically_if_no_other_options),
      payload(_payload) {}

GetPaymentInformationAction::LoginDetails::~LoginDetails() = default;

GetPaymentInformationAction::GetPaymentInformationAction(
    ActionDelegate* delegate,
    const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_get_payment_information());
}

GetPaymentInformationAction::~GetPaymentInformationAction() {
  delegate_->GetPersonalDataManager()->RemoveObserver(this);

  // Report UMA histograms.
  if (shown_to_user_) {
    Metrics::RecordPaymentRequestPrefilledSuccess(initially_prefilled,
                                                  action_successful_);
    Metrics::RecordPaymentRequestAutofillChanged(personal_data_changed_,
                                                 action_successful_);
  }
}

void GetPaymentInformationAction::InternalProcessAction(
    ProcessActionCallback callback) {
  callback_ = std::move(callback);
  auto payment_options = CreateOptionsFromProto();
  if (!payment_options) {
    EndAction(ClientStatus(INVALID_ACTION));
    return;
  }

  // If Chrome password manager logins are requested, we need to asynchronously
  // obtain them before showing the payment request.
  auto get_payment_information = proto_.get_payment_information();
  auto password_manager_option = std::find_if(
      get_payment_information.login_details().login_options().begin(),
      get_payment_information.login_details().login_options().end(),
      [&](const LoginDetailsProto::LoginOptionProto& option) {
        return option.type_case() ==
               LoginDetailsProto::LoginOptionProto::kPasswordManager;
      });
  bool requests_pwm_logins =
      password_manager_option !=
      get_payment_information.login_details().login_options().end();

  payment_options->confirm_callback = base::BindOnce(
      &GetPaymentInformationAction::OnGetPaymentInformation,
      weak_ptr_factory_.GetWeakPtr(), std::move(get_payment_information));
  payment_options->additional_actions_callback =
      base::BindOnce(&GetPaymentInformationAction::OnAdditionalActionTriggered,
                     weak_ptr_factory_.GetWeakPtr());
  payment_options->terms_link_callback = base::BindOnce(
      &GetPaymentInformationAction::OnTermsAndConditionsLinkClicked,
      weak_ptr_factory_.GetWeakPtr());
  if (requests_pwm_logins) {
    delegate_->GetWebsiteLoginFetcher()->GetLoginsForUrl(
        delegate_->GetWebContents()->GetLastCommittedURL(),
        base::BindOnce(&GetPaymentInformationAction::OnGetLogins,
                       weak_ptr_factory_.GetWeakPtr(), *password_manager_option,
                       std::move(payment_options)));
  } else {
    ShowToUser(std::move(payment_options));
  }
}

void GetPaymentInformationAction::EndAction(const ClientStatus& status) {
  action_successful_ = status.ok();
  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

void GetPaymentInformationAction::OnGetLogins(
    const LoginDetailsProto::LoginOptionProto& login_option,
    std::unique_ptr<PaymentRequestOptions> payment_options,
    std::vector<WebsiteLoginFetcher::Login> logins) {
  for (const auto& login : logins) {
    LoginChoice choice = {
        base::NumberToString(payment_options->login_choices.size()),
        login.username, login_option.preselection_priority()};
    payment_options->login_choices.emplace_back(std::move(choice));
    login_details_map_.emplace(
        choice.identifier,
        std::make_unique<LoginDetails>(
            login_option.choose_automatically_if_no_other_options(),
            login_option.payload(), login));
  }
  ShowToUser(std::move(payment_options));
}

void GetPaymentInformationAction::ShowToUser(
    std::unique_ptr<PaymentRequestOptions> payment_options) {
  // Create and set initial state.
  auto payment_information = std::make_unique<PaymentInformation>();
  auto get_payment_information = proto_.get_payment_information();
  switch (get_payment_information.terms_and_conditions_state()) {
    case GetPaymentInformationProto::NOT_SELECTED:
      payment_information->terms_and_conditions = NOT_SELECTED;
      break;
    case GetPaymentInformationProto::ACCEPTED:
      payment_information->terms_and_conditions = ACCEPTED;
      break;
    case GetPaymentInformationProto::REVIEW_REQUIRED:
      payment_information->terms_and_conditions = REQUIRES_REVIEW;
      break;
  }

  if (payment_options->request_login_choice &&
      payment_options->login_choices.empty()) {
    EndAction(ClientStatus(PAYMENT_REQUEST_ERROR));
    return;
  }

  // Special case: if the only available login option has
  // |choose_automatically_if_no_other_options=true|, the section will not be
  // shown.
  bool only_login_requested =
      payment_options->request_login_choice &&
      !payment_options->request_payer_name &&
      !payment_options->request_payer_email &&
      !payment_options->request_payer_phone &&
      !payment_options->request_shipping &&
      !payment_options->request_payment_method &&
      !get_payment_information.request_terms_and_conditions();

  if (payment_options->login_choices.size() == 1 &&
      login_details_map_.at(payment_options->login_choices.at(0).identifier)
          ->choose_automatically_if_no_other_options) {
    payment_options->request_login_choice = false;
    payment_information->login_choice_identifier.assign(
        payment_options->login_choices[0].identifier);

    // If only the login section is requested and the choice has already been
    // made implicitly, the entire UI will not be shown and the action will
    // complete immediately.
    if (only_login_requested) {
      payment_information->succeed = true;
      std::move(payment_options->confirm_callback)
          .Run(std::move(payment_information));
      return;
    }
  }

  // Gather info for UMA histograms.
  if (!shown_to_user_) {
    shown_to_user_ = true;
    initially_prefilled = IsInitialAutofillDataComplete(
        delegate_->GetPersonalDataManager(), *payment_options);
    delegate_->GetPersonalDataManager()->AddObserver(this);
  }

  if (get_payment_information.has_prompt()) {
    delegate_->SetStatusMessage(get_payment_information.prompt());
  }
  delegate_->GetPaymentInformation(std::move(payment_options),
                                   std::move(payment_information));
}

void GetPaymentInformationAction::OnGetPaymentInformation(
    const GetPaymentInformationProto& get_payment_information,
    std::unique_ptr<PaymentInformation> payment_information) {
  if (!callback_)
    return;

  bool succeed = payment_information->succeed;
  if (succeed) {
    if (get_payment_information.ask_for_payment()) {
      DCHECK(payment_information->card);
      std::string card_issuer_network =
          autofill::data_util::GetPaymentRequestData(
              payment_information->card->network())
              .basic_card_issuer_network;
      processed_action_proto_->mutable_payment_details()
          ->set_card_issuer_network(card_issuer_network);
      delegate_->GetClientMemory()->set_selected_card(
          std::move(payment_information->card));

      if (!get_payment_information.billing_address_name().empty()) {
        DCHECK(payment_information->billing_address);
        delegate_->GetClientMemory()->set_selected_address(
            get_payment_information.billing_address_name(),
            std::move(payment_information->billing_address));
      }
    }

    if (!get_payment_information.shipping_address_name().empty()) {
      DCHECK(payment_information->shipping_address);
      delegate_->GetClientMemory()->set_selected_address(
          get_payment_information.shipping_address_name(),
          std::move(payment_information->shipping_address));
    }

    if (get_payment_information.has_contact_details()) {
      auto contact_details_proto = get_payment_information.contact_details();
      autofill::AutofillProfile contact_profile;
      contact_profile.SetRawInfo(
          autofill::ServerFieldType::NAME_FULL,
          base::ASCIIToUTF16(payment_information->payer_name));
      autofill::data_util::NameParts parts = autofill::data_util::SplitName(
          base::ASCIIToUTF16(payment_information->payer_name));
      contact_profile.SetRawInfo(autofill::ServerFieldType::NAME_FIRST,
                                 parts.given);
      contact_profile.SetRawInfo(autofill::ServerFieldType::NAME_MIDDLE,
                                 parts.middle);
      contact_profile.SetRawInfo(autofill::ServerFieldType::NAME_LAST,
                                 parts.family);
      contact_profile.SetRawInfo(
          autofill::ServerFieldType::EMAIL_ADDRESS,
          base::ASCIIToUTF16(payment_information->payer_email));
      contact_profile.SetRawInfo(
          autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER,
          base::ASCIIToUTF16(payment_information->payer_phone));
      if (!contact_details_proto.contact_details_name().empty()) {
        delegate_->GetClientMemory()->set_selected_address(
            contact_details_proto.contact_details_name(),
            std::make_unique<autofill::AutofillProfile>(contact_profile));
      }
    }

    if (get_payment_information.has_login_details()) {
      auto login_details =
          login_details_map_.find(payment_information->login_choice_identifier);
      DCHECK(login_details != login_details_map_.end());
      if (login_details->second->login.has_value()) {
        delegate_->GetClientMemory()->set_selected_login(
            *login_details->second->login);
      }

      processed_action_proto_->mutable_payment_details()->set_login_payload(
          login_details->second->payload);
    }

    processed_action_proto_->mutable_payment_details()
        ->set_is_terms_and_conditions_accepted(
            payment_information->terms_and_conditions ==
            TermsAndConditionsState::ACCEPTED);
    processed_action_proto_->mutable_payment_details()->set_payer_email(
        payment_information->payer_email);
  }

  EndAction(succeed ? ClientStatus(ACTION_APPLIED)
                    : ClientStatus(PAYMENT_REQUEST_ERROR));
}

void GetPaymentInformationAction::OnAdditionalActionTriggered(int index) {
  if (!callback_)
    return;

  processed_action_proto_->mutable_payment_details()
      ->set_additional_action_index(index);
  EndAction(ClientStatus(ACTION_APPLIED));
}

void GetPaymentInformationAction::OnTermsAndConditionsLinkClicked(int link) {
  if (!callback_)
    return;

  processed_action_proto_->mutable_payment_details()->set_terms_link(link);
  EndAction(ClientStatus(ACTION_APPLIED));
}

std::unique_ptr<PaymentRequestOptions>
GetPaymentInformationAction::CreateOptionsFromProto() {
  auto payment_options = std::make_unique<PaymentRequestOptions>();
  auto get_payment_information = proto_.get_payment_information();

  if (get_payment_information.has_contact_details()) {
    auto contact_details = get_payment_information.contact_details();
    payment_options->request_payer_email =
        contact_details.request_payer_email();
    payment_options->request_payer_name = contact_details.request_payer_name();
    payment_options->request_payer_phone =
        contact_details.request_payer_phone();
  }

  std::copy(get_payment_information.supported_basic_card_networks().begin(),
            get_payment_information.supported_basic_card_networks().end(),
            std::back_inserter(payment_options->supported_basic_card_networks));

  payment_options->request_shipping =
      !get_payment_information.shipping_address_name().empty();
  payment_options->request_payment_method =
      get_payment_information.ask_for_payment();
  payment_options->require_billing_postal_code =
      get_payment_information.require_billing_postal_code();
  payment_options->billing_postal_code_missing_text =
      get_payment_information.billing_postal_code_missing_text();
  if (payment_options->require_billing_postal_code &&
      payment_options->billing_postal_code_missing_text.empty()) {
    return nullptr;
  }
  payment_options->request_login_choice =
      get_payment_information.has_login_details();
  payment_options->login_section_title.assign(
      get_payment_information.login_details().section_title());

  // Transform login options to concrete login choices.
  for (const auto& login_option :
       get_payment_information.login_details().login_options()) {
    switch (login_option.type_case()) {
      case LoginDetailsProto::LoginOptionProto::kCustom: {
        LoginChoice choice = {
            base::NumberToString(payment_options->login_choices.size()),
            login_option.custom().label(),
            login_option.has_preselection_priority()
                ? login_option.preselection_priority()
                : -1};
        payment_options->login_choices.emplace_back(std::move(choice));
        login_details_map_.emplace(
            choice.identifier,
            std::make_unique<LoginDetails>(
                login_option.choose_automatically_if_no_other_options(),
                login_option.payload()));
        break;
      }
      case LoginDetailsProto::LoginOptionProto::kPasswordManager: {
        // Will be retrieved later.
        break;
      }
      case LoginDetailsProto::LoginOptionProto::TYPE_NOT_SET: {
        // Login option specified, but type not set (should never happen).
        return nullptr;
      }
    }
  }

  // TODO(crbug.com/806868): Maybe we could refactor this to make the confirm
  // chip and direct_action part of the additional_actions.
  std::string confirm_text = get_payment_information.confirm_button_text();
  if (confirm_text.empty()) {
    confirm_text =
        l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_PAYMENT_INFO_CONFIRM);
  }
  payment_options->confirm_action.mutable_chip()->set_text(confirm_text);
  payment_options->confirm_action.mutable_chip()->set_type(HIGHLIGHTED_ACTION);
  *payment_options->confirm_action.mutable_direct_action() =
      get_payment_information.confirm_direct_action();

  for (auto action : get_payment_information.additional_actions()) {
    payment_options->additional_actions.push_back(action);
  }

  if (get_payment_information.request_terms_and_conditions()) {
    payment_options->show_terms_as_checkbox =
        get_payment_information.show_terms_as_checkbox();
    payment_options->accept_terms_and_conditions_text =
        get_payment_information.accept_terms_and_conditions_text();
    if (payment_options->accept_terms_and_conditions_text.empty()) {
      payment_options->accept_terms_and_conditions_text =
          l10n_util::GetStringUTF8(
              IDS_AUTOFILL_ASSISTANT_3RD_PARTY_TERMS_ACCEPT);
    }
  }

  return payment_options;
}

bool GetPaymentInformationAction::IsInitialAutofillDataComplete(
    autofill::PersonalDataManager* personal_data_manager,
    const PaymentRequestOptions& payment_options) const {
  bool request_contact = payment_options.request_payer_name ||
                         payment_options.request_payer_email ||
                         payment_options.request_payer_phone;
  if (request_contact || payment_options.request_shipping) {
    auto profiles = personal_data_manager->GetProfiles();
    if (request_contact) {
      auto completeContactIter =
          std::find_if(profiles.begin(), profiles.end(),
                       [&payment_options](const auto& profile) {
                         return IsCompleteContact(*profile, payment_options);
                       });
      if (completeContactIter == profiles.end()) {
        return false;
      }
    }

    if (payment_options.request_shipping) {
      auto completeAddressIter =
          std::find_if(profiles.begin(), profiles.end(),
                       [&payment_options](const auto* profile) {
                         return IsCompleteAddress(*profile, payment_options);
                       });
      if (completeAddressIter == profiles.end()) {
        return false;
      }
    }
  }

  if (payment_options.request_payment_method) {
    auto credit_cards = personal_data_manager->GetCreditCards();
    auto completeCardIter = std::find_if(
        credit_cards.begin(), credit_cards.end(),
        [&payment_options](const auto* credit_card) {
          return IsCompleteCreditCard(*credit_card, payment_options);
        });
    if (completeCardIter == credit_cards.end()) {
      return false;
    }
  }
  return true;
}

bool GetPaymentInformationAction::IsCompleteContact(
    const autofill::AutofillProfile& profile,
    const PaymentRequestOptions& payment_options) {
  if (payment_options.request_payer_name &&
      profile.GetRawInfo(autofill::NAME_FULL).empty()) {
    return false;
  }

  if (payment_options.request_payer_email &&
      profile.GetRawInfo(autofill::EMAIL_ADDRESS).empty()) {
    return false;
  }

  if (payment_options.request_payer_phone &&
      profile.GetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER).empty()) {
    return false;
  }
  return true;
}

bool GetPaymentInformationAction::IsCompleteAddress(
    const autofill::AutofillProfile& profile,
    const PaymentRequestOptions& payment_options) {
  if (!payment_options.request_shipping) {
    return true;
  }

  auto address_data = autofill::i18n::CreateAddressDataFromAutofillProfile(
      profile, base::android::GetDefaultLocaleString());
  return autofill::addressinput::HasAllRequiredFields(*address_data);
}

bool GetPaymentInformationAction::IsCompleteCreditCard(
    const autofill::CreditCard& credit_card,
    const PaymentRequestOptions& payment_options) {
  if (credit_card.record_type() != autofill::CreditCard::MASKED_SERVER_CARD &&
      !credit_card.HasValidCardNumber()) {
    // Can't check validity of masked server card numbers because they are
    // incomplete until decrypted.
    return false;
  }

  if (!credit_card.HasValidExpirationDate() ||
      credit_card.billing_address_id().empty()) {
    return false;
  }

  std::string basic_card_network =
      autofill::data_util::GetPaymentRequestData(credit_card.network())
          .basic_card_issuer_network;
  if (!payment_options.supported_basic_card_networks.empty() &&
      std::find(payment_options.supported_basic_card_networks.begin(),
                payment_options.supported_basic_card_networks.end(),
                basic_card_network) ==
          payment_options.supported_basic_card_networks.end()) {
    return false;
  }
  return true;
}

void GetPaymentInformationAction::OnPersonalDataChanged() {
  personal_data_changed_ = true;
  delegate_->GetPersonalDataManager()->RemoveObserver(this);
}

}  // namespace autofill_assistant
