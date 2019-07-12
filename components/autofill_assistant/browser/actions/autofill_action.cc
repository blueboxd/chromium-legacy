// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/autofill_action.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"
#include "components/autofill_assistant/browser/client_memory.h"
#include "components/autofill_assistant/browser/client_status.h"

namespace autofill_assistant {

AutofillAction::AutofillAction(ActionDelegate* delegate,
                               const ActionProto& proto)
    : Action(delegate, proto), weak_ptr_factory_(this) {
  if (proto.has_use_address()) {
    is_autofill_card_ = false;
    prompt_ = proto.use_address().prompt();
    name_ = proto.use_address().name();
    selector_ = Selector(proto.use_address().form_field_element());
    required_fields_value_status_.resize(
        proto_.use_address().required_fields_size(), UNKNOWN);
  } else {
    DCHECK(proto.has_use_card());
    is_autofill_card_ = true;
    prompt_ = proto.use_card().prompt();
    name_ = "";
    selector_ = Selector(proto.use_card().form_field_element());
  }
  selector_.MustBeVisible();
  DCHECK(!selector_.empty());
}

AutofillAction::~AutofillAction() = default;

void AutofillAction::InternalProcessAction(
    ProcessActionCallback action_callback) {
  process_action_callback_ = std::move(action_callback);

  // Ensure data already selected in a previous action.
  DCHECK((is_autofill_card_ &&
          delegate_->GetClientMemory()->has_selected_card()) ||
         (!is_autofill_card_ &&
          delegate_->GetClientMemory()->has_selected_address(name_)));

  bool has_valid_data =
      (is_autofill_card_ && delegate_->GetClientMemory()->selected_card()) ||
      (!is_autofill_card_ &&
       delegate_->GetClientMemory()->selected_address(name_));
  if (!has_valid_data) {
    auto* error_info = processed_action_proto_->mutable_status_details()
                           ->mutable_autofill_error_info();
    error_info->set_address_key_requested(name_);
    error_info->set_client_memory_address_key_names(
        delegate_->GetClientMemory()->GetAllAddressKeyNames());
    EndAction(PRECONDITION_FAILED);
    return;
  }

  FillFormWithData();
}

void AutofillAction::EndAction(ProcessedActionStatusProto status) {
  EndAction(ClientStatus(status));
}

void AutofillAction::EndAction(const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(process_action_callback_).Run(std::move(processed_action_proto_));
}

void AutofillAction::FillFormWithData() {
  delegate_->ShortWaitForElement(
      selector_, base::BindOnce(&AutofillAction::OnWaitForElement,
                                weak_ptr_factory_.GetWeakPtr()));
}

void AutofillAction::OnWaitForElement(bool element_found) {
  if (!element_found) {
    EndAction(ELEMENT_RESOLUTION_FAILED);
    return;
  }

  DCHECK(!selector_.empty());
  if (is_autofill_card_) {
    delegate_->GetFullCard(base::BindOnce(&AutofillAction::OnGetFullCard,
                                          weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  DVLOG(3) << "Retrieving address from client memory under '" << name_ << "'.";
  const autofill::AutofillProfile* profile =
      delegate_->GetClientMemory()->selected_address(name_);
  DCHECK(profile);
  delegate_->FillAddressForm(
      profile, selector_,
      base::BindOnce(&AutofillAction::OnAddressFormFilled,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AutofillAction::OnGetFullCard(std::unique_ptr<autofill::CreditCard> card,
                                   const base::string16& cvc) {
  if (!card) {
    // Gracefully shutdown the script. The empty message forces the use of the
    // default message.
    EndAction(GET_FULL_CARD_FAILED);
    return;
  }

  delegate_->FillCardForm(std::move(card), cvc, selector_,
                          base::BindOnce(&AutofillAction::OnCardFormFilled,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void AutofillAction::OnCardFormFilled(const ClientStatus& status) {
  // TODO(crbug.com/806868): Implement required fields checking for cards.
  EndAction(status);
}

void AutofillAction::OnAddressFormFilled(const ClientStatus& status) {
  // In case Autofill failed, we fail the action.
  if (!status.ok()) {
    EndAction(status);
    return;
  }

  CheckRequiredFields(/* allow_fallback */ true);
}

void AutofillAction::CheckRequiredFields(bool allow_fallback) {
  // If there are no required fields, finish the action successfully.
  if (proto_.use_address().required_fields().empty()) {
    EndAction(ACTION_APPLIED);
    return;
  }

  DCHECK(!batch_element_checker_);
  batch_element_checker_ = std::make_unique<BatchElementChecker>();
  for (int i = 0; i < proto_.use_address().required_fields_size(); i++) {
    auto& required_address_field = proto_.use_address().required_fields(i);
    DCHECK_GT(required_address_field.element().selectors_size(), 0);
    batch_element_checker_->AddFieldValueCheck(
        Selector(required_address_field.element()),
        base::BindOnce(&AutofillAction::OnGetRequiredFieldValue,
                       weak_ptr_factory_.GetWeakPtr(), i));
  }
  batch_element_checker_->AddAllDoneCallback(
      base::BindOnce(&AutofillAction::OnCheckRequiredFieldsDone,
                     weak_ptr_factory_.GetWeakPtr(), allow_fallback));
  delegate_->RunElementChecks(batch_element_checker_.get());
}

void AutofillAction::OnGetRequiredFieldValue(int required_fields_index,
                                             bool exists,
                                             const std::string& value) {
  required_fields_value_status_[required_fields_index] =
      value.empty() ? EMPTY : NOT_EMPTY;
}

void AutofillAction::OnCheckRequiredFieldsDone(bool allow_fallback) {
  batch_element_checker_.reset();

  // We process all fields with an empty value in order to perform the fallback
  // on all those fields, if any.
  bool validation_successful = true;
  for (FieldValueStatus status : required_fields_value_status_) {
    if (status == EMPTY) {
      validation_successful = false;
      break;
    }
  }

  if (validation_successful) {
    EndAction(ACTION_APPLIED);
    return;
  }

  if (!allow_fallback) {
    // Validation failed and we don't want to try the fallback.
    EndAction(MANUAL_FALLBACK);
    return;
  }

  // If there are any fallbacks for the empty fields, set them, otherwise fail
  // immediately.
  bool has_fallbacks = false;
  DVLOG(3) << "Retrieving address from client memory under '" << name_ << "'.";
  auto* profile = delegate_->GetClientMemory()->selected_address(name_);
  DCHECK(profile);
  for (int i = 0; i < proto_.use_address().required_fields_size(); i++) {
    if (required_fields_value_status_[i] == EMPTY &&
        !GetAddressFieldValue(
             profile, proto_.use_address().required_fields(i).address_field())
             .empty()) {
      has_fallbacks = true;
      break;
    }
  }
  if (!has_fallbacks) {
    EndAction(MANUAL_FALLBACK);
    return;
  }

  // Set the fallback values and check again.
  SetFallbackFieldValuesSequentially(0);
}

void AutofillAction::SetFallbackFieldValuesSequentially(
    int required_fields_index) {
  DCHECK_GE(required_fields_index, 0);

  // Skip non-empty fields.
  const auto& required_fields = proto_.use_address().required_fields();
  while (required_fields_index < required_fields.size() &&
         required_fields_value_status_[required_fields_index] != EMPTY) {
    required_fields_index++;
  }

  // If there are no more fields to set, check the required fields again,
  // but this time we don't want to try the fallback in case of failure.
  if (required_fields_index >= required_fields.size()) {
    DCHECK_EQ(required_fields_index, required_fields.size());

    CheckRequiredFields(/* allow_fallback */ false);
    return;
  }

  // Set the next field to its fallback value.
  DVLOG(3) << "Retrieving address from client memory under '" << name_ << "'.";
  std::string fallback_value = base::UTF16ToUTF8(GetAddressFieldValue(
      delegate_->GetClientMemory()->selected_address(name_),
      required_fields.Get(required_fields_index).address_field()));
  if (fallback_value.empty()) {
    // If there is no fallback value, we skip this failed field.
    SetFallbackFieldValuesSequentially(++required_fields_index);
    return;
  }

  DCHECK_GT(
      required_fields.Get(required_fields_index).element().selectors_size(), 0);
  delegate_->SetFieldValue(
      Selector(required_fields.Get(required_fields_index).element()),
      fallback_value,
      required_fields.Get(required_fields_index).simulate_key_presses(),
      required_fields.Get(required_fields_index).delay_in_millisecond(),
      base::BindOnce(&AutofillAction::OnSetFallbackFieldValue,
                     weak_ptr_factory_.GetWeakPtr(), required_fields_index));
}

void AutofillAction::OnSetFallbackFieldValue(int required_fields_index,
                                             const ClientStatus& status) {
  if (!status.ok()) {
    // Fallback failed: we stop the script without checking the fields.
    EndAction(MANUAL_FALLBACK);
    return;
  }
  SetFallbackFieldValuesSequentially(++required_fields_index);
}

base::string16 AutofillAction::GetAddressFieldValue(
    const autofill::AutofillProfile* profile,
    const UseAddressProto::RequiredField::AddressField& address_field) {
  // TODO(crbug.com/806868): Get the actual application locale.
  std::string app_locale = "en-US";
  switch (address_field) {
    case UseAddressProto::RequiredField::FIRST_NAME:
      return profile->GetInfo(autofill::NAME_FIRST, app_locale);
    case UseAddressProto::RequiredField::LAST_NAME:
      return profile->GetInfo(autofill::NAME_LAST, app_locale);
    case UseAddressProto::RequiredField::FULL_NAME:
      return profile->GetInfo(autofill::NAME_FULL, app_locale);
    case UseAddressProto::RequiredField::PHONE_NUMBER:
      return profile->GetInfo(autofill::PHONE_HOME_WHOLE_NUMBER, app_locale);
    case UseAddressProto::RequiredField::EMAIL:
      return profile->GetInfo(autofill::EMAIL_ADDRESS, app_locale);
    case UseAddressProto::RequiredField::ORGANIZATION:
      return profile->GetInfo(autofill::COMPANY_NAME, app_locale);
    case UseAddressProto::RequiredField::COUNTRY_CODE:
      return profile->GetInfo(autofill::ADDRESS_HOME_COUNTRY, app_locale);
    case UseAddressProto::RequiredField::REGION:
      return profile->GetInfo(autofill::ADDRESS_HOME_STATE, app_locale);
    case UseAddressProto::RequiredField::STREET_ADDRESS:
      return profile->GetInfo(autofill::ADDRESS_HOME_STREET_ADDRESS,
                              app_locale);
    case UseAddressProto::RequiredField::LOCALITY:
      return profile->GetInfo(autofill::ADDRESS_HOME_CITY, app_locale);
    case UseAddressProto::RequiredField::DEPENDANT_LOCALITY:
      return profile->GetInfo(autofill::ADDRESS_HOME_DEPENDENT_LOCALITY,
                              app_locale);
    case UseAddressProto::RequiredField::POSTAL_CODE:
      return profile->GetInfo(autofill::ADDRESS_HOME_ZIP, app_locale);
    case UseAddressProto::RequiredField::UNDEFINED:
      NOTREACHED();
      return base::string16();
  }
}
}  // namespace autofill_assistant
