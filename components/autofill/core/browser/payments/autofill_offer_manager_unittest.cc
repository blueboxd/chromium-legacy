// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/payments/test_payments_client.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

static const char kTestAppLocale[] = "en";

static const int64_t kOneSecondMicros = 1000000;

namespace autofill {

namespace payments {

class AutofillOfferManagerTest : public testing::Test {
 public:
  AutofillOfferManagerTest() = default;
  ~AutofillOfferManagerTest() override = default;

  void SetUp() override {
    payments_client_ = new TestPaymentsClient(
        autofill_driver_.GetURLLoaderFactory(),
        autofill_client_.GetIdentityManager(), &personal_data_manager_);
    autofill_client_.set_test_payments_client(
        std::unique_ptr<TestPaymentsClient>(payments_client_));
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestAutofillClient autofill_client_;
  TestAutofillDriver autofill_driver_;
  TestPersonalDataManager personal_data_manager_;
  TestPaymentsClient* payments_client_;
  AutofillOfferManager autofill_offer_manager_;
};

TEST_F(AutofillOfferManagerTest, InitFirstCallSucceeds) {
  autofill_offer_manager_.Init(&autofill_client_, kTestAppLocale);
  // Should call GetOfferData, as this is the first time Init() has been called
  // and the |last_updated_timestamp| has not been set.
  EXPECT_EQ(payments_client_->get_offer_data_calls(), 1);
}

TEST_F(AutofillOfferManagerTest, InitBeforeOfferDataExpiry_OneSecond) {
  autofill_offer_manager_.last_updated_timestamp_ = AutofillClock::Now();
  task_environment_.FastForwardBy(base::TimeDelta::FromMicroseconds(
      kOfferDataExpiryTimeInMicros - kOneSecondMicros));
  autofill_offer_manager_.Init(&autofill_client_, kTestAppLocale);
  // Should not call GetOfferData because it hasn't been long enough since the
  // last successful request.
  EXPECT_EQ(payments_client_->get_offer_data_calls(), 0);
}

TEST_F(AutofillOfferManagerTest, InitAfterOfferDataExpiry_OneSecond) {
  autofill_offer_manager_.last_updated_timestamp_ = AutofillClock::Now();
  task_environment_.FastForwardBy(base::TimeDelta::FromMicroseconds(
      kOfferDataExpiryTimeInMicros + kOneSecondMicros));
  autofill_offer_manager_.Init(&autofill_client_, kTestAppLocale);
  // Should call GetOfferData because it has been long enough since the last
  // successful request.
  EXPECT_EQ(payments_client_->get_offer_data_calls(), 1);
}

TEST_F(AutofillOfferManagerTest, InitBeforeTimerExpiry_OneSecond) {
  payments_client_->SetShouldReturnOfferData(false);
  autofill_offer_manager_.Init(&autofill_client_, kTestAppLocale);
  EXPECT_EQ(payments_client_->get_offer_data_calls(), 1);
  task_environment_.FastForwardBy(base::TimeDelta::FromMicroseconds(
      kRequestExpiryTimeInMicros - kOneSecondMicros));
  autofill_offer_manager_.Init(&autofill_client_, kTestAppLocale);
  // Should not call GetOfferData a second time because even though the request
  // has not returned, it also has not expired.
  EXPECT_EQ(payments_client_->get_offer_data_calls(), 1);
}

TEST_F(AutofillOfferManagerTest, InitAfterTimerExpiry_OneSecond) {
  payments_client_->SetShouldReturnOfferData(false);
  autofill_offer_manager_.Init(&autofill_client_, kTestAppLocale);
  EXPECT_EQ(payments_client_->get_offer_data_calls(), 1);
  task_environment_.FastForwardBy(base::TimeDelta::FromMicroseconds(
      kRequestExpiryTimeInMicros + kOneSecondMicros));
  autofill_offer_manager_.Init(&autofill_client_, kTestAppLocale);
  // Should call GetOfferData a second time because even though the request has
  // not returned, it has expired.
  EXPECT_EQ(payments_client_->get_offer_data_calls(), 2);
}

TEST_F(AutofillOfferManagerTest, InitAfterOfferDataExpiryButRequestActive) {
  payments_client_->SetShouldReturnOfferData(false);
  autofill_offer_manager_.last_updated_timestamp_ = AutofillClock::Now();
  task_environment_.FastForwardBy(base::TimeDelta::FromMicroseconds(
      kOfferDataExpiryTimeInMicros + kOneSecondMicros));
  autofill_offer_manager_.Init(&autofill_client_, kTestAppLocale);
  // Should call GetOfferData because the offer data has expired.
  EXPECT_EQ(payments_client_->get_offer_data_calls(), 1);
  task_environment_.FastForwardBy(base::TimeDelta::FromMicroseconds(
      kRequestExpiryTimeInMicros - kOneSecondMicros));
  autofill_offer_manager_.Init(&autofill_client_, kTestAppLocale);
  // Should not call GetOfferData a second time because even though the request
  // has not returned, it has not expired.
  EXPECT_EQ(payments_client_->get_offer_data_calls(), 1);
}

}  // namespace payments

}  // namespace autofill
