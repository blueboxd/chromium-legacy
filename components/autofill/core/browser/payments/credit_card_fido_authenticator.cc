// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/credit_card_fido_authenticator.h"

#include <string>
#include <utility>
#include <vector>

#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "third_party/blink/public/mojom/webauthn/internal_authenticator.mojom.h"

namespace autofill {

CreditCardFIDOAuthenticator::CreditCardFIDOAuthenticator(AutofillDriver* driver,
                                                         AutofillClient* client)
    : autofill_driver_(driver),
      autofill_client_(client),
      payments_client_(client->GetPaymentsClient()) {}

CreditCardFIDOAuthenticator::~CreditCardFIDOAuthenticator() {}

void CreditCardFIDOAuthenticator::Authenticate(
    const CreditCard* card,
    base::WeakPtr<Requester> requester,
    base::TimeTicks form_parsed_timestamp,
    base::Value request_options) {
  card_ = card;
  requester_ = requester;
  form_parsed_timestamp_ = form_parsed_timestamp;

  requester_->OnFIDOAuthenticationComplete(/*did_succeed=*/false);
}

void CreditCardFIDOAuthenticator::IsUserVerifiable(
    base::OnceCallback<void(bool)> callback) {
  if (base::FeatureList::IsEnabled(
          features::kAutofillCreditCardAuthentication)) {
    autofill_driver_->ConnectToAuthenticator(
        mojo::MakeRequest(&authenticator_));
    authenticator_->IsUserVerifyingPlatformAuthenticatorAvailable(
        std::move(callback));
  } else {
    std::move(callback).Run(false);
  }
}

bool CreditCardFIDOAuthenticator::IsUserOptedIn() {
  // TODO(crbug/949269): Check pref-store for user opt-in.
  return false;
}

}  // namespace autofill
