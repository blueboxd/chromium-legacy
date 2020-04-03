// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/util/values/values_util.h"
#include "build/build_config.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/permissions/adaptive_quiet_notification_permission_ui_enabler.h"
#include "chrome/browser/permissions/crowd_deny_fake_safe_browsing_database_manager.h"
#include "chrome/browser/permissions/crowd_deny_preload_data.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_state.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/permissions/features.h"
#include "components/permissions/notification_permission_ui_selector.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/core/db/test_database_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#endif

const double kTestEngagementScore = 29;

class ChromePermissionRequestManagerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  using SiteReputation =
      CrowdDenyPreloadData::SiteReputation::NotificationUserExperienceQuality;

  ChromePermissionRequestManagerTest()
      : ChromeRenderViewHostTestHarness(),
        request1_("test1",
                  permissions::PermissionRequestType::QUOTA,
                  permissions::PermissionRequestGestureType::GESTURE),
        request2_("test2",
                  permissions::PermissionRequestType::DOWNLOAD,
                  permissions::PermissionRequestGestureType::NO_GESTURE),
        request_mic_(
            "mic",
            permissions::PermissionRequestType::PERMISSION_MEDIASTREAM_MIC,
            permissions::PermissionRequestGestureType::NO_GESTURE),
        request_camera_(
            "cam",
            permissions::PermissionRequestType::PERMISSION_MEDIASTREAM_CAMERA,
            permissions::PermissionRequestGestureType::NO_GESTURE) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SetContents(CreateTestWebContents());
    url_ = GURL("http://www.google.com");
    NavigateAndCommit(url_);

    SiteEngagementService::Get(profile())->ResetBaseScoreForURL(
        url_, kTestEngagementScore);

    permissions::PermissionRequestManager::CreateForWebContents(web_contents());
    manager_ =
        permissions::PermissionRequestManager::FromWebContents(web_contents());
    prompt_factory_ =
        std::make_unique<permissions::MockPermissionPromptFactory>(manager_);

    db_manager_ =
        base::MakeRefCounted<CrowdDenyFakeSafeBrowsingDatabaseManager>();
    sb_factory_ =
        std::make_unique<safe_browsing::TestSafeBrowsingServiceFactory>();
    sb_factory_->SetTestDatabaseManager(db_manager_.get());
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(
        sb_factory_->CreateSafeBrowsingService());
  }

  void TearDown() override {
    prompt_factory_ = nullptr;
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void Accept() {
    manager_->Accept();
    base::RunLoop().RunUntilIdle();
  }

  void Deny() {
    manager_->Deny();
    base::RunLoop().RunUntilIdle();
  }

  void Closing() {
    manager_->Closing();
    base::RunLoop().RunUntilIdle();
  }

  void WaitForBubbleToBeShown() {
    manager_->DocumentOnLoadCompletedInMainFrame();
    base::RunLoop().RunUntilIdle();
  }

  virtual void NavigationEntryCommitted(
      const content::LoadCommittedDetails& details) {
    manager_->NavigationEntryCommitted(details);
  }

  void SetUpPreloadDataForUrl(const GURL& url, SiteReputation reputation) {
    CrowdDenyPreloadData::GetInstance()
        ->set_origin_notification_user_experience_for_testing(
            url::Origin::Create(url), reputation);
  }

  void SetUpSafeBrowsingMetadataForUrl(const GURL& url,
                                       bool unsolicited_notifications) {
    safe_browsing::ThreatMetadata test_metadata;
    if (unsolicited_notifications)
      test_metadata.api_permissions.emplace("NOTIFICATIONS");
    db_manager_->SetSimulatedMetadataForUrl(url, test_metadata);
  }

#if defined(OS_CHROMEOS)
  std::unique_ptr<permissions::MockPermissionRequest> MakeRequestInWebKioskMode(
      const GURL& url,
      const GURL& app_url) {
    const AccountId account_id = AccountId::FromUserEmail("lala@example.com");

    auto fake_user_manager =
        std::make_unique<chromeos::FakeChromeUserManager>();
    // Stealing the pointer from unique ptr before it goes to the scoped user
    // manager.
    chromeos::FakeChromeUserManager* user_manager = fake_user_manager.get();
    auto scoped_user_manager =
        std::make_unique<user_manager::ScopedUserManager>(
            std::move(fake_user_manager));
    user_manager->AddWebKioskAppUser(account_id);
    user_manager->LoginUser(account_id);

    auto kiosk_app_manager = std::make_unique<chromeos::WebKioskAppManager>();
    kiosk_app_manager->AddAppForTesting(account_id, app_url);

    NavigateAndCommit(url);
    auto request = std::make_unique<permissions::MockPermissionRequest>(
        /*text*/ "test",
        permissions::PermissionRequestType::PERMISSION_GEOLOCATION, url);
    manager_->AddRequest(request.get());
    return request;
  }
#endif

 protected:
  GURL url_;
  permissions::MockPermissionRequest request1_;
  permissions::MockPermissionRequest request2_;
  permissions::MockPermissionRequest request_mic_;
  permissions::MockPermissionRequest request_camera_;
  permissions::PermissionRequestManager* manager_;
  std::unique_ptr<permissions::MockPermissionPromptFactory> prompt_factory_;
  scoped_refptr<CrowdDenyFakeSafeBrowsingDatabaseManager> db_manager_;
  std::unique_ptr<safe_browsing::TestSafeBrowsingServiceFactory> sb_factory_;
};

TEST_F(ChromePermissionRequestManagerTest, UMAForSimpleAcceptedGestureBubble) {
  base::HistogramTester histograms;

  manager_->AddRequest(&request1_);
  WaitForBubbleToBeShown();
  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptShown,
      static_cast<base::HistogramBase::Sample>(
          permissions::PermissionRequestType::QUOTA),
      1);
  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptShownGesture,
      static_cast<base::HistogramBase::Sample>(
          permissions::PermissionRequestType::QUOTA),
      1);
  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::kPermissionsPromptShownNoGesture, 0);
  histograms.ExpectTotalCount("Permissions.Engagement.Accepted.Quota", 0);

  Accept();
  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptAccepted,
      static_cast<base::HistogramBase::Sample>(
          permissions::PermissionRequestType::QUOTA),
      1);
  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::kPermissionsPromptDenied, 0);

  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptAcceptedGesture,
      static_cast<base::HistogramBase::Sample>(
          permissions::PermissionRequestType::QUOTA),
      1);
  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::kPermissionsPromptAcceptedNoGesture, 0);
  histograms.ExpectUniqueSample("Permissions.Engagement.Accepted.Quota",
                                kTestEngagementScore, 1);
}

TEST_F(ChromePermissionRequestManagerTest, UMAForSimpleDeniedNoGestureBubble) {
  base::HistogramTester histograms;

  manager_->AddRequest(&request2_);
  WaitForBubbleToBeShown();

  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::kPermissionsPromptShownGesture, 0);
  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptShownNoGesture,
      static_cast<base::HistogramBase::Sample>(
          permissions::PermissionRequestType::DOWNLOAD),
      1);
  histograms.ExpectTotalCount("Permissions.Engagement.Denied.MultipleDownload",
                              0);
  // No need to test the other UMA for showing prompts again, they were tested
  // in UMAForSimpleAcceptedBubble.

  Deny();
  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::kPermissionsPromptAccepted, 0);
  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptDenied,
      static_cast<base::HistogramBase::Sample>(
          permissions::PermissionRequestType::DOWNLOAD),
      1);

  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptDeniedNoGesture,
      static_cast<base::HistogramBase::Sample>(
          permissions::PermissionRequestType::DOWNLOAD),
      1);
  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::kPermissionsPromptDeniedGesture, 0);
  histograms.ExpectUniqueSample(
      "Permissions.Engagement.Denied.MultipleDownload", kTestEngagementScore,
      1);
}

TEST_F(ChromePermissionRequestManagerTest, UMAForMergedAcceptedBubble) {
  base::HistogramTester histograms;

  manager_->AddRequest(&request_mic_);
  manager_->AddRequest(&request_camera_);
  WaitForBubbleToBeShown();

  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptShown,
      static_cast<base::HistogramBase::Sample>(
          permissions::PermissionRequestType::MULTIPLE),
      1);
  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::kPermissionsPromptShownGesture, 0);
  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::kPermissionsPromptShownNoGesture, 0);
  histograms.ExpectTotalCount(
      "Permissions.Engagement.Accepted.AudioAndVideoCapture", 0);

  Accept();

  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptAccepted,
      static_cast<base::HistogramBase::Sample>(
          permissions::PermissionRequestType::MULTIPLE),
      1);
  histograms.ExpectUniqueSample(
      "Permissions.Engagement.Accepted.AudioAndVideoCapture",
      kTestEngagementScore, 1);
}

TEST_F(ChromePermissionRequestManagerTest, UMAForMergedDeniedBubble) {
  base::HistogramTester histograms;

  manager_->AddRequest(&request_mic_);
  manager_->AddRequest(&request_camera_);
  WaitForBubbleToBeShown();
  histograms.ExpectTotalCount(
      "Permissions.Engagement.Denied.AudioAndVideoCapture", 0);
  // No need to test UMA for showing prompts again, they were tested in
  // UMAForMergedAcceptedBubble.

  Deny();

  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptDenied,
      static_cast<base::HistogramBase::Sample>(
          permissions::PermissionRequestType::MULTIPLE),
      1);
  histograms.ExpectUniqueSample(
      "Permissions.Engagement.Denied.AudioAndVideoCapture",
      kTestEngagementScore, 1);
}

TEST_F(ChromePermissionRequestManagerTest, UMAForIgnores) {
  base::HistogramTester histograms;

  manager_->AddRequest(&request1_);
  WaitForBubbleToBeShown();
  histograms.ExpectTotalCount("Permissions.Engagement.Ignored.Quota", 0);

  GURL youtube("http://www.youtube.com/");
  NavigateAndCommit(youtube);
  histograms.ExpectUniqueSample("Permissions.Engagement.Ignored.Quota",
                                kTestEngagementScore, 1);

  permissions::MockPermissionRequest youtube_request(
      "request2", permissions::PermissionRequestType::PERMISSION_GEOLOCATION,
      youtube);
  manager_->AddRequest(&youtube_request);
  WaitForBubbleToBeShown();

  NavigateAndCommit(GURL("http://www.google.com/"));
  histograms.ExpectUniqueSample("Permissions.Engagement.Ignored.Geolocation", 0,
                                1);
}

TEST_F(ChromePermissionRequestManagerTest,
       NotificationsUnderClientSideEmbargoAfterSeveralDenies) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kQuietNotificationPrompts,
        {{QuietNotificationPermissionUiConfig::kEnableAdaptiveActivation,
          "true"}}}},
      {permissions::features::kBlockRepeatedNotificationPermissionPrompts});

  auto* permission_ui_enabler =
      AdaptiveQuietNotificationPermissionUiEnabler::GetForProfile(profile());

  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
      prefs::kEnableQuietNotificationPermissionUi));
  // TODO(hkamila): Collapse the below blocks into a single for statement.
  GURL notification1("http://www.notification1.com/");
  NavigateAndCommit(notification1);
  permissions::MockPermissionRequest notification1_request(
      "request1", permissions::PermissionRequestType::PERMISSION_NOTIFICATIONS,
      notification1);
  manager_->AddRequest(&notification1_request);
  WaitForBubbleToBeShown();
  Deny();

  GURL notification2("http://www.notification2.com/");
  NavigateAndCommit(notification2);
  permissions::MockPermissionRequest notification2_request(
      "request2", permissions::PermissionRequestType::PERMISSION_NOTIFICATIONS,
      notification2);
  manager_->AddRequest(&notification2_request);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Deny();

  GURL notification3("http://www.notification3.com/");
  NavigateAndCommit(notification3);
  permissions::MockPermissionRequest notification3_request(
      "request3", permissions::PermissionRequestType::PERMISSION_NOTIFICATIONS,
      notification3);
  manager_->AddRequest(&notification3_request);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Accept();

  // Only show quiet UI after 3 consecutive denies of the permission prompt.
  GURL notification4("http://www.notification4.com/");
  NavigateAndCommit(notification4);
  permissions::MockPermissionRequest notification4_request(
      "request4", permissions::PermissionRequestType::PERMISSION_NOTIFICATIONS,
      notification4);
  manager_->AddRequest(&notification4_request);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Deny();

  GURL notification5("http://www.notification5.com/");
  NavigateAndCommit(notification5);
  permissions::MockPermissionRequest notification5_request(
      "request5", permissions::PermissionRequestType::PERMISSION_NOTIFICATIONS,
      notification5);
  manager_->AddRequest(&notification5_request);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Deny();

  // Only denying the notification permission should count toward the threshold,
  // other permissions should not.
  GURL camera_url("http://www.camera.com/");
  NavigateAndCommit(camera_url);
  manager_->AddRequest(&request_camera_);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Deny();

  GURL microphone_url("http://www.microphone.com/");
  NavigateAndCommit(microphone_url);
  manager_->AddRequest(&request_mic_);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Deny();

  GURL notification6("http://www.notification6.com/");
  NavigateAndCommit(notification6);
  permissions::MockPermissionRequest notification6_request(
      "request6", permissions::PermissionRequestType::PERMISSION_NOTIFICATIONS,
      notification6);
  manager_->AddRequest(&notification6_request);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Deny();

  // After the 3rd consecutive denies, show the quieter version of the
  // permission prompt.
  GURL notification7("http://www.notification7.com/");
  NavigateAndCommit(notification7);
  permissions::MockPermissionRequest notification7_request(
      "request7", permissions::PermissionRequestType::PERMISSION_NOTIFICATIONS,
      notification7);
  // For the first quiet permission prompt, show a promo.
  EXPECT_TRUE(QuietNotificationPermissionUiState::ShouldShowPromo(profile()));
  manager_->AddRequest(&notification7_request);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(manager_->ShouldCurrentRequestUseQuietUI());
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      prefs::kEnableQuietNotificationPermissionUi));
  Accept();

  base::SimpleTestClock clock_;
  clock_.SetNow(base::Time::Now());
  permission_ui_enabler->set_clock_for_testing(&clock_);

  // One accept through the quiet UI, doesn't switch the user back to the
  // disabled state once the permission is set.
  GURL notification8("http://www.notification8.com/");
  NavigateAndCommit(notification8);
  permissions::MockPermissionRequest notification8_request(
      "request8", permissions::PermissionRequestType::PERMISSION_NOTIFICATIONS,
      notification8);
  // For the rest of the quiet permission prompts, do not show promo.
  EXPECT_TRUE(QuietNotificationPermissionUiState::ShouldShowPromo(profile()));
  manager_->AddRequest(&notification8_request);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(manager_->ShouldCurrentRequestUseQuietUI());

  // Clearing interaction history, or turning off quiet mode in preferences does
  // not change the state of the currently showing quiet UI.
  permission_ui_enabler->ClearInteractionHistory(base::Time(),
                                                 base::Time::Max());
  profile()->GetPrefs()->ClearPref(prefs::kEnableQuietNotificationPermissionUi);
  EXPECT_TRUE(manager_->ShouldCurrentRequestUseQuietUI());
  Deny();

  base::Time recorded_time = clock_.Now();
  clock_.Advance(base::TimeDelta::FromDays(1));
  base::Time from_time = clock_.Now();
  permission_ui_enabler->set_clock_for_testing(&clock_);
  GURL notification9("http://www.notification9.com/");
  NavigateAndCommit(notification9);
  permissions::MockPermissionRequest notification9_request(
      "request9", permissions::PermissionRequestType::PERMISSION_NOTIFICATIONS,
      notification9);
  manager_->AddRequest(&notification9_request);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Deny();

  clock_.Advance(base::TimeDelta::FromDays(1));
  base::Time to_time = clock_.Now();
  permission_ui_enabler->set_clock_for_testing(&clock_);
  GURL notification10("http://www.notification10.com/");
  NavigateAndCommit(notification10);
  permissions::MockPermissionRequest notification10_request(
      "request10", permissions::PermissionRequestType::PERMISSION_NOTIFICATIONS,
      notification10);
  manager_->AddRequest(&notification10_request);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Deny();

  clock_.Advance(base::TimeDelta::FromDays(1));
  permission_ui_enabler->set_clock_for_testing(&clock_);
  GURL notification11("http://www.notification11.com/");
  NavigateAndCommit(notification11);
  permissions::MockPermissionRequest notification11_request(
      "request11", permissions::PermissionRequestType::PERMISSION_NOTIFICATIONS,
      notification11);
  manager_->AddRequest(&notification11_request);
  WaitForBubbleToBeShown();
  Deny();

  ListPrefUpdate update(profile()->GetPrefs(),
                        prefs::kNotificationPermissionActions);
  base::Value::ConstListView permissions_actions = update->GetList();
  permission_ui_enabler->ClearInteractionHistory(from_time, to_time);

  // Check that we have cleared all entries >= |from_time| and <|end_time|.
  EXPECT_EQ(permissions_actions.size(), 3u);
  EXPECT_EQ((util::ValueToTime(permissions_actions[0].FindKey("time")))
                .value_or(base::Time()),
            recorded_time);
}

#if defined(OS_CHROMEOS)
TEST_F(ChromePermissionRequestManagerTest, TestWebKioskModeSameOrigin) {
  auto request =
      MakeRequestInWebKioskMode(/*url*/ GURL("https://google.com/page"),
                                /*app_url*/ GURL("https://google.com/launch"));

  WaitForBubbleToBeShown();
  // It should be granted by default.
  EXPECT_TRUE(request->granted());
}

TEST_F(ChromePermissionRequestManagerTest, TestWebKioskModeDifferentOrigin) {
  auto request =
      MakeRequestInWebKioskMode(/*url*/ GURL("https://example.com/page"),
                                /*app_url*/ GURL("https://google.com/launch"));

  WaitForBubbleToBeShown();
  // It should not be granted by default.
  EXPECT_FALSE(request->granted());
  EXPECT_TRUE(request->finished());
}
#endif  // defined(OS_CHROMEOS)

TEST_F(ChromePermissionRequestManagerTest, TestCrowdDenyHoldbackChance) {
  const struct {
    std::string holdback_chance;
    bool enabled_in_prefs;
    bool expect_quiet_ui;
    bool expect_histogram_bucket;
  } kTestCases[] = {
      // 100% chance to holdback, the UI used should be the normal UI.
      {"1.0", false, false, true},
      // 0% chance to holdback, the UI used should be the quiet UI.
      {"0.0", false, true, false},
      // 100% chance to holdback but the quiet UI is enabled by the user in
      // prefs, the UI used should be the quiet UI.
      {"1.0", true, true, true},
  };

  GURL url("https://spammy.com");

  SetUpPreloadDataForUrl(
      url, CrowdDenyPreloadData::SiteReputation::UNSOLICITED_PROMPTS);
  SetUpSafeBrowsingMetadataForUrl(url, true);

  for (const auto& test : kTestCases) {
    base::HistogramTester histograms;

    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kQuietNotificationPrompts,
        {{QuietNotificationPermissionUiConfig::kEnableAdaptiveActivation,
          "true"},
         {QuietNotificationPermissionUiConfig::kEnableCrowdDenyTriggering,
          "true"},
         {QuietNotificationPermissionUiConfig::kCrowdDenyHoldBackChance,
          test.holdback_chance}});

    if (test.enabled_in_prefs) {
      profile()->GetPrefs()->SetBoolean(
          prefs::kEnableQuietNotificationPermissionUi, true);
    }

    permissions::MockPermissionRequest request(
        "request", permissions::PermissionRequestType::PERMISSION_NOTIFICATIONS,
        url);

    manager_->AddRequest(&request);
    WaitForBubbleToBeShown();
    EXPECT_EQ(test.expect_quiet_ui, manager_->ShouldCurrentRequestUseQuietUI());
    Closing();

    histograms.ExpectBucketCount(
        "Permissions.CrowdDeny.DidHoldbackQuietUi",
        static_cast<base::HistogramBase::Sample>(test.expect_histogram_bucket),
        1);
  }
}

TEST_F(ChromePermissionRequestManagerTest,
       PreloadDataNeedsSafeBrowsingConfirmation) {
  const struct {
    SiteReputation preload_data_reputation;
    bool safe_browsing_unsolicited_notifications;
    bool expect_quiet_ui;
  } kTestCases[] = {
      {CrowdDenyPreloadData::SiteReputation::UNSOLICITED_PROMPTS, true, true},
      {CrowdDenyPreloadData::SiteReputation::UNSOLICITED_PROMPTS, false, false},
      {CrowdDenyPreloadData::SiteReputation::ACCEPTABLE, true, false},
      {CrowdDenyPreloadData::SiteReputation::UNKNOWN, true, false},
      {CrowdDenyPreloadData::SiteReputation::UNKNOWN, false, false},
  };

  const GURL url("https://spammy.com");

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kQuietNotificationPrompts,
      {{QuietNotificationPermissionUiConfig::kEnableAdaptiveActivation, "true"},
       {QuietNotificationPermissionUiConfig::kEnableCrowdDenyTriggering,
        "true"},
       {QuietNotificationPermissionUiConfig::kCrowdDenyHoldBackChance, "0"}});

  for (const auto& test : kTestCases) {
    SetUpPreloadDataForUrl(url, test.preload_data_reputation);
    SetUpSafeBrowsingMetadataForUrl(
        url, test.safe_browsing_unsolicited_notifications);

    permissions::MockPermissionRequest request(
        "request", permissions::PermissionRequestType::PERMISSION_NOTIFICATIONS,
        url);

    manager_->AddRequest(&request);
    WaitForBubbleToBeShown();
    EXPECT_EQ(test.expect_quiet_ui, manager_->ShouldCurrentRequestUseQuietUI());
    Closing();
  }
}
