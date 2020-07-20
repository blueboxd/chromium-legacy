// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "components/payments/core/features.h"
#include "content/public/test/browser_test.h"

namespace payments {
namespace {

enum EnforceFullDelegationFlag {
  ENABLED,
  DISABLED,
};

class PaymentHandlerEnforceFullDelegationTest
    : public PaymentRequestPlatformBrowserTestBase,
      public testing::WithParamInterface<EnforceFullDelegationFlag> {
 public:
  PaymentHandlerEnforceFullDelegationTest() {
    if (GetParam() == ENABLED) {
      scoped_feature_list_.InitAndEnableFeature(
          payments::features::kEnforceFullDelegation);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          payments::features::kEnforceFullDelegation);
    }
  }
  ~PaymentHandlerEnforceFullDelegationTest() override = default;

  void SetUpOnMainThread() override {
    PaymentRequestPlatformBrowserTestBase::SetUpOnMainThread();
    NavigateTo("/enforce_full_delegation.com/index.html");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(PaymentHandlerEnforceFullDelegationTest,
                       ShowPaymentSheetWhenEnabledRejectWhenDisabled) {
  std::string expected = "success";
  EXPECT_EQ(expected, content::EvalJs(GetActiveWebContents(), "install()"));
  EXPECT_EQ(expected, content::EvalJs(GetActiveWebContents(),
                                      "addDefaultSupportedMethod()"));
  EXPECT_EQ(expected,
            content::EvalJs(GetActiveWebContents(), "enableDelegations([])"));
  EXPECT_EQ(expected,
            content::EvalJs(
                GetActiveWebContents(),
                "createPaymentRequestWithOptions({requestPayerName: true})"));

  if (GetParam() == ENABLED) {
    ResetEventWaiterForSingleEvent(TestEvent::kNotSupportedError);
  } else {
    ResetEventWaiterForSingleEvent(TestEvent::kAppListReady);
  }

  EXPECT_EQ(expected, content::EvalJs(GetActiveWebContents(), "show()"));
  WaitForObservedEvent();

  if (GetParam() == ENABLED) {
    EXPECT_GE(1u, test_controller()->app_descriptions().size());
  }
}

// Run all tests with both values for
// features::kEnforceFullDelegation.
INSTANTIATE_TEST_SUITE_P(All,
                         PaymentHandlerEnforceFullDelegationTest,
                         ::testing::Values(ENABLED, DISABLED));
}  // namespace
}  // namespace payments
