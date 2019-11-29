// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/payments/payment_request_test_controller.h"
#include "components/autofill/core/browser/test_event_waiter.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/payments/content/service_worker_payment_app_finder.h"
#include "components/payments/core/test_payment_manifest_downloader.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"

#if defined(OS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

namespace payments {

class PaymentHandlerJustInTimeInstallationTest
    : public PlatformBrowserTest,
      public PaymentRequestTestObserver {
 protected:
  // PaymentRequestTestObserver events that can be waited on by the EventWaiter.
  enum TestEvent : int {
    PAYMENT_COMPLETED,
  };

  PaymentHandlerJustInTimeInstallationTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        kylepay_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    test_controller_.SetObserver(this);
  }

  ~PaymentHandlerJustInTimeInstallationTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    // HTTPS server only serves a valid cert for localhost, so this is needed to
    // load pages from the fake "kylepay.com" without an interstitial.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUpOnMainThread() override {
    // Map all out-going DNS lookups to the local server. This must be used in
    // conjunction with switches::kIgnoreCertificateErrors to work.
    host_resolver()->AddRule("*", "127.0.0.1");

    kylepay_server_.ServeFilesFromSourceDirectory(
        "components/test/data/payments/kylepay.com/");
    ASSERT_TRUE(kylepay_server_.Start());

    // Set up test manifest downloader that knows how to fake origin.
    content::BrowserContext* context =
        GetActiveWebContents()->GetBrowserContext();
    auto downloader = std::make_unique<TestDownloader>(
        content::BrowserContext::GetDefaultStoragePartition(context)
            ->GetURLLoaderFactoryForBrowserProcess());
    downloader->AddTestServerURL("https://kylepay.com/",
                                 kylepay_server_.GetURL("kylepay.com", "/"));
    ServiceWorkerPaymentAppFinder::GetInstance()
        ->SetDownloaderAndIgnorePortInOriginComparisonForTesting(
            std::move(downloader));

    https_server_.ServeFilesFromSourceDirectory(
        "components/test/data/payments");
    ASSERT_TRUE(https_server_.Start());

    ASSERT_TRUE(content::NavigateToURL(
        GetActiveWebContents(),
        https_server_.GetURL("/payment_request_bobpay_and_cards_test.html")));

    test_controller_.SetUpOnMainThread();
    PlatformBrowserTest::SetUpOnMainThread();
  }

  // PaymentRequestTestObserver implementation.
  void OnCompleteCalled() override {
    if (event_waiter_)
      event_waiter_->OnEvent(TestEvent::PAYMENT_COMPLETED);
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void ResetEventWaiterForSequence(std::list<TestEvent> event_sequence) {
    event_waiter_ = std::make_unique<autofill::EventWaiter<TestEvent>>(
        std::move(event_sequence));
  }

  void WaitForObservedEvent() { event_waiter_->Wait(); }

  void ExpectBodyContains(const std::string expected_string) {
    EXPECT_THAT(content::EvalJs(GetActiveWebContents(),
                                "window.document.body.textContent")
                    .ExtractString(),
                ::testing::HasSubstr(expected_string));
  }

 private:
  PaymentRequestTestController test_controller_;
  net::EmbeddedTestServer https_server_;
  net::EmbeddedTestServer kylepay_server_;
  std::unique_ptr<autofill::EventWaiter<TestEvent>> event_waiter_;
};

// kylepay.com hosts an installable payment app which handles both shipping
// address and payer's contact information.
IN_PROC_BROWSER_TEST_F(PaymentHandlerJustInTimeInstallationTest,
                       InstallPaymentAppAndPay) {
  ResetEventWaiterForSequence({TestEvent::PAYMENT_COMPLETED});
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      "testPaymentMethods([{supportedMethods: 'https://kylepay.com/webpay'}], "
      "false/*= requestShippingContact */);"));
  WaitForObservedEvent();

  // kylepay should be installed just-in-time and used for testing.
  ExpectBodyContains("kylepay.com/webpay");
}

IN_PROC_BROWSER_TEST_F(PaymentHandlerJustInTimeInstallationTest,
                       InstallPaymentAppAndPayWithDelegation) {
  ResetEventWaiterForSequence({TestEvent::PAYMENT_COMPLETED});
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      "testPaymentMethods([{supportedMethods: 'https://kylepay.com/webpay'}], "
      "true/*= requestShippingContact */);"));
  WaitForObservedEvent();

  // kylepay should be installed just-in-time and used for testing.
  ExpectBodyContains("kylepay.com/webpay");
}

}  // namespace payments
