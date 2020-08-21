// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

static constexpr char kInvokePaymentRequest[] =
    "getStatusForMethodData([{"
    "  supportedMethods: 'secure-payment-confirmation',"
    "  data: {"
    "    action: 'authenticate',"
    "    instrumentId: 'x',"
    "    networkData: Uint8Array.from('x', c => c.charCodeAt(0)),"
    "    timeout: 60000,"
    "    fallbackUrl: 'https://fallback.example/url'"
    "}}])";

class SecurePaymentConfirmationTest
    : public PaymentRequestPlatformBrowserTestBase {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PaymentRequestPlatformBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }
};

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationTest, PaymentSheetShowsApp) {
  NavigateTo("a.com", "/payment_handler_status.html");
  ResetEventWaiterForSingleEvent(TestEvent::kAppListReady);

  // ExecJs starts executing JavaScript and immediately returns, not waiting for
  // any promise to return.
  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(), kInvokePaymentRequest));

  WaitForObservedEvent();
  ASSERT_FALSE(test_controller()->app_descriptions().empty());
  EXPECT_EQ(1u, test_controller()->app_descriptions().size());
  EXPECT_EQ("Stub label", test_controller()->app_descriptions().front().label);
}

// Intentionally do not enable the "SecurePaymentConfirmation" Blink runtime
// feature.
class SecurePaymentConfirmationDisabledTest
    : public PaymentRequestPlatformBrowserTestBase {};

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDisabledTest,
                       PaymentMethodNotSupported) {
  NavigateTo("a.com", "/payment_handler_status.html");

  // EvalJs waits for JavaScript promise to resolve.
  EXPECT_EQ(
      "The payment method \"secure-payment-confirmation\" is not supported.",
      content::EvalJs(GetActiveWebContents(), kInvokePaymentRequest));
}

}  // namespace
}  // namespace payments
