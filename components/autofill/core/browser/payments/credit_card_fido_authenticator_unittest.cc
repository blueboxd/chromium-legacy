// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/credit_card_fido_authenticator.h"

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
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_download_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/form_events.h"
#include "components/autofill/core/browser/payments/test_authentication_requester.h"
#include "components/autofill/core/browser/payments/test_credit_card_fido_authenticator.h"
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
#include "components/prefs/pref_service.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_params_manager.h"
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

using base::ASCIIToUTF16;

namespace autofill {
namespace {

const char kTestGUID[] = "00000000-0000-0000-0000-000000000001";
const char kTestNumber[] = "4234567890123456";  // Visa

std::string NextMonth() {
  base::Time::Exploded now;
  base::Time::Now().LocalExplode(&now);
  return base::NumberToString(now.month % 12 + 1);
}

}  // namespace

class CreditCardFIDOAuthenticatorTest : public testing::Test {
 public:
  CreditCardFIDOAuthenticatorTest() {}

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

    requester_.reset(new TestAuthenticationRequester());
    autofill_driver_ =
        std::make_unique<testing::NiceMock<TestAutofillDriver>>();
    request_context_ = new net::TestURLRequestContextGetter(
        base::ThreadTaskRunnerHandle::Get());
    autofill_driver_->SetURLRequestContext(request_context_.get());

    payments::TestPaymentsClient* payments_client =
        new payments::TestPaymentsClient(
            autofill_driver_->GetURLLoaderFactory(),
            autofill_client_.GetIdentityManager(), &personal_data_manager_);
    autofill_client_.set_test_payments_client(
        std::unique_ptr<payments::TestPaymentsClient>(payments_client));
    fido_authenticator_ = std::make_unique<CreditCardFIDOAuthenticator>(
        autofill_driver_.get(), &autofill_client_);
  }

  void TearDown() override {
    // Order of destruction is important as AutofillDriver relies on
    // PersonalDataManager to be around when it gets destroyed.
    autofill_driver_.reset();

    request_context_ = nullptr;
  }

  CreditCard CreateServerCard(std::string guid, std::string number) {
    CreditCard masked_server_card = CreditCard();
    test::SetCreditCardInfo(&masked_server_card, "Elvis Presley",
                            number.c_str(), NextMonth().c_str(),
                            test::NextYear().c_str(), "1");
    masked_server_card.set_guid(guid);
    masked_server_card.set_record_type(CreditCard::MASKED_SERVER_CARD);

    personal_data_manager_.ClearCreditCards();
    personal_data_manager_.AddServerCreditCard(masked_server_card);

    return masked_server_card;
  }

 protected:
  std::unique_ptr<TestAuthenticationRequester> requester_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  TestAutofillClient autofill_client_;
  std::unique_ptr<TestAutofillDriver> autofill_driver_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  scoped_refptr<AutofillWebDataService> database_;
  TestPersonalDataManager personal_data_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<CreditCardFIDOAuthenticator> fido_authenticator_;
};

TEST_F(CreditCardFIDOAuthenticatorTest, IsUserOptedInFalse) {
  EXPECT_FALSE(fido_authenticator_->IsUserOptedIn());
}

TEST_F(CreditCardFIDOAuthenticatorTest, IsUserVerifiableFalse) {
  fido_authenticator_->IsUserVerifiable(
      base::BindOnce(&TestAuthenticationRequester::IsUserVerifiableCallback,
                     requester_->GetWeakPtr()));
  EXPECT_FALSE(requester_->is_user_verifiable().value());
}

TEST_F(CreditCardFIDOAuthenticatorTest, AuthenticateCardFailure) {
  CreditCard card = CreateServerCard(kTestGUID, kTestNumber);

  fido_authenticator_->Authenticate(&card, requester_->GetWeakPtr(),
                                    base::TimeTicks::Now(), base::Value());
  EXPECT_FALSE(requester_->did_succeed());
}

}  // namespace autofill
