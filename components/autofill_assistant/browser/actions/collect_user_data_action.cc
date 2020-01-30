// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/collect_user_data_action.h"

#include <algorithm>
#include <set>
#include <utility>
#include "base/bind.h"
#include "base/callback.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/address_i18n.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/user_data_util.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "components/autofill_assistant/browser/website_login_fetcher_impl.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "third_party/libaddressinput/chromium/addressinput_util.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using autofill_assistant::CollectUserDataOptions;
using autofill_assistant::DateTimeProto;
using autofill_assistant::TermsAndConditionsState;
using autofill_assistant::UserData;

bool IsCompleteContact(
    const autofill::AutofillProfile* profile,
    const CollectUserDataOptions& collect_user_data_options) {
  if (!collect_user_data_options.request_payer_name &&
      !collect_user_data_options.request_payer_email &&
      !collect_user_data_options.request_payer_phone) {
    return true;
  }

  if (!profile) {
    return false;
  }

  if (collect_user_data_options.request_payer_name &&
      profile->GetRawInfo(autofill::NAME_FULL).empty()) {
    return false;
  }

  if (collect_user_data_options.request_payer_email &&
      profile->GetRawInfo(autofill::EMAIL_ADDRESS).empty()) {
    return false;
  }

  if (collect_user_data_options.request_payer_phone &&
      profile->GetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER).empty()) {
    return false;
  }
  return true;
}

bool IsCompleteAddress(const autofill::AutofillProfile* profile,
                       bool require_postal_code) {
  if (!profile) {
    return false;
  }
  // We use a hard coded locale here since it's not used in the autofill:: code
  // anyway. I.e. creating this profile ends up in FormGroup::GetInfoImpl, which
  // simply ignores the app_locale.
  auto address_data =
      autofill::i18n::CreateAddressDataFromAutofillProfile(*profile, "en-US");
  if (!autofill::addressinput::HasAllRequiredFields(*address_data)) {
    return false;
  }

  if (require_postal_code && address_data->postal_code.empty()) {
    return false;
  }

  return true;
}

bool IsCompleteShippingAddress(
    const autofill::AutofillProfile* profile,
    const CollectUserDataOptions& collect_user_data_options) {
  return !collect_user_data_options.request_shipping ||
         IsCompleteAddress(profile, /* require_postal_code = */ false);
}

bool IsCompleteCreditCard(
    const autofill::CreditCard* credit_card,
    const autofill::AutofillProfile* billing_profile,
    const CollectUserDataOptions& collect_user_data_options) {
  if (!collect_user_data_options.request_payment_method) {
    return true;
  }

  if (!credit_card || !billing_profile) {
    return false;
  }

  if (!IsCompleteAddress(
          billing_profile,
          collect_user_data_options.require_billing_postal_code)) {
    return false;
  }

  if (credit_card->record_type() != autofill::CreditCard::MASKED_SERVER_CARD &&
      !credit_card->HasValidCardNumber()) {
    // Can't check validity of masked server card numbers because they are
    // incomplete until decrypted.
    return false;
  }

  if (!credit_card->HasValidExpirationDate() ||
      credit_card->billing_address_id().empty()) {
    return false;
  }

  std::string basic_card_network =
      autofill::data_util::GetPaymentRequestData(credit_card->network())
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

bool IsValidLoginChoice(
    const std::string& choice_identifier,
    const CollectUserDataOptions& collect_user_data_options) {
  return !collect_user_data_options.request_login_choice ||
         !choice_identifier.empty();
}

bool IsValidTermsChoice(
    TermsAndConditionsState terms_state,
    const CollectUserDataOptions& collect_user_data_options) {
  return collect_user_data_options.accept_terms_and_conditions_text.empty() ||
         terms_state != TermsAndConditionsState::NOT_SELECTED;
}

// Comparison function for |DateTimeProto|.
// Returns 0 if equal, < 0 if |first| < |second|, > 0 if |second| > |first|.
int CompareDateTimes(const DateTimeProto& first, const DateTimeProto& second) {
  auto first_tuple = std::make_tuple(
      first.date().year(), first.date().month(), first.date().day(),
      first.time().hour(), first.time().minute(), first.time().second());
  auto second_tuple = std::make_tuple(
      second.date().year(), second.date().month(), second.date().day(),
      second.time().hour(), second.time().minute(), second.time().second());
  if (first_tuple < second_tuple) {
    return -1;
  } else if (second_tuple < first_tuple) {
    return 1;
  }
  return 0;
}

bool IsValidDateTimeRange(
    const DateTimeProto& start,
    const DateTimeProto& end,
    const CollectUserDataOptions& collect_user_data_options) {
  return !collect_user_data_options.request_date_time_range ||
         CompareDateTimes(start, end) < 0;
}

bool IsValidUserFormSection(
    const autofill_assistant::UserFormSectionProto& proto) {
  if (proto.title().empty()) {
    DVLOG(2) << "UserFormSectionProto: Empty title not allowed.";
    return false;
  }
  switch (proto.section_case()) {
    case autofill_assistant::UserFormSectionProto::kStaticTextSection:
      if (proto.static_text_section().text().empty()) {
        DVLOG(2) << "StaticTextSectionProto: Empty text not allowed.";
        return false;
      }
      break;
    case autofill_assistant::UserFormSectionProto::kTextInputSection: {
      if (proto.text_input_section().input_fields().empty()) {
        DVLOG(2) << "TextInputProto: At least one input must be specified.";
        return false;
      }
      std::set<std::string> memory_keys;
      for (const auto& input_field :
           proto.text_input_section().input_fields()) {
        if (input_field.client_memory_key().empty()) {
          DVLOG(2) << "TextInputProto: Memory key must be specified.";
          return false;
        }
        if (!memory_keys.insert(input_field.client_memory_key()).second) {
          DVLOG(2) << "TextInputProto: Duplicate memory keys ("
                   << input_field.client_memory_key() << ").";
          return false;
        }
      }
      break;
    }
    case autofill_assistant::UserFormSectionProto::SECTION_NOT_SET:
      DVLOG(2) << "UserFormSectionProto: section oneof not set.";
      return false;
  }
  return true;
}

}  // namespace

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
    Metrics::RecordPaymentRequestMandatoryPostalCode(
        proto_.collect_user_data().require_billing_postal_code(),
        initial_card_has_billing_postal_code_, action_successful_);
  }
}

void CollectUserDataAction::InternalProcessAction(
    ProcessActionCallback callback) {
  callback_ = std::move(callback);
  if (!CreateOptionsFromProto()) {
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

  collect_user_data_options_->confirm_callback =
      base::BindOnce(&CollectUserDataAction::OnGetUserData,
                     weak_ptr_factory_.GetWeakPtr(), collect_user_data);
  collect_user_data_options_->additional_actions_callback =
      base::BindOnce(&CollectUserDataAction::OnAdditionalActionTriggered,
                     weak_ptr_factory_.GetWeakPtr());
  collect_user_data_options_->terms_link_callback =
      base::BindOnce(&CollectUserDataAction::OnTermsAndConditionsLinkClicked,
                     weak_ptr_factory_.GetWeakPtr());
  if (requests_pwm_logins) {
    delegate_->GetWebsiteLoginFetcher()->GetLoginsForUrl(
        delegate_->GetWebContents()->GetLastCommittedURL(),
        base::BindOnce(&CollectUserDataAction::OnGetLogins,
                       weak_ptr_factory_.GetWeakPtr(),
                       *password_manager_option));
  } else {
    ShowToUser();
  }
}

void CollectUserDataAction::EndAction(const ClientStatus& status) {
  delegate_->CleanUpAfterPrompt();
  action_successful_ = status.ok();
  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

void CollectUserDataAction::OnGetLogins(
    const LoginDetailsProto::LoginOptionProto& login_option,
    std::vector<WebsiteLoginFetcher::Login> logins) {
  for (const auto& login : logins) {
    auto identifier =
        base::NumberToString(collect_user_data_options_->login_choices.size());
    collect_user_data_options_->login_choices.emplace_back(
        identifier, login.username, login_option.sublabel(),
        login_option.sublabel_accessibility_hint(),
        login_option.preselection_priority(),
        login_option.has_info_popup()
            ? base::make_optional(login_option.info_popup())
            : base::nullopt);
    login_details_map_.emplace(
        identifier, std::make_unique<LoginDetails>(
                        login_option.choose_automatically_if_no_other_options(),
                        login_option.payload(), login));
  }
  ShowToUser();
}

void CollectUserDataAction::ShowToUser() {
  // Set initial state.
  delegate_->WriteUserData(base::BindOnce(&CollectUserDataAction::OnShowToUser,
                                          weak_ptr_factory_.GetWeakPtr()));
}

void CollectUserDataAction::OnShowToUser(UserData* user_data,
                                         UserData::FieldChange* field_change) {
  // merge the new proto_ into the existing user_data. the proto_ always takes
  // precedence over the existing user_data.
  *field_change = UserData::FieldChange::ALL;
  user_data->succeed_ = false;
  auto collect_user_data = proto_.collect_user_data();
  // the backend should explicitly set the terms and conditions state on every
  // new action.
  switch (collect_user_data.terms_and_conditions_state()) {
    case CollectUserDataProto::NOT_SELECTED:
      user_data->terms_and_conditions_ = NOT_SELECTED;
      break;
    case CollectUserDataProto::ACCEPTED:
      user_data->terms_and_conditions_ = ACCEPTED;
      break;
    case CollectUserDataProto::REVIEW_REQUIRED:
      user_data->terms_and_conditions_ = REQUIRES_REVIEW;
      break;
  }
  for (const auto& additional_section :
       collect_user_data.additional_prepended_sections()) {
    if (additional_section.section_case() ==
        UserFormSectionProto::kTextInputSection) {
      for (const auto& text_input :
           additional_section.text_input_section().input_fields()) {
        user_data->additional_values_[text_input.client_memory_key()] =
            text_input.value();
      }
    }
  }
  for (const auto& additional_section :
       collect_user_data.additional_appended_sections()) {
    if (additional_section.section_case() ==
        UserFormSectionProto::kTextInputSection) {
      for (const auto& text_input :
           additional_section.text_input_section().input_fields()) {
        user_data->additional_values_[text_input.client_memory_key()] =
            text_input.value();
      }
    }
  }

  if (collect_user_data_options_->request_login_choice &&
      collect_user_data_options_->login_choices.empty()) {
    EndAction(ClientStatus(COLLECT_USER_DATA_ERROR));
    return;
  }

  // Special case: if the only available login option has
  // |choose_automatically_if_no_other_options=true|, the section will not be
  // shown.
  bool only_login_requested =
      collect_user_data_options_->request_login_choice &&
      !collect_user_data_options_->request_payer_name &&
      !collect_user_data_options_->request_payer_email &&
      !collect_user_data_options_->request_payer_phone &&
      !collect_user_data_options_->request_shipping &&
      !collect_user_data_options_->request_payment_method &&
      !collect_user_data.request_terms_and_conditions();

  if (collect_user_data_options_->login_choices.size() == 1 &&
      login_details_map_
          .at(collect_user_data_options_->login_choices.at(0).identifier)
          ->choose_automatically_if_no_other_options) {
    collect_user_data_options_->request_login_choice = false;

    user_data->login_choice_identifier_.assign(
        collect_user_data_options_->login_choices[0].identifier);

    // If only the login section is requested and the choice has already been
    // made implicitly, the entire UI will not be shown and the action will
    // complete immediately.
    if (only_login_requested) {
      user_data->succeed_ = true;
      std::move(collect_user_data_options_->confirm_callback)
          .Run(user_data, nullptr);
      return;
    }
  }

  // Add available profiles and start listening.
  delegate_->GetPersonalDataManager()->AddObserver(this);
  UpdatePersonalDataManagerProfiles(user_data);
  UpdatePersonalDataManagerCards(user_data);

  // Gather info for UMA histograms.
  if (!shown_to_user_) {
    shown_to_user_ = true;
    initially_prefilled =
        CheckInitialAutofillDataComplete(delegate_->GetPersonalDataManager());
  }

  if (collect_user_data.has_prompt()) {
    delegate_->SetStatusMessage(collect_user_data.prompt());
  }
  delegate_->Prompt(/* user_actions = */ nullptr,
                    /* disable_force_expand_sheet = */ false);
  delegate_->CollectUserData(collect_user_data_options_.get());
}

void CollectUserDataAction::OnGetUserData(
    const CollectUserDataProto& collect_user_data,
    UserData* user_data,
    const UserModel* user_model) {
  if (!callback_)
    return;

  bool succeed = user_data->succeed_;
  if (succeed) {
    if (collect_user_data.request_payment_method()) {
      DCHECK(user_data->selected_card_);
      std::string card_issuer_network =
          autofill::data_util::GetPaymentRequestData(
              user_data->selected_card_->network())
              .basic_card_issuer_network;
      processed_action_proto_->mutable_collect_user_data_result()
          ->set_card_issuer_network(card_issuer_network);
    }

    if (collect_user_data.has_contact_details()) {
      auto contact_details_proto = collect_user_data.contact_details();
      auto* selected_profile = user_data->selected_address(
          contact_details_proto.contact_details_name());
      DCHECK(selected_profile);

      if (contact_details_proto.request_payer_name()) {
        Metrics::RecordPaymentRequestFirstNameOnly(
            selected_profile->GetRawInfo(autofill::NAME_LAST).empty());
      }

      if (contact_details_proto.request_payer_email()) {
        processed_action_proto_->mutable_collect_user_data_result()
            ->set_payer_email(base::UTF16ToUTF8(
                selected_profile->GetRawInfo(autofill::EMAIL_ADDRESS)));
      }
    }

    if (collect_user_data.has_login_details()) {
      auto login_details =
          login_details_map_.find(user_data->login_choice_identifier_);
      DCHECK(login_details != login_details_map_.end());
      if (login_details->second->login.has_value()) {
        user_data->selected_login_ = *login_details->second->login;
      }

      processed_action_proto_->mutable_collect_user_data_result()
          ->set_login_payload(login_details->second->payload);
    }

    if (collect_user_data.has_date_time_range()) {
      *processed_action_proto_->mutable_collect_user_data_result()
           ->mutable_date_time_start() = user_data->date_time_range_start_;
      *processed_action_proto_->mutable_collect_user_data_result()
           ->mutable_date_time_end() = user_data->date_time_range_end_;
    }

    for (const auto& value : user_data->additional_values_) {
      if (!value.second.empty()) {
        processed_action_proto_->mutable_collect_user_data_result()
            ->add_set_text_input_memory_keys(value.first);
      }
    }

    processed_action_proto_->mutable_collect_user_data_result()
        ->set_is_terms_and_conditions_accepted(
            user_data->terms_and_conditions_ ==
            TermsAndConditionsState::ACCEPTED);
    if (user_model != nullptr &&
        collect_user_data.has_generic_user_interface()) {
      *processed_action_proto_->mutable_collect_user_data_result()
           ->mutable_model() =
          collect_user_data.generic_user_interface().model();
      user_model->UpdateProto(
          processed_action_proto_->mutable_collect_user_data_result()
              ->mutable_model());
    }
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

bool CollectUserDataAction::CreateOptionsFromProto() {
  DCHECK(collect_user_data_options_ == nullptr);
  collect_user_data_options_ = std::make_unique<CollectUserDataOptions>();
  auto collect_user_data = proto_.collect_user_data();

  if (collect_user_data.has_contact_details()) {
    auto contact_details = collect_user_data.contact_details();
    collect_user_data_options_->request_payer_email =
        contact_details.request_payer_email();
    collect_user_data_options_->request_payer_name =
        contact_details.request_payer_name();
    collect_user_data_options_->request_payer_phone =
        contact_details.request_payer_phone();
    if (collect_user_data_options_->request_payer_email ||
        collect_user_data_options_->request_payer_name ||
        collect_user_data_options_->request_payer_phone) {
      if (!contact_details.has_contact_details_name()) {
        DVLOG(1) << "Contact details name missing";
        return false;
      }
    }

    collect_user_data_options_->contact_details_name =
        contact_details.contact_details_name();
  }

  for (const auto& network :
       collect_user_data.supported_basic_card_networks()) {
    if (!autofill::data_util::IsValidBasicCardIssuerNetwork(network)) {
      DVLOG(1) << "Invalid basic card network: " << network;
      return false;
    }
  }
  std::copy(collect_user_data.supported_basic_card_networks().begin(),
            collect_user_data.supported_basic_card_networks().end(),
            std::back_inserter(
                collect_user_data_options_->supported_basic_card_networks));

  collect_user_data_options_->shipping_address_name =
      collect_user_data.shipping_address_name();
  collect_user_data_options_->request_shipping =
      !collect_user_data.shipping_address_name().empty();
  collect_user_data_options_->request_payment_method =
      collect_user_data.request_payment_method();
  collect_user_data_options_->require_billing_postal_code =
      collect_user_data.require_billing_postal_code();
  collect_user_data_options_->billing_postal_code_missing_text =
      collect_user_data.billing_postal_code_missing_text();
  if (collect_user_data_options_->require_billing_postal_code &&
      collect_user_data_options_->billing_postal_code_missing_text.empty()) {
    return false;
  }
  collect_user_data_options_->billing_address_name =
      collect_user_data.billing_address_name();
  if (collect_user_data_options_->request_payment_method &&
      collect_user_data_options_->billing_address_name.empty()) {
    return false;
  }

  collect_user_data_options_->credit_card_expired_text =
      collect_user_data.credit_card_expired_text();
  // TODO(b/146195295): Remove fallback and enforce non-empty backend string.
  if (collect_user_data_options_->credit_card_expired_text.empty()) {
    collect_user_data_options_->credit_card_expired_text =
        l10n_util::GetStringUTF8(
            IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRED);
  }
  collect_user_data_options_->request_login_choice =
      collect_user_data.has_login_details();
  collect_user_data_options_->login_section_title.assign(
      collect_user_data.login_details().section_title());

  // Transform login options to concrete login choices.
  for (const auto& login_option :
       collect_user_data.login_details().login_options()) {
    switch (login_option.type_case()) {
      case LoginDetailsProto::LoginOptionProto::kCustom: {
        LoginChoice choice = {
            base::NumberToString(
                collect_user_data_options_->login_choices.size()),
            login_option.custom().label(),
            login_option.sublabel(),
            login_option.has_sublabel_accessibility_hint()
                ? base::make_optional(
                      login_option.sublabel_accessibility_hint())
                : base::nullopt,
            login_option.has_preselection_priority()
                ? login_option.preselection_priority()
                : -1,
            login_option.has_info_popup()
                ? base::make_optional(login_option.info_popup())
                : base::nullopt};
        collect_user_data_options_->login_choices.emplace_back(
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
        DVLOG(1) << "LoginDetailsProto::LoginOptionProto::TYPE_NOT_SET";
        return false;
      }
    }
  }

  if (collect_user_data.has_date_time_range()) {
    if (!collect_user_data.date_time_range().has_start_label() ||
        !collect_user_data.date_time_range().has_end_label() ||
        !collect_user_data.date_time_range().has_start() ||
        !collect_user_data.date_time_range().has_end() ||
        !collect_user_data.date_time_range().has_min() ||
        !collect_user_data.date_time_range().has_max()) {
      DVLOG(1) << "Invalid action: missing one or more of the required fields "
                  "'start', 'end', 'min', 'max', 'start_label', end_label'.";
      return false;
    }
    if (collect_user_data.date_time_range().invalid_error_message().empty()) {
      DVLOG(1) << "Invalid action: date_time_range.invalid_error_message "
                  "must not be empty.";
      return false;
    }
    collect_user_data_options_->request_date_time_range = true;
    collect_user_data_options_->date_time_range =
        collect_user_data.date_time_range();
  }

  for (const auto& section :
       collect_user_data.additional_prepended_sections()) {
    if (!IsValidUserFormSection(section)) {
      DVLOG(1)
          << "Invalid UserFormSectionProto in additional_prepended_sections";
      return false;
    }
    collect_user_data_options_->additional_prepended_sections.emplace_back(
        section);
  }
  for (const auto& section : collect_user_data.additional_appended_sections()) {
    if (!IsValidUserFormSection(section)) {
      DVLOG(1)
          << "Invalid UserFormSectionProto in additional_appended_sections";
      return false;
    }
    collect_user_data_options_->additional_appended_sections.emplace_back(
        section);
  }

  if (collect_user_data.has_info_section_text() &&
      collect_user_data.info_section_text().empty()) {
    DVLOG(1) << "Info text set but empty.";
    return false;
  }

  if (collect_user_data.has_generic_user_interface()) {
    collect_user_data_options_->generic_user_interface =
        collect_user_data.generic_user_interface();
  }

  // TODO(crbug.com/806868): Maybe we could refactor this to make the confirm
  // chip and direct_action part of the additional_actions.
  std::string confirm_text = collect_user_data.confirm_button_text();
  if (confirm_text.empty()) {
    confirm_text =
        l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_PAYMENT_INFO_CONFIRM);
  }
  collect_user_data_options_->confirm_action.mutable_chip()->set_text(
      confirm_text);
  collect_user_data_options_->confirm_action.mutable_chip()->set_type(
      HIGHLIGHTED_ACTION);
  *collect_user_data_options_->confirm_action.mutable_direct_action() =
      collect_user_data.confirm_direct_action();

  for (auto action : collect_user_data.additional_actions()) {
    collect_user_data_options_->additional_actions.push_back(action);
  }

  if (collect_user_data.request_terms_and_conditions()) {
    collect_user_data_options_->show_terms_as_checkbox =
        collect_user_data.show_terms_as_checkbox();

    if (collect_user_data.accept_terms_and_conditions_text().empty()) {
      return false;
    }
    collect_user_data_options_->accept_terms_and_conditions_text =
        collect_user_data.accept_terms_and_conditions_text();

    if (!collect_user_data.show_terms_as_checkbox() &&
        collect_user_data.terms_require_review_text().empty()) {
      return false;
    }
    collect_user_data_options_->terms_require_review_text =
        collect_user_data.terms_require_review_text();
  }

  if (collect_user_data.has_info_section_text()) {
    collect_user_data_options_->info_section_text =
        collect_user_data.info_section_text();
  }

  collect_user_data_options_->privacy_notice_text =
      collect_user_data.privacy_notice_text();

  collect_user_data_options_->default_email =
      delegate_->GetAccountEmailAddress();

  return true;
}

bool CollectUserDataAction::CheckInitialAutofillDataComplete(
    autofill::PersonalDataManager* personal_data_manager) {
  DCHECK(collect_user_data_options_ != nullptr);
  bool request_contact = collect_user_data_options_->request_payer_name ||
                         collect_user_data_options_->request_payer_email ||
                         collect_user_data_options_->request_payer_phone;
  if (request_contact || collect_user_data_options_->request_shipping) {
    auto profiles = personal_data_manager->GetProfiles();
    if (request_contact) {
      auto completeContactIter = std::find_if(
          profiles.begin(), profiles.end(), [this](const auto& profile) {
            return IsCompleteContact(profile,
                                     *this->collect_user_data_options_.get());
          });
      if (completeContactIter == profiles.end()) {
        return false;
      }
    }

    if (collect_user_data_options_->request_shipping) {
      auto completeAddressIter = std::find_if(
          profiles.begin(), profiles.end(), [this](const auto* profile) {
            return IsCompleteShippingAddress(
                profile, *this->collect_user_data_options_.get());
          });
      if (completeAddressIter == profiles.end()) {
        return false;
      }
    }
  }

  if (collect_user_data_options_->request_payment_method) {
    auto credit_cards = personal_data_manager->GetCreditCards();
    auto completeCardIter = std::find_if(
        credit_cards.begin(), credit_cards.end(),
        [this, personal_data_manager](const auto* credit_card) {
          // TODO(b/142630213): Figure out how to retrieve billing profile if
          // user has turned off addresses in Chrome settings.
          return IsCompleteCreditCard(
              credit_card,
              credit_card != nullptr
                  ? personal_data_manager->GetProfileByGUID(credit_card->guid())
                  : nullptr,
              *this->collect_user_data_options_.get());
        });
    if (completeCardIter == credit_cards.end()) {
      return false;
    }
    if (collect_user_data_options_->require_billing_postal_code) {
      initial_card_has_billing_postal_code_ = true;
    }
  }
  return true;
}

// static
bool CollectUserDataAction::IsUserDataComplete(
    const UserData& user_data,
    const CollectUserDataOptions& options) {
  auto* selected_profile =
      user_data.selected_address(options.contact_details_name);
  auto* billing_address =
      user_data.selected_address(options.billing_address_name);
  auto* shipping_address =
      user_data.selected_address(options.shipping_address_name);
  return IsCompleteContact(selected_profile, options) &&
         IsCompleteShippingAddress(shipping_address, options) &&
         IsCompleteCreditCard(user_data.selected_card_.get(), billing_address,
                              options) &&
         IsValidLoginChoice(user_data.login_choice_identifier_, options) &&
         IsValidTermsChoice(user_data.terms_and_conditions_, options) &&
         IsValidDateTimeRange(user_data.date_time_range_start_,
                              user_data.date_time_range_end_, options);
}

void CollectUserDataAction::UpdatePersonalDataManagerProfiles(
    UserData* user_data,
    UserData::FieldChange* field_change) {
  if (user_data == nullptr) {
    return;
  }

  bool found_profile = false;
  bool found_shipping_address = false;

  auto* selected_profile = user_data->selected_address(
      collect_user_data_options_->contact_details_name);
  auto* shipping_address = user_data->selected_address(
      collect_user_data_options_->shipping_address_name);
  user_data->available_profiles_.clear();
  for (const auto* profile :
       delegate_->GetPersonalDataManager()->GetProfilesToSuggest()) {
    user_data->available_profiles_.emplace_back(
        std::make_unique<autofill::AutofillProfile>(*profile));

    if (selected_profile != nullptr &&
        CompareContactDetails(*collect_user_data_options_, profile,
                              selected_profile)) {
      found_profile = true;
    }

    if (shipping_address != nullptr &&
        profile->Compare(*shipping_address) == 0) {
      found_shipping_address = true;
    }
  }

  if (!found_profile && selected_profile != nullptr) {
    auto it = user_data->selected_addresses_.find(
        collect_user_data_options_->contact_details_name);
    if (it != user_data->selected_addresses_.end()) {
      user_data->selected_addresses_.erase(it);
    }
  }

  if (!user_data->has_selected_address(
          collect_user_data_options_->contact_details_name) &&
      (collect_user_data_options_->request_payer_name ||
       collect_user_data_options_->request_payer_phone ||
       collect_user_data_options_->request_payer_email)) {
    int default_selection = GetDefaultContactProfile(
        *collect_user_data_options_, user_data->available_profiles_);
    if (default_selection != -1) {
      user_data->selected_addresses_.emplace(
          collect_user_data_options_->contact_details_name,
          std::make_unique<autofill::AutofillProfile>(
              *(user_data->available_profiles_[default_selection])));
    }
  }

  if (!found_shipping_address && shipping_address != nullptr) {
    auto it = user_data->selected_addresses_.find(
        collect_user_data_options_->shipping_address_name);
    if (it != user_data->selected_addresses_.end()) {
      user_data->selected_addresses_.erase(it);
    }
  }
  if (!user_data->has_selected_address(
          collect_user_data_options_->shipping_address_name) &&
      collect_user_data_options_->request_shipping) {
    int default_selection = GetDefaultAddressProfile(
        *collect_user_data_options_, user_data->available_profiles_);
    if (default_selection != -1) {
      user_data->selected_addresses_.emplace(
          collect_user_data_options_->shipping_address_name,
          std::make_unique<autofill::AutofillProfile>(
              *(user_data->available_profiles_[default_selection])));
    }
  }

  if (field_change != nullptr) {
    *field_change = UserData::FieldChange::AVAILABLE_PROFILES;
  }
}

void CollectUserDataAction::UpdatePersonalDataManagerCards(
    UserData* user_data,
    UserData::FieldChange* field_change) {
  DCHECK(user_data != nullptr);

  bool found_card = false;
  user_data->available_payment_instruments_.clear();
  for (const auto* card :
       delegate_->GetPersonalDataManager()->GetCreditCardsToSuggest(true)) {
    if (collect_user_data_options_->supported_basic_card_networks.empty() ||
        std::find(
            collect_user_data_options_->supported_basic_card_networks.begin(),
            collect_user_data_options_->supported_basic_card_networks.end(),
            autofill::data_util::GetPaymentRequestData(card->network())
                .basic_card_issuer_network) !=
            collect_user_data_options_->supported_basic_card_networks.end()) {
      auto payment_instrument = std::make_unique<PaymentInstrument>();
      payment_instrument->card = std::make_unique<autofill::CreditCard>(*card);

      if (!card->billing_address_id().empty()) {
        auto* billing_address =
            delegate_->GetPersonalDataManager()->GetProfileByGUID(
                card->billing_address_id());
        if (billing_address != nullptr) {
          payment_instrument->billing_address =
              std::make_unique<autofill::AutofillProfile>(*billing_address);
        }
      }

      user_data->available_payment_instruments_.emplace_back(
          std::move(payment_instrument));

      if (user_data->selected_card_ != nullptr &&
          card->Compare(*user_data->selected_card_) == 0) {
        found_card = true;
      }
    }
  }

  if (!found_card) {
    user_data->selected_card_.reset();
    auto it = user_data->selected_addresses_.find(
        collect_user_data_options_->billing_address_name);
    if (it != user_data->selected_addresses_.end()) {
      user_data->selected_addresses_.erase(it);
    }
  }
  if (user_data->selected_card_ == nullptr &&
      collect_user_data_options_->request_payment_method) {
    int default_selection = GetDefaultPaymentInstrument(
        *collect_user_data_options_, user_data->available_payment_instruments_);
    if (default_selection != -1) {
      const auto& default_payment_instrument =
          user_data->available_payment_instruments_[default_selection];
      user_data->selected_card_ = std::make_unique<autofill::CreditCard>(
          *(default_payment_instrument->card));
      if (default_payment_instrument->billing_address != nullptr) {
        user_data->selected_addresses_.emplace(
            collect_user_data_options_->billing_address_name,
            std::make_unique<autofill::AutofillProfile>(
                *(default_payment_instrument->billing_address)));
      }
    }
  }

  if (field_change != nullptr) {
    *field_change = UserData::FieldChange::AVAILABLE_PAYMENT_INSTRUMENTS;
  }
}

void CollectUserDataAction::OnPersonalDataChanged() {
  personal_data_changed_ = true;

  delegate_->WriteUserData(
      base::BindOnce(&CollectUserDataAction::UpdatePersonalDataManagerProfiles,
                     weak_ptr_factory_.GetWeakPtr()));
  delegate_->WriteUserData(
      base::BindOnce(&CollectUserDataAction::UpdatePersonalDataManagerCards,
                     weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace autofill_assistant
