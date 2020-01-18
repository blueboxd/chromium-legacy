// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/payments/payment_request_test_controller.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/payments/core/features.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"

#if defined(OS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif  // defined(OS_ANDROID)

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

class PaymentRequestCanMakePaymentEventTest : public PlatformBrowserTest {
 public:
  PaymentRequestCanMakePaymentEventTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // HTTPS server only serves a valid cert for localhost, so this is needed to
    // load pages from "a.com" without an interstitial.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    ASSERT_TRUE(https_server_->InitializeAndListen());
    https_server_->ServeFilesFromSourceDirectory(
        "components/test/data/payments");
    https_server_->StartAcceptingConnections();
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void NavigateTo(const std::string& host, const std::string& file_path) {
    EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                       https_server_->GetURL(host, file_path)));
  }

  std::string GetPaymentMethodForHost(const std::string& host) {
    return https_server_->GetURL(host, "/").spec();
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
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
