// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/collect_user_data_action.h"

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

CollectUserDataAction::LoginDetails::LoginDetails(
    bool _choose_automatically_if_no_other_options,
    const std::string& _payload,
    const WebsiteLoginFetcher::Login& _login)
    : choose_automatically_if_no_other_options(
          _choose_automatically_if_no_other_options),
      payload(_payload),
      login(_login) {}

CollectUserDataAction::LoginDetails::LoginDetails(
    bool _choose_automatically_if_no_other_options,
    const std::string& _payload)
    : choose_automatically_if_no_other_options(
          _choose_automatically_if_no_other_options),
      payload(_payload) {}

CollectUserDataAction::LoginDetails::~LoginDetails() = default;

CollectUserDataAction::CollectUserDataAction(ActionDelegate* delegate,
                                             const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_collect_user_data());
}

CollectUserDataAction::~CollectUserDataAction() {
  delegate_->GetPersonalDataManager()->RemoveObserver(this);

  // Report UMA histograms.
  if (shown_to_user_) {
    Metrics::RecordPaymentRequestPrefilledSuccess(initially_prefilled,
                                                  action_successful_);
    Metrics::RecordPaymentRequestAutofillChanged(personal_data_changed_,
                                                 action_successful_);
  }
}

void CollectUserDataAction::InternalProcessAction(
    ProcessActionCallback callback) {
  callback_ = std::move(callback);
  auto collect_user_data_options = CreateOptionsFromProto();
  if (!collect_user_data_options) {
    EndAction(ClientStatus(INVALID_ACTION));
    return;
  }

  // If Chrome password manager logins are requested, we need to asynchronously
  // obtain them before showing the UI.
  auto collect_user_data = proto_.collect_user_data();
  auto password_manager_option = std::find_if(
      collect_user_data.login_details().login_options().begin(),
      collect_user_data.login_details().login_options().end(),
      [&](const LoginDetailsProto::LoginOptionProto& option) {
        return option.type_case() ==
               LoginDetailsProto::LoginOptionProto::kPasswordManager;
      });
  bool requests_pwm_logins =
      password_manager_option !=
      collect_user_data.login_details().login_options().end();

  collect_user_data_options->confirm_callback = base::BindOnce(
      &CollectUserDataAction::OnGetUserData, weak_ptr_factory_.GetWeakPtr(),
      std::move(collect_user_data));
  collect_user_data_options->additional_actions_callback =
      base::BindOnce(&CollectUserDataAction::OnAdditionalActionTriggered,
                     weak_ptr_factory_.GetWeakPtr());
  collect_user_data_options->terms_link_callback =
      base::BindOnce(&CollectUserDataAction::OnTermsAndConditionsLinkClicked,
                     weak_ptr_factory_.GetWeakPtr());
  if (requests_pwm_logins) {
    delegate_->GetWebsiteLoginFetcher()->GetLoginsForUrl(
        delegate_->GetWebContents()->GetLastCommittedURL(),
        base::BindOnce(&CollectUserDataAction::OnGetLogins,
                       weak_ptr_factory_.GetWeakPtr(), *password_manager_option,
                       std::move(collect_user_data_options)));
  } else {
    ShowToUser(std::move(collect_user_data_options));
  }
}

void CollectUserDataAction::EndAction(const ClientStatus& status) {
  action_successful_ = status.ok();
  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

void CollectUserDataAction::OnGetLogins(
    const LoginDetailsProto::LoginOptionProto& login_option,
    std::unique_ptr<CollectUserDataOptions> collect_user_data_options,
    std::vector<WebsiteLoginFetcher::Login> logins) {
  for (const auto& login : logins) {
    LoginChoice choice = {
        base::NumberToString(collect_user_data_options->login_choices.size()),
        login.username, login_option.preselection_priority()};
    collect_user_data_options->login_choices.emplace_back(std::move(choice));
    login_details_map_.emplace(
        choice.identifier,
        std::make_unique<LoginDetails>(
            login_option.choose_automatically_if_no_other_options(),
            login_option.payload(), login));
  }
  ShowToUser(std::move(collect_user_data_options));
}

void CollectUserDataAction::ShowToUser(
    std::unique_ptr<CollectUserDataOptions> collect_user_data_options) {
  // Create and set initial state.
  auto user_data = std::make_unique<UserData>();
  auto collect_user_data = proto_.collect_user_data();
  switch (collect_user_data.terms_and_conditions_state()) {
    case CollectUserDataProto::NOT_SELECTED:
      user_data->terms_and_conditions = NOT_SELECTED;
      break;
    case CollectUserDataProto::ACCEPTED:
      user_data->terms_and_conditions = ACCEPTED;
      break;
    case CollectUserDataProto::REVIEW_REQUIRED:
      user_data->terms_and_conditions = REQUIRES_REVIEW;
      break;
  }

  if (collect_user_data_options->request_login_choice &&
      collect_user_data_options->login_choices.empty()) {
    EndAction(ClientStatus(COLLECT_USER_DATA_ERROR));
    return;
  }

  // Special case: if the only available login option has
  // |choose_automatically_if_no_other_options=true|, the section will not be
  // shown.
  bool only_login_requested =
      collect_user_data_options->request_login_choice &&
      !collect_user_data_options->request_payer_name &&
      !collect_user_data_options->request_payer_email &&
      !collect_user_data_options->request_payer_phone &&
      !collect_user_data_options->request_shipping &&
      !collect_user_data_options->request_payment_method &&
      !collect_user_data.request_terms_and_conditions();

  if (collect_user_data_options->login_choices.size() == 1 &&
      login_details_map_
          .at(collect_user_data_options->login_choices.at(0).identifier)
          ->choose_automatically_if_no_other_options) {
    collect_user_data_options->request_login_choice = false;
    user_data->login_choice_identifier.assign(
        collect_user_data_options->login_choices[0].identifier);

    // If only the login section is requested and the choice has already been
    // made implicitly, the entire UI will not be shown and the action will
    // complete immediately.
    if (only_login_requested) {
      user_data->succeed = true;
      std::move(collect_user_data_options->confirm_callback)
          .Run(std::move(user_data));
      return;
    }
  }

  // Gather info for UMA histograms.
  if (!shown_to_user_) {
    shown_to_user_ = true;
    initially_prefilled = IsInitialAutofillDataComplete(
        delegate_->GetPersonalDataManager(), *collect_user_data_options);
    delegate_->GetPersonalDataManager()->AddObserver(this);
  }

  if (collect_user_data.has_prompt()) {
    delegate_->SetStatusMessage(collect_user_data.prompt());
  }
  delegate_->CollectUserData(std::move(collect_user_data_options),
                             std::move(user_data));
}

void CollectUserDataAction::OnGetUserData(
    const CollectUserDataProto& collect_user_data,
    std::unique_ptr<UserData> user_data) {
  if (!callback_)
    return;

  bool succeed = user_data->succeed;
  if (succeed) {
    if (collect_user_data.request_payment_method()) {
      DCHECK(user_data->card);
      std::string card_issuer_network =
          autofill::data_util::GetPaymentRequestData(user_data->card->network())
              .basic_card_issuer_network;
      processed_action_proto_->mutable_collect_user_data_result()
          ->set_card_issuer_network(card_issuer_network);
      delegate_->GetClientMemory()->set_selected_card(
          std::move(user_data->card));

      if (!collect_user_data.billing_address_name().empty()) {
        DCHECK(user_data->billing_address);
        delegate_->GetClientMemory()->set_selected_address(
            collect_user_data.billing_address_name(),
            std::move(user_data->billing_address));
      }
    }

    if (!collect_user_data.shipping_address_name().empty()) {
      DCHECK(user_data->shipping_address);
      delegate_->GetClientMemory()->set_selected_address(
          collect_user_data.shipping_address_name(),
          std::move(user_data->shipping_address));
    }

    if (collect_user_data.has_contact_details()) {
      auto contact_details_proto = collect_user_data.contact_details();
      autofill::AutofillProfile contact_profile;
      contact_profile.SetRawInfo(autofill::ServerFieldType::NAME_FULL,
                                 base::UTF8ToUTF16(user_data->payer_name));
      autofill::data_util::NameParts parts = autofill::data_util::SplitName(
          base::UTF8ToUTF16(user_data->payer_name));
      contact_profile.SetRawInfo(autofill::ServerFieldType::NAME_FIRST,
                                 parts.given);
      contact_profile.SetRawInfo(autofill::ServerFieldType::NAME_MIDDLE,
                                 parts.middle);
      contact_profile.SetRawInfo(autofill::ServerFieldType::NAME_LAST,
                                 parts.family);
      contact_profile.SetRawInfo(autofill::ServerFieldType::EMAIL_ADDRESS,
                                 base::UTF8ToUTF16(user_data->payer_email));
      contact_profile.SetRawInfo(
          autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER,
          base::UTF8ToUTF16(user_data->payer_phone));
      if (!contact_details_proto.contact_details_name().empty()) {
        delegate_->GetClientMemory()->set_selected_address(
            contact_details_proto.contact_details_name(),
            std::make_unique<autofill::AutofillProfile>(contact_profile));
      }
    }

    if (collect_user_data.has_login_details()) {
      auto login_details =
          login_details_map_.find(user_data->login_choice_identifier);
      DCHECK(login_details != login_details_map_.end());
      if (login_details->second->login.has_value()) {
        delegate_->GetClientMemory()->set_selected_login(
            *login_details->second->login);
      }

      processed_action_proto_->mutable_collect_user_data_result()
          ->set_login_payload(login_details->second->payload);
    }

    processed_action_proto_->mutable_collect_user_data_result()
        ->set_is_terms_and_conditions_accepted(
            user_data->terms_and_conditions ==
            TermsAndConditionsState::ACCEPTED);
    processed_action_proto_->mutable_collect_user_data_result()
        ->set_payer_email(user_data->payer_email);
  }

  EndAction(succeed ? ClientStatus(ACTION_APPLIED)
                    : ClientStatus(COLLECT_USER_DATA_ERROR));
}

void CollectUserDataAction::OnAdditionalActionTriggered(int index) {
  if (!callback_)
    return;

  processed_action_proto_->mutable_collect_user_data_result()
      ->set_additional_action_index(index);
  EndAction(ClientStatus(ACTION_APPLIED));
}

void CollectUserDataAction::OnTermsAndConditionsLinkClicked(int link) {
  if (!callback_)
    return;

  processed_action_proto_->mutable_collect_user_data_result()->set_terms_link(
      link);
  EndAction(ClientStatus(ACTION_APPLIED));
}

std::unique_ptr<CollectUserDataOptions>
CollectUserDataAction::CreateOptionsFromProto() {
  auto collect_user_data_options = std::make_unique<CollectUserDataOptions>();
  auto collect_user_data = proto_.collect_user_data();

  if (collect_user_data.has_contact_details()) {
    auto contact_details = collect_user_data.contact_details();
    collect_user_data_options->request_payer_email =
        contact_details.request_payer_email();
    collect_user_data_options->request_payer_name =
        contact_details.request_payer_name();
    collect_user_data_options->request_payer_phone =
        contact_details.request_payer_phone();
  }

  std::copy(collect_user_data.supported_basic_card_networks().begin(),
            collect_user_data.supported_basic_card_networks().end(),
            std::back_inserter(
                collect_user_data_options->supported_basic_card_networks));

  collect_user_data_options->request_shipping =
      !collect_user_data.shipping_address_name().empty();
  collect_user_data_options->request_payment_method =
      collect_user_data.request_payment_method();
  collect_user_data_options->require_billing_postal_code =
      collect_user_data.require_billing_postal_code();
  collect_user_data_options->billing_postal_code_missing_text =
      collect_user_data.billing_postal_code_missing_text();
  if (collect_user_data_options->require_billing_postal_code &&
      collect_user_data_options->billing_postal_code_missing_text.empty()) {
    return nullptr;
  }
  collect_user_data_options->request_login_choice =
      collect_user_data.has_login_details();
  collect_user_data_options->login_section_title.assign(
      collect_user_data.login_details().section_title());

  // Transform login options to concrete login choices.
  for (const auto& login_option :
       collect_user_data.login_details().login_options()) {
    switch (login_option.type_case()) {
      case LoginDetailsProto::LoginOptionProto::kCustom: {
        LoginChoice choice = {
            base::NumberToString(
                collect_user_data_options->login_choices.size()),
            login_option.custom().label(),
            login_option.has_preselection_priority()
                ? login_option.preselection_priority()
                : -1};
        collect_user_data_options->login_choices.emplace_back(
            std::move(choice));
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
  std::string confirm_text = collect_user_data.confirm_button_text();
  if (confirm_text.empty()) {
    confirm_text =
        l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_PAYMENT_INFO_CONFIRM);
  }
  collect_user_data_options->confirm_action.mutable_chip()->set_text(
      confirm_text);
  collect_user_data_options->confirm_action.mutable_chip()->set_type(
      HIGHLIGHTED_ACTION);
  *collect_user_data_options->confirm_action.mutable_direct_action() =
      collect_user_data.confirm_direct_action();

  for (auto action : collect_user_data.additional_actions()) {
    collect_user_data_options->additional_actions.push_back(action);
  }

  if (collect_user_data.request_terms_and_conditions()) {
    collect_user_data_options->show_terms_as_checkbox =
        collect_user_data.show_terms_as_checkbox();
    collect_user_data_options->accept_terms_and_conditions_text =
        collect_user_data.accept_terms_and_conditions_text();
    if (collect_user_data_options->accept_terms_and_conditions_text.empty()) {
      collect_user_data_options->accept_terms_and_conditions_text =
          l10n_util::GetStringUTF8(
              IDS_AUTOFILL_ASSISTANT_3RD_PARTY_TERMS_ACCEPT);
    }
  }

  collect_user_data_options->default_email =
      delegate_->GetAccountEmailAddress();

  return collect_user_data_options;
}

bool CollectUserDataAction::IsInitialAutofillDataComplete(
    autofill::PersonalDataManager* personal_data_manager,
    const CollectUserDataOptions& collect_user_data_options) const {
  bool request_contact = collect_user_data_options.request_payer_name ||
                         collect_user_data_options.request_payer_email ||
                         collect_user_data_options.request_payer_phone;
  if (request_contact || collect_user_data_options.request_shipping) {
    auto profiles = personal_data_manager->GetProfiles();
    if (request_contact) {
      auto completeContactIter = std::find_if(
          profiles.begin(), profiles.end(),
          [&collect_user_data_options](const auto& profile) {
            return IsCompleteContact(*profile, collect_user_data_options);
          });
      if (completeContactIter == profiles.end()) {
        return false;
      }
    }

    if (collect_user_data_options.request_shipping) {
      auto completeAddressIter = std::find_if(
          profiles.begin(), profiles.end(),
          [&collect_user_data_options](const auto* profile) {
            return IsCompleteAddress(*profile, collect_user_data_options);
          });
      if (completeAddressIter == profiles.end()) {
        return false;
      }
    }
  }

  if (collect_user_data_options.request_payment_method) {
    auto credit_cards = personal_data_manager->GetCreditCards();
    auto completeCardIter = std::find_if(
        credit_cards.begin(), credit_cards.end(),
        [&collect_user_data_options](const auto* credit_card) {
          return IsCompleteCreditCard(*credit_card, collect_user_data_options);
        });
    if (completeCardIter == credit_cards.end()) {
      return false;
    }
  }
  return true;
}

bool CollectUserDataAction::IsCompleteContact(
    const autofill::AutofillProfile& profile,
    const CollectUserDataOptions& collect_user_data_options) {
  if (collect_user_data_options.request_payer_name &&
      profile.GetRawInfo(autofill::NAME_FULL).empty()) {
    return false;
  }

  if (collect_user_data_options.request_payer_email &&
      profile.GetRawInfo(autofill::EMAIL_ADDRESS).empty()) {
    return false;
  }

  if (collect_user_data_options.request_payer_phone &&
      profile.GetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER).empty()) {
    return false;
  }
  return true;
}

bool CollectUserDataAction::IsCompleteAddress(
    const autofill::AutofillProfile& profile,
    const CollectUserDataOptions& collect_user_data_options) {
  if (!collect_user_data_options.request_shipping) {
    return true;
  }

  auto address_data = autofill::i18n::CreateAddressDataFromAutofillProfile(
      profile, base::android::GetDefaultLocaleString());
  return autofill::addressinput::HasAllRequiredFields(*address_data);
}

bool CollectUserDataAction::IsCompleteCreditCard(
    const autofill::CreditCard& credit_card,
    const CollectUserDataOptions& collect_user_data_options) {
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
  if (!collect_user_data_options.supported_basic_card_networks.empty() &&
      std::find(collect_user_data_options.supported_basic_card_networks.begin(),
                collect_user_data_options.supported_basic_card_networks.end(),
                basic_card_network) ==
          collect_user_data_options.supported_basic_card_networks.end()) {
    return false;
  }
  return true;
}

void CollectUserDataAction::OnPersonalDataChanged() {
  personal_data_changed_ = true;
  delegate_->GetPersonalDataManager()->RemoveObserver(this);
}

}  // namespace autofill_assistant
