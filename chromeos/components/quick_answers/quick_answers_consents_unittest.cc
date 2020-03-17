// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/quick_answers_consents.h"

#include <memory>
#include <string>

#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace quick_answers {

class QuickAnswersConsentTest : public testing::Test {
 public:
  QuickAnswersConsentTest() = default;

  ~QuickAnswersConsentTest() override = default;

  void SetUp() override {
    prefs::RegisterProfilePrefs(pref_service_.registry());
    consent_ = std::make_unique<QuickAnswersConsent>(&pref_service_);
  }

  void TearDown() override { consent_.reset(); }

  PrefService* pref_service() { return &pref_service_; }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<QuickAnswersConsent> consent_;
};

TEST_F(QuickAnswersConsentTest, ShouldShowConsentHasConsented) {
  EXPECT_TRUE(consent_->ShouldShowConsent());

  pref_service()->SetBoolean(prefs::kQuickAnswersConsented, true);

  // Verify that it is consented.
  EXPECT_FALSE(consent_->ShouldShowConsent());
}

TEST_F(QuickAnswersConsentTest, ShouldShowConsentHasReachedImpressionCap) {
  EXPECT_TRUE(consent_->ShouldShowConsent());

  pref_service()->SetInteger(prefs::kQuickAnswersConsentImpressionCount, 3);

  // Verify that impression cap is reached.
  EXPECT_FALSE(consent_->ShouldShowConsent());
}

TEST_F(QuickAnswersConsentTest, ShouldShowConsentHasReachedDurationCap) {
  EXPECT_TRUE(consent_->ShouldShowConsent());

  pref_service()->SetInteger(prefs::kQuickAnswersConsentImpressionDuration, 7);
  // Not reach impression duration cap yet.
  EXPECT_TRUE(consent_->ShouldShowConsent());

  pref_service()->SetInteger(prefs::kQuickAnswersConsentImpressionDuration, 8);
  // Reach impression duration cap.
  EXPECT_FALSE(consent_->ShouldShowConsent());
}

TEST_F(QuickAnswersConsentTest, AcceptConsent) {
  EXPECT_TRUE(consent_->ShouldShowConsent());

  consent_->StartConsent();

  // Consent is accepted after 6 seconds.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(6));
  consent_->AcceptConsent(ConsentInteractionType::kAccept);

  // Verify that it is consented.
  ASSERT_TRUE(pref_service()->GetBoolean(prefs::kQuickAnswersConsented));
  // Verify that the duration is recorded.
  ASSERT_EQ(6, pref_service()->GetInteger(
                   prefs::kQuickAnswersConsentImpressionDuration));
  // Verify that it is consented.
  EXPECT_FALSE(consent_->ShouldShowConsent());
}

TEST_F(QuickAnswersConsentTest, DismissConsent) {
  // Start consent.
  consent_->StartConsent();

  // Dismiss consent after reaching the impression cap.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(8));
  consent_->DismissConsent();

  // Verify that the impression count is recorded.
  ASSERT_EQ(8, pref_service()->GetInteger(
                   prefs::kQuickAnswersConsentImpressionDuration));
}

}  // namespace quick_answers
}  // namespace chromeos
