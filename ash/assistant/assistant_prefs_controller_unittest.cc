// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_prefs_controller.h"

#include <memory>

#include "ash/assistant/assistant_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/prefs/pref_service.h"

namespace ash {

class TestAssistantPrefsObserver : public AssistantPrefsObserver {
 public:
  TestAssistantPrefsObserver() = default;
  ~TestAssistantPrefsObserver() override = default;

  // AssistantPrefsObserver:
  void OnAssistantConsentStatusUpdated(int consent_status) override {
    consent_status_ = consent_status;
  }

  int consent_status() { return consent_status_; }

 private:
  int consent_status_ = chromeos::assistant::prefs::ConsentStatus::kUnknown;

  DISALLOW_COPY_AND_ASSIGN(TestAssistantPrefsObserver);
};

class AssistantPrefsControllerTest : public AshTestBase {
 protected:
  AssistantPrefsControllerTest() = default;
  ~AssistantPrefsControllerTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::switches::kAssistantFeature);
    ASSERT_TRUE(chromeos::switches::IsAssistantEnabled());

    AshTestBase::SetUp();

    prefs_ = Shell::Get()->assistant_controller()->prefs_controller()->prefs();
    DCHECK(prefs_);

    observer_ = std::make_unique<TestAssistantPrefsObserver>();
  }

  PrefService* prefs() { return prefs_; }

  TestAssistantPrefsObserver* observer() { return observer_.get(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  PrefService* prefs_ = nullptr;
  std::unique_ptr<TestAssistantPrefsObserver> observer_;

  DISALLOW_COPY_AND_ASSIGN(AssistantPrefsControllerTest);
};

TEST_F(AssistantPrefsControllerTest, InitObserver) {
  prefs()->SetInteger(
      chromeos::assistant::prefs::kAssistantConsentStatus,
      chromeos::assistant::prefs::ConsentStatus::kActivityControlAccepted);

  // The observer class should get an instant notification about the current
  // pref value.
  Shell::Get()->assistant_controller()->prefs_controller()->AddObserver(
      observer());
  EXPECT_EQ(chromeos::assistant::prefs::ConsentStatus::kActivityControlAccepted,
            observer()->consent_status());
}

TEST_F(AssistantPrefsControllerTest, NotifyConsentStatus) {
  Shell::Get()->assistant_controller()->prefs_controller()->AddObserver(
      observer());

  prefs()->SetInteger(chromeos::assistant::prefs::kAssistantConsentStatus,
                      chromeos::assistant::prefs::ConsentStatus::kUnauthorized);
  EXPECT_EQ(chromeos::assistant::prefs::ConsentStatus::kUnauthorized,
            observer()->consent_status());

  prefs()->SetInteger(
      chromeos::assistant::prefs::kAssistantConsentStatus,
      chromeos::assistant::prefs::ConsentStatus::kActivityControlAccepted);
  EXPECT_EQ(chromeos::assistant::prefs::ConsentStatus::kActivityControlAccepted,
            observer()->consent_status());
}

}  // namespace ash
