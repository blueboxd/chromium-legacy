// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/payments/payment_request_test_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace payments {
namespace {

class ExpandablePaymentHandlerBrowserTest : public PlatformBrowserTest {
 public:
  ExpandablePaymentHandlerBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        http_server_(net::EmbeddedTestServer::TYPE_HTTP) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{chrome::android::kScrollToExpandPaymentHandler},
        /*disabled_features=*/{});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpOnMainThread() override {
    https_server_.ServeFilesFromSourceDirectory(
        "components/test/data/payments/");
    ASSERT_TRUE(https_server_.Start());
    ASSERT_TRUE(content::NavigateToURL(
        GetActiveWebContents(),
        https_server_.GetURL("/maxpay.com/merchant.html")));
    http_server_.ServeFilesFromSourceDirectory(
        "components/test/data/payments/");
    ASSERT_TRUE(http_server_.Start());
    test_controller_.SetUpOnMainThread();
    PlatformBrowserTest::SetUpOnMainThread();
  }

  GURL GetHttpPageUrl() {
    return http_server_.GetURL("/maxpay.com/merchant.html");
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  PaymentRequestTestController test_controller_;

 private:
  net::EmbeddedTestServer https_server_;
  net::EmbeddedTestServer http_server_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Make sure merchants can confirm the payment.
IN_PROC_BROWSER_TEST_F(ExpandablePaymentHandlerBrowserTest, ConfirmPayment) {
  std::string expected = "success";
  EXPECT_EQ(expected, content::EvalJs(GetActiveWebContents(), "install()"));
  EXPECT_EQ("app_is_ready",
            content::EvalJs(
                GetActiveWebContents(),
                "launchAndWaitUntilReady('./payment_handler_window.html')"));

  DCHECK(test_controller_.GetPaymentHandlerWebContents());
  EXPECT_EQ("confirmed",
            content::EvalJs(test_controller_.GetPaymentHandlerWebContents(),
                            "confirm()"));
  EXPECT_EQ("success", content::EvalJs(GetActiveWebContents(), "getResult()"));
}

// Make sure the security icon is clickable.
IN_PROC_BROWSER_TEST_F(ExpandablePaymentHandlerBrowserTest, ClickSecurityIcon) {
  std::string expected = "success";
  EXPECT_EQ(expected, content::EvalJs(GetActiveWebContents(), "install()"));
  EXPECT_EQ("app_is_ready",
            content::EvalJs(
                GetActiveWebContents(),
                "launchAndWaitUntilReady('./payment_handler_window.html')"));

  DCHECK(test_controller_.GetPaymentHandlerWebContents());
  EXPECT_TRUE(test_controller_.ClickPaymentHandlerSecurityIcon());
}

// Make sure merchants can cancel the payment.
IN_PROC_BROWSER_TEST_F(ExpandablePaymentHandlerBrowserTest, CancelPayment) {
  std::string expected = "success";
  EXPECT_EQ(expected, content::EvalJs(GetActiveWebContents(), "install()"));
  EXPECT_EQ("app_is_ready",
            content::EvalJs(
                GetActiveWebContents(),
                "launchAndWaitUntilReady('./payment_handler_window.html')"));

  DCHECK(test_controller_.GetPaymentHandlerWebContents());
  EXPECT_EQ("canceled",
            content::EvalJs(test_controller_.GetPaymentHandlerWebContents(),
                            "cancel()"));
  EXPECT_EQ("unknown", content::EvalJs(GetActiveWebContents(), "getResult()"));
}

// Make sure merchants can fail the payment.
IN_PROC_BROWSER_TEST_F(ExpandablePaymentHandlerBrowserTest, PaymentFailed) {
  std::string expected = "success";
  EXPECT_EQ(expected, content::EvalJs(GetActiveWebContents(), "install()"));
  EXPECT_EQ("app_is_ready",
            content::EvalJs(
                GetActiveWebContents(),
                "launchAndWaitUntilReady('./payment_handler_window.html')"));

  DCHECK(test_controller_.GetPaymentHandlerWebContents());
  EXPECT_EQ("failed",
            content::EvalJs(test_controller_.GetPaymentHandlerWebContents(),
                            "fail()"));
  EXPECT_EQ("fail", content::EvalJs(GetActiveWebContents(), "getResult()"));
}

// Make sure payment apps served from an http connection are rejected.
IN_PROC_BROWSER_TEST_F(ExpandablePaymentHandlerBrowserTest,
                       OpenWindowRejectHttp) {
  std::string expected = "success";
  EXPECT_EQ(expected, content::EvalJs(GetActiveWebContents(), "install()"));
  EXPECT_EQ("open_window_failed",
            content::EvalJs(
                GetActiveWebContents(),
                "launchAndWaitUntilReady('" + GetHttpPageUrl().spec() + "')"));
}

// Make sure openWindow() can be resolved into window client.
IN_PROC_BROWSER_TEST_F(ExpandablePaymentHandlerBrowserTest, WindowClientReady) {
  std::string expected = "success";
  EXPECT_EQ(expected, content::EvalJs(GetActiveWebContents(), "install()"));
  EXPECT_EQ("app_is_ready",
            content::EvalJs(
                GetActiveWebContents(),
                "launchAndWaitUntilReady('./payment_handler_window.html')"));

  DCHECK(test_controller_.GetPaymentHandlerWebContents());
  EXPECT_EQ(true,
            content::EvalJs(test_controller_.GetPaymentHandlerWebContents(),
                            "isWindowClientReady()"));
}
}  // namespace
}  // namespace payments
