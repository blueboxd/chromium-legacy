// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/fallback_handler/required_fields_fallback_handler.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/optional.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"
#include "components/autofill_assistant/browser/client_status.h"

namespace autofill_assistant {
namespace {

const char kSelectElementTag[] = "SELECT";

AutofillErrorInfoProto::AutofillFieldError* AddAutofillError(
    const RequiredField& required_field,
    ClientStatus* client_status) {
  auto* field_error = client_status->mutable_details()
                          ->mutable_autofill_error_info()
                          ->add_autofill_field_error();
  *field_error->mutable_field() =
      required_field.selector.ToElementReferenceProto();
  field_error->set_value_expression(required_field.value_expression);
  return field_error;
}

void FillStatusDetailsWithMissingFallbackData(
    const RequiredField& required_field,
    ClientStatus* client_status) {
  auto* field_error = AddAutofillError(required_field, client_status);
  field_error->set_no_fallback_value(true);
}

void FillStatusDetailsWithError(const RequiredField& required_field,
                                ProcessedActionStatusProto error_status,
                                ClientStatus* client_status) {
  auto* field_error = AddAutofillError(required_field, client_status);
  field_error->set_status(error_status);
}

}  // namespace

RequiredFieldsFallbackHandler::RequiredFieldsFallbackHandler(
    const std::vector<RequiredField>& required_fields,
    ActionDelegate* action_delegate) {
  required_fields_.assign(required_fields.begin(), required_fields.end());
  action_delegate_ = action_delegate;
}

RequiredFieldsFallbackHandler::~RequiredFieldsFallbackHandler() = default;

void RequiredFieldsFallbackHandler::CheckAndFallbackRequiredFields(
    const ClientStatus& initial_autofill_status,
    std::unique_ptr<FallbackData> fallback_data,
    base::OnceCallback<void(const ClientStatus&,
                            const base::Optional<ClientStatus>&)>
        status_update_callback) {
  DCHECK(fallback_data != nullptr);

  client_status_ = initial_autofill_status;
  status_update_callback_ = std::move(status_update_callback);

  if (required_fields_.empty()) {
    if (!initial_autofill_status.ok()) {
      VLOG(1) << __func__ << " Autofill failed and no fallback provided "
              << initial_autofill_status.proto_status();
    }

    std::move(status_update_callback_)
        .Run(initial_autofill_status, base::nullopt);
    return;
  }

  CheckAllRequiredFields(std::move(fallback_data));
}

void RequiredFieldsFallbackHandler::CheckAllRequiredFields(
    std::unique_ptr<FallbackData> fallback_data) {
  DCHECK(!batch_element_checker_);
  batch_element_checker_ = std::make_unique<BatchElementChecker>();
  for (size_t i = 0; i < required_fields_.size(); i++) {
    // First run (with fallback data) we skip checking forced fields, since
    // we overwrite them anyway. Second run (without fallback data) forced
    // fields should be checked.
    if (required_fields_[i].forced && fallback_data != nullptr) {
      continue;
    }

    // We cannot check the value of elements with custom fallback clicks. Those
    // elements are JS driven structures that in most cases lack a "value"
    // attribute. We define a successful click on the element as successfully
    // filling the form field.
    if (required_fields_[i].fallback_click_element.has_value()) {
      continue;
    }

    batch_element_checker_->AddFieldValueCheck(
        required_fields_[i].selector,
        base::BindOnce(&RequiredFieldsFallbackHandler::OnGetRequiredFieldValue,
                       weak_ptr_factory_.GetWeakPtr(), i));
  }

  batch_element_checker_->AddAllDoneCallback(
      base::BindOnce(&RequiredFieldsFallbackHandler::OnCheckRequiredFieldsDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(fallback_data)));
  action_delegate_->RunElementChecks(batch_element_checker_.get());
}

void RequiredFieldsFallbackHandler::OnGetRequiredFieldValue(
    size_t required_fields_index,
    const ClientStatus& element_status,
    const std::string& value) {
  required_fields_[required_fields_index].status =
      value.empty() ? RequiredField::EMPTY : RequiredField::NOT_EMPTY;
}

void RequiredFieldsFallbackHandler::OnCheckRequiredFieldsDone(
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
    std::move(status_update_callback_)
        .Run(ClientStatus(ACTION_APPLIED), client_status_);
    return;
  }

  if (fallback_data == nullptr) {
    // Validation failed and we don't want to try the fallback.
    std::move(status_update_callback_)
        .Run(ClientStatus(AUTOFILL_INCOMPLETE), client_status_);
    return;
  }

  // If there are any fallbacks for the empty fields, set them, otherwise fail
  // immediately.
  bool has_fallbacks = false;
  for (const RequiredField& required_field : required_fields_) {
    if (!required_field.ShouldFallback(/* has_fallback_data= */ true)) {
      continue;
    }

    if (fallback_data->GetValueByExpression(required_field.value_expression)
            .has_value()) {
      has_fallbacks = true;
    } else {
      FillStatusDetailsWithMissingFallbackData(required_field, &client_status_);
    }
  }
  if (!has_fallbacks) {
    std::move(status_update_callback_)
        .Run(ClientStatus(AUTOFILL_INCOMPLETE), client_status_);
    return;
  }

  // Set the fallback values and check again.
  SetFallbackFieldValuesSequentially(0, std::move(fallback_data));
}

void RequiredFieldsFallbackHandler::SetFallbackFieldValuesSequentially(
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

    return CheckAllRequiredFields(/* fallback_data= */ nullptr);
  }

  // Set the next field to its fallback value.
  const RequiredField& required_field = required_fields_[required_fields_index];
  auto fallback_value =
      fallback_data->GetValueByExpression(required_field.value_expression);
  if (!fallback_value.has_value()) {
    VLOG(3) << "No fallback for " << required_field.selector;
    // If there is no fallback value, we skip this failed field.
    return SetFallbackFieldValuesSequentially(++required_fields_index,
                                              std::move(fallback_data));
  }

  if (required_field.fallback_click_element.has_value()) {
    DVLOG(3) << "Clicking on " << required_field.selector;
    ClickType click_type = required_field.click_type;
    if (click_type == ClickType::NOT_SET) {
      // default: TAP
      click_type = ClickType::TAP;
    }
    action_delegate_->ClickOrTapElement(
        required_field.selector, click_type,
        base::BindOnce(
            &RequiredFieldsFallbackHandler::OnClickOrTapFallbackElement,
            weak_ptr_factory_.GetWeakPtr(), fallback_value.value(),
            required_fields_index, std::move(fallback_data)));
    return;
  }

  DVLOG(3) << "Getting element tag for " << required_field.selector;
  action_delegate_->GetElementTag(
      required_field.selector,
      base::BindOnce(&RequiredFieldsFallbackHandler::OnGetFallbackFieldTag,
                     weak_ptr_factory_.GetWeakPtr(), fallback_value.value(),
                     required_fields_index, std::move(fallback_data)));
}

void RequiredFieldsFallbackHandler::OnGetFallbackFieldTag(
    const std::string& value,
    size_t required_fields_index,
    std::unique_ptr<FallbackData> fallback_data,
    const ClientStatus& element_tag_status,
    const std::string& element_tag) {
  if (!element_tag_status.ok()) {
    DVLOG(3) << "Status for element tag was "
             << element_tag_status.proto_status();
  }

  const RequiredField& required_field = required_fields_[required_fields_index];
  VLOG(3) << "Setting fallback value for " << required_field.selector << " ("
          << element_tag << ")";
  if (element_tag == kSelectElementTag) {
    DropdownSelectStrategy select_strategy;
    if (required_field.select_strategy != UNSPECIFIED_SELECT_STRATEGY) {
      select_strategy = required_field.select_strategy;
    } else {
      // This is the legacy default.
      select_strategy = LABEL_STARTS_WITH;
    }

    action_delegate_->SelectOption(
        required_field.selector, value, select_strategy,
        base::BindOnce(&RequiredFieldsFallbackHandler::OnSetFallbackFieldValue,
                       weak_ptr_factory_.GetWeakPtr(), required_fields_index,
                       std::move(fallback_data)));
    return;
  }

  action_delegate_->SetFieldValue(
      required_field.selector, value, required_field.fill_strategy,
      required_field.delay_in_millisecond,
      base::BindOnce(&RequiredFieldsFallbackHandler::OnSetFallbackFieldValue,
                     weak_ptr_factory_.GetWeakPtr(), required_fields_index,
                     std::move(fallback_data)));
}

void RequiredFieldsFallbackHandler::OnClickOrTapFallbackElement(
    const std::string& value,
    size_t required_fields_index,
    std::unique_ptr<FallbackData> fallback_data,
    const ClientStatus& element_click_status) {
  const RequiredField& required_field = required_fields_[required_fields_index];
  if (!element_click_status.ok()) {
    FillStatusDetailsWithError(
        required_field, element_click_status.proto_status(), &client_status_);

    // Fallback failed: we stop the script without checking the other fields.
    std::move(status_update_callback_)
        .Run(ClientStatus(AUTOFILL_INCOMPLETE), client_status_);
    return;
  }

  DCHECK(required_field.fallback_click_element.has_value());
  Selector value_selector = required_field.fallback_click_element.value();
  value_selector.inner_text_pattern = value;
  value_selector.must_be_visible = true;

  DVLOG(3) << "Finding option for " << required_field.selector;
  action_delegate_->ShortWaitForElement(
      value_selector,
      base::BindOnce(&RequiredFieldsFallbackHandler::OnShortWaitForElement,
                     weak_ptr_factory_.GetWeakPtr(), value_selector,
                     required_fields_index, std::move(fallback_data)));
}

void RequiredFieldsFallbackHandler::OnShortWaitForElement(
    const Selector& selector_to_click,
    size_t required_fields_index,
    std::unique_ptr<FallbackData> fallback_data,
    const ClientStatus& find_element_status) {
  const RequiredField& required_field = required_fields_[required_fields_index];
  if (!find_element_status.ok()) {
    FillStatusDetailsWithError(
        required_field, find_element_status.proto_status(), &client_status_);

    // Fallback failed: we stop the script without checking the other fields.
    std::move(status_update_callback_)
        .Run(ClientStatus(AUTOFILL_INCOMPLETE), client_status_);
    return;
  }

  DVLOG(3) << "Clicking option for " << required_field.selector;
  ClickType click_type = required_field.click_type;
  if (click_type == ClickType::NOT_SET) {
    // default: TAP
    click_type = ClickType::TAP;
  }
  action_delegate_->ClickOrTapElement(
      selector_to_click, click_type,
      base::BindOnce(&RequiredFieldsFallbackHandler::OnSetFallbackFieldValue,
                     weak_ptr_factory_.GetWeakPtr(), required_fields_index,
                     std::move(fallback_data)));
}

void RequiredFieldsFallbackHandler::OnSetFallbackFieldValue(
    size_t required_fields_index,
    std::unique_ptr<FallbackData> fallback_data,
    const ClientStatus& set_field_status) {
  if (!set_field_status.ok()) {
    FillStatusDetailsWithError(required_fields_[required_fields_index],
                               set_field_status.proto_status(),
                               &client_status_);

    // Fallback failed: we stop the script without checking the other fields.
    std::move(status_update_callback_)
        .Run(ClientStatus(AUTOFILL_INCOMPLETE), client_status_);
    return;
  }

  SetFallbackFieldValuesSequentially(++required_fields_index,
                                     std::move(fallback_data));
}

}  // namespace autofill_assistant
