// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

class PaymentRequestMinimalUITest
    : public PaymentRequestPlatformBrowserTestBase {
 public:
  enum class Behavior {
    CONFIRM_MINIMAL_UI,
    DISMISS_MINIMAL_UI,
  };

  PaymentRequestMinimalUITest() {
    features_.InitAndEnableFeature(::features::kWebPaymentsMinimalUI);
  }

  void OnMinimalUIReady() override {
    switch (minimal_ui_behavior_) {
      case Behavior::CONFIRM_MINIMAL_UI:
        ASSERT_TRUE(test_controller()->ConfirmMinimalUI());
        break;
      case Behavior::DISMISS_MINIMAL_UI:
        ASSERT_TRUE(test_controller()->DismissMinimalUI());
        break;
    }
  }

  std::string GetPaymentMethodForHost(const std::string& host) {
    return https_server()->GetURL(host, "/").spec();
  }

  void InstallPaymentHandlerAndSetMinimalUIBehavior(Behavior behavior) {
    NavigateTo("a.com", "/payment_handler_installer.html");
    ASSERT_EQ("success",
              content::EvalJs(GetActiveWebContents(),
                              "install('minimal_ui_app.js', [], true)"));
    minimal_ui_behavior_ = behavior;
  }

 private:
  Behavior minimal_ui_behavior_;
  base::test::ScopedFeatureList features_;
};

#if defined(OS_ANDROID)
// Only Android has a minimal UI, so there's no way to dismiss the minimal UI on
// desktop.
IN_PROC_BROWSER_TEST_F(PaymentRequestMinimalUITest, Dismiss) {
  InstallPaymentHandlerAndSetMinimalUIBehavior(Behavior::DISMISS_MINIMAL_UI);
  NavigateTo("b.com", "/payment_handler_status.html");

  EXPECT_EQ(
      "User closed the Payment Request UI.",
      content::EvalJs(GetActiveWebContents(),
                      content::JsReplace("getStatus($1)",
                                         GetPaymentMethodForHost("a.com"))));
}
#endif  // OS_ANDROID

IN_PROC_BROWSER_TEST_F(PaymentRequestMinimalUITest, HasEnrolledInstrument) {
  InstallPaymentHandlerAndSetMinimalUIBehavior(Behavior::DISMISS_MINIMAL_UI);
  NavigateTo("b.com", "/has_enrolled_instrument_checker.html");

  EXPECT_EQ("true", content::EvalJs(
                        GetActiveWebContents(),
                        content::JsReplace("hasEnrolledInstrument($1)",
                                           GetPaymentMethodForHost("a.com"))));
}

IN_PROC_BROWSER_TEST_F(PaymentRequestMinimalUITest, ConfirmPayment) {
  // TODO(crbug.com/1061574): ConfirmPayment times out on
  // android-marshmallow-arm64-rel and "Lollipop Phone Tester" bots.
  if (test_controller()->IsAndroidMarshmallowOrLollipop())
    return;

  InstallPaymentHandlerAndSetMinimalUIBehavior(Behavior::CONFIRM_MINIMAL_UI);
  NavigateTo("b.com", "/payment_handler_status.html");

  EXPECT_EQ("success", content::EvalJs(GetActiveWebContents(),
                                       content::JsReplace(
                                           "getStatus($1)",
                                           GetPaymentMethodForHost("a.com"))));
}

}  // namespace
}  // namespace payments
