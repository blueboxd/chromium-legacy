// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/android/chrome_feature_list.h"
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
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
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
    test_controller_.SetUpOnMainThread();
    PlatformBrowserTest::SetUpOnMainThread();
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  PaymentRequestTestController test_controller_;

 private:
  net::EmbeddedTestServer https_server_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ExpandablePaymentHandlerBrowserTest, ConfirmPayment) {
  std::string expected = "success";
  EXPECT_EQ(expected, content::EvalJs(GetActiveWebContents(), "install()"));
  EXPECT_EQ("app_is_ready", content::EvalJs(GetActiveWebContents(),
                                            "launchAndWaitUntilReady()"));

  DCHECK(test_controller_.GetPaymentHandlerWebContents());
  EXPECT_EQ("confirmed",
            content::EvalJs(test_controller_.GetPaymentHandlerWebContents(),
                            "confirm()"));
  EXPECT_EQ("success", content::EvalJs(GetActiveWebContents(), "getResult()"));
}

IN_PROC_BROWSER_TEST_F(ExpandablePaymentHandlerBrowserTest, CancelPayment) {
  std::string expected = "success";
  EXPECT_EQ(expected, content::EvalJs(GetActiveWebContents(), "install()"));
  EXPECT_EQ("app_is_ready", content::EvalJs(GetActiveWebContents(),
                                            "launchAndWaitUntilReady()"));

  DCHECK(test_controller_.GetPaymentHandlerWebContents());
  EXPECT_EQ("canceled",
            content::EvalJs(test_controller_.GetPaymentHandlerWebContents(),
                            "cancel()"));
}
}  // namespace
}  // namespace payments
