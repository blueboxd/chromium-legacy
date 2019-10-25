// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/use_credit_card_action.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/actions/required_fields_fallback_handler.h"
#include "components/autofill_assistant/browser/client_memory.h"
#include "components/autofill_assistant/browser/client_status.h"

namespace autofill_assistant {
using RequiredField = RequiredFieldsFallbackHandler::RequiredField;
using FallbackData = RequiredFieldsFallbackHandler::FallbackData;

UseCreditCardAction::UseCreditCardAction(ActionDelegate* delegate,
                                         const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto.has_use_card());
  prompt_ = proto.use_card().prompt();
  std::vector<RequiredField> required_fields;
  for (const auto& required_field_proto : proto_.use_card().required_fields()) {
    required_fields.emplace_back();
    RequiredField& required_field = required_fields.back();
    required_field.card_field = required_field_proto.card_field();
    required_field.selector = Selector(required_field_proto.element());
    required_field.simulate_key_presses =
        required_field_proto.simulate_key_presses();
    required_field.delay_in_millisecond =
        required_field_proto.delay_in_millisecond();
    required_field.forced = required_field_proto.forced();
  }

  required_fields_fallback_handler_ =
      std::make_unique<RequiredFieldsFallbackHandler>(
          required_fields,
          base::BindRepeating(&UseCreditCardAction::GetFallbackValue,
                              base::Unretained(this)),
          base::BindOnce(&UseCreditCardAction::EndAction,
                         base::Unretained(this)),
          delegate);
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
    EndAction(ClientStatus(PRECONDITION_FAILED));
    return;
  }

  FillFormWithData();
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
    EndAction(ClientStatus(element_status.proto_status()));
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
    EndAction(ClientStatus(GET_FULL_CARD_FAILED));
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

  required_fields_fallback_handler_->CheckAndFallbackRequiredFields(
      std::move(fallback_data));
}

std::string UseCreditCardAction::GetFallbackValue(
    const RequiredField& required_field,
    const FallbackData& fallback_data) {
  auto field = required_field.card_field;
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

    case UseCreditCardProto::RequiredField::CREDIT_CARD_EXP_MM_YY:
      if (fallback_data.expiration_month > 0 &&
          fallback_data.expiration_year > 0)
        return base::StrCat(
            {base::StringPrintf("%02d", fallback_data.expiration_month), "/",
             base::StringPrintf("%02d", fallback_data.expiration_year % 100)});
      break;

    case UseCreditCardProto::RequiredField::UNDEFINED:
      NOTREACHED();
      return "";
  }
  return "";
}
}  // namespace autofill_assistant
