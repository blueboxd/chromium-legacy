// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "build/build_config.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

class AbortPaymentHandlerTest : public PaymentRequestPlatformBrowserTestBase {};

IN_PROC_BROWSER_TEST_F(AbortPaymentHandlerTest,
                       CanAbortInvokedInstalledPaymentHandler) {
  std::string method_name = https_server()->GetURL("a.com", "/").spec();
  method_name = method_name.substr(0, method_name.length() - 1);
  ASSERT_NE('/', method_name[method_name.length() - 1]);
  NavigateTo("a.com", "/payment_handler_installer.html");
  ASSERT_EQ("success", content::EvalJs(
                           GetActiveWebContents(),
                           content::JsReplace(
                               "install('abort_responder_app.js', [$1], false)",
                               method_name)));

  NavigateTo("b.com", "/payment_handler_aborter.html");
  EXPECT_EQ(
      "Abort completed",
      content::EvalJs(GetActiveWebContents(),
                      content::JsReplace("launchAndAbort($1, $2)", method_name,
                                         /*abortResponse=*/true)));
}

// TODO(crbug.com/1083242): Andorid implementation of
// ServiceWorkerPaymentApp.java is unable to abort payments for invoked
// just-in-time installed payment handlers. When ServiceWorkerPaymentApp.java is
// replaced with JniPaymentApp.java that owns an instance of the correctly
// working service_worker_payment_app.cc, this test should be enabled on
// Android.
#if defined(OS_ANDROID)
#define MAYBE_CanAbortInvokedJitPaymentHandler \
  DISABLED_CanAbortInvokedJitPaymentHandler
#else
#define MAYBE_CanAbortInvokedJitPaymentHandler CanAbortInvokedJitPaymentHandler
#endif  // OS_ANDROID
IN_PROC_BROWSER_TEST_F(AbortPaymentHandlerTest,
                       MAYBE_CanAbortInvokedJitPaymentHandler) {
  std::string method_name =
      https_server()->GetURL("a.com", "/abort_responder_app.json").spec();
  ASSERT_NE('/', method_name[method_name.length() - 1]);

  NavigateTo("b.com", "/payment_handler_aborter.html");
  EXPECT_EQ(
      "Abort completed",
      content::EvalJs(GetActiveWebContents(),
                      content::JsReplace("launchAndAbort($1, $2)", method_name,
                                         /*abortResponse=*/true)));
}

IN_PROC_BROWSER_TEST_F(AbortPaymentHandlerTest,
                       InstalledPaymentHandlerCanRefuseAbort) {
  std::string method_name = https_server()->GetURL("a.com", "/").spec();
  method_name = method_name.substr(0, method_name.length() - 1);
  ASSERT_NE('/', method_name[method_name.length() - 1]);
  NavigateTo("a.com", "/payment_handler_installer.html");
  ASSERT_EQ("success", content::EvalJs(
                           GetActiveWebContents(),
                           content::JsReplace(
                               "install('abort_responder_app.js', [$1], false)",
                               method_name)));

  NavigateTo("b.com", "/payment_handler_aborter.html");
  EXPECT_EQ(
      "Unable to abort the payment",
      content::EvalJs(GetActiveWebContents(),
                      content::JsReplace("launchAndAbort($1, $2)", method_name,
                                         /*abortResponse=*/false)));
}

IN_PROC_BROWSER_TEST_F(AbortPaymentHandlerTest,
                       JitPaymentHandlerCanRefuseAbort) {
  std::string method_name =
      https_server()->GetURL("a.com", "/abort_responder_app.json").spec();
  ASSERT_NE('/', method_name[method_name.length() - 1]);

  NavigateTo("b.com", "/payment_handler_aborter.html");
  EXPECT_EQ(
      "Unable to abort the payment",
      content::EvalJs(GetActiveWebContents(),
                      content::JsReplace("launchAndAbort($1, $2)", method_name,
                                         /*abortResponse=*/false)));
}

}  // namespace
}  // namespace payments
