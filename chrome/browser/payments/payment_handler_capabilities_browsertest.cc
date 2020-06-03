// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "build/build_config.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "chrome/test/payments/payment_request_test_controller.h"
#include "chrome/test/payments/test_event_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

class PaymentHandlerCapabilitiesTest
    : public PaymentRequestPlatformBrowserTestBase {
 public:
  void ExpectAppTotals(const std::map<std::string, std::string>& expected) {
    EXPECT_EQ(expected.size(), test_controller()->app_descriptions().size());
    for (const auto& app : test_controller()->app_descriptions()) {
      auto iter = expected.find(app.sublabel);
      ASSERT_NE(expected.end(), iter)
          << "Origin \"" << app.sublabel << "\" was not expected.";
      EXPECT_EQ(iter->second, app.total)
          << app.sublabel << " should have a total of \"" << iter->second
          << "\", but \"" << app.total << "\" was found instead.";
    }
  }
};

// Modified price should be displayed for to the payment handler with the
// matching capabilities.
IN_PROC_BROWSER_TEST_F(PaymentHandlerCapabilitiesTest, Modifiers) {
  NavigateTo("alicepay.com", "/payment_handler_installer.html");
  EXPECT_EQ(
      "success",
      content::EvalJs(GetActiveWebContents(),
                      "installWithCapabilities('alicepay.com/app1/app.js', "
                      "'basic-card', {supportedNetworks: ['visa']})"));
  NavigateTo("bobpay.com", "/payment_handler_installer.html");
  EXPECT_EQ(
      "success",
      content::EvalJs(GetActiveWebContents(),
                      "installWithCapabilities('bobpay.com/app1/app.js', "
                      "'basic-card', {supportedNetworks: ['mastercard']})"));

  ResetEventWaiterForSingleEvent(TestEvent::kShowAppsReady);
  NavigateTo("test.com",
             "/payment_request_bobpay_and_basic_card_with_modifiers_test.html");
  EXPECT_TRUE(
      content::ExecJs(GetActiveWebContents(), "visaSupportedNetwork()"));
  WaitForObservedEvent();

  std::map<std::string, std::string> expected;
  // Android pre-formats modified values.
#if defined(OS_ANDROID)
  expected["alicepay.com"] = "$4.00";
#else
  expected["alicepay.com"] = "USD 4.00";
#endif
  expected["bobpay.com"] = "USD 5.00";

  ExpectAppTotals(expected);
}

}  // namespace
}  // namespace payments
