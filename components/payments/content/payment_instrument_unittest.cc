// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_instrument.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/payments/content/service_worker_payment_instrument.h"
#include "components/payments/core/autofill_payment_instrument.h"
#include "components/payments/core/mock_payment_request_delegate.h"
#include "content/public/browser/stored_payment_app.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"

namespace payments {

class PaymentInstrumentTest : public testing::Test,
                              public PaymentRequestSpec::Observer {
 protected:
  PaymentInstrumentTest()
      : address_(autofill::test::GetFullProfile()),
        local_card_(autofill::test::GetCreditCard()),
        billing_profiles_({&address_}) {
    local_card_.set_billing_address_id(address_.guid());
    CreateSpec();
  }

  std::unique_ptr<ServiceWorkerPaymentInstrument>
  CreateServiceWorkerPaymentInstrument(bool can_preselect) {
    constexpr int kBitmapDimension = 16;

    std::unique_ptr<content::StoredPaymentApp> stored_app =
        std::make_unique<content::StoredPaymentApp>();
    stored_app->registration_id = 123456;
    stored_app->scope = GURL("https://bobpay.com");
    stored_app->name = "bobpay";
    stored_app->icon = std::make_unique<SkBitmap>();
    if (can_preselect) {
      stored_app->icon->allocN32Pixels(kBitmapDimension, kBitmapDimension);
      stored_app->icon->eraseColor(SK_ColorRED);
    }
    return std::make_unique<ServiceWorkerPaymentInstrument>(
        &browser_context_, GURL("https://testmerchant.com"),
        GURL("https://testmerchant.com/bobpay"), spec_.get(),
        std::move(stored_app), &delegate_);
  }

  autofill::CreditCard& local_credit_card() { return local_card_; }
  std::vector<autofill::AutofillProfile*>& billing_profiles() {
    return billing_profiles_;
  }

 private:
  // PaymentRequestSpec::Observer
  void OnSpecUpdated() override {}

  void CreateSpec() {
    std::vector<mojom::PaymentMethodDataPtr> method_data;
    spec_ = std::make_unique<PaymentRequestSpec>(
        mojom::PaymentOptions::New(), mojom::PaymentDetails::New(),
        std::move(method_data), this, "en-US");
  }

  content::TestBrowserThreadBundle thread_bundle_;
  content::TestBrowserContext browser_context_;
  autofill::AutofillProfile address_;
  autofill::CreditCard local_card_;
  std::vector<autofill::AutofillProfile*> billing_profiles_;
  MockPaymentRequestDelegate delegate_;
  std::unique_ptr<PaymentRequestSpec> spec_;

  DISALLOW_COPY_AND_ASSIGN(PaymentInstrumentTest);
};

TEST_F(PaymentInstrumentTest, SortInstruments) {
  std::vector<PaymentInstrument*> instruments;
  // Add a complete instrument with mismatching type.
  autofill::CreditCard complete_dismatching_card = local_credit_card();
  AutofillPaymentInstrument complete_dismatching_cc_instrument(
      "visa", complete_dismatching_card,
      /*matches_merchant_card_type_exactly=*/false, billing_profiles(), "en-US",
      nullptr);
  instruments.push_back(&complete_dismatching_cc_instrument);

  // Add an instrument with no billing address.
  autofill::CreditCard card_with_no_address = local_credit_card();
  card_with_no_address.set_billing_address_id("");
  AutofillPaymentInstrument cc_instrument_with_no_address(
      "visa", card_with_no_address, /*matches_merchant_card_type_exactly=*/true,
      billing_profiles(), "en-US", nullptr);
  instruments.push_back(&cc_instrument_with_no_address);

  // Add an expired instrument.
  autofill::CreditCard expired_card = local_credit_card();
  expired_card.SetExpirationYear(2016);
  AutofillPaymentInstrument expired_cc_instrument(
      "visa", expired_card, /*matches_merchant_card_type_exactly=*/true,
      billing_profiles(), "en-US", nullptr);
  instruments.push_back(&expired_cc_instrument);

  // Add a non-preselectable sw based payment instrument.
  std::unique_ptr<ServiceWorkerPaymentInstrument>
      non_preselectable_sw_instrument =
          CreateServiceWorkerPaymentInstrument(false);
  instruments.push_back(non_preselectable_sw_instrument.get());

  // Add a preselectable sw based payment instrument.
  std::unique_ptr<ServiceWorkerPaymentInstrument> preselectable_sw_instrument =
      CreateServiceWorkerPaymentInstrument(true);
  instruments.push_back(preselectable_sw_instrument.get());

  // Add an instrument with no name.
  autofill::CreditCard card_with_no_name = local_credit_card();
  card_with_no_name.SetInfo(
      autofill::AutofillType(autofill::CREDIT_CARD_NAME_FULL),
      base::ASCIIToUTF16(""), "en-US");
  AutofillPaymentInstrument cc_instrument_with_no_name(
      "visa", card_with_no_name, /*matches_merchant_card_type_exactly=*/true,
      billing_profiles(), "en-US", nullptr);
  instruments.push_back(&cc_instrument_with_no_name);

  // Add a complete matching instrument.
  autofill::CreditCard complete_matching_card = local_credit_card();
  AutofillPaymentInstrument complete_matching_cc_instrument(
      "visa", complete_matching_card,
      /*matches_merchant_card_type_exactly=*/true, billing_profiles(), "en-US",
      nullptr);
  instruments.push_back(&complete_matching_cc_instrument);

  // Add an instrument with no number.
  autofill::CreditCard card_with_no_number = local_credit_card();
  card_with_no_number.SetNumber(base::ASCIIToUTF16(""));
  AutofillPaymentInstrument cc_instrument_with_no_number(
      "visa", card_with_no_number, /*matches_merchant_card_type_exactly=*/true,
      billing_profiles(), "en-US", nullptr);
  instruments.push_back(&cc_instrument_with_no_number);

  // Add a complete matching instrument that is most frequently used.
  autofill::CreditCard complete_frequently_used_card = local_credit_card();
  AutofillPaymentInstrument complete_frequently_used_cc_instrument(
      "visa", complete_frequently_used_card,
      /*matches_merchant_card_type_exactly=*/true, billing_profiles(), "en-US",
      nullptr);
  instruments.push_back(&complete_frequently_used_cc_instrument);
  // Record use of this instrument.
  complete_frequently_used_cc_instrument.credit_card()->RecordAndLogUse();

  // Sort the instruments and validate the new order.
  PaymentInstrument::SortInstruments(&instruments);
  size_t i = 0;
  EXPECT_EQ(instruments[i++], preselectable_sw_instrument.get());
  EXPECT_EQ(instruments[i++], non_preselectable_sw_instrument.get());
  // Autfill instruments (credit cards) come after sw instruments.
  EXPECT_EQ(instruments[i++], &complete_frequently_used_cc_instrument);
  EXPECT_EQ(instruments[i++], &complete_matching_cc_instrument);
  EXPECT_EQ(instruments[i++], &complete_dismatching_cc_instrument);
  EXPECT_EQ(instruments[i++], &expired_cc_instrument);
  EXPECT_EQ(instruments[i++], &cc_instrument_with_no_name);
  EXPECT_EQ(instruments[i++], &cc_instrument_with_no_address);
  EXPECT_EQ(instruments[i++], &cc_instrument_with_no_number);
}

}  // namespace payments
