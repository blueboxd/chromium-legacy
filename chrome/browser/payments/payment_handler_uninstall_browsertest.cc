// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "content/public/test/browser_test.h"

namespace payments {

namespace {

class PaymentHandlerUninstallTest
    : public PaymentRequestPlatformBrowserTestBase {
 protected:
  PaymentHandlerUninstallTest() = default;
  ~PaymentHandlerUninstallTest() override = default;

  void SetUpOnMainThread() override {
    PaymentRequestPlatformBrowserTestBase::SetUpOnMainThread();
    NavigateTo("/payment_handler.html");
  }
};

IN_PROC_BROWSER_TEST_F(PaymentHandlerUninstallTest, URLBasedPaymentMethod) {
  EXPECT_EQ("success", content::EvalJs(GetActiveWebContents(), "install()"));

  // Launch the payment request and confirm checkout completion.
  ResetEventWaiterForSingleEvent(TestEvent::kPaymentCompleted);
  EXPECT_EQ("success", content::EvalJs(GetActiveWebContents(), "launch()"));
  WaitForObservedEvent();

  // Uninstall the payment app and verify that a new request.show() gets
  // rejected after the app uninstallation.
  EXPECT_EQ("success", content::EvalJs(GetActiveWebContents(), "uninstall()"));
  ResetEventWaiterForSingleEvent(TestEvent::kNotSupportedError);
  EXPECT_EQ("success", content::EvalJs(GetActiveWebContents(),
                                       "launchWithoutWaitForResponse()"));
  WaitForObservedEvent();
}

IN_PROC_BROWSER_TEST_F(PaymentHandlerUninstallTest, BasicCard) {
  EXPECT_EQ("success",
            content::EvalJs(GetActiveWebContents(), "install('basic-card')"));

  // Launch the payment request and validate that one app is available.
  ResetEventWaiterForSingleEvent(TestEvent::kAppListReady);
  EXPECT_EQ("success", content::EvalJs(GetActiveWebContents(),
                                       "launchWithoutWaitForResponse()"));
  WaitForObservedEvent();
  EXPECT_EQ(1u, test_controller()->app_descriptions().size());

  EXPECT_EQ("success", content::EvalJs(GetActiveWebContents(), "abort()"));

  // Uninstall the payment app and verify that there is no payment app
  // available. A new request.show() will not get rejected though since the user
  // will still have the option to add a credit card.
  EXPECT_EQ("success", content::EvalJs(GetActiveWebContents(), "uninstall()"));
  ResetEventWaiterForSingleEvent(TestEvent::kAppListReady);
  EXPECT_EQ("success", content::EvalJs(GetActiveWebContents(),
                                       "launchWithoutWaitForResponse()"));
  WaitForObservedEvent();
  EXPECT_EQ(0u, test_controller()->app_descriptions().size());
}

}  // namespace

}  // namespace payments
