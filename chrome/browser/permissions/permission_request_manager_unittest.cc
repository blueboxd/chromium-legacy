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
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/util/values/values_util.h"
#include "build/build_config.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/permissions/adaptive_quiet_notification_permission_ui_enabler.h"
#include "chrome/browser/permissions/crowd_deny_fake_safe_browsing_database_manager.h"
#include "chrome/browser/permissions/crowd_deny_preload_data.h"
#include "chrome/browser/permissions/mock_permission_request.h"
#include "chrome/browser/permissions/notification_permission_ui_selector.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_state.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/ui/permission_bubble/mock_permission_prompt_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
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

class PermissionRequestManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  using SiteReputation =
      CrowdDenyPreloadData::SiteReputation::NotificationUserExperienceQuality;

  PermissionRequestManagerTest()
      : ChromeRenderViewHostTestHarness(),
        request1_("test1",
                  PermissionRequestType::QUOTA,
                  PermissionRequestGestureType::GESTURE),
        request2_("test2",
                  PermissionRequestType::DOWNLOAD,
                  PermissionRequestGestureType::NO_GESTURE),
        request_mic_("mic",
                     PermissionRequestType::PERMISSION_MEDIASTREAM_MIC,
                     PermissionRequestGestureType::NO_GESTURE),
        request_camera_("cam",
                        PermissionRequestType::PERMISSION_MEDIASTREAM_CAMERA,
                        PermissionRequestGestureType::NO_GESTURE),
        iframe_request_same_domain_(
            "iframe",
            PermissionRequestType::PERMISSION_NOTIFICATIONS,
            GURL("http://www.google.com/some/url")),
        iframe_request_other_domain_(
            "iframe",
            PermissionRequestType::PERMISSION_GEOLOCATION,
            GURL("http://www.youtube.com")),
        iframe_request_mic_other_domain_(
            "iframe",
            PermissionRequestType::PERMISSION_MEDIASTREAM_MIC,
            GURL("http://www.youtube.com")) {}
  ~PermissionRequestManagerTest() override {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SetContents(CreateTestWebContents());
    url_ = GURL("http://www.google.com");
    NavigateAndCommit(url_);

    SiteEngagementService::Get(profile())->ResetBaseScoreForURL(
        url_, kTestEngagementScore);

    PermissionRequestManager::CreateForWebContents(web_contents());
    manager_ = PermissionRequestManager::FromWebContents(web_contents());
    prompt_factory_.reset(new MockPermissionPromptFactory(manager_));

    db_manager_ =
        base::MakeRefCounted<CrowdDenyFakeSafeBrowsingDatabaseManager>();
    sb_factory_ =
        std::make_unique<safe_browsing::TestSafeBrowsingServiceFactory>();
    sb_factory_->SetTestDatabaseManager(db_manager_.get());
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(
        sb_factory_->CreateSafeBrowsingService());
  }

  void TearDown() override {
    prompt_factory_.reset();
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

  void WaitForFrameLoad() {
    // PermissionRequestManager ignores all parameters. Yay?
    manager_->DOMContentLoaded(NULL);
    base::RunLoop().RunUntilIdle();
  }

  void WaitForBubbleToBeShown() {
    manager_->DocumentOnLoadCompletedInMainFrame();
    base::RunLoop().RunUntilIdle();
  }

  void MockTabSwitchAway() {
    manager_->OnVisibilityChanged(content::Visibility::HIDDEN);
  }

  void MockTabSwitchBack() {
    manager_->OnVisibilityChanged(content::Visibility::VISIBLE);
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
  std::unique_ptr<MockPermissionRequest> MakeRequestInWebKioskMode(
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
    auto request = std::make_unique<MockPermissionRequest>(
        /*text*/ "test", PermissionRequestType::PERMISSION_GEOLOCATION, url);
    manager_->AddRequest(request.get());
    return request;
  }
#endif

 protected:
  GURL url_;
  MockPermissionRequest request1_;
  MockPermissionRequest request2_;
  MockPermissionRequest request_mic_;
  MockPermissionRequest request_camera_;
  MockPermissionRequest iframe_request_same_domain_;
  MockPermissionRequest iframe_request_other_domain_;
  MockPermissionRequest iframe_request_mic_other_domain_;
  PermissionRequestManager* manager_;
  std::unique_ptr<MockPermissionPromptFactory> prompt_factory_;
  scoped_refptr<CrowdDenyFakeSafeBrowsingDatabaseManager> db_manager_;
  std::unique_ptr<safe_browsing::TestSafeBrowsingServiceFactory> sb_factory_;
};

TEST_F(PermissionRequestManagerTest, SingleRequest) {
  manager_->AddRequest(&request1_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);

  Accept();
  EXPECT_TRUE(request1_.granted());
}

TEST_F(PermissionRequestManagerTest, SingleRequestViewFirst) {
  manager_->AddRequest(&request1_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);

  Accept();
  EXPECT_TRUE(request1_.granted());
}

// Most requests should never be grouped.
TEST_F(PermissionRequestManagerTest, TwoRequestsUngrouped) {
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&request2_);

  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request1_.granted());

  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request2_.granted());
}

// Only mic/camera requests from the same origin should be grouped.
TEST_F(PermissionRequestManagerTest, MicCameraGrouped) {
  manager_->AddRequest(&request_mic_);
  manager_->AddRequest(&request_camera_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 2);

  Accept();
  EXPECT_TRUE(request_mic_.granted());
  EXPECT_TRUE(request_camera_.granted());

  // If the requests come from different origins, they should not be grouped.
  manager_->AddRequest(&iframe_request_mic_other_domain_);
  manager_->AddRequest(&request_camera_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
}

TEST_F(PermissionRequestManagerTest, TwoRequestsTabSwitch) {
  manager_->AddRequest(&request_mic_);
  manager_->AddRequest(&request_camera_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 2);

  MockTabSwitchAway();
#if defined(OS_ANDROID)
  EXPECT_TRUE(prompt_factory_->is_visible());
#else
  EXPECT_FALSE(prompt_factory_->is_visible());
#endif

  MockTabSwitchBack();
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 2);

  Accept();
  EXPECT_TRUE(request_mic_.granted());
  EXPECT_TRUE(request_camera_.granted());
}

TEST_F(PermissionRequestManagerTest, NoRequests) {
  WaitForBubbleToBeShown();
  EXPECT_FALSE(prompt_factory_->is_visible());
}

TEST_F(PermissionRequestManagerTest, PermissionRequestWhileTabSwitchedAway) {
  MockTabSwitchAway();
  manager_->AddRequest(&request1_);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(prompt_factory_->is_visible());

  MockTabSwitchBack();
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
}

TEST_F(PermissionRequestManagerTest, TwoRequestsDoNotCoalesce) {
  manager_->AddRequest(&request1_);
  WaitForBubbleToBeShown();
  manager_->AddRequest(&request2_);

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
}

TEST_F(PermissionRequestManagerTest, TwoRequestsShownInTwoBubbles) {
  manager_->AddRequest(&request1_);
  WaitForBubbleToBeShown();
  manager_->AddRequest(&request2_);

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);

  Accept();
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
  ASSERT_EQ(prompt_factory_->show_count(), 2);
}

TEST_F(PermissionRequestManagerTest, TestAddDuplicateRequest) {
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&request1_);

  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
}

TEST_F(PermissionRequestManagerTest, SequentialRequests) {
  manager_->AddRequest(&request1_);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());

  Accept();
  EXPECT_TRUE(request1_.granted());

  EXPECT_FALSE(prompt_factory_->is_visible());

  manager_->AddRequest(&request2_);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  Accept();
  EXPECT_FALSE(prompt_factory_->is_visible());
  EXPECT_TRUE(request2_.granted());
}

TEST_F(PermissionRequestManagerTest, SameRequestRejected) {
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&request1_);
  EXPECT_FALSE(request1_.finished());

  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
}

TEST_F(PermissionRequestManagerTest, DuplicateQueuedRequest) {
  manager_->AddRequest(&request1_);
  WaitForBubbleToBeShown();
  manager_->AddRequest(&request2_);

  MockPermissionRequest dupe_request("test1");
  manager_->AddRequest(&dupe_request);
  EXPECT_FALSE(dupe_request.finished());
  EXPECT_FALSE(request1_.finished());

  MockPermissionRequest dupe_request2("test2");
  manager_->AddRequest(&dupe_request2);
  EXPECT_FALSE(dupe_request2.finished());
  EXPECT_FALSE(request2_.finished());

  Accept();
  EXPECT_TRUE(dupe_request.finished());
  EXPECT_TRUE(request1_.finished());

  WaitForBubbleToBeShown();
  Accept();
  EXPECT_TRUE(dupe_request2.finished());
  EXPECT_TRUE(request2_.finished());
}

TEST_F(PermissionRequestManagerTest, ForgetRequestsOnPageNavigation) {
  manager_->AddRequest(&request1_);
  WaitForBubbleToBeShown();
  manager_->AddRequest(&request2_);
  manager_->AddRequest(&iframe_request_other_domain_);

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);

  NavigateAndCommit(GURL("http://www2.google.com/"));
  WaitForBubbleToBeShown();

  EXPECT_FALSE(prompt_factory_->is_visible());
  EXPECT_TRUE(request1_.finished());
  EXPECT_TRUE(request2_.finished());
  EXPECT_TRUE(iframe_request_other_domain_.finished());
}

TEST_F(PermissionRequestManagerTest, MainFrameNoRequestIFrameRequest) {
  manager_->AddRequest(&iframe_request_same_domain_);
  WaitForBubbleToBeShown();
  WaitForFrameLoad();

  EXPECT_TRUE(prompt_factory_->is_visible());
  Closing();
  EXPECT_TRUE(iframe_request_same_domain_.finished());
}

TEST_F(PermissionRequestManagerTest, MainFrameAndIFrameRequestSameDomain) {
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&iframe_request_same_domain_);
  WaitForFrameLoad();
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(1, prompt_factory_->request_count());
  Closing();
  EXPECT_TRUE(request1_.finished());
  EXPECT_FALSE(iframe_request_same_domain_.finished());
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(1, prompt_factory_->request_count());
  Closing();
  EXPECT_FALSE(prompt_factory_->is_visible());
  EXPECT_TRUE(iframe_request_same_domain_.finished());
}

TEST_F(PermissionRequestManagerTest, MainFrameAndIFrameRequestOtherDomain) {
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&iframe_request_other_domain_);
  WaitForFrameLoad();
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  Closing();
  EXPECT_TRUE(request1_.finished());
  EXPECT_FALSE(iframe_request_other_domain_.finished());
  EXPECT_TRUE(prompt_factory_->is_visible());
  Closing();
  EXPECT_TRUE(iframe_request_other_domain_.finished());
}

TEST_F(PermissionRequestManagerTest, IFrameRequestWhenMainRequestVisible) {
  manager_->AddRequest(&request1_);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());

  manager_->AddRequest(&iframe_request_same_domain_);
  WaitForFrameLoad();
  ASSERT_EQ(prompt_factory_->request_count(), 1);
  Closing();
  EXPECT_TRUE(request1_.finished());
  EXPECT_FALSE(iframe_request_same_domain_.finished());
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
  Closing();
  EXPECT_TRUE(iframe_request_same_domain_.finished());
}

TEST_F(PermissionRequestManagerTest,
       IFrameRequestOtherDomainWhenMainRequestVisible) {
  manager_->AddRequest(&request1_);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());

  manager_->AddRequest(&iframe_request_other_domain_);
  WaitForFrameLoad();
  Closing();
  EXPECT_TRUE(request1_.finished());
  EXPECT_FALSE(iframe_request_other_domain_.finished());
  EXPECT_TRUE(prompt_factory_->is_visible());
  Closing();
  EXPECT_TRUE(iframe_request_other_domain_.finished());
}

TEST_F(PermissionRequestManagerTest, RequestsDontNeedUserGesture) {
  WaitForFrameLoad();
  WaitForBubbleToBeShown();
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&iframe_request_other_domain_);
  manager_->AddRequest(&request2_);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(prompt_factory_->is_visible());
}

TEST_F(PermissionRequestManagerTest, UMAForSimpleAcceptedGestureBubble) {
  base::HistogramTester histograms;

  manager_->AddRequest(&request1_);
  WaitForBubbleToBeShown();
  histograms.ExpectUniqueSample(
      PermissionUmaUtil::kPermissionsPromptShown,
      static_cast<base::HistogramBase::Sample>(PermissionRequestType::QUOTA),
      1);
  histograms.ExpectUniqueSample(
      PermissionUmaUtil::kPermissionsPromptShownGesture,
      static_cast<base::HistogramBase::Sample>(PermissionRequestType::QUOTA),
      1);
  histograms.ExpectTotalCount(
      PermissionUmaUtil::kPermissionsPromptShownNoGesture, 0);
  histograms.ExpectTotalCount("Permissions.Engagement.Accepted.Quota", 0);

  Accept();
  histograms.ExpectUniqueSample(
      PermissionUmaUtil::kPermissionsPromptAccepted,
      static_cast<base::HistogramBase::Sample>(PermissionRequestType::QUOTA),
      1);
  histograms.ExpectTotalCount(
      PermissionUmaUtil::kPermissionsPromptDenied, 0);

  histograms.ExpectUniqueSample(
      PermissionUmaUtil::kPermissionsPromptAcceptedGesture,
      static_cast<base::HistogramBase::Sample>(PermissionRequestType::QUOTA),
      1);
  histograms.ExpectTotalCount(
      PermissionUmaUtil::kPermissionsPromptAcceptedNoGesture, 0);
  histograms.ExpectUniqueSample("Permissions.Engagement.Accepted.Quota",
                                kTestEngagementScore, 1);
}

TEST_F(PermissionRequestManagerTest, UMAForSimpleDeniedNoGestureBubble) {
  base::HistogramTester histograms;

  manager_->AddRequest(&request2_);
  WaitForBubbleToBeShown();

  histograms.ExpectTotalCount(
      PermissionUmaUtil::kPermissionsPromptShownGesture, 0);
  histograms.ExpectUniqueSample(
      PermissionUmaUtil::kPermissionsPromptShownNoGesture,
      static_cast<base::HistogramBase::Sample>(PermissionRequestType::DOWNLOAD),
      1);
  histograms.ExpectTotalCount("Permissions.Engagement.Denied.MultipleDownload",
                              0);
  // No need to test the other UMA for showing prompts again, they were tested
  // in UMAForSimpleAcceptedBubble.

  Deny();
  histograms.ExpectTotalCount(
      PermissionUmaUtil::kPermissionsPromptAccepted, 0);
  histograms.ExpectUniqueSample(
      PermissionUmaUtil::kPermissionsPromptDenied,
      static_cast<base::HistogramBase::Sample>(PermissionRequestType::DOWNLOAD),
      1);

  histograms.ExpectUniqueSample(
      PermissionUmaUtil::kPermissionsPromptDeniedNoGesture,
      static_cast<base::HistogramBase::Sample>(PermissionRequestType::DOWNLOAD),
      1);
  histograms.ExpectTotalCount(
      PermissionUmaUtil::kPermissionsPromptDeniedGesture, 0);
  histograms.ExpectUniqueSample(
      "Permissions.Engagement.Denied.MultipleDownload", kTestEngagementScore,
      1);
}

// This code path (calling Accept on a non-merged bubble, with no accepted
// permission) would never be used in actual Chrome, but its still tested for
// completeness.
TEST_F(PermissionRequestManagerTest, UMAForSimpleDeniedBubbleAlternatePath) {
  base::HistogramTester histograms;

  manager_->AddRequest(&request1_);
  WaitForBubbleToBeShown();
  // No need to test UMA for showing prompts again, they were tested in
  // UMAForSimpleAcceptedBubble.

  Deny();
  histograms.ExpectUniqueSample(
      PermissionUmaUtil::kPermissionsPromptDenied,
      static_cast<base::HistogramBase::Sample>(PermissionRequestType::QUOTA),
      1);
}

TEST_F(PermissionRequestManagerTest, UMAForMergedAcceptedBubble) {
  base::HistogramTester histograms;

  manager_->AddRequest(&request_mic_);
  manager_->AddRequest(&request_camera_);
  WaitForBubbleToBeShown();

  histograms.ExpectUniqueSample(
      PermissionUmaUtil::kPermissionsPromptShown,
      static_cast<base::HistogramBase::Sample>(PermissionRequestType::MULTIPLE),
      1);
  histograms.ExpectTotalCount(
      PermissionUmaUtil::kPermissionsPromptShownGesture, 0);
  histograms.ExpectTotalCount(
      PermissionUmaUtil::kPermissionsPromptShownNoGesture, 0);
  histograms.ExpectTotalCount(
      "Permissions.Engagement.Accepted.AudioAndVideoCapture", 0);

  Accept();

  histograms.ExpectUniqueSample(
      PermissionUmaUtil::kPermissionsPromptAccepted,
      static_cast<base::HistogramBase::Sample>(PermissionRequestType::MULTIPLE),
      1);
  histograms.ExpectUniqueSample(
      "Permissions.Engagement.Accepted.AudioAndVideoCapture",
      kTestEngagementScore, 1);
}

TEST_F(PermissionRequestManagerTest, UMAForMergedDeniedBubble) {
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
      PermissionUmaUtil::kPermissionsPromptDenied,
      static_cast<base::HistogramBase::Sample>(PermissionRequestType::MULTIPLE),
      1);
  histograms.ExpectUniqueSample(
      "Permissions.Engagement.Denied.AudioAndVideoCapture",
      kTestEngagementScore, 1);
}

TEST_F(PermissionRequestManagerTest, UMAForIgnores) {
  base::HistogramTester histograms;

  manager_->AddRequest(&request1_);
  WaitForBubbleToBeShown();
  histograms.ExpectTotalCount("Permissions.Engagement.Ignored.Quota", 0);

  GURL youtube("http://www.youtube.com/");
  NavigateAndCommit(youtube);
  histograms.ExpectUniqueSample("Permissions.Engagement.Ignored.Quota",
                                kTestEngagementScore, 1);

  MockPermissionRequest youtube_request(
      "request2", PermissionRequestType::PERMISSION_GEOLOCATION, youtube);
  manager_->AddRequest(&youtube_request);
  WaitForBubbleToBeShown();

  NavigateAndCommit(GURL("http://www.google.com/"));
  histograms.ExpectUniqueSample("Permissions.Engagement.Ignored.Geolocation", 0,
                                1);
}

TEST_F(PermissionRequestManagerTest, UMAForTabSwitching) {
  base::HistogramTester histograms;

  manager_->AddRequest(&request1_);
  WaitForBubbleToBeShown();
  histograms.ExpectUniqueSample(
      PermissionUmaUtil::kPermissionsPromptShown,
      static_cast<base::HistogramBase::Sample>(PermissionRequestType::QUOTA),
      1);

  MockTabSwitchAway();
  MockTabSwitchBack();
  histograms.ExpectUniqueSample(
      PermissionUmaUtil::kPermissionsPromptShown,
      static_cast<base::HistogramBase::Sample>(PermissionRequestType::QUOTA),
      1);
}

TEST_F(PermissionRequestManagerTest,
       NotificationsUnderClientSideEmbargoAfterSeveralDenies) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kQuietNotificationPrompts,
        {{QuietNotificationPermissionUiConfig::kEnableAdaptiveActivation,
          "true"}}}},
      {features::kBlockRepeatedNotificationPermissionPrompts});

  auto* permission_ui_enabler =
      AdaptiveQuietNotificationPermissionUiEnabler::GetForProfile(profile());

  EXPECT_FALSE(
      QuietNotificationPermissionUiState::IsQuietUiEnabledInPrefs(profile()));
  // TODO(hkamila): Collapse the below blocks into a single for statement.
  GURL notification1("http://www.notification1.com/");
  NavigateAndCommit(notification1);
  MockPermissionRequest notification1_request(
      "request1", PermissionRequestType::PERMISSION_NOTIFICATIONS,
      notification1);
  manager_->AddRequest(&notification1_request);
  WaitForBubbleToBeShown();
  Deny();

  GURL notification2("http://www.notification2.com/");
  NavigateAndCommit(notification2);
  MockPermissionRequest notification2_request(
      "request2", PermissionRequestType::PERMISSION_NOTIFICATIONS,
      notification2);
  manager_->AddRequest(&notification2_request);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Deny();

  GURL notification3("http://www.notification3.com/");
  NavigateAndCommit(notification3);
  MockPermissionRequest notification3_request(
      "request3", PermissionRequestType::PERMISSION_NOTIFICATIONS,
      notification3);
  manager_->AddRequest(&notification3_request);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Accept();

  // Only show quiet UI after 3 consecutive denies of the permission prompt.
  GURL notification4("http://www.notification4.com/");
  NavigateAndCommit(notification4);
  MockPermissionRequest notification4_request(
      "request4", PermissionRequestType::PERMISSION_NOTIFICATIONS,
      notification4);
  manager_->AddRequest(&notification4_request);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Deny();

  GURL notification5("http://www.notification5.com/");
  NavigateAndCommit(notification5);
  MockPermissionRequest notification5_request(
      "request5", PermissionRequestType::PERMISSION_NOTIFICATIONS,
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
  MockPermissionRequest notification6_request(
      "request6", PermissionRequestType::PERMISSION_NOTIFICATIONS,
      notification6);
  manager_->AddRequest(&notification6_request);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Deny();

  // After the 3rd consecutive denies, show the quieter version of the
  // permission prompt.
  GURL notification7("http://www.notification7.com/");
  NavigateAndCommit(notification7);
  MockPermissionRequest notification7_request(
      "request7", PermissionRequestType::PERMISSION_NOTIFICATIONS,
      notification7);
  // For the first quiet permission prompt, show a promo.
  EXPECT_TRUE(QuietNotificationPermissionUiState::ShouldShowPromo(profile()));
  manager_->AddRequest(&notification7_request);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(manager_->ShouldCurrentRequestUseQuietUI());
  EXPECT_TRUE(
      QuietNotificationPermissionUiState::IsQuietUiEnabledInPrefs(profile()));
  Accept();

  base::SimpleTestClock clock_;
  clock_.SetNow(base::Time::Now());
  permission_ui_enabler->set_clock_for_testing(&clock_);

  // One accept through the quiet UI, doesn't switch the user back to the
  // disabled state once the permission is set.
  GURL notification8("http://www.notification8.com/");
  NavigateAndCommit(notification8);
  MockPermissionRequest notification8_request(
      "request8", PermissionRequestType::PERMISSION_NOTIFICATIONS,
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
  QuietNotificationPermissionUiState::DisableQuietUiInPrefs(profile());
  EXPECT_TRUE(manager_->ShouldCurrentRequestUseQuietUI());
  Deny();

  base::Time recorded_time = clock_.Now();
  clock_.Advance(base::TimeDelta::FromDays(1));
  base::Time from_time = clock_.Now();
  permission_ui_enabler->set_clock_for_testing(&clock_);
  GURL notification9("http://www.notification9.com/");
  NavigateAndCommit(notification9);
  MockPermissionRequest notification9_request(
      "request9", PermissionRequestType::PERMISSION_NOTIFICATIONS,
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
  MockPermissionRequest notification10_request(
      "request10", PermissionRequestType::PERMISSION_NOTIFICATIONS,
      notification10);
  manager_->AddRequest(&notification10_request);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Deny();

  clock_.Advance(base::TimeDelta::FromDays(1));
  permission_ui_enabler->set_clock_for_testing(&clock_);
  GURL notification11("http://www.notification11.com/");
  NavigateAndCommit(notification11);
  MockPermissionRequest notification11_request(
      "request11", PermissionRequestType::PERMISSION_NOTIFICATIONS,
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

// Simulate a NotificationPermissionUiSelector that simply returns a
// predefined |ui_to_use| every time.
class MockNotificationPermissionUiSelector
    : public NotificationPermissionUiSelector {
 public:
  explicit MockNotificationPermissionUiSelector(UiToUse ui_to_use, bool async) {
    ui_to_use_ = ui_to_use;
    async_ = async;
  }

  void SelectUiToUse(PermissionRequest* request,
                     DecisionMadeCallback callback) override {
    base::Optional<QuietUiReason> reason;
    if (ui_to_use_ == UiToUse::kQuietUi)
      reason = QuietUiReason::kEnabledInPrefs;
    if (async_) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), ui_to_use_, reason));
    } else {
      std::move(callback).Run(ui_to_use_, reason);
    }
  }

  static void CreateForManager(PermissionRequestManager* manager,
                               UiToUse ui_to_use,
                               bool async) {
    manager->set_notification_permission_ui_selector_for_testing(
        std::make_unique<MockNotificationPermissionUiSelector>(ui_to_use,
                                                               async));
  }

 private:
  UiToUse ui_to_use_;
  bool async_;
};

TEST_F(PermissionRequestManagerTest,
       UiSelectorNotUsedForPermissionsOtherThanNotification) {
  for (auto* request : {&request_mic_, &request_camera_}) {
    MockNotificationPermissionUiSelector::CreateForManager(
        manager_, NotificationPermissionUiSelector::UiToUse::kQuietUi,
        false /* async */);

    manager_->AddRequest(request);
    WaitForBubbleToBeShown();

    ASSERT_TRUE(prompt_factory_->is_visible());
    ASSERT_TRUE(
        prompt_factory_->RequestTypeSeen(request->GetPermissionRequestType()));
    EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
    Accept();

    EXPECT_TRUE(request->granted());
  }
}

TEST_F(PermissionRequestManagerTest, UiSelectorUsedForNotifications) {
  using UiToUse = NotificationPermissionUiSelector::UiToUse;
  const struct {
    NotificationPermissionUiSelector::UiToUse ui_to_use;
    bool async;
  } kTests[] = {
      {UiToUse::kQuietUi, true},
      {UiToUse::kNormalUi, true},
      {UiToUse::kQuietUi, false},
      {UiToUse::kNormalUi, false},
  };

  for (const auto& test : kTests) {
    MockNotificationPermissionUiSelector::CreateForManager(
        manager_, test.ui_to_use, test.async);

    MockPermissionRequest request(
        "foo", PermissionRequestType::PERMISSION_NOTIFICATIONS,
        PermissionRequestGestureType::GESTURE);

    manager_->AddRequest(&request);
    WaitForBubbleToBeShown();

    EXPECT_TRUE(prompt_factory_->is_visible());
    EXPECT_TRUE(
        prompt_factory_->RequestTypeSeen(request.GetPermissionRequestType()));
    EXPECT_EQ(test.ui_to_use == UiToUse::kQuietUi,
              manager_->ShouldCurrentRequestUseQuietUI());
    Accept();

    EXPECT_TRUE(request.granted());
  }
}

TEST_F(PermissionRequestManagerTest,
       UiSelectionHappensSeparatelyForEachRequest) {
  using UiToUse = NotificationPermissionUiSelector::UiToUse;
  MockNotificationPermissionUiSelector::CreateForManager(
      manager_, UiToUse::kQuietUi, true);
  MockPermissionRequest request1(
      "request1", PermissionRequestType::PERMISSION_NOTIFICATIONS,
      PermissionRequestGestureType::GESTURE);
  manager_->AddRequest(&request1);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(manager_->ShouldCurrentRequestUseQuietUI());
  Accept();

  MockPermissionRequest request2(
      "request2", PermissionRequestType::PERMISSION_NOTIFICATIONS,
      PermissionRequestGestureType::GESTURE);
  MockNotificationPermissionUiSelector::CreateForManager(
      manager_, UiToUse::kNormalUi, true);
  manager_->AddRequest(&request2);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Accept();
}

#if defined(OS_CHROMEOS)
TEST_F(PermissionRequestManagerTest, TestWebKioskModeSameOrigin) {
  auto request =
      MakeRequestInWebKioskMode(/*url*/ GURL("https://google.com/page"),
                                /*app_url*/ GURL("https://google.com/launch"));

  WaitForBubbleToBeShown();
  // It should be granted by default.
  EXPECT_TRUE(request->granted());
}

TEST_F(PermissionRequestManagerTest, TestWebKioskModeDifferentOrigin) {
  auto request =
      MakeRequestInWebKioskMode(/*url*/ GURL("https://example.com/page"),
                                /*app_url*/ GURL("https://google.com/launch"));

  WaitForBubbleToBeShown();
  // It should not be granted by default.
  EXPECT_FALSE(request->granted());
  EXPECT_TRUE(request->finished());
}
#endif  // defined(OS_CHROMEOS)

TEST_F(PermissionRequestManagerTest, TestCrowdDenyHoldbackChance) {
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

    if (test.enabled_in_prefs)
      QuietNotificationPermissionUiState::EnableQuietUiInPrefs(profile());

    MockPermissionRequest request(
        "request", PermissionRequestType::PERMISSION_NOTIFICATIONS, url);

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

TEST_F(PermissionRequestManagerTest, PreloadDataNeedsSafeBrowsingConfirmation) {
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

    MockPermissionRequest request(
        "request", PermissionRequestType::PERMISSION_NOTIFICATIONS, url);

    manager_->AddRequest(&request);
    WaitForBubbleToBeShown();
    EXPECT_EQ(test.expect_quiet_ui, manager_->ShouldCurrentRequestUseQuietUI());
    Closing();
  }
}
