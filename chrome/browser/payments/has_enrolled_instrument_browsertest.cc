// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/payments/personal_data_manager_test_util.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/payments/payment_request_test_controller.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/payments/core/features.h"
#include "components/payments/core/journey_logger.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

namespace payments {
namespace {

// TODO(https://crbug.com/994799): Unify error messages between desktop and
// Android.
const char kNotSupportedMessage[] =
#if defined(OS_ANDROID)
    "NotSupportedError: Payment method not supported. "
#else
    "NotSupportedError: The payment method \"basic-card\" is not supported. "
#endif  // OS_ANDROID
    "User does not have valid information on file.";

autofill::CreditCard GetCardWithBillingAddress(
    const autofill::AutofillProfile& profile) {
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(profile.guid());
  return card;
}

enum HasEnrolledInstrumentMode {
  STRICT_HAS_ENROLLED_INSTRUMENT,
  LEGACY_HAS_ENROLLED_INSTRUMENT,
};

// A parameterized test to test both values of
// features::kStrictHasEnrolledAutofillInstrument.
class HasEnrolledInstrumentTest
    : public PlatformBrowserTest,
      public testing::WithParamInterface<HasEnrolledInstrumentMode> {
 public:
  HasEnrolledInstrumentTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    if (GetParam() == STRICT_HAS_ENROLLED_INSTRUMENT) {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{features::kStrictHasEnrolledAutofillInstrument},
          /*disabled_features=*/{features::kPaymentRequestSkipToGPay});
    }
  }

  ~HasEnrolledInstrumentTest() override {}

  void SetUpOnMainThread() override {
    https_server_.ServeFilesFromSourceDirectory(
        "components/test/data/payments");
    ASSERT_TRUE(https_server_.Start());
    ASSERT_TRUE(content::NavigateToURL(
        GetActiveWebContents(),
        https_server_.GetURL("/has_enrolled_instrument.html")));
    test_controller_.SetUpOnMainThread();
    PlatformBrowserTest::SetUpOnMainThread();
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  const std::string& not_supported_message() const {
    return not_supported_message_;
  }

  // Helper function to test that all variations of hasEnrolledInstrument()
  // returns |expected|.
  void ExpectHasEnrolledInstrumentIs(bool expected) {
    EXPECT_EQ(expected, content::EvalJs(GetActiveWebContents(),
                                        "hasEnrolledInstrument()"));
    EXPECT_EQ(expected,
              content::EvalJs(GetActiveWebContents(),
                              "hasEnrolledInstrument({requestShipping:true})"));
    EXPECT_EQ(expected, content::EvalJs(
                            GetActiveWebContents(),
                            "hasEnrolledInstrument({requestPayerEmail:true})"));
  }

  // Helper function to test that all variants of show() rejects with
  // not_supported_message().
  void ExpectShowRejects() {
    // Only check show() if feature is on.
    if (GetParam() == STRICT_HAS_ENROLLED_INSTRUMENT) {
      base::HistogramTester histogram_tester;
      base::HistogramBase::Count expected_count =
          histogram_tester.GetBucketCount(
              "PaymentRequest.CheckoutFunnel.NoShow",
              JourneyLogger::NOT_SHOWN_REASON_NO_SUPPORTED_PAYMENT_METHOD);

      // Check code path where show() is called before instruments are ready.
      EXPECT_EQ(not_supported_message(),
                content::EvalJs(GetActiveWebContents(), "show()"));
      // TODO(crbug.com/1027322): Fix NoShow logging on Android.
#if !defined(OS_ANDROID)
      expected_count++;
#endif
      histogram_tester.ExpectBucketCount(
          "PaymentRequest.CheckoutFunnel.NoShow",
          JourneyLogger::NOT_SHOWN_REASON_NO_SUPPORTED_PAYMENT_METHOD,
          expected_count);
      EXPECT_EQ(not_supported_message(),
                content::EvalJs(GetActiveWebContents(),
                                "show({requestShipping:true})"));
      EXPECT_EQ(not_supported_message(),
                content::EvalJs(GetActiveWebContents(),
                                "show({requestPayerEmail:true})"));

      // Check code path where show() is called after instruments are ready.
      EXPECT_EQ(not_supported_message(),
                content::EvalJs(GetActiveWebContents(), "delayedShow()"));
      EXPECT_EQ(not_supported_message(),
                content::EvalJs(GetActiveWebContents(),
                                "delayedShow({requestShipping:true})"));
      EXPECT_EQ(not_supported_message(),
                content::EvalJs(GetActiveWebContents(),
                                "delayedShow({requestPayerEmail:true})"));
    }
  }

 private:
  PaymentRequestTestController test_controller_;
  net::EmbeddedTestServer https_server_;
  std::string not_supported_message_ = kNotSupportedMessage;
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(HasEnrolledInstrumentTest);
};

IN_PROC_BROWSER_TEST_P(HasEnrolledInstrumentTest, NoCard) {
  ExpectHasEnrolledInstrumentIs(false);
  ExpectShowRejects();
}

IN_PROC_BROWSER_TEST_P(HasEnrolledInstrumentTest, NoBillingAddress) {
  test::AddCreditCard(GetActiveWebContents()->GetBrowserContext(),
                      autofill::test::GetCreditCard());
  ExpectHasEnrolledInstrumentIs(GetParam() != STRICT_HAS_ENROLLED_INSTRUMENT);
  ExpectShowRejects();
}

IN_PROC_BROWSER_TEST_P(HasEnrolledInstrumentTest,
                       HaveShippingNoBillingAddress) {
  test::AddAutofillProfile(GetActiveWebContents()->GetBrowserContext(),
                           autofill::test::GetFullProfile());
  test::AddCreditCard(GetActiveWebContents()->GetBrowserContext(),
                      autofill::test::GetCreditCard());

  ExpectHasEnrolledInstrumentIs(GetParam() != STRICT_HAS_ENROLLED_INSTRUMENT);
  ExpectShowRejects();
}

IN_PROC_BROWSER_TEST_P(HasEnrolledInstrumentTest,
                       HaveShippingAndBillingAddress) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  test::AddAutofillProfile(GetActiveWebContents()->GetBrowserContext(),
                           address);
  test::AddCreditCard(GetActiveWebContents()->GetBrowserContext(),
                      GetCardWithBillingAddress(address));

  ExpectHasEnrolledInstrumentIs(true);
}

IN_PROC_BROWSER_TEST_P(HasEnrolledInstrumentTest, InvalidCardNumber) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  test::AddAutofillProfile(GetActiveWebContents()->GetBrowserContext(),
                           address);
  autofill::CreditCard card = GetCardWithBillingAddress(address);
  card.SetRawInfo(autofill::ServerFieldType::CREDIT_CARD_NUMBER,
                  base::ASCIIToUTF16("1111111111111111"));
  test::AddCreditCard(GetActiveWebContents()->GetBrowserContext(), card);

  ExpectHasEnrolledInstrumentIs(false);
  ExpectShowRejects();
}

IN_PROC_BROWSER_TEST_P(HasEnrolledInstrumentTest, ExpiredCard) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  test::AddAutofillProfile(GetActiveWebContents()->GetBrowserContext(),
                           address);
  autofill::CreditCard card = GetCardWithBillingAddress(address);
  card.SetExpirationYear(2000);
  test::AddCreditCard(GetActiveWebContents()->GetBrowserContext(), card);

  ExpectHasEnrolledInstrumentIs(GetParam() != STRICT_HAS_ENROLLED_INSTRUMENT);
  ExpectShowRejects();
}

// TODO(https://crbug.com/994799): Unify autofill data validation and returned
// data across platforms.
IN_PROC_BROWSER_TEST_P(HasEnrolledInstrumentTest,
                       HaveNoNameShippingAndBillingAddress) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();

  address.SetRawInfo(autofill::ServerFieldType::NAME_FIRST, base::string16());
  address.SetRawInfo(autofill::ServerFieldType::NAME_MIDDLE, base::string16());
  address.SetRawInfo(autofill::ServerFieldType::NAME_LAST, base::string16());

  test::AddAutofillProfile(GetActiveWebContents()->GetBrowserContext(),
                           address);
  test::AddCreditCard(GetActiveWebContents()->GetBrowserContext(),
                      GetCardWithBillingAddress(address));

  // Recipient name is required for shipping address in strict mode.
  EXPECT_EQ(GetParam() != STRICT_HAS_ENROLLED_INSTRUMENT,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestShipping:true})"));
  if (GetParam() == STRICT_HAS_ENROLLED_INSTRUMENT) {
    EXPECT_EQ(not_supported_message(),
              content::EvalJs(GetActiveWebContents(),
                              "show({requestShipping:true})"));
  }

  // Recipient name should be required for billing address in strict mode, but
  // current desktop implementation doesn't match this requirement.
  // TODO(https://crbug.com/994799): Unify autofill data requirements between
  // desktop and Android.
#if defined(OS_ANDROID)
  bool is_no_name_billing_address_valid =
      GetParam() != STRICT_HAS_ENROLLED_INSTRUMENT;
#else
  bool is_no_name_billing_address_valid = true;
#endif

  EXPECT_EQ(is_no_name_billing_address_valid,
            content::EvalJs(GetActiveWebContents(), "hasEnrolledInstrument()"));
  EXPECT_EQ(is_no_name_billing_address_valid,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestPayerEmail:true})"));
  if (!is_no_name_billing_address_valid) {
    EXPECT_EQ(not_supported_message(),
              content::EvalJs(GetActiveWebContents(), "show()"));
    EXPECT_EQ(not_supported_message(),
              content::EvalJs(GetActiveWebContents(),
                              "show({requestPayerEmail:true})"));
  }
}

IN_PROC_BROWSER_TEST_P(HasEnrolledInstrumentTest,
                       HaveNoStreetShippingAndBillingAddress) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  address.SetRawInfo(autofill::ServerFieldType::ADDRESS_HOME_STREET_ADDRESS,
                     base::string16());
  test::AddAutofillProfile(GetActiveWebContents()->GetBrowserContext(),
                           address);
  test::AddCreditCard(GetActiveWebContents()->GetBrowserContext(),
                      GetCardWithBillingAddress(address));

  ExpectHasEnrolledInstrumentIs(GetParam() != STRICT_HAS_ENROLLED_INSTRUMENT);
  ExpectShowRejects();
}

IN_PROC_BROWSER_TEST_P(HasEnrolledInstrumentTest, NoEmailAddress) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  address.SetRawInfo(autofill::ServerFieldType::EMAIL_ADDRESS,
                     base::string16());
  test::AddAutofillProfile(GetActiveWebContents()->GetBrowserContext(),
                           address);
  test::AddCreditCard(GetActiveWebContents()->GetBrowserContext(),
                      GetCardWithBillingAddress(address));

  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(), "hasEnrolledInstrument()"));
  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestShipping:true})"));
  // StrictHasEnrolledAutofillInstrument considers a profile with missing email
  // address as invalid.
  EXPECT_EQ(GetParam() != STRICT_HAS_ENROLLED_INSTRUMENT,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestPayerEmail:true})"));
  if (GetParam() == STRICT_HAS_ENROLLED_INSTRUMENT) {
    EXPECT_EQ(not_supported_message(),
              content::EvalJs(GetActiveWebContents(),
                              "show({requestPayerEmail:true})"));
  }
}

IN_PROC_BROWSER_TEST_P(HasEnrolledInstrumentTest, InvalidEmailAddress) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  address.SetRawInfo(autofill::ServerFieldType::EMAIL_ADDRESS,
                     base::ASCIIToUTF16("this-is-not-a-valid-email-address"));
  test::AddAutofillProfile(GetActiveWebContents()->GetBrowserContext(),
                           address);
  test::AddCreditCard(GetActiveWebContents()->GetBrowserContext(),
                      GetCardWithBillingAddress(address));

  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(), "hasEnrolledInstrument()"));
  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestShipping:true})"));

  // StrictHasEnrolledAutofillInstrument considers a profile with missing email
  // address as invalid.
  EXPECT_EQ(GetParam() != STRICT_HAS_ENROLLED_INSTRUMENT,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestPayerEmail:true})"));
  if (GetParam() == STRICT_HAS_ENROLLED_INSTRUMENT) {
    EXPECT_EQ(not_supported_message(),
              content::EvalJs(GetActiveWebContents(),
                              "show({requestPayerEmail:true})"));
  }
}

// Run all tests with both values for
// features::kStrictHasEnrolledAutofillInstrument.
INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    HasEnrolledInstrumentTest,
    ::testing::Values(STRICT_HAS_ENROLLED_INSTRUMENT,
                      LEGACY_HAS_ENROLLED_INSTRUMENT));
}  // namespace
}  // namespace payments
