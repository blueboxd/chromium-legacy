// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/use_credit_card_action.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"
#include "components/autofill_assistant/browser/client_memory.h"
#include "components/autofill_assistant/browser/client_status.h"

namespace autofill_assistant {
UseCreditCardAction::FallbackData::FallbackData() {}

UseCreditCardAction::UseCreditCardAction(ActionDelegate* delegate,
                                         const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto.has_use_card());
  prompt_ = proto.use_card().prompt();
  for (const auto& required_field_proto : proto_.use_card().required_fields()) {
    required_fields_.emplace_back();
    RequiredField& required_field = required_fields_.back();
    required_field.card_field = required_field_proto.card_field();
    required_field.selector = Selector(required_field_proto.element());
    required_field.simulate_key_presses =
        required_field_proto.simulate_key_presses();
    required_field.delay_in_millisecond =
        required_field_proto.delay_in_millisecond();
    required_field.forced = required_field_proto.forced();
  }

  selector_ = Selector(proto.use_card().form_field_element());
  selector_.MustBeVisible();
  DCHECK(!selector_.empty());
}

UseCreditCardAction::~UseCreditCardAction() = default;

void UseCreditCardAction::InternalProcessAction(
    ProcessActionCallback action_callback) {
  process_action_callback_ = std::move(action_callback);

  // Ensure data already selected in a previous action.
  auto* client_memory = delegate_->GetClientMemory();
  if (!client_memory->has_selected_card()) {
    EndAction(PRECONDITION_FAILED);
    return;
  }

  FillFormWithData();
}

void UseCreditCardAction::EndAction(ProcessedActionStatusProto status) {
  EndAction(ClientStatus(status));
}

void UseCreditCardAction::EndAction(const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(process_action_callback_).Run(std::move(processed_action_proto_));
}

void UseCreditCardAction::FillFormWithData() {
  delegate_->ShortWaitForElement(
      selector_, base::BindOnce(&UseCreditCardAction::OnWaitForElement,
                                weak_ptr_factory_.GetWeakPtr()));
}

void UseCreditCardAction::OnWaitForElement(const ClientStatus& element_status) {
  if (!element_status.ok()) {
    EndAction(element_status.proto_status());
    return;
  }

  delegate_->GetFullCard(base::BindOnce(&UseCreditCardAction::OnGetFullCard,
                                        weak_ptr_factory_.GetWeakPtr()));
  return;
}

void UseCreditCardAction::OnGetFullCard(
    std::unique_ptr<autofill::CreditCard> card,
    const base::string16& cvc) {
  if (!card) {
    EndAction(GET_FULL_CARD_FAILED);
    return;
  }

  auto fallback_data = std::make_unique<FallbackData>();
  fallback_data->cvc = base::UTF16ToUTF8(cvc);
  fallback_data->expiration_month = card->expiration_month();
  fallback_data->expiration_year = card->expiration_year();
  fallback_data->card_holder_name =
      base::UTF16ToUTF8(card->GetRawInfo(autofill::CREDIT_CARD_NAME_FULL));
  fallback_data->card_number =
      base::UTF16ToUTF8(card->GetRawInfo(autofill::CREDIT_CARD_NUMBER));

  delegate_->FillCardForm(
      std::move(card), cvc, selector_,
      base::BindOnce(&UseCreditCardAction::OnFormFilled,
                     weak_ptr_factory_.GetWeakPtr(), std::move(fallback_data)));
}

void UseCreditCardAction::OnFormFilled(
    std::unique_ptr<FallbackData> fallback_data,
    const ClientStatus& status) {
  // In case Autofill failed, we fail the action.
  if (!status.ok()) {
    EndAction(status);
    return;
  }
  CheckRequiredFields(std::move(fallback_data));
}

void UseCreditCardAction::CheckRequiredFields(
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
        base::BindOnce(&UseCreditCardAction::OnGetRequiredFieldValue,
                       weak_ptr_factory_.GetWeakPtr(), i));
  }
  batch_element_checker_->AddAllDoneCallback(
      base::BindOnce(&UseCreditCardAction::OnCheckRequiredFieldsDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(fallback_data)));
  delegate_->RunElementChecks(batch_element_checker_.get());
}

void UseCreditCardAction::OnGetRequiredFieldValue(
    size_t required_fields_index,
    const ClientStatus& element_status,
    const std::string& value) {
  required_fields_[required_fields_index].status =
      value.empty() ? EMPTY : NOT_EMPTY;
}

void UseCreditCardAction::OnCheckRequiredFieldsDone(
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

void UseCreditCardAction::SetFallbackFieldValuesSequentially(
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
      base::BindOnce(&UseCreditCardAction::OnSetFallbackFieldValue,
                     weak_ptr_factory_.GetWeakPtr(), required_fields_index,
                     std::move(fallback_data)));
}

void UseCreditCardAction::OnSetFallbackFieldValue(
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

std::string UseCreditCardAction::GetFallbackValue(
    const RequiredField& required_field,
    const FallbackData& fallback_data) {
  if (required_field.card_field !=
      UseCreditCardProto::RequiredField::UNDEFINED) {
    return GetCreditCardFieldValue(required_field.card_field, fallback_data);
  }

  NOTREACHED() << "Unsupported field type for " << required_field.selector;
  return "";
}

std::string UseCreditCardAction::GetCreditCardFieldValue(
    UseCreditCardProto::RequiredField::CardField field,
    const FallbackData& fallback_data) {
  switch (field) {
    case UseCreditCardProto::RequiredField::CREDIT_CARD_VERIFICATION_CODE:
      return fallback_data.cvc;

    case UseCreditCardProto::RequiredField::CREDIT_CARD_EXP_MONTH:
      if (fallback_data.expiration_month > 0)
        return base::StringPrintf("%02d", fallback_data.expiration_month);
      break;

    case UseCreditCardProto::RequiredField::CREDIT_CARD_EXP_2_DIGIT_YEAR:
      if (fallback_data.expiration_year > 0)
        return base::StringPrintf("%02d", fallback_data.expiration_year % 100);
      break;

    case UseCreditCardProto::RequiredField::CREDIT_CARD_EXP_4_DIGIT_YEAR:
      if (fallback_data.expiration_year > 0)
        return base::NumberToString(fallback_data.expiration_year);
      break;

    case UseCreditCardProto::RequiredField::CREDIT_CARD_CARD_HOLDER_NAME:
      return fallback_data.card_holder_name;
      break;

    case UseCreditCardProto::RequiredField::CREDIT_CARD_NUMBER:
      return fallback_data.card_number;
      break;

    case UseCreditCardProto::RequiredField::UNDEFINED:
      NOTREACHED();
      return "";
  }
  return "";
}
}  // namespace autofill_assistant
