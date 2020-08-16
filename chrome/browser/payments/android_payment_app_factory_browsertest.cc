// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "components/payments/core/features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

class AndroidPaymentAppFactoryTest
    : public PaymentRequestPlatformBrowserTestBase {
 public:
  AndroidPaymentAppFactoryTest() {
    feature_list_.InitAndEnableFeature(features::kAppStoreBilling);
  }

  ~AndroidPaymentAppFactoryTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(AndroidPaymentAppFactoryTest, SmokeTest) {
  NavigateTo("a.com", "/app_store_billing_tests/index.html");
  ASSERT_EQ("success", content::EvalJs(GetActiveWebContents(),
                                       content::JsReplace(
                                           "addSupportedMethod($1)",
                                           "https://play.google.com/billing")));
  ASSERT_EQ("success",
            content::EvalJs(GetActiveWebContents(), "createPaymentRequest()"));
  ASSERT_EQ("false",
            content::EvalJs(GetActiveWebContents(), "canMakePayment()"));
}

}  // namespace
}  // namespace payments
