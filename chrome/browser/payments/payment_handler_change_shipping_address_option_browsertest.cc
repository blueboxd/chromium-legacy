// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/payments/payment_request_test_controller.h"
#include "content/public/common/content_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

namespace payments {
namespace {

constexpr const char* kNoMerchantResponseExpectedOutput =
    "PaymentRequest.show(): changeShipping[Address|Option]() returned: null";
constexpr const char* kPromiseRejectedExpectedOutput =
    "PaymentRequest.show() rejected with: Error for test";
constexpr const char* kExeptionThrownExpectedOutput =
    "PaymentRequest.show() rejected with: Error: Error for test";
constexpr const char* kSuccessfulMerchantResponseExpectedOutput =
    "PaymentRequest.show(): changeShipping[Address|Option]() returned: "
    "{\"error\":\"Error for "
    "test\",\"modifiers\":[{\"data\":{\"soup\":\"potato\"},"
    "\"supportedMethods\":\"https://127.0.0.1/"
    "pay\",\"total\":{\"amount\":{\"currency\":\"EUR\",\"value\":\"0.03\"},"
    "\"label\":\"\",\"pending\":false}}],\"paymentMethodErrors\":{\"country\":"
    "\"Unsupported "
    "country\"},\"shippingAddressErrors\":{\"addressLine\":\"\",\"city\":\"\","
    "\"country\":\"US only "
    "shipping\",\"dependentLocality\":\"\",\"organization\":\"\",\"phone\":"
    "\"\",\"postalCode\":\"\",\"recipient\":\"\",\"region\":\"\","
    "\"sortingCode\":\"\"},\"shippingOptions\":[{\"amount\":{\"currency\":"
    "\"JPY\",\"value\":\"0.05\"},\"id\":\"id\",\"label\":\"Shipping "
    "option\",\"selected\":true}],\"total\":{\"currency\":\"GBP\",\"value\":"
    "\"0.02\"}}";

// Looks for the "supportedMethods" URL and removes its port number.
std::string ClearPortNumber(const std::string& may_contain_method_url) {
  std::string before;
  std::string method;
  std::string after;
  GURL::Replacements port;
  port.ClearPort();
  return re2::RE2::FullMatch(
             may_contain_method_url,
             "(.*\"supportedMethods\":\")(https://.*)(\",\"total\".*)", &before,
             &method, &after)
             ? before + GURL(method).ReplaceComponents(port).spec() + after
             : may_contain_method_url;
}

enum class ChangeType { kAddressChange, kOptionChange };

struct TestCase {
  TestCase(const std::string& init_test_code,
           const std::string& expected_output,
           ChangeType change_type)
      : init_test_code(init_test_code),
        expected_output(expected_output),
        change_type(change_type) {}

  ~TestCase() {}

  const std::string init_test_code;
  const std::string expected_output;
  const ChangeType change_type;
};

class PaymentHandlerChangeShippingAddressOptionTest
    : public PlatformBrowserTest,
      public testing::WithParamInterface<TestCase> {
 protected:
  PaymentHandlerChangeShippingAddressOptionTest() {}
  ~PaymentHandlerChangeShippingAddressOptionTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void SetUpOnMainThread() override {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->ServeFilesFromSourceDirectory(
        "components/test/data/payments");
    ASSERT_TRUE(https_server_->Start());
    ASSERT_TRUE(content::NavigateToURL(
        GetActiveWebContents(),
        https_server_->GetURL("/change_shipping_address_option.html")));
    test_controller_.SetUpOnMainThread();
    PlatformBrowserTest::SetUpOnMainThread();
  }

  std::string getTestType() {
    return GetParam().change_type == ChangeType::kOptionChange ? "option"
                                                               : "address";
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  PaymentRequestTestController test_controller_;
};

IN_PROC_BROWSER_TEST_P(PaymentHandlerChangeShippingAddressOptionTest, Test) {
  std::string actual_output;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      GetActiveWebContents(),
      "install('change_shipping_" + getTestType() + "_app.js');",
      &actual_output));
  ASSERT_EQ(actual_output, "instruments.set(): Payment handler installed.");

  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(),
                                     GetParam().init_test_code));

  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      GetActiveWebContents(),
      "outputChangeShippingAddressOptionReturnValue(request);",
      &actual_output));

  // The test expectations are hard-coded, but the embedded test server changes
  // its port number in every test, e.g., https://a.com:34548.
  ASSERT_EQ(ClearPortNumber(actual_output), GetParam().expected_output)
      << "When executing " << GetParam().init_test_code;
}

INSTANTIATE_TEST_SUITE_P(
    NoMerchantResponse,
    PaymentHandlerChangeShippingAddressOptionTest,
    testing::Values(TestCase("initTestNoHandler();",
                             kNoMerchantResponseExpectedOutput,
                             ChangeType::kAddressChange),
                    TestCase("initTestNoHandler();",
                             kNoMerchantResponseExpectedOutput,
                             ChangeType::kOptionChange)));

INSTANTIATE_TEST_SUITE_P(
    ErrorCases,
    PaymentHandlerChangeShippingAddressOptionTest,
    testing::Values(TestCase("initTestReject('shippingaddresschange')",
                             kPromiseRejectedExpectedOutput,
                             ChangeType::kAddressChange),
                    TestCase("initTestReject('shippingoptionchange')",
                             kPromiseRejectedExpectedOutput,
                             ChangeType::kOptionChange),
                    TestCase("initTestThrow('shippingaddresschange')",
                             kExeptionThrownExpectedOutput,
                             ChangeType::kAddressChange),
                    TestCase("initTestThrow('shippingoptionchange')",
                             kExeptionThrownExpectedOutput,
                             ChangeType::kOptionChange)));

INSTANTIATE_TEST_SUITE_P(
    MerchantResponse,
    PaymentHandlerChangeShippingAddressOptionTest,
    testing::Values(TestCase("initTestDetails('shippingaddresschange')",
                             kSuccessfulMerchantResponseExpectedOutput,
                             ChangeType::kAddressChange),
                    TestCase("initTestDetails('shippingoptionchange')",
                             kSuccessfulMerchantResponseExpectedOutput,
                             ChangeType::kOptionChange)));

}  // namespace
}  // namespace payments
