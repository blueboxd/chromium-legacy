// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/payments/payment_request_test_controller.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/test_event_waiter.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/payments/core/features.h"
#include "components/sync/driver/test_sync_service.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"

#if defined(OS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

namespace payments {

class WaitForFinishedPersonalDataManagerObserver
    : public autofill::PersonalDataManagerObserver {
 public:
  explicit WaitForFinishedPersonalDataManagerObserver(
      base::OnceClosure callback)
      : callback_(std::move(callback)) {}

  // autofill::PersonalDataManagerObserver implementation.
  void OnPersonalDataChanged() override {}
  void OnPersonalDataFinishedProfileTasks() override {
    std::move(callback_).Run();
  }

 private:
  base::OnceClosure callback_;
};

class PaymentRequestCanMakePaymentTestBase : public PlatformBrowserTest,
                                             public PaymentRequestTestObserver {
 protected:
  // Various events that can be waited on by the EventWaiter.
  enum TestEvent : int {
    CAN_MAKE_PAYMENT_CALLED,
    CAN_MAKE_PAYMENT_RETURNED,
    HAS_ENROLLED_INSTRUMENT_CALLED,
    HAS_ENROLLED_INSTRUMENT_RETURNED,
    CONNECTION_TERMINATED,
    NOT_SUPPORTED_ERROR,
    ABORT_CALLED,
  };

  PaymentRequestCanMakePaymentTestBase() : payment_request_controller_(this) {
    feature_list_.InitAndDisableFeature(
        ::features::kPaymentRequestHasEnrolledInstrument);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // HTTPS server only serves a valid cert for localhost, so this is needed to
    // load pages from "a.com" without an interstitial.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    ASSERT_TRUE(https_server_->InitializeAndListen());
    https_server_->ServeFilesFromSourceDirectory(
        "components/test/data/payments");
    https_server_->StartAcceptingConnections();

    Profile* profile = Profile::FromBrowserContext(
        GetActiveWebContents()->GetBrowserContext());
    autofill::PersonalDataManager* personal_data_manager =
        autofill::PersonalDataManagerFactory::GetForProfile(profile);
    personal_data_manager->SetSyncServiceForTest(&sync_service_);

    payment_request_controller_.SetUpOnMainThread();
  }

  void SetIncognito(bool is_incognito) {
    payment_request_controller_.SetIncognito(is_incognito);
  }

  void SetValidSsl(bool valid_ssl) {
    payment_request_controller_.SetValidSsl(valid_ssl);
  }

  void SetCanMakePaymentEnabledPref(bool can_make_payment) {
    payment_request_controller_.SetCanMakePaymentEnabledPref(can_make_payment);
  }

  void ExpectBodyContains(const std::vector<std::string>& expected_strings) {
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string extract_contents_js =
        "(function() { "
        "window.domAutomationController.send(window.document.body.textContent);"
        " "
        "})()";
    std::string contents;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        web_contents, extract_contents_js, &contents));
    for (const std::string& expected_string : expected_strings) {
      EXPECT_NE(std::string::npos, contents.find(expected_string))
          << "String \"" << expected_string
          << "\" is not present in the content \"" << contents << "\"";
    }
  }

  void AddAutofillProfile(const autofill::AutofillProfile& autofill_profile) {
    Profile* profile = Profile::FromBrowserContext(
        GetActiveWebContents()->GetBrowserContext());
    autofill::PersonalDataManager* personal_data_manager =
        autofill::PersonalDataManagerFactory::GetForProfile(profile);
    size_t profile_count = personal_data_manager->GetProfiles().size();

    base::RunLoop data_loop;
    WaitForFinishedPersonalDataManagerObserver personal_data_observer(
        data_loop.QuitClosure());
    personal_data_manager->AddObserver(&personal_data_observer);

    personal_data_manager->AddProfile(autofill_profile);
    data_loop.Run();

    personal_data_manager->RemoveObserver(&personal_data_observer);
    EXPECT_EQ(profile_count + 1, personal_data_manager->GetProfiles().size());
  }

  void AddCreditCard(const autofill::CreditCard& card) {
    Profile* profile = Profile::FromBrowserContext(
        GetActiveWebContents()->GetBrowserContext());
    autofill::PersonalDataManager* personal_data_manager =
        autofill::PersonalDataManagerFactory::GetForProfile(profile);
    if (card.record_type() != autofill::CreditCard::LOCAL_CARD) {
      personal_data_manager->AddServerCreditCardForTest(
          std::make_unique<autofill::CreditCard>(card));
      return;
    }
    size_t card_count = personal_data_manager->GetCreditCards().size();

    base::RunLoop data_loop;
    WaitForFinishedPersonalDataManagerObserver personal_data_observer(
        data_loop.QuitClosure());
    personal_data_manager->AddObserver(&personal_data_observer);

    personal_data_manager->AddCreditCard(card);
    data_loop.Run();

    personal_data_manager->RemoveObserver(&personal_data_observer);
    EXPECT_EQ(card_count + 1, personal_data_manager->GetCreditCards().size());
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void NavigateTo(const std::string& file_path) {
    EXPECT_TRUE(content::NavigateToURL(
        GetActiveWebContents(), https_server_->GetURL("a.com", file_path)));
  }

  // PaymentRequestTestObserver implementation.
  void OnCanMakePaymentCalled() override {
    if (event_waiter_)
      event_waiter_->OnEvent(TestEvent::CAN_MAKE_PAYMENT_CALLED);
  }
  void OnCanMakePaymentReturned() override {
    if (event_waiter_)
      event_waiter_->OnEvent(TestEvent::CAN_MAKE_PAYMENT_RETURNED);
  }
  void OnHasEnrolledInstrumentCalled() override {
    if (event_waiter_)
      event_waiter_->OnEvent(TestEvent::HAS_ENROLLED_INSTRUMENT_CALLED);
  }
  void OnHasEnrolledInstrumentReturned() override {
    if (event_waiter_)
      event_waiter_->OnEvent(TestEvent::HAS_ENROLLED_INSTRUMENT_RETURNED);
  }
  void OnNotSupportedError() override {
    if (event_waiter_)
      event_waiter_->OnEvent(TestEvent::NOT_SUPPORTED_ERROR);
  }
  void OnConnectionTerminated() override {
    if (event_waiter_)
      event_waiter_->OnEvent(TestEvent::CONNECTION_TERMINATED);
  }
  void OnAbortCalled() override {
    if (event_waiter_)
      event_waiter_->OnEvent(TestEvent::ABORT_CALLED);
  }

  void ResetEventWaiterForSequence(std::list<TestEvent> event_sequence) {
    event_waiter_ = std::make_unique<autofill::EventWaiter<TestEvent>>(
        std::move(event_sequence));
  }
  void WaitForObservedEvent() { event_waiter_->Wait(); }

 private:
  PaymentRequestTestController payment_request_controller_;

  base::test::ScopedFeatureList feature_list_;
  syncer::TestSyncService sync_service_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<autofill::EventWaiter<TestEvent>> event_waiter_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequestCanMakePaymentTestBase);
};

class PaymentRequestCanMakePaymentQueryTest
    : public PaymentRequestCanMakePaymentTestBase {
 protected:
  PaymentRequestCanMakePaymentQueryTest() = default;

  void CallCanMakePayment() {
    ResetEventWaiterForSequence({TestEvent::CAN_MAKE_PAYMENT_CALLED,
                                 TestEvent::CAN_MAKE_PAYMENT_RETURNED});
    ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "buy();"));
    WaitForObservedEvent();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestCanMakePaymentQueryTest);
};

// Visa is required, and user has a visa instrument.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       CanMakePayment_Supported) {
  NavigateTo("/payment_request_can_make_payment_query_test.html");
  const autofill::CreditCard card = autofill::test::GetCreditCard();  // Visa.
  AddCreditCard(card);

  CallCanMakePayment();

  ExpectBodyContains({"true"});
}

// Visa is required, and user has a visa instrument, but canMakePayment is
// disabled by user preference.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       CanMakePayment_SupportedButDisabled) {
  SetCanMakePaymentEnabledPref(false);
  NavigateTo("/payment_request_can_make_payment_query_test.html");
  const autofill::CreditCard card = autofill::test::GetCreditCard();  // Visa.
  AddCreditCard(card);

  CallCanMakePayment();

  ExpectBodyContains({"false"});
}

// Visa is required, and user has a masked visa instrument, and Google Pay cards
// in basic-card is disabled.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       CanMakePayment_Supported_GooglePayCardsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      payments::features::kReturnGooglePayInBasicCard);
  NavigateTo("/payment_request_can_make_payment_query_test.html");
  autofill::CreditCard card = autofill::test::GetMaskedServerCard();
  card.SetNumber(base::ASCIIToUTF16("4111111111111111"));  // We need a visa.
  card.SetNetworkForMaskedCard(autofill::kVisaCard);
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);

  CallCanMakePayment();

  ExpectBodyContains({"false"});
}

// Visa is required, and user has a masked visa instrument, and Google Pay cards
// in basic-card is enabled.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       CanMakePayment_Supported_GooglePayCardsEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      payments::features::kReturnGooglePayInBasicCard);
  NavigateTo("/payment_request_can_make_payment_query_test.html");
  autofill::CreditCard card = autofill::test::GetMaskedServerCard();
  card.SetNumber(base::ASCIIToUTF16("4111111111111111"));  // We need a visa.
  card.SetNetworkForMaskedCard(autofill::kVisaCard);
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);

  CallCanMakePayment();

  ExpectBodyContains({"true"});
}

// Pages without a valid SSL certificate always get "false" from
// .canMakePayment().
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       CanMakePayment_InvalidSSL) {
  NavigateTo("/payment_request_can_make_payment_query_test.html");
  SetValidSsl(false);

  const autofill::CreditCard card = autofill::test::GetCreditCard();  // Visa.
  AddCreditCard(card);

  CallCanMakePayment();

  ExpectBodyContains({"false"});
}

// Visa is required, user has a visa instrument, and user is in incognito
// mode.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       CanMakePayment_Supported_InIncognitoMode) {
  NavigateTo("/payment_request_can_make_payment_query_test.html");
  SetIncognito(true);

  const autofill::CreditCard card = autofill::test::GetCreditCard();  // Visa.
  AddCreditCard(card);

  CallCanMakePayment();

  ExpectBodyContains({"true"});
}

// Visa is required, and user doesn't have a visa instrument.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       CanMakePayment_NotSupported) {
  NavigateTo("/payment_request_can_make_payment_query_test.html");
  const autofill::CreditCard card = autofill::test::GetCreditCard2();  // Amex.
  AddCreditCard(card);

  CallCanMakePayment();

  ExpectBodyContains({"false"});
}

// Visa is required, user doesn't have a visa instrument and the user is in
// incognito mode. In this case canMakePayment returns false as in a normal
// profile.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       CanMakePayment_NotSupported_InIncognitoMode) {
  NavigateTo("/payment_request_can_make_payment_query_test.html");
  SetIncognito(true);

  const autofill::CreditCard card = autofill::test::GetCreditCard2();  // Amex.
  AddCreditCard(card);

  CallCanMakePayment();

  ExpectBodyContains({"false"});
}

class PaymentRequestCanMakePaymentQueryCCTest
    : public PaymentRequestCanMakePaymentTestBase {
 protected:
  PaymentRequestCanMakePaymentQueryCCTest() = default;

  // If |visa| is true, then the method data is:
  //
  //   [{supportedMethods: ['visa']}]
  //
  // If |visa| is false, then the method data is:
  //
  //   [{supportedMethods: ['mastercard']}]
  void CallCanMakePayment(bool visa) {
    ResetEventWaiterForSequence({TestEvent::CAN_MAKE_PAYMENT_CALLED,
                                 TestEvent::CAN_MAKE_PAYMENT_RETURNED});
    ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(),
                                       visa ? "buy();" : "other_buy();"));
    WaitForObservedEvent();
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequestCanMakePaymentQueryCCTest);
};

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryCCTest, QueryQuota) {
  NavigateTo("/payment_request_can_make_payment_query_cc_test.html");
  // Query "visa" payment method.
  CallCanMakePayment(/*visa=*/true);

  // User does not have a visa card.
  ExpectBodyContains({"false"});

  // Query "mastercard" payment method.
  CallCanMakePayment(/*visa=*/false);

  // Query quota exceeded.
  ExpectBodyContains({"NotAllowedError"});

  AddCreditCard(autofill::test::GetCreditCard());  // visa

  // Query "visa" payment method.
  CallCanMakePayment(/*visa=*/true);

  // User now has a visa card. The query is cached, but the result is always
  // fresh.
  ExpectBodyContains({"true"});

  // Query "mastercard" payment method.
  CallCanMakePayment(/*visa=*/false);

  // Query quota exceeded.
  ExpectBodyContains({"NotAllowedError"});
}

// canMakePayment should return result in incognito mode as in normal mode to
// avoid incognito mode detection.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryCCTest,
                       QueryQuotaInIncognito) {
  NavigateTo("/payment_request_can_make_payment_query_cc_test.html");
  SetIncognito(true);

  // Query "visa" payment method.
  CallCanMakePayment(/*visa=*/true);

  ExpectBodyContains({"false"});

  // Query "mastercard" payment method.
  CallCanMakePayment(/*visa=*/false);

  ExpectBodyContains({"NotAllowedError"});

  AddCreditCard(autofill::test::GetCreditCard());  // visa

  // Query "visa" payment method.
  CallCanMakePayment(/*visa=*/true);

  ExpectBodyContains({"true"});

  // Query "mastercard" payment method.
  CallCanMakePayment(/*visa=*/false);

  ExpectBodyContains({"NotAllowedError"});
}

class PaymentRequestCanMakePaymentQueryPMITest
    : public PaymentRequestCanMakePaymentTestBase {
 protected:
  enum class CheckFor {
    BASIC_VISA,
    BASIC_CARD,
    ALICE_PAY,
    BOB_PAY,
    BOB_PAY_AND_BASIC_CARD,
    BOB_PAY_AND_VISA,
  };

  PaymentRequestCanMakePaymentQueryPMITest() {
    script_[CheckFor::BASIC_VISA] = "checkBasicVisa();";
    script_[CheckFor::BASIC_CARD] = "checkBasicCard();";
    script_[CheckFor::ALICE_PAY] = "checkAlicePay();";
    script_[CheckFor::BOB_PAY] = "checkBobPay();";
    script_[CheckFor::BOB_PAY_AND_BASIC_CARD] = "checkBobPayAndBasicCard();";
    script_[CheckFor::BOB_PAY_AND_VISA] = "checkBobPayAndVisa();";
  }

  void CallCanMakePayment(CheckFor check_for) {
    ResetEventWaiterForSequence({TestEvent::CAN_MAKE_PAYMENT_CALLED,
                                 TestEvent::CAN_MAKE_PAYMENT_RETURNED});
    ASSERT_TRUE(
        content::ExecuteScript(GetActiveWebContents(), script_[check_for]));
    WaitForObservedEvent();
  }

 private:
  base::flat_map<CheckFor, std::string> script_;
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequestCanMakePaymentQueryPMITest);
};

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryPMITest,
                       QueryQuotaForBasicCards) {
  NavigateTo("/payment_request_payment_method_identifier_test.html");
  // Query "basic-card" payment method with "supportedNetworks": ["visa"] in the
  // payment method specific data.
  CallCanMakePayment(CheckFor::BASIC_VISA);

  // User does not have a visa card.
  ExpectBodyContains({"false"});

  // Query "basic-card" payment method without "supportedNetworks" parameter.
  CallCanMakePayment(CheckFor::BASIC_CARD);

  // Query quota exceeded.
  ExpectBodyContains({"NotAllowedError"});

  AddCreditCard(autofill::test::GetCreditCard());  // visa

  // Query "basic-card" payment method with "supportedNetworks": ["visa"] in the
  // payment method specific data.
  CallCanMakePayment(CheckFor::BASIC_VISA);

  // User now has a visa card. The query is cached, but the result is always
  // fresh.
  ExpectBodyContains({"true"});

  // Query "basic-card" payment method without "supportedNetworks" parameter.
  CallCanMakePayment(CheckFor::BASIC_CARD);

  // Query quota exceeded.
  ExpectBodyContains({"NotAllowedError"});
}

// canMakePayment() should return result as in normal mode to avoid incognito
// mode detection.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryPMITest,
                       QueryQuotaForBasicCardsInIncognito) {
  NavigateTo("/payment_request_payment_method_identifier_test.html");
  SetIncognito(true);

  // Query "basic-card" payment method with "supportedNetworks": ["visa"] in the
  // payment method specific data.
  CallCanMakePayment(CheckFor::BASIC_VISA);

  ExpectBodyContains({"false"});

  // Query "basic-card" payment method without "supportedNetworks" parameter.
  CallCanMakePayment(CheckFor::BASIC_CARD);

  ExpectBodyContains({"NotAllowedError"});

  AddCreditCard(autofill::test::GetCreditCard());  // visa

  // Query "basic-card" payment method with "supportedNetworks": ["visa"] in the
  // payment method specific data.
  CallCanMakePayment(CheckFor::BASIC_VISA);

  ExpectBodyContains({"true"});

  // Query "basic-card" payment method without "supportedNetworks" parameter.
  CallCanMakePayment(CheckFor::BASIC_CARD);

  ExpectBodyContains({"NotAllowedError"});
}

// If the device does not have any payment apps installed, canMakePayment()
// should return false for them.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryPMITest,
                       QueryQuotaForPaymentApps) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kWebPaymentsPerMethodCanMakePaymentQuota);
  NavigateTo("/payment_request_payment_method_identifier_test.html");
  CallCanMakePayment(CheckFor::ALICE_PAY);

  ExpectBodyContains({"false"});

  CallCanMakePayment(CheckFor::BOB_PAY);

  ExpectBodyContains({"false"});

  CallCanMakePayment(CheckFor::ALICE_PAY);

  ExpectBodyContains({"false"});
}

// If the device does not have any payment apps installed, canMakePayment()
// queries for both payment apps and basic-card depend only on what cards the
// user has on file.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryPMITest,
                       QueryQuotaForPaymentAppsAndCards) {
  NavigateTo("/payment_request_payment_method_identifier_test.html");
  CallCanMakePayment(CheckFor::BOB_PAY_AND_VISA);

  ExpectBodyContains({"false"});

  CallCanMakePayment(CheckFor::BOB_PAY_AND_BASIC_CARD);

  ExpectBodyContains({"NotAllowedError"});

  AddCreditCard(autofill::test::GetCreditCard2());  // Amex

  CallCanMakePayment(CheckFor::BOB_PAY_AND_VISA);

  ExpectBodyContains({"false"});

  CallCanMakePayment(CheckFor::BOB_PAY_AND_BASIC_CARD);

  ExpectBodyContains({"NotAllowedError"});

  AddCreditCard(autofill::test::GetCreditCard());  // Visa

  CallCanMakePayment(CheckFor::BOB_PAY_AND_VISA);

  ExpectBodyContains({"true"});

  CallCanMakePayment(CheckFor::BOB_PAY_AND_BASIC_CARD);

  ExpectBodyContains({"NotAllowedError"});
}

// Querying for payment apps in incognito returns result as normal mode to avoid
// incognito mode detection. Multiple queries for different apps are rejected
// with NotSupportedError to avoid user fingerprinting.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryPMITest,
                       QueryQuotaForPaymentAppsInIncognitoMode) {
  NavigateTo("/payment_request_payment_method_identifier_test.html");
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enable_features=*/{::features::kServiceWorkerPaymentApps,
                           features::kWebPaymentsPerMethodCanMakePaymentQuota},
      /*disable_features=*/{});

  SetIncognito(true);

  CallCanMakePayment(CheckFor::ALICE_PAY);

  ExpectBodyContains({"false"});

  CallCanMakePayment(CheckFor::BOB_PAY);

  ExpectBodyContains({"false"});

  CallCanMakePayment(CheckFor::ALICE_PAY);

  ExpectBodyContains({"false"});

  CallCanMakePayment(CheckFor::BOB_PAY);

  ExpectBodyContains({"false"});
}

// Querying for both payment apps and autofill cards in incognito returns result
// as in normal mode to avoid incognito mode detection. Multiple queries for
// different payment methods are rejected with NotSupportedError to avoid user
// fingerprinting.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryPMITest,
                       NoQueryQuotaForPaymentAppsAndCardsInIncognito) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kWebPaymentsPerMethodCanMakePaymentQuota);
  NavigateTo("/payment_request_payment_method_identifier_test.html");
  SetIncognito(true);

  CallCanMakePayment(CheckFor::BOB_PAY_AND_VISA);

  ExpectBodyContains({"false"});

  CallCanMakePayment(CheckFor::BOB_PAY_AND_BASIC_CARD);

  ExpectBodyContains({"NotAllowedError"});

  AddCreditCard(autofill::test::GetCreditCard2());  // Amex

  CallCanMakePayment(CheckFor::BOB_PAY_AND_VISA);

  ExpectBodyContains({"false"});

  CallCanMakePayment(CheckFor::BOB_PAY_AND_BASIC_CARD);

  ExpectBodyContains({"NotAllowedError"});

  AddCreditCard(autofill::test::GetCreditCard());  // Visa

  CallCanMakePayment(CheckFor::BOB_PAY_AND_VISA);

  ExpectBodyContains({"true"});

  CallCanMakePayment(CheckFor::BOB_PAY);

  ExpectBodyContains({"false"});

  CallCanMakePayment(CheckFor::BASIC_VISA);

  ExpectBodyContains({"true"});
}

}  // namespace payments
