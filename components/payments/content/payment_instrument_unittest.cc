// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_instrument.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/payments/content/mock_identity_observer.h"
#include "components/payments/content/service_worker_payment_instrument.h"
#include "components/payments/core/autofill_payment_instrument.h"
#include "components/payments/core/mock_payment_request_delegate.h"
#include "content/public/browser/stored_payment_app.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"

namespace payments {

namespace {

enum class RequiredPaymentOptions {
  // None of the shipping address or contact information is required.
  kNone,
  // Shipping Address is required.
  kShippingAddress,
  // Payer's contact information(phone, name, email) is required.
  kContactInformation,
  // Payer's email is required.
  kPayerEmail,
  // Both contact information and shipping address are required.
  kContactInformationAndShippingAddress,
};

}  // namespace

class PaymentInstrumentTest
    : public testing::TestWithParam<RequiredPaymentOptions>,
      public PaymentRequestSpec::Observer {
 protected:
  PaymentInstrumentTest()
      : address_(autofill::test::GetFullProfile()),
        local_card_(autofill::test::GetCreditCard()),
        billing_profiles_({&address_}),
        required_options_(GetParam()) {
    local_card_.set_billing_address_id(address_.guid());
    CreateSpec();
  }

  std::unique_ptr<ServiceWorkerPaymentInstrument>
  CreateServiceWorkerPaymentInstrument(bool can_preselect,
                                       bool handles_shipping,
                                       bool handles_name,
                                       bool handles_phone,
                                       bool handles_email) {
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
    if (handles_shipping) {
      stored_app->supported_delegations.shipping_address = true;
    }
    if (handles_name) {
      stored_app->supported_delegations.payer_name = true;
    }
    if (handles_phone) {
      stored_app->supported_delegations.payer_phone = true;
    }
    if (handles_email) {
      stored_app->supported_delegations.payer_email = true;
    }

    return std::make_unique<ServiceWorkerPaymentInstrument>(
        &browser_context_, GURL("https://testmerchant.com"),
        GURL("https://testmerchant.com/bobpay"), spec_.get(),
        std::move(stored_app), &delegate_, identity_observer_.AsWeakPtr());
  }

  autofill::CreditCard& local_credit_card() { return local_card_; }
  std::vector<autofill::AutofillProfile*>& billing_profiles() {
    return billing_profiles_;
  }

  RequiredPaymentOptions required_options() const { return required_options_; }

 private:
  // PaymentRequestSpec::Observer
  void OnSpecUpdated() override {}

  void CreateSpec() {
    std::vector<mojom::PaymentMethodDataPtr> method_data;
    mojom::PaymentOptionsPtr payment_options = mojom::PaymentOptions::New();
    switch (required_options_) {
      case RequiredPaymentOptions::kNone:
        break;
      case RequiredPaymentOptions::kShippingAddress:
        payment_options->request_shipping = true;
        break;
      case RequiredPaymentOptions::kContactInformation:
        payment_options->request_payer_name = true;
        payment_options->request_payer_email = true;
        payment_options->request_payer_phone = true;
        break;
      case RequiredPaymentOptions::kPayerEmail:
        payment_options->request_payer_email = true;
        break;
      case RequiredPaymentOptions::kContactInformationAndShippingAddress:
        payment_options->request_shipping = true;
        payment_options->request_payer_name = true;
        payment_options->request_payer_email = true;
        payment_options->request_payer_phone = true;
        break;
    }
    spec_ = std::make_unique<PaymentRequestSpec>(
        std::move(payment_options), mojom::PaymentDetails::New(),
        std::move(method_data), this, "en-US");
  }

  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
  autofill::AutofillProfile address_;
  autofill::CreditCard local_card_;
  std::vector<autofill::AutofillProfile*> billing_profiles_;
  MockPaymentRequestDelegate delegate_;
  MockIdentityObserver identity_observer_;
  RequiredPaymentOptions required_options_;
  std::unique_ptr<PaymentRequestSpec> spec_;

  DISALLOW_COPY_AND_ASSIGN(PaymentInstrumentTest);
};

INSTANTIATE_TEST_SUITE_P(
    ,
    PaymentInstrumentTest,
    ::testing::Values(
        RequiredPaymentOptions::kNone,
        RequiredPaymentOptions::kShippingAddress,
        RequiredPaymentOptions::kContactInformation,
        RequiredPaymentOptions::kPayerEmail,
        RequiredPaymentOptions::kContactInformationAndShippingAddress));

TEST_P(PaymentInstrumentTest, SortInstruments) {
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
      non_preselectable_sw_instrument = CreateServiceWorkerPaymentInstrument(
          false /* = can_preselect */, false /* = handles_shipping */,
          false /* = handles_name */, false /* = handles_phone */,
          false /* = handles_email */);
  instruments.push_back(non_preselectable_sw_instrument.get());

  // Add a preselectable sw based payment instrument.
  std::unique_ptr<ServiceWorkerPaymentInstrument> preselectable_sw_instrument =
      CreateServiceWorkerPaymentInstrument(
          true /* = can_preselect */, false /* = handles_shipping */,
          false /* = handles_name */, false /* = handles_phone */,
          false /* = handles_email */);
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

TEST_P(PaymentInstrumentTest, SortInstrumentsBasedOnSupportedDelegations) {
  std::vector<PaymentInstrument*> instruments;
  // Add a preselectable sw based payment instrument which does not support
  // shipping or contact delegation.
  std::unique_ptr<ServiceWorkerPaymentInstrument> does_not_support_delegations =
      CreateServiceWorkerPaymentInstrument(
          true /* = can_preselect */, false /* = handles_shipping */,
          false /* = handles_name */, false /* = handles_phone */,
          false /* = handles_email */);
  instruments.push_back(does_not_support_delegations.get());

  // Add a preselectable sw based payment instrument which handles shipping.
  std::unique_ptr<ServiceWorkerPaymentInstrument> handles_shipping_address =
      CreateServiceWorkerPaymentInstrument(
          true /* = can_preselect */, true /* = handles_shipping */,
          false /* = handles_name */, false /* = handles_phone */,
          false /* = handles_email */);
  instruments.push_back(handles_shipping_address.get());

  // Add a preselectable sw based payment instrument which handles payer's
  // email.
  std::unique_ptr<ServiceWorkerPaymentInstrument> handles_payer_email =
      CreateServiceWorkerPaymentInstrument(
          true /* = can_preselect */, false /* = handles_shipping */,
          false /* = handles_name */, false /* = handles_phone */,
          true /* = handles_email */);
  instruments.push_back(handles_payer_email.get());

  // Add a preselectable sw based payment instrument which handles contact
  // information.
  std::unique_ptr<ServiceWorkerPaymentInstrument> handles_contact_info =
      CreateServiceWorkerPaymentInstrument(
          true /* = can_preselect */, false /* = handles_shipping */,
          true /* = handles_name */, true /* = handles_phone */,
          true /* = handles_email */);
  instruments.push_back(handles_contact_info.get());

  // Add a preselectable sw based payment instrument which handles both shipping
  // address and contact information.
  std::unique_ptr<ServiceWorkerPaymentInstrument> handles_shipping_and_contact =
      CreateServiceWorkerPaymentInstrument(
          true /* = can_preselect */, true /* = handles_shipping */,
          true /* = handles_name */, true /* = handles_phone */,
          true /* = handles_email */);
  instruments.push_back(handles_shipping_and_contact.get());

  PaymentInstrument::SortInstruments(&instruments);
  size_t i = 0;

  switch (required_options()) {
    case RequiredPaymentOptions::kNone: {
      // When no payemnt option is required the order of the payment instruments
      // does not change.
      EXPECT_EQ(instruments[i++], does_not_support_delegations.get());
      EXPECT_EQ(instruments[i++], handles_shipping_address.get());
      EXPECT_EQ(instruments[i++], handles_payer_email.get());
      EXPECT_EQ(instruments[i++], handles_contact_info.get());
      EXPECT_EQ(instruments[i++], handles_shipping_and_contact.get());
      break;
    }
    case RequiredPaymentOptions::kShippingAddress: {
      // Instruments that handle shipping address come first.
      EXPECT_EQ(instruments[i++], handles_shipping_address.get());
      EXPECT_EQ(instruments[i++], handles_shipping_and_contact.get());
      // The order is unchanged for instruments that do not handle shipping.
      EXPECT_EQ(instruments[i++], does_not_support_delegations.get());
      EXPECT_EQ(instruments[i++], handles_payer_email.get());
      EXPECT_EQ(instruments[i++], handles_contact_info.get());
      break;
    }
    case RequiredPaymentOptions::kContactInformation: {
      // Instruments that handle all required contact information come first.
      EXPECT_EQ(instruments[i++], handles_contact_info.get());
      EXPECT_EQ(instruments[i++], handles_shipping_and_contact.get());
      // The instrument that partially handles contact information comes next.
      EXPECT_EQ(instruments[i++], handles_payer_email.get());
      // The order for instruments that do not handle contact information is not
      // changed.
      EXPECT_EQ(instruments[i++], does_not_support_delegations.get());
      EXPECT_EQ(instruments[i++], handles_shipping_address.get());
      break;
    }
    case RequiredPaymentOptions::kPayerEmail: {
      EXPECT_EQ(instruments[i++], handles_payer_email.get());
      EXPECT_EQ(instruments[i++], handles_contact_info.get());
      EXPECT_EQ(instruments[i++], handles_shipping_and_contact.get());
      EXPECT_EQ(instruments[i++], does_not_support_delegations.get());
      EXPECT_EQ(instruments[i++], handles_shipping_address.get());
      break;
    }
    case RequiredPaymentOptions::kContactInformationAndShippingAddress: {
      EXPECT_EQ(instruments[i++], handles_shipping_and_contact.get());
      EXPECT_EQ(instruments[i++], handles_shipping_address.get());
      EXPECT_EQ(instruments[i++], handles_contact_info.get());
      EXPECT_EQ(instruments[i++], handles_payer_email.get());
      EXPECT_EQ(instruments[i++], does_not_support_delegations.get());
      break;
    }
  }
}

}  // namespace payments
