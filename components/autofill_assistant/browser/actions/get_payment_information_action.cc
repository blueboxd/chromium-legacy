// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/get_payment_information_action.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_memory.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_assistant {

GetPaymentInformationAction::GetPaymentInformationAction(
    ActionDelegate* delegate,
    const ActionProto& proto)
    : Action(delegate, proto), weak_ptr_factory_(this) {
  DCHECK(proto_.has_get_payment_information());
}

GetPaymentInformationAction::~GetPaymentInformationAction() {}

void GetPaymentInformationAction::InternalProcessAction(
    ProcessActionCallback callback) {
  const GetPaymentInformationProto& get_payment_information =
      proto_.get_payment_information();

  auto payment_options = std::make_unique<PaymentRequestOptions>();
  payment_options->request_terms_and_conditions =
      get_payment_information.request_terms_and_conditions();
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

  std::string confirm_text = get_payment_information.confirm_button_text();
  if (confirm_text.empty()) {
    confirm_text =
        l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_PAYMENT_INFO_CONFIRM);
  }
  payment_options->confirm_chip.set_text(confirm_text);
  payment_options->confirm_chip.set_type(HIGHLIGHTED_ACTION);
  payment_options->confirm_direct_action =
      get_payment_information.confirm_direct_action();

  switch (get_payment_information.terms_and_conditions_state()) {
    case GetPaymentInformationProto::NOT_SELECTED:
      payment_options->initial_terms_and_conditions = NOT_SELECTED;
      break;
    case GetPaymentInformationProto::ACCEPTED:
      payment_options->initial_terms_and_conditions = ACCEPTED;
      break;
    case GetPaymentInformationProto::REVIEW_REQUIRED:
      payment_options->initial_terms_and_conditions = REQUIRES_REVIEW;
      break;
  }

  payment_options->callback =
      base::BindOnce(&GetPaymentInformationAction::OnGetPaymentInformation,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(get_payment_information), std::move(callback));
  if (get_payment_information.has_prompt()) {
    delegate_->SetStatusMessage(get_payment_information.prompt());
  }
  delegate_->GetPaymentInformation(std::move(payment_options));
}

void GetPaymentInformationAction::OnGetPaymentInformation(
    const GetPaymentInformationProto& get_payment_information,
    ProcessActionCallback callback,
    std::unique_ptr<PaymentInformation> payment_information) {
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
      DVLOG(3) << "Storing selected credit card in client memory.";
      delegate_->GetClientMemory()->set_selected_card(
          std::move(payment_information->card));

      if (!get_payment_information.billing_address_name().empty()) {
        DCHECK(payment_information->billing_address);
        DVLOG(3) << "Storing billing address in client memory under '"
                 << get_payment_information.billing_address_name() << "'.";
        delegate_->GetClientMemory()->set_selected_address(
            get_payment_information.billing_address_name(),
            std::move(payment_information->billing_address));
      }
    }

    if (!get_payment_information.shipping_address_name().empty()) {
      DCHECK(payment_information->shipping_address);
      DVLOG(3) << "Storing shipping address in client memory under '"
               << get_payment_information.shipping_address_name() << "'.";
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
        DVLOG(3) << "Storing contact info in client memory under '"
                 << contact_details_proto.contact_details_name() << "'.";
        delegate_->GetClientMemory()->set_selected_address(
            contact_details_proto.contact_details_name(),
            std::make_unique<autofill::AutofillProfile>(contact_profile));
      }
    }
    processed_action_proto_->mutable_payment_details()
        ->set_is_terms_and_conditions_accepted(
            payment_information->terms_and_conditions ==
            TermsAndConditionsState::ACCEPTED);
    processed_action_proto_->mutable_payment_details()->set_payer_email(
        payment_information->payer_email);
  }

  UpdateProcessedAction(succeed ? ACTION_APPLIED : PAYMENT_REQUEST_ERROR);
  std::move(callback).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
