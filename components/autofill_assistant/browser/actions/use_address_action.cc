// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/use_address_action.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"
#include "components/autofill_assistant/browser/client_memory.h"
#include "components/autofill_assistant/browser/client_status.h"

namespace autofill_assistant {

UseAddressAction::UseAddressAction(ActionDelegate* delegate,
                                   const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto.has_use_address());
  prompt_ = proto.use_address().prompt();
  name_ = proto.use_address().name();
  for (const auto& required_field_proto :
       proto_.use_address().required_fields()) {
    required_fields_.emplace_back();
    RequiredField& required_field = required_fields_.back();
    required_field.address_field = required_field_proto.address_field();
    required_field.selector = Selector(required_field_proto.element());
    required_field.simulate_key_presses =
        required_field_proto.simulate_key_presses();
    required_field.delay_in_millisecond =
        required_field_proto.delay_in_millisecond();
    required_field.forced = required_field_proto.forced();
  }

  selector_ = Selector(proto.use_address().form_field_element());
  selector_.MustBeVisible();
  DCHECK(!selector_.empty());
}

UseAddressAction::~UseAddressAction() = default;

void UseAddressAction::InternalProcessAction(
    ProcessActionCallback action_callback) {
  process_action_callback_ = std::move(action_callback);

  // Ensure data already selected in a previous action.
  auto* client_memory = delegate_->GetClientMemory();
  if (!client_memory->has_selected_address(name_)) {
    auto* error_info = processed_action_proto_->mutable_status_details()
                           ->mutable_autofill_error_info();
    error_info->set_address_key_requested(name_);
    error_info->set_client_memory_address_key_names(
        client_memory->GetAllAddressKeyNames());
    error_info->set_address_pointee_was_null(
        !client_memory->has_selected_address(name_) ||
        !client_memory->selected_address(name_));
    EndAction(PRECONDITION_FAILED);
    return;
  }

  FillFormWithData();
}

void UseAddressAction::EndAction(ProcessedActionStatusProto status) {
  EndAction(ClientStatus(status));
}

void UseAddressAction::EndAction(const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(process_action_callback_).Run(std::move(processed_action_proto_));
}

void UseAddressAction::FillFormWithData() {
  delegate_->ShortWaitForElement(
      selector_, base::BindOnce(&UseAddressAction::OnWaitForElement,
                                weak_ptr_factory_.GetWeakPtr()));
}

void UseAddressAction::OnWaitForElement(const ClientStatus& element_status) {
  if (!element_status.ok()) {
    EndAction(element_status.proto_status());
    return;
  }

  DCHECK(!selector_.empty());
  DVLOG(3) << "Retrieving address from client memory under '" << name_ << "'.";
  const autofill::AutofillProfile* profile =
      delegate_->GetClientMemory()->selected_address(name_);
  DCHECK(profile);
  auto fallback_data = std::make_unique<FallbackData>();
  fallback_data->profile = profile;
  delegate_->FillAddressForm(
      profile, selector_,
      base::BindOnce(&UseAddressAction::OnFormFilled,
                     weak_ptr_factory_.GetWeakPtr(), std::move(fallback_data)));
}

void UseAddressAction::OnFormFilled(std::unique_ptr<FallbackData> fallback_data,
                                    const ClientStatus& status) {
  // In case Autofill failed, we fail the action.
  if (!status.ok()) {
    EndAction(status);
    return;
  }
  CheckRequiredFields(std::move(fallback_data));
}

void UseAddressAction::CheckRequiredFields(
    std::unique_ptr<FallbackData> fallback_data) {
  // If there are no required fields, finish the action successfully.
  if (required_fields_.empty()) {
    EndAction(ACTION_APPLIED);
    return;
  }

  DCHECK(!batch_element_checker_);
  batch_element_checker_ = std::make_unique<BatchElementChecker>();
  for (size_t i = 0; i < required_fields_.size(); i++) {
    if (required_fields_[i].forced)
      continue;

    batch_element_checker_->AddFieldValueCheck(
        required_fields_[i].selector,
        base::BindOnce(&UseAddressAction::OnGetRequiredFieldValue,
                       weak_ptr_factory_.GetWeakPtr(), i));
  }
  batch_element_checker_->AddAllDoneCallback(
      base::BindOnce(&UseAddressAction::OnCheckRequiredFieldsDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(fallback_data)));
  delegate_->RunElementChecks(batch_element_checker_.get());
}

void UseAddressAction::OnGetRequiredFieldValue(
    size_t required_fields_index,
    const ClientStatus& element_status,
    const std::string& value) {
  required_fields_[required_fields_index].status =
      value.empty() ? EMPTY : NOT_EMPTY;
}

void UseAddressAction::OnCheckRequiredFieldsDone(
    std::unique_ptr<FallbackData> fallback_data) {
  batch_element_checker_.reset();

  // We process all fields with an empty value in order to perform the fallback
  // on all those fields, if any.
  bool should_fallback = false;
  for (const RequiredField& required_field : required_fields_) {
    if (required_field.ShouldFallback(fallback_data != nullptr)) {
      should_fallback = true;
      break;
    }
  }

  if (!should_fallback) {
    EndAction(ACTION_APPLIED);
    return;
  }

  if (!fallback_data) {
    // Validation failed and we don't want to try the fallback.
    EndAction(MANUAL_FALLBACK);
    return;
  }

  // If there are any fallbacks for the empty fields, set them, otherwise fail
  // immediately.
  bool has_fallbacks = false;
  for (const RequiredField& required_field : required_fields_) {
    if (!required_field.ShouldFallback(/* has_fallback_data= */ true))
      continue;

    if (!GetFallbackValue(required_field, *fallback_data).empty()) {
      has_fallbacks = true;
    }
  }
  if (!has_fallbacks) {
    EndAction(MANUAL_FALLBACK);
    return;
  }

  // Set the fallback values and check again.
  SetFallbackFieldValuesSequentially(0, std::move(fallback_data));
}

void UseAddressAction::SetFallbackFieldValuesSequentially(
    size_t required_fields_index,
    std::unique_ptr<FallbackData> fallback_data) {
  // Skip non-empty fields.
  while (required_fields_index < required_fields_.size() &&
         !required_fields_[required_fields_index].ShouldFallback(
             fallback_data != nullptr)) {
    required_fields_index++;
  }

  // If there are no more fields to set, check the required fields again,
  // but this time we don't want to try the fallback in case of failure.
  if (required_fields_index >= required_fields_.size()) {
    DCHECK_EQ(required_fields_index, required_fields_.size());

    CheckRequiredFields(/* fallback_data= */ nullptr);
    return;
  }

  // Set the next field to its fallback value.
  const RequiredField& required_field = required_fields_[required_fields_index];
  std::string fallback_value = GetFallbackValue(required_field, *fallback_data);
  if (fallback_value.empty()) {
    DVLOG(3) << "No fallback for " << required_field.selector;
    // If there is no fallback value, we skip this failed field.
    SetFallbackFieldValuesSequentially(++required_fields_index,
                                       std::move(fallback_data));
    return;
  }
  DVLOG(3) << "Setting fallback value for " << required_field.selector;

  delegate_->SetFieldValue(
      required_field.selector, fallback_value,
      required_field.simulate_key_presses, required_field.delay_in_millisecond,
      base::BindOnce(&UseAddressAction::OnSetFallbackFieldValue,
                     weak_ptr_factory_.GetWeakPtr(), required_fields_index,
                     std::move(fallback_data)));
}

void UseAddressAction::OnSetFallbackFieldValue(
    size_t required_fields_index,
    std::unique_ptr<FallbackData> fallback_data,
    const ClientStatus& status) {
  if (!status.ok()) {
    // Fallback failed: we stop the script without checking the fields.
    EndAction(MANUAL_FALLBACK);
    return;
  }
  SetFallbackFieldValuesSequentially(++required_fields_index,
                                     std::move(fallback_data));
}

std::string UseAddressAction::GetFallbackValue(
    const RequiredField& required_field,
    const FallbackData& fallback_data) {
  if (required_field.address_field !=
          UseAddressProto::RequiredField::UNDEFINED &&
      fallback_data.profile) {
    return base::UTF16ToUTF8(GetAddressFieldValue(
        fallback_data.profile, required_field.address_field));
  }

  NOTREACHED() << "Unsupported field type for " << required_field.selector;
  return "";
}

base::string16 UseAddressAction::GetAddressFieldValue(
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
