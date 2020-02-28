// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/metrics/user_metrics.h"
#include "base/optional.h"
#include "base/test/scoped_feature_list.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/views/hats/hats_bubble_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_test.h"

namespace {

base::test::ScopedFeatureList::FeatureAndParams probability_zero{
    features::kHappinessTrackingSurveysForDesktop,
    {{"probability", "0.000"}}};
base::test::ScopedFeatureList::FeatureAndParams probability_one{
    features::kHappinessTrackingSurveysForDesktop,
    {{"probability", "1.000"},
     {"survey", kHatsSurveyTriggerSatisfaction},
     {"en_site_id", "test_site_id"}}};
base::test::ScopedFeatureList::FeatureAndParams settings_probability_one{
    features::kHappinessTrackingSurveysForDesktopSettings,
    {{"probability", "1.000"},
     {"survey", kHatsSurveyTriggerSettings},
     {"en_site_id", "test_site_id"}}};
base::test::ScopedFeatureList::FeatureAndParams improved_cookie_controls{
    content_settings::kImprovedCookieControls,
    {{"DefaultInIncognito", "true"}}};

class ScopedSetMetricsConsent {
 public:
  // Enables or disables metrics consent based off of |consent|.
  explicit ScopedSetMetricsConsent(bool consent) : consent_(consent) {
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        &consent_);
  }

  ~ScopedSetMetricsConsent() {
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        nullptr);
  }

 private:
  const bool consent_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSetMetricsConsent);
};

class HatsServiceBrowserTestBase : public InProcessBrowserTest {
 protected:
  explicit HatsServiceBrowserTestBase(
      std::vector<base::test::ScopedFeatureList::FeatureAndParams>
          enabled_features)
      : enabled_features_(enabled_features) {
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features_, {});
  }

  HatsServiceBrowserTestBase() = default;

  ~HatsServiceBrowserTestBase() override = default;

  HatsService* GetHatsService() {
    return HatsServiceFactory::GetForProfile(browser()->profile(), true);
  }

  void SetMetricsConsent(bool consent) {
    scoped_metrics_consent_.emplace(consent);
  }

  bool HatsBubbleShown() {
    views::BubbleDialogDelegateView* bubble = HatsBubbleView::GetHatsBubble();
    if (!bubble)
      return false;

    views::Widget* widget = bubble->GetWidget();
    return widget && widget->IsVisible();
  }

 private:
  base::Optional<ScopedSetMetricsConsent> scoped_metrics_consent_;

  base::test::ScopedFeatureList scoped_feature_list_;

  std::vector<base::test::ScopedFeatureList::FeatureAndParams>
      enabled_features_;

  DISALLOW_COPY_AND_ASSIGN(HatsServiceBrowserTestBase);
};

class HatsServiceProbabilityZero : public HatsServiceBrowserTestBase {
 protected:
  HatsServiceProbabilityZero()
      : HatsServiceBrowserTestBase({probability_zero}) {}

  ~HatsServiceProbabilityZero() override = default;

  DISALLOW_COPY_AND_ASSIGN(HatsServiceProbabilityZero);
};

class HatsServiceProbabilityOne : public HatsServiceBrowserTestBase {
 protected:
  HatsServiceProbabilityOne()
      : HatsServiceBrowserTestBase(
            {probability_one, settings_probability_one}) {}

  ~HatsServiceProbabilityOne() override = default;

 private:
  void SetUpOnMainThread() override {
    HatsServiceBrowserTestBase::SetUpOnMainThread();

    // Set the profile creation time to be old enough to ensure triggering.
    browser()->profile()->SetCreationTimeForTesting(
        base::Time::Now() - base::TimeDelta::FromDays(45));
  }

  void TearDownOnMainThread() override {
    GetHatsService()->SetSurveyMetadataForTesting({});
  }

  DISALLOW_COPY_AND_ASSIGN(HatsServiceProbabilityOne);
};

class HatsServiceImprovedCookieControlsEnabled
    : public HatsServiceBrowserTestBase {
 protected:
  HatsServiceImprovedCookieControlsEnabled()
      : HatsServiceBrowserTestBase(
            {probability_one, improved_cookie_controls}) {}

  ~HatsServiceImprovedCookieControlsEnabled() override = default;

 private:
  void SetUpOnMainThread() override {
    HatsServiceBrowserTestBase::SetUpOnMainThread();

    // Set the profile creation time to be old enough to ensure triggering.
    browser()->profile()->SetCreationTimeForTesting(
        base::Time::Now() - base::TimeDelta::FromDays(45));
  }

  void TearDownOnMainThread() override {
    GetHatsService()->SetSurveyMetadataForTesting({});
  }

  DISALLOW_COPY_AND_ASSIGN(HatsServiceImprovedCookieControlsEnabled);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(HatsServiceBrowserTestBase, BubbleNotShownOnDefault) {
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_FALSE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityZero, NoShow) {
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_FALSE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, NoShowConsentNotGiven) {
  SetMetricsConsent(false);
  ASSERT_FALSE(
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven());
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_FALSE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, TriggerMismatchNoShow) {
  SetMetricsConsent(true);
  ASSERT_TRUE(
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven());
  GetHatsService()->LaunchSurvey("nonexistent-trigger");
  EXPECT_FALSE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, AlwaysShow) {
  SetMetricsConsent(true);
  ASSERT_TRUE(
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven());
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_TRUE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, AlsoShowsSettingsSurvey) {
  SetMetricsConsent(true);
  ASSERT_TRUE(
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven());
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSettings);
  EXPECT_TRUE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne,
                       DoubleShowOnlyResultsInOneShow) {
  SetMetricsConsent(true);
  ASSERT_TRUE(
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven());

  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_TRUE(HatsBubbleShown());
  views::BubbleDialogDelegateView* bubble1 = HatsBubbleView::GetHatsBubble();

  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_TRUE(HatsBubbleShown());
  EXPECT_EQ(bubble1, HatsBubbleView::GetHatsBubble());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, SameMajorVersionNoShow) {
  SetMetricsConsent(true);
  HatsService::SurveyMetadata metadata;
  metadata.last_major_version = version_info::GetVersion().components()[0];
  GetHatsService()->SetSurveyMetadataForTesting(metadata);
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_FALSE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, DifferentMajorVersionShow) {
  SetMetricsConsent(true);
  HatsService::SurveyMetadata metadata;
  metadata.last_major_version = 42;
  ASSERT_NE(42u, version_info::GetVersion().components()[0]);
  GetHatsService()->SetSurveyMetadataForTesting(metadata);
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_TRUE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne,
                       SurveyStartedBeforeRequiredElapsedTimeNoShow) {
  SetMetricsConsent(true);
  HatsService::SurveyMetadata metadata;
  metadata.last_survey_started_time = base::Time::Now();
  GetHatsService()->SetSurveyMetadataForTesting(metadata);
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_FALSE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, ProfileTooYoungToShow) {
  SetMetricsConsent(true);
  // Set creation time to only 15 days.
  static_cast<ProfileImpl*>(browser()->profile())
      ->SetCreationTimeForTesting(base::Time::Now() -
                                  base::TimeDelta::FromDays(15));
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_FALSE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, ProfileOldEnoughToShow) {
  SetMetricsConsent(true);
  // Set creation time to 31 days. This is just past the threshold.
  static_cast<ProfileImpl*>(browser()->profile())
      ->SetCreationTimeForTesting(base::Time::Now() -
                                  base::TimeDelta::FromDays(31));
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_TRUE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, IncognitoModeDisabledNoShow) {
  SetMetricsConsent(true);
  // Disable incognito mode for this profile.
  PrefService* pref_service = browser()->profile()->GetPrefs();
  pref_service->SetInteger(prefs::kIncognitoModeAvailability,
                           IncognitoModePrefs::DISABLED);
  EXPECT_EQ(IncognitoModePrefs::DISABLED,
            IncognitoModePrefs::GetAvailability(pref_service));

  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_FALSE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, CookiesBlockedNoShow) {
  SetMetricsConsent(true);
  auto* settings_map =
      HostContentSettingsMapFactory::GetInstance()->GetForProfile(
          browser()->profile());
  settings_map->SetDefaultContentSetting(ContentSettingsType::COOKIES,
                                         CONTENT_SETTING_BLOCK);
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_FALSE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne,
                       ThirdPartyCookiesBlockedNoShow) {
  SetMetricsConsent(true);
  PrefService* pref_service = browser()->profile()->GetPrefs();
  pref_service->SetBoolean(prefs::kBlockThirdPartyCookies, true);
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_FALSE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceImprovedCookieControlsEnabled,
                       ThirdPartyCookiesBlockedInIncognitoNoShow) {
  SetMetricsConsent(true);
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_FALSE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceImprovedCookieControlsEnabled,
                       ThirdPartyCookiesAllowedInIncognitoShow) {
  SetMetricsConsent(true);
  PrefService* pref_service = browser()->profile()->GetPrefs();
  pref_service->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kOff));
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_TRUE(HatsBubbleShown());
}
