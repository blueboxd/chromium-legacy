// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "content/public/test/browser_test.h"

// This test suite verifies that the the "canmakepayment" event does not fire
// for standardized payment methods. The test uses hasEnrolledInstrument() which
// returns "false" for standardized payment methods when "canmakepayment" is
// suppressed on desktop, "true" on Android. The platform discrepancy is tracked
// in https://crbug.com/994799 and should be solved in
// https://crbug.com/1022512.

namespace payments {

#if defined(OS_ANDROID)
static constexpr char kTestFileName[] = "can_make_payment_false_responder.js";
static constexpr char kExpectedResult[] = "true";
#else
static constexpr char kTestFileName[] = "can_make_payment_true_responder.js";
static constexpr char kExpectedResult[] = "false";
#endif  // defined(OS_ANDROID)

class PaymentRequestCanMakePaymentEventTest
    : public PaymentRequestPlatformBrowserTestBase {
 public:
  PaymentRequestCanMakePaymentEventTest() = default;

  std::string GetPaymentMethodForHost(const std::string& host) {
    return https_server()->GetURL(host, "/").spec();
  }
};

// A payment handler with two standardized payment methods ("interledger" and
// "basic-card") and one URL-based payment method (its own scope) does not
// receive a "canmakepayment" event from a PaymentRequest for "interledger"
// payment method.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentEventTest,
                       TwoStandardOneUrl) {
  NavigateTo("a.com", "/payment_handler_installer.html");
  EXPECT_EQ("success",
            content::EvalJs(GetActiveWebContents(),
                            "install('" + std::string(kTestFileName) +
                                "', ['interledger', 'basic-card'], true)"));
  NavigateTo("b.com", "/has_enrolled_instrument_checker.html");

  EXPECT_EQ(kExpectedResult,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument('interledger')"));
}

// A payment handler with two standardized payment methods ("interledger" and
// "basic-card") does not receive a "canmakepayment" event from a PaymentRequest
// for "interledger" payment method.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentEventTest, TwoStandard) {
  NavigateTo("a.com", "/payment_handler_installer.html");
  EXPECT_EQ("success",
            content::EvalJs(GetActiveWebContents(),
                            "install('" + std::string(kTestFileName) +
                                "', ['interledger', 'basic-card'], false)"));
  NavigateTo("b.com", "/has_enrolled_instrument_checker.html");

  EXPECT_EQ(kExpectedResult,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument('interledger')"));
}

// A payment handler with one standardized payment method ("interledger") does
// not receive a "canmakepayment" event from a PaymentRequest for "interledger"
// payment method.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentEventTest, OneStandard) {
  NavigateTo("a.com", "/payment_handler_installer.html");
  EXPECT_EQ("success",
            content::EvalJs(GetActiveWebContents(),
                            "install('" + std::string(kTestFileName) +
                                "', ['interledger'], false)"));
  NavigateTo("b.com", "/has_enrolled_instrument_checker.html");

  EXPECT_EQ(kExpectedResult,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument('interledger')"));
}

}  // namespace payments
