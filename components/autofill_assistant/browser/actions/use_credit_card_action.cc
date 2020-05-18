// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/use_credit_card_action.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/actions/fallback_handler/fallback_data.h"
#include "components/autofill_assistant/browser/actions/fallback_handler/required_field.h"
#include "components/autofill_assistant/browser/actions/fallback_handler/required_fields_fallback_handler.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/user_model.h"

namespace autofill_assistant {
namespace {
std::unique_ptr<FallbackData> CreateFallbackData(
    const base::string16& cvc,
    const autofill::CreditCard& card) {
  auto fallback_data = std::make_unique<FallbackData>();

  fallback_data->field_values.emplace(
      static_cast<int>(
          UseCreditCardProto::RequiredField::CREDIT_CARD_VERIFICATION_CODE),
      base::UTF16ToUTF8(cvc));
  fallback_data->field_values.emplace(
      (int)UseCreditCardProto::RequiredField::CREDIT_CARD_RAW_NUMBER,
      base::UTF16ToUTF8(card.GetRawInfo(autofill::CREDIT_CARD_NUMBER)));
  fallback_data->field_values.emplace(
      static_cast<int>(UseCreditCardProto::RequiredField::CREDIT_CARD_NETWORK),
      autofill::data_util::GetPaymentRequestData(card.network())
          .basic_card_issuer_network);

  fallback_data->AddFormGroup(card);
  return fallback_data;
}
}  // namespace

UseCreditCardAction::UseCreditCardAction(ActionDelegate* delegate,
                                         const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto.has_use_card());
  std::vector<RequiredField> required_fields;
  for (const auto& required_field_proto : proto_.use_card().required_fields()) {
    if (required_field_proto.value_expression().empty()) {
      DVLOG(3) << "No fallback filling information provided, skipping field";
      continue;
    }

    required_fields.emplace_back();
    required_fields.back().FromProto(required_field_proto);
  }

  required_fields_fallback_handler_ =
      std::make_unique<RequiredFieldsFallbackHandler>(required_fields,
                                                      delegate);
  selector_ = Selector(proto.use_card().form_field_element());
  selector_.MustBeVisible();
}

UseCreditCardAction::~UseCreditCardAction() = default;

void UseCreditCardAction::InternalProcessAction(
    ProcessActionCallback action_callback) {
  process_action_callback_ = std::move(action_callback);

  if (selector_.empty()) {
    VLOG(1) << "UseCreditCard failed: |selector| empty";
    EndAction(ClientStatus(INVALID_ACTION));
    return;
  }

  // Ensure data already selected in a previous action.
  if (proto_.use_card().has_model_identifier()) {
    if (proto_.use_card().model_identifier().empty()) {
      VLOG(1) << "UseCreditCard failed: |model_identifier| set but empty";
      EndAction(ClientStatus(INVALID_ACTION));
      return;
    }
    auto credit_card_value = delegate_->GetUserModel()->GetValue(
        proto_.use_card().model_identifier());
    if (!credit_card_value.has_value()) {
      VLOG(1) << "UseCreditCard failed: "
              << proto_.use_card().model_identifier()
              << " not found in user model";
      EndAction(ClientStatus(PRECONDITION_FAILED));
      return;
    }
    if (credit_card_value->credit_cards().values().size() != 1) {
      VLOG(1) << "UseCreditCard failed: expected single card for "
              << proto_.use_card().model_identifier() << ", but got "
              << *credit_card_value;
    }
    auto* credit_card = delegate_->GetUserModel()->GetCreditCard(
        credit_card_value->credit_cards().values(0).guid());
    if (credit_card == nullptr) {
      VLOG(1) << "UseCreditCard failed: card not found for guid "
              << *credit_card_value;
      EndAction(ClientStatus(PRECONDITION_FAILED));
      return;
    }
    credit_card_ = std::make_unique<autofill::CreditCard>(*credit_card);
  } else {
    auto* credit_card = delegate_->GetUserData()->selected_card_.get();
    if (credit_card == nullptr) {
      VLOG(1) << "UseCreditCard failed: card not found in user_data";
      EndAction(ClientStatus(PRECONDITION_FAILED));
      return;
    }
    credit_card_ = std::make_unique<autofill::CreditCard>(*credit_card);
  }
  DCHECK(credit_card_ != nullptr);

  FillFormWithData();
}

void UseCreditCardAction::EndAction(
    const ClientStatus& final_status,
    const base::Optional<ClientStatus>& optional_details_status) {
  UpdateProcessedAction(final_status);
  if (optional_details_status.has_value() && !optional_details_status->ok()) {
    processed_action_proto_->mutable_status_details()->MergeFrom(
        optional_details_status->details());
  }
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

  DCHECK(credit_card_ != nullptr);
  delegate_->GetFullCard(credit_card_.get(),
                         base::BindOnce(&UseCreditCardAction::OnGetFullCard,
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

  auto fallback_data = CreateFallbackData(cvc, *card);
  delegate_->FillCardForm(
      std::move(card), cvc, selector_,
      base::BindOnce(&UseCreditCardAction::OnFormFilled,
                     weak_ptr_factory_.GetWeakPtr(), std::move(fallback_data)));
}

void UseCreditCardAction::OnFormFilled(
    std::unique_ptr<FallbackData> fallback_data,
    const ClientStatus& status) {
  required_fields_fallback_handler_->CheckAndFallbackRequiredFields(
      status, std::move(fallback_data),
      base::BindOnce(&UseCreditCardAction::EndAction,
                     weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace autofill_assistant
