// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_APP_FACTORY_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_APP_FACTORY_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/core/payment_app.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"

class GURL;

namespace autofill {
class AutofillProfile;
}  // namespace autofill

namespace content {
class WebContents;
}  // namespace content

namespace url {
class Origin;
}  // namespace url

namespace payments {

class PaymentRequestSpec;
class ContentPaymentRequestDelegate;

// Base class for a factory that can create instances of payment apps.
class PaymentAppFactory {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual content::WebContents* GetWebContents() = 0;
    virtual ContentPaymentRequestDelegate* GetPaymentRequestDelegate() = 0;
    virtual PaymentRequestSpec* GetSpec() = 0;
    virtual const GURL& GetTopOrigin() = 0;
    virtual const GURL& GetFrameOrigin() = 0;
    virtual const std::vector<autofill::AutofillProfile*>&
    GetBillingProfiles() = 0;
    virtual bool IsRequestedAutofillDataAvailable() = 0;
    virtual bool MayCrawlForInstallablePaymentApps() = 0;

    // Called after an app is installed. Used for just-in-time installable
    // payment handlers, for example.
    virtual void OnPaymentAppInstalled(const url::Origin& origin,
                                       int64_t registration_id) = 0;

    // Called when an app is created.
    virtual void OnPaymentAppCreated(std::unique_ptr<PaymentApp> app) = 0;

    // Called when there is an error creating a payment app. Called when unable
    // to download a web app manifest, for example.
    virtual void OnPaymentAppCreationError(
        const std::string& error_message) = 0;

    // Called when all apps of this factory have been created.
    virtual void OnDoneCreatingPaymentApps() = 0;
  };

  explicit PaymentAppFactory(PaymentApp::Type type);
  virtual ~PaymentAppFactory();

  PaymentApp::Type type() const { return type_; }

  virtual void Create(base::WeakPtr<Delegate> delegate) = 0;

 private:
  const PaymentApp::Type type_;

  DISALLOW_COPY_AND_ASSIGN(PaymentAppFactory);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_APP_FACTORY_H_
