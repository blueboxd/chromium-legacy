// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_FIDO_AUTHENTICATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_FIDO_AUTHENTICATOR_H_

#include <memory>

#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/full_card_request.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "third_party/blink/public/mojom/webauthn/internal_authenticator.mojom.h"

namespace autofill {

using blink::mojom::InternalAuthenticatorPtr;

// Authenticates credit card unmasking through FIDO authentication, using the
// WebAuthn specification, standardized by the FIDO alliance. The Webauthn
// specification defines an API to cryptographically bind a server and client,
// and verify that binding. More information can be found here:
// - https://www.w3.org/TR/webauthn-1/
// - https://fidoalliance.org/fido2/
class CreditCardFIDOAuthenticator {
 public:
  class Requester {
   public:
    virtual ~Requester() {}
    virtual void OnFIDOAuthenticationComplete(
        bool did_succeed,
        const CreditCard* card = nullptr) = 0;
  };
  CreditCardFIDOAuthenticator(AutofillDriver* driver, AutofillClient* client);
  virtual ~CreditCardFIDOAuthenticator();

  // Authentication
  void Authenticate(const CreditCard* card,
                    base::WeakPtr<Requester> requester,
                    base::TimeTicks form_parsed_timestamp,
                    base::Value request_options);

  // Invokes callback with true if user has a verifying platform authenticator.
  // e.g. Touch/Face ID, Windows Hello, Android fingerprint, etc., is available
  // and enabled. Otherwise invokes callback with false.
  virtual void IsUserVerifiable(base::OnceCallback<void(bool)> callback);

  // Returns true only if the user has opted-in to use WebAuthn for autofill.
  virtual bool IsUserOptedIn();

 private:
  friend class AutofillManagerTest;
  friend class CreditCardAccessManagerTest;
  friend class CreditCardFIDOAuthenticatorTest;

  // Card being unmasked.
  const CreditCard* card_;

  // Meant for histograms recorded in FullCardRequest.
  base::TimeTicks form_parsed_timestamp_;

  // The associated autofill driver. Weak reference.
  AutofillDriver* const autofill_driver_;

  // The associated autofill client. Weak reference.
  AutofillClient* const autofill_client_;

  // Payments client to make requests to Google Payments.
  payments::PaymentsClient* const payments_client_;

  // Authenticator pointer to facilitate WebAuthn.
  InternalAuthenticatorPtr authenticator_;

  // Responsible for getting the full card details, including the PAN and the
  // CVC.
  std::unique_ptr<payments::FullCardRequest> full_card_request_;

  // Weak pointer to object that is requesting authentication.
  base::WeakPtr<Requester> requester_;

  base::WeakPtrFactory<CreditCardFIDOAuthenticator> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CreditCardFIDOAuthenticator);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_FIDO_AUTHENTICATOR_H_
