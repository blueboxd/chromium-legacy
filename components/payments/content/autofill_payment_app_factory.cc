// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/autofill_payment_app_factory.h"

#include <vector>

#include "base/feature_list.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/payments/content/content_payment_request_delegate.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/core/autofill_payment_app.h"
#include "components/payments/core/features.h"

namespace payments {

// static
std::unique_ptr<PaymentApp>
AutofillPaymentAppFactory::ConvertCardToPaymentAppIfSupportedNetwork(
    const autofill::CreditCard& card,
    base::WeakPtr<Delegate> delegate) {
  std::string basic_card_network =
      autofill::data_util::GetPaymentRequestData(card.network())
          .basic_card_issuer_network;
  if (!delegate->GetSpec()->supported_card_networks_set().count(
          basic_card_network) ||
      !delegate->GetSpec()->supported_card_types_set().count(
          card.card_type())) {
    return nullptr;
  }

  // The total number of card types: credit, debit, prepaid, unknown.
  constexpr size_t kTotalNumberOfCardTypes = 4U;

  // Whether the card type (credit, debit, prepaid) matches the type that the
  // merchant has requested exactly. This should be false for unknown card
  // types, if the merchant cannot accept some card types.
  bool matches_merchant_card_type_exactly =
      card.card_type() != autofill::CreditCard::CARD_TYPE_UNKNOWN ||
      delegate->GetSpec()->supported_card_types_set().size() ==
          kTotalNumberOfCardTypes;

  auto app = std::make_unique<AutofillPaymentApp>(
      basic_card_network, card, matches_merchant_card_type_exactly,
      delegate->GetBillingProfiles(),
      delegate->GetPaymentRequestDelegate()->GetApplicationLocale(),
      delegate->GetPaymentRequestDelegate());

  app->set_is_requested_autofill_data_available(
      delegate->IsRequestedAutofillDataAvailable());

  return app;
}

AutofillPaymentAppFactory::AutofillPaymentAppFactory()
    : PaymentAppFactory(PaymentApp::Type::AUTOFILL) {}

AutofillPaymentAppFactory::~AutofillPaymentAppFactory() = default;

void AutofillPaymentAppFactory::Create(base::WeakPtr<Delegate> delegate) {
  const std::vector<autofill::CreditCard*>& cards =
      delegate->GetPaymentRequestDelegate()
          ->GetPersonalDataManager()
          ->GetCreditCardsToSuggest(
              /*include_server_cards=*/base::FeatureList::IsEnabled(
                  features::kReturnGooglePayInBasicCard));

  for (autofill::CreditCard* card : cards) {
    auto app = ConvertCardToPaymentAppIfSupportedNetwork(*card, delegate);
    if (app)
      delegate->OnPaymentAppCreated(std::move(app));
  }

  delegate->OnDoneCreatingPaymentApps();
}

}  // namespace payments
