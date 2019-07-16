// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/credit_card_access_manager.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/metrics_hashes.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_download_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/form_events.h"
#include "components/autofill/core/browser/payments/test_payments_client.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/prefs/pref_service.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/version_info/channel.h"
#include "net/base/url_util.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

#if !defined(OS_IOS)
#include "components/autofill/core/browser/payments/test_credit_card_fido_authenticator.h"
#endif

using base::ASCIIToUTF16;

namespace autofill {
namespace {

const char kTestGUID[] = "00000000-0000-0000-0000-000000000001";
const char kTestNumber[] = "4234567890123456";  // Visa

class TestAccessor : public CreditCardAccessManager::Accessor {
 public:
  TestAccessor() {}

  base::WeakPtr<TestAccessor> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void OnCreditCardFetched(bool did_succeed,
                           const CreditCard* card,
                           const base::string16& cvc) override {
    did_succeed_ = did_succeed;
    if (did_succeed_) {
      DCHECK(card);
      number_ = card->number();
    }
  }

  base::string16 number() { return number_; }

  bool did_succeed() { return did_succeed_; }

 private:
  // Is set to true if authentication was successful.
  bool did_succeed_ = false;
  // The card number returned from OnCreditCardFetched().
  base::string16 number_;
  base::WeakPtrFactory<TestAccessor> weak_ptr_factory_{this};
};

std::string NextYear() {
  base::Time::Exploded now;
  base::Time::Now().LocalExplode(&now);
  return base::NumberToString(now.year + 1);
}

std::string NextMonth() {
  base::Time::Exploded now;
  base::Time::Now().LocalExplode(&now);
  return base::NumberToString(now.month % 12 + 1);
}

}  // namespace

class CreditCardAccessManagerTest : public testing::Test {
 public:
  CreditCardAccessManagerTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::DEFAULT,
            base::test::ScopedTaskEnvironment::ThreadPoolExecutionMode::
                QUEUED) {}

  void SetUp() override {
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    personal_data_manager_.Init(/*profile_database=*/database_,
                                /*account_database=*/nullptr,
                                /*pref_service=*/autofill_client_.GetPrefs(),
                                /*identity_manager=*/nullptr,
                                /*client_profile_validator=*/nullptr,
                                /*history_service=*/nullptr,
                                /*is_off_the_record=*/false);
    personal_data_manager_.SetPrefService(autofill_client_.GetPrefs());

    accessor_.reset(new TestAccessor());
    autofill_driver_ =
        std::make_unique<testing::NiceMock<TestAutofillDriver>>();
    request_context_ = new net::TestURLRequestContextGetter(
        base::ThreadTaskRunnerHandle::Get());
    autofill_driver_->SetURLRequestContext(request_context_.get());

    payments_client_ = new payments::TestPaymentsClient(
        autofill_driver_->GetURLLoaderFactory(),
        autofill_client_.GetIdentityManager(), &personal_data_manager_);
    autofill_client_.set_test_payments_client(
        std::unique_ptr<payments::TestPaymentsClient>(payments_client_));
    credit_card_access_manager_ = std::make_unique<CreditCardAccessManager>(
        autofill_driver_.get(), &autofill_client_, &personal_data_manager_,
        nullptr);
#if !defined(OS_IOS)
    credit_card_access_manager_->set_fido_authenticator_for_testing(
        std::make_unique<TestCreditCardFIDOAuthenticator>(
            autofill_driver_.get(), &autofill_client_));
#endif
  }

  void TearDown() override {
    // Order of destruction is important as AutofillDriver relies on
    // PersonalDataManager to be around when it gets destroyed.
    autofill_driver_.reset();

    personal_data_manager_.SetPrefService(nullptr);
    personal_data_manager_.ClearCreditCards();

    request_context_ = nullptr;
  }

  bool IsAuthenticationInProgress() {
    return credit_card_access_manager_->is_authentication_in_progress();
  }

  void CreateLocalCard(std::string guid, std::string number = std::string()) {
    CreditCard local_card = CreditCard();
    test::SetCreditCardInfo(&local_card, "Elvis Presley", number.c_str(),
                            NextMonth().c_str(), NextYear().c_str(), "1");
    local_card.set_guid(guid);
    local_card.set_record_type(CreditCard::LOCAL_CARD);

    personal_data_manager_.ClearCreditCards();
    personal_data_manager_.AddCreditCard(local_card);
  }

  void CreateServerCard(std::string guid, std::string number = std::string()) {
    CreditCard masked_server_card = CreditCard();
    test::SetCreditCardInfo(&masked_server_card, "Elvis Presley",
                            number.c_str(), NextMonth().c_str(),
                            NextYear().c_str(), "1");
    masked_server_card.set_guid(guid);
    masked_server_card.set_record_type(CreditCard::MASKED_SERVER_CARD);

    personal_data_manager_.ClearCreditCards();
    personal_data_manager_.AddServerCreditCard(masked_server_card);
  }

  CreditCardCVCAuthenticator* GetCVCAuthenticator() {
    return credit_card_access_manager_->GetOrCreateCVCAuthenticator();
  }

  // Returns true if full card request was sent from CVC auth.
  bool GetRealPanForCVCAuth(AutofillClient::PaymentsRpcResult result,
                            const std::string& real_pan) {
    payments::FullCardRequest* full_card_request =
        GetCVCAuthenticator()->full_card_request_.get();

    if (!full_card_request)
      return false;

    full_card_request->OnDidGetRealPan(result, real_pan);
    return true;
  }

#if !defined(OS_IOS)
  // Returns true if full card request was sent from FIDO auth.
  bool GetRealPanForFIDOAuth(AutofillClient::PaymentsRpcResult result,
                             const std::string& real_pan) {
    payments::FullCardRequest* full_card_request =
        GetFIDOAuthenticator()->full_card_request_.get();

    if (!full_card_request)
      return false;

    full_card_request->OnDidGetRealPan(result, real_pan);
    return true;
  }

  TestCreditCardFIDOAuthenticator* GetFIDOAuthenticator() {
    return static_cast<TestCreditCardFIDOAuthenticator*>(
        credit_card_access_manager_->GetOrCreateFIDOAuthenticator());
  }

  void OnFIDOUserVerification(bool did_succeed) {
    // TODO(crbug/949269): Currently CreditCardFIDOAuthenticator fails by
    // default. Once implemented, update this function along with
    // TestCreditCardFIDOAuthenticator to mock a user verification gesture.
  }
#endif

  void InvokeUnmaskDetailsTimeout() {
    credit_card_access_manager_->ready_to_start_authentication_.Signal();
  }

  void WaitForCallbacks() { scoped_task_environment_.RunUntilIdle(); }

 protected:
  std::unique_ptr<TestAccessor> accessor_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  payments::TestPaymentsClient* payments_client_;
  TestAutofillClient autofill_client_;
  std::unique_ptr<TestAutofillDriver> autofill_driver_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  scoped_refptr<AutofillWebDataService> database_;
  TestPersonalDataManager personal_data_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<CreditCardAccessManager> credit_card_access_manager_;
};

// Ensures GetCreditCard() successfully retrieves Card.
TEST_F(CreditCardAccessManagerTest, GetCreditCardSuccess) {
  CreateLocalCard(kTestGUID);

  CreditCard* card = credit_card_access_manager_->GetCreditCard(kTestGUID);
  EXPECT_NE(card, nullptr);
}

// Ensures GetCreditCard() returns nullptr for invalid GUID.
TEST_F(CreditCardAccessManagerTest, GetCreditCardFailure) {
  CreditCard* card = credit_card_access_manager_->GetCreditCard(kTestGUID);
  EXPECT_EQ(card, nullptr);
}

// Ensures DeleteCard() successfully removes local cards.
TEST_F(CreditCardAccessManagerTest, RemoveLocalCreditCard) {
  CreateLocalCard(kTestGUID);
  CreditCard* card = credit_card_access_manager_->GetCreditCard(kTestGUID);

  EXPECT_TRUE(personal_data_manager_.GetCreditCardWithGUID(kTestGUID));
  EXPECT_TRUE(credit_card_access_manager_->DeleteCard(card));
  EXPECT_FALSE(personal_data_manager_.GetCreditCardWithGUID(kTestGUID));
}

// Ensures DeleteCard() does nothing for server cards.
TEST_F(CreditCardAccessManagerTest, RemoveServerCreditCard) {
  CreateServerCard(kTestGUID);
  CreditCard* card = credit_card_access_manager_->GetCreditCard(kTestGUID);

  EXPECT_TRUE(personal_data_manager_.GetCreditCardWithGUID(kTestGUID));
  EXPECT_FALSE(credit_card_access_manager_->DeleteCard(card));

  // Cannot delete server cards.
  EXPECT_TRUE(personal_data_manager_.GetCreditCardWithGUID(kTestGUID));
}

// Ensures GetDeletionConfirmationText(~) returns correct values for local
// cards.
TEST_F(CreditCardAccessManagerTest, LocalCardGetDeletionConfirmationText) {
  CreateLocalCard(kTestGUID);
  CreditCard* card = credit_card_access_manager_->GetCreditCard(kTestGUID);

  base::string16 title = base::string16();
  base::string16 body = base::string16();
  EXPECT_TRUE(credit_card_access_manager_->GetDeletionConfirmationText(
      card, &title, &body));

  // |title| and |body| should be updated appropriately.
  EXPECT_EQ(title, card->NetworkOrBankNameAndLastFourDigits());
  EXPECT_EQ(body,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_DELETE_CREDIT_CARD_SUGGESTION_CONFIRMATION_BODY));
}

// Ensures GetDeletionConfirmationText(~) returns false for server cards.
TEST_F(CreditCardAccessManagerTest, ServerCardGetDeletionConfirmationText) {
  CreateServerCard(kTestGUID);
  CreditCard* card = credit_card_access_manager_->GetCreditCard(kTestGUID);

  base::string16 title = base::string16();
  base::string16 body = base::string16();
  EXPECT_FALSE(credit_card_access_manager_->GetDeletionConfirmationText(
      card, &title, &body));

  // |title| and |body| should remain unchanged.
  EXPECT_EQ(title, base::string16());
  EXPECT_EQ(body, base::string16());
}

// Tests retrieving local cards.
TEST_F(CreditCardAccessManagerTest, FetchLocalCardSuccess) {
  CreateLocalCard(kTestGUID, kTestNumber);
  CreditCard* card = credit_card_access_manager_->GetCreditCard(kTestGUID);

  credit_card_access_manager_->PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());

  EXPECT_TRUE(accessor_->did_succeed());
  EXPECT_EQ(ASCIIToUTF16(kTestNumber), accessor_->number());
}

// Ensures that FetchCreditCard() reports a failure when a card does not exist.
TEST_F(CreditCardAccessManagerTest, FetchNullptrFailure) {
  personal_data_manager_.ClearCreditCards();

  credit_card_access_manager_->PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager_->FetchCreditCard(nullptr,
                                               accessor_->GetWeakPtr());
  EXPECT_FALSE(accessor_->did_succeed());
}

// Ensures that FetchCreditCard() returns the full PAN upon a successful
// response from payments.
TEST_F(CreditCardAccessManagerTest, FetchServerCardCVCSuccess) {
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card = credit_card_access_manager_->GetCreditCard(kTestGUID);

  credit_card_access_manager_->PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());

  EXPECT_TRUE(GetRealPanForCVCAuth(AutofillClient::SUCCESS, kTestNumber));
  EXPECT_TRUE(accessor_->did_succeed());
  EXPECT_EQ(ASCIIToUTF16(kTestNumber), accessor_->number());
}

// Ensures that FetchCreditCard() returns a failure upon a negative response
// from the server.
TEST_F(CreditCardAccessManagerTest, FetchServerCardCVCNetworkError) {
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card = credit_card_access_manager_->GetCreditCard(kTestGUID);

  credit_card_access_manager_->PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());

  EXPECT_TRUE(
      GetRealPanForCVCAuth(AutofillClient::NETWORK_ERROR, std::string()));
  EXPECT_FALSE(accessor_->did_succeed());
}

// Ensures that FetchCreditCard() returns a failure upon a negative response
// from the server.
TEST_F(CreditCardAccessManagerTest, FetchServerCardCVCPermanentFailure) {
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card = credit_card_access_manager_->GetCreditCard(kTestGUID);

  credit_card_access_manager_->PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());

  EXPECT_TRUE(
      GetRealPanForCVCAuth(AutofillClient::PERMANENT_FAILURE, std::string()));
  EXPECT_FALSE(accessor_->did_succeed());
}

// Ensures that a "try again" response from payments does not end the flow.
TEST_F(CreditCardAccessManagerTest, FetchServerCardCVCTryAgainFailure) {
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card = credit_card_access_manager_->GetCreditCard(kTestGUID);

  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());

  EXPECT_TRUE(
      GetRealPanForCVCAuth(AutofillClient::TRY_AGAIN_FAILURE, std::string()));
  EXPECT_FALSE(accessor_->did_succeed());

  EXPECT_TRUE(GetRealPanForCVCAuth(AutofillClient::SUCCESS, kTestNumber));
  EXPECT_TRUE(accessor_->did_succeed());
  EXPECT_EQ(ASCIIToUTF16(kTestNumber), accessor_->number());
}

#if !defined(OS_IOS)
// Ensures that CVC prompt is invoked after WebAuthn fails.
TEST_F(CreditCardAccessManagerTest, FetchServerCardFIDOFailureCVCFallback) {
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card = credit_card_access_manager_->GetCreditCard(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  GetFIDOAuthenticator()->SetUserOptIn(true);
  payments_client_->AddFidoEligibleCard(card->server_id());

  credit_card_access_manager_->PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());
  WaitForCallbacks();

  // FIDO Failure.
  OnFIDOUserVerification(/*did_succeed=*/false);
  EXPECT_FALSE(GetRealPanForFIDOAuth(AutofillClient::SUCCESS, kTestNumber));
  EXPECT_FALSE(accessor_->did_succeed());

  // Followed by a fallback to CVC.
  EXPECT_TRUE(GetRealPanForCVCAuth(AutofillClient::SUCCESS, kTestNumber));
  EXPECT_TRUE(accessor_->did_succeed());
  EXPECT_EQ(ASCIIToUTF16(kTestNumber), accessor_->number());
}

// Ensures that CVC prompt is invoked when the pre-flight call to Google
// Payments times out.
TEST_F(CreditCardAccessManagerTest, FetchServerCardFIDOTimeoutCVCFallback) {
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card = credit_card_access_manager_->GetCreditCard(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  GetFIDOAuthenticator()->SetUserOptIn(true);

  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());
  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();

  EXPECT_TRUE(GetRealPanForCVCAuth(AutofillClient::SUCCESS, kTestNumber));
  EXPECT_TRUE(accessor_->did_succeed());
  EXPECT_EQ(ASCIIToUTF16(kTestNumber), accessor_->number());
}
#endif

// Ensures that |is_authentication_in_progress_| is set correctly.
TEST_F(CreditCardAccessManagerTest, AuthenticationInProgress) {
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card = credit_card_access_manager_->GetCreditCard(kTestGUID);

  EXPECT_FALSE(IsAuthenticationInProgress());

  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());
  EXPECT_TRUE(IsAuthenticationInProgress());

  EXPECT_TRUE(GetRealPanForCVCAuth(AutofillClient::SUCCESS, kTestNumber));
  EXPECT_FALSE(IsAuthenticationInProgress());
}

}  // namespace autofill
