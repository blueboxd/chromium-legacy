// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "content/public/test/browser_test.h"

namespace payments {
namespace {

using EmptyParametersTest = PaymentRequestPlatformBrowserTestBase;

IN_PROC_BROWSER_TEST_F(EmptyParametersTest, NoCrash) {
  NavigateTo("/empty_parameters_test.html");
  EXPECT_EQ(true, content::EvalJs(GetActiveWebContents(), "runTest()"));
}

}  // namespace
}  // namespace payments
