// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/payments/personal_data_manager_test_util.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/payments/payment_request_test_controller.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_event_waiter.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/payments/content/service_worker_payment_app_finder.h"
#include "components/payments/core/features.h"
#include "components/payments/core/test_payment_manifest_downloader.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

namespace payments {

enum SkipToGPayMode {
  ALWAYS_SKIP_TO_GPAY,
  SKIP_TO_GPAY_IF_NO_CARD,
};

enum SkipToGPayTestConfig {
  TEST_HAS_COMPLETE_AUTOFILL_INSTRUMENT,
  TEST_INCOMPLETE_AUTOFILL_INSTRUMENT,
  TEST_NO_AUTOFILL_INSTRUMENT,
};

// Parameterized test fixture that tests the two skip-to-GPay modes (i.e.
// always-skip and skip-if-no-card) for all three cases of:
// - user has a complete autofill instrument
// - user has an incomplete autofill instrument
// - user doesn't have any autofill instrument
class HybridRequestSkipUITest
    : public PlatformBrowserTest,
      public PaymentRequestTestObserver,
      public testing::WithParamInterface<
          std::tuple<SkipToGPayMode, SkipToGPayTestConfig>> {
 public:
  // PaymentRequestTestObserver events that can be waited on by the EventWaiter.
  enum TestEvent : int {
    SHOW_APPS_READY,
  };

  HybridRequestSkipUITest()
      : gpay_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    test_controller_.SetObserver(this);

    if (GetTestMode() == ALWAYS_SKIP_TO_GPAY) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kPaymentRequestSkipToGPay);
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{features::kPaymentRequestSkipToGPayIfNoCard},
          /*disabled_features=*/{features::kPaymentRequestSkipToGPay});
    }
  }

  ~HybridRequestSkipUITest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // HTTPS server only serves a valid cert for localhost, so this is needed to
    // load pages from the fake "google.com" without an interstitial.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUpOnMainThread() override {
    // Map all out-going DNS lookups to the local server. This must be used in
    // conjunction with switches::kIgnoreCertificateErrors to work.
    host_resolver()->AddRule("*", "127.0.0.1");

    gpay_server_.ServeFilesFromSourceDirectory(
        "components/test/data/payments/google.com/");
    ASSERT_TRUE(gpay_server_.Start());

    https_server_.ServeFilesFromSourceDirectory(
        "components/test/data/payments/");
    ASSERT_TRUE(https_server_.Start());

    content::BrowserContext* context =
        GetActiveWebContents()->GetBrowserContext();
    auto downloader = std::make_unique<TestDownloader>(
        content::BrowserContext::GetDefaultStoragePartition(context)
            ->GetURLLoaderFactoryForBrowserProcess());
    downloader->AddTestServerURL("https://google.com/",
                                 gpay_server_.GetURL("google.com", "/"));
    ServiceWorkerPaymentAppFinder::GetInstance()
        ->SetDownloaderAndIgnorePortInOriginComparisonForTesting(
            std::move(downloader));

    ASSERT_TRUE(
        NavigateTo(https_server_.GetURL("/hybrid_request_skip_ui_test.html")));

    // Inject autofill instrument based on test config.
    if (GetTestConfig() == TEST_HAS_COMPLETE_AUTOFILL_INSTRUMENT) {
      autofill::CreditCard card = autofill::test::GetCreditCard();
      autofill::AutofillProfile address = autofill::test::GetFullProfile();
      test::AddAutofillProfile(GetActiveWebContents()->GetBrowserContext(),
                               address);
      card.set_billing_address_id(address.guid());
      test::AddCreditCard(GetActiveWebContents()->GetBrowserContext(), card);
    } else if (GetTestConfig() == TEST_INCOMPLETE_AUTOFILL_INSTRUMENT) {
      autofill::CreditCard card = autofill::test::GetCreditCard();
      test::AddCreditCard(GetActiveWebContents()->GetBrowserContext(), card);
    }

    test_controller_.SetUpOnMainThread();
    PlatformBrowserTest::SetUpOnMainThread();
  }

  // PaymentRequestTestObserver
  void OnShowAppsReady() override {
    if (event_waiter_)
      event_waiter_->OnEvent(TestEvent::SHOW_APPS_READY);
  }

  void ResetEventWaiterForSequence(std::list<TestEvent> event_sequence) {
    event_waiter_ = std::make_unique<autofill::EventWaiter<TestEvent>>(
        std::move(event_sequence));
  }

  void WaitForObservedEvent() { event_waiter_->Wait(); }

  // Convenience methods for accessing the test parameterization.
  SkipToGPayMode GetTestMode() const { return std::get<0>(GetParam()); }
  SkipToGPayTestConfig GetTestConfig() const { return std::get<1>(GetParam()); }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  bool NavigateTo(const GURL& url) {
    return content::NavigateToURL(GetActiveWebContents(), url);
  }

  // Runs a single test case and checks that |expected_result| is returned.
  void RunTest(const char* js_snippet, const char* expected_result) {
    if (GetTestMode() == SKIP_TO_GPAY_IF_NO_CARD &&
        GetTestConfig() == TEST_HAS_COMPLETE_AUTOFILL_INSTRUMENT) {
      // Skip-to-GPay is not activated in this combination because user has a
      // usable autofill instrument. Just verify that the payment sheet is
      // shown.
      ResetEventWaiterForSequence({TestEvent::SHOW_APPS_READY});
      EXPECT_TRUE(content::ExecJs(GetActiveWebContents(), js_snippet));
      WaitForObservedEvent();
      return;
    }

    // Skip-to-GPay should have been activated. Verify result.
    EXPECT_EQ(expected_result,
              content::EvalJs(GetActiveWebContents(), js_snippet));
  }

 protected:
  net::EmbeddedTestServer gpay_server_;
  net::EmbeddedTestServer https_server_;
  base::test::ScopedFeatureList scoped_feature_list_;
  PaymentRequestTestController test_controller_;
  std::unique_ptr<autofill::EventWaiter<TestEvent>> event_waiter_;
};

IN_PROC_BROWSER_TEST_P(HybridRequestSkipUITest, NothingRequested) {
  RunTest("buy({apiVersion: 1})",
          "{\"details\":{\"apiVersion\":1},\"shippingAddress\":null,"
          "\"shippingOption\":null,\"payerName\":null,\"payerEmail\":null,"
          "\"payerPhone\":null}");
}

IN_PROC_BROWSER_TEST_P(HybridRequestSkipUITest, ShippingRequested_V1) {
  RunTest(
      "buy({apiVersion: 1, requestShipping: true})",
      "{\"details\":{\"apiVersion\":1},\"shippingAddress\":{\"country\":\"CA\","
      "\"addressLine\":[\"123 Main Street\",\"Unit "
      "A\"],\"region\":\"ON\",\"city\":\"Toronto\",\"dependentLocality\":\"\","
      "\"postalCode\":\"12345\",\"sortingCode\":\"\",\"organization\":\"\","
      "\"recipient\":\"Browser "
      "Test\",\"phone\":\"\"},\"shippingOption\":\"free-shipping\","
      "\"payerName\":null,\"payerEmail\":null,\"payerPhone\":null}");
}

IN_PROC_BROWSER_TEST_P(HybridRequestSkipUITest, ShippingRequested_V2) {
  RunTest(
      "buy({apiVersion: 2, requestShipping: true})",
      "{\"details\":{\"apiVersion\":2},\"shippingAddress\":{\"country\":\"CA\","
      "\"addressLine\":[\"123 Main Street\",\"Unit "
      "A\"],\"region\":\"ON\",\"city\":\"Toronto\",\"dependentLocality\":\"\","
      "\"postalCode\":\"12345\",\"sortingCode\":\"\",\"organization\":\"\","
      "\"recipient\":\"Browser "
      "Test\",\"phone\":\"\"},\"shippingOption\":\"free-shipping\","
      "\"payerName\":null,\"payerEmail\":null,\"payerPhone\":null}");
}

IN_PROC_BROWSER_TEST_P(HybridRequestSkipUITest, EmailRequested_V1) {
  RunTest("buy({apiVersion: 1, requestEmail: true})",
          "{\"details\":{\"apiVersion\":1},\"shippingAddress\":null,"
          "\"shippingOption\":null,\"payerName\":null,\"payerEmail\":"
          "\"paymentrequest@chromium.org\",\"payerPhone\":null}");
}

IN_PROC_BROWSER_TEST_P(HybridRequestSkipUITest, EmailRequested_V2) {
  RunTest("buy({apiVersion: 2, requestEmail: true})",
          "{\"details\":{\"apiVersion\":2},\"shippingAddress\":null,"
          "\"shippingOption\":null,\"payerName\":null,\"payerEmail\":"
          "\"paymentrequest@chromium.org\",\"payerPhone\":null}");
}

IN_PROC_BROWSER_TEST_P(HybridRequestSkipUITest, NameRequested_V1) {
  RunTest("buy({apiVersion: 1, requestName: true})",
          "{\"details\":{\"apiVersion\":1,\"cardInfo\":{}},\"shippingAddress\":"
          "null,\"shippingOption\":null,\"payerName\":\"Browser "
          "Test\",\"payerEmail\":null,\"payerPhone\":null}");
}

IN_PROC_BROWSER_TEST_P(HybridRequestSkipUITest, NameRequested_V2) {
  RunTest(
      "buy({apiVersion: 2, requestName: true})",
      "{\"details\":{\"apiVersion\":2,\"paymentMethodData\":{\"type\":\"CARD\","
      "\"info\":{}}},\"shippingAddress\":null,\"shippingOption\":null,"
      "\"payerName\":\"BrowserTest\",\"payerEmail\":null,\"payerPhone\":null}");
}

IN_PROC_BROWSER_TEST_P(HybridRequestSkipUITest, PhoneRequested_V1) {
  RunTest("buy({apiVersion: 1, requestPhone: true})",
          "{\"details\":{\"apiVersion\":1,\"cardInfo\":{}},\"shippingAddress\":"
          "null,\"shippingOption\":null,\"payerName\":null,\"payerEmail\":null,"
          "\"payerPhone\":\"+1 234-567-8900\"}");
}

IN_PROC_BROWSER_TEST_P(HybridRequestSkipUITest, PhoneRequested_V2) {
  RunTest(
      "buy({apiVersion: 2, requestPhone: true})",
      "{\"details\":{\"apiVersion\":2,\"paymentMethodData\":{\"type\":\"CARD\","
      "\"info\":{}}},\"shippingAddress\":null,\"shippingOption\":null,"
      "\"payerName\":null,\"payerEmail\":null,\"payerPhone\":\"+1 "
      "234-567-8900\"}");
}

IN_PROC_BROWSER_TEST_P(HybridRequestSkipUITest, AllRequested_V1) {
  RunTest(
      "buy({apiVersion: 1, requestShipping: true, requestEmail: "
      "true, requestName: true, requestPhone: true})",
      "{\"details\":{\"apiVersion\":1,\"cardInfo\":{}},\"shippingAddress\":{"
      "\"country\":\"CA\",\"addressLine\":[\"123 Main Street\",\"Unit "
      "A\"],\"region\":\"ON\",\"city\":\"Toronto\",\"dependentLocality\":\"\","
      "\"postalCode\":\"12345\",\"sortingCode\":\"\",\"organization\":\"\","
      "\"recipient\":\"Browser "
      "Test\",\"phone\":\"\"},\"shippingOption\":\"free-shipping\","
      "\"payerName\":\"Browser "
      "Test\",\"payerEmail\":\"paymentrequest@chromium.org\",\"payerPhone\":\"+"
      "1 234-567-8900\"}");
}

IN_PROC_BROWSER_TEST_P(HybridRequestSkipUITest, AllRequested_V2) {
  RunTest(
      "buy({apiVersion: 2, requestShipping: true, requestEmail: "
      "true, requestName: true, requestPhone: true})",
      "{\"details\":{\"apiVersion\":2,\"paymentMethodData\":{\"type\":\"CARD\","
      "\"info\":{}}},\"shippingAddress\":{\"country\":\"CA\",\"addressLine\":["
      "\"123 Main Street\",\"Unit "
      "A\"],\"region\":\"ON\",\"city\":\"Toronto\",\"dependentLocality\":\"\","
      "\"postalCode\":\"12345\",\"sortingCode\":\"\",\"organization\":\"\","
      "\"recipient\":\"Browser "
      "Test\",\"phone\":\"\"},\"shippingOption\":\"free-shipping\","
      "\"payerName\":\"BrowserTest\",\"payerEmail\":\"paymentrequest@chromium."
      "org\",\"payerPhone\":\"+1 234-567-8900\"}");
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    HybridRequestSkipUITest,
    ::testing::Combine(::testing::Values(ALWAYS_SKIP_TO_GPAY,
                                         SKIP_TO_GPAY_IF_NO_CARD),
                       ::testing::Values(TEST_HAS_COMPLETE_AUTOFILL_INSTRUMENT,
                                         TEST_INCOMPLETE_AUTOFILL_INSTRUMENT,
                                         TEST_NO_AUTOFILL_INSTRUMENT)));

}  // namespace payments
