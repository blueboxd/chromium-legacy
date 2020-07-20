// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "components/permissions/notification_permission_ui_selector.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/permissions/test/test_permissions_client.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace permissions {

namespace {
using QuietUiReason = NotificationPermissionUiSelector::QuietUiReason;
}

class PermissionRequestManagerTest : public content::RenderViewHostTestHarness {
 public:
  PermissionRequestManagerTest()
      : content::RenderViewHostTestHarness(),
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
        request_ptz_("ptz",
                     PermissionRequestType::PERMISSION_CAMERA_PAN_TILT_ZOOM,
                     PermissionRequestGestureType::NO_GESTURE),
        iframe_request_same_domain_(
            "iframe",
            PermissionRequestType::PERMISSION_NOTIFICATIONS,
            GURL("http://www.google.com/some/url")),
        iframe_request_other_domain_(
            "iframe",
            PermissionRequestType::PERMISSION_GEOLOCATION,
            GURL("http://www.youtube.com")),
        iframe_request_camera_other_domain_(
            "iframe",
            PermissionRequestType::PERMISSION_MEDIASTREAM_CAMERA,
            GURL("http://www.youtube.com")),
        iframe_request_mic_other_domain_(
            "iframe",
            PermissionRequestType::PERMISSION_MEDIASTREAM_MIC,
            GURL("http://www.youtube.com")) {}

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    SetContents(CreateTestWebContents());
    url_ = GURL("http://www.google.com");
    NavigateAndCommit(url_);

    PermissionRequestManager::CreateForWebContents(web_contents());
    manager_ = PermissionRequestManager::FromWebContents(web_contents());
    prompt_factory_ = std::make_unique<MockPermissionPromptFactory>(manager_);
  }

  void TearDown() override {
    prompt_factory_ = nullptr;
    content::RenderViewHostTestHarness::TearDown();
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
    manager_->DOMContentLoaded(nullptr);
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

 protected:
  GURL url_;
  MockPermissionRequest request1_;
  MockPermissionRequest request2_;
  MockPermissionRequest request_mic_;
  MockPermissionRequest request_camera_;
  MockPermissionRequest request_ptz_;
  MockPermissionRequest iframe_request_same_domain_;
  MockPermissionRequest iframe_request_other_domain_;
  MockPermissionRequest iframe_request_camera_other_domain_;
  MockPermissionRequest iframe_request_mic_other_domain_;
  PermissionRequestManager* manager_;
  std::unique_ptr<MockPermissionPromptFactory> prompt_factory_;
  TestPermissionsClient client_;
};

TEST_F(PermissionRequestManagerTest, SingleRequest) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);

  Accept();
  EXPECT_TRUE(request1_.granted());
}

TEST_F(PermissionRequestManagerTest, SingleRequestViewFirst) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);

  Accept();
  EXPECT_TRUE(request1_.granted());
}

// Most requests should never be grouped.
TEST_F(PermissionRequestManagerTest, TwoRequestsUngrouped) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request2_);

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
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_mic_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_camera_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 2);

  Accept();
  EXPECT_TRUE(request_mic_.granted());
  EXPECT_TRUE(request_camera_.granted());

  // If the requests come from different origins, they should not be grouped.
  manager_->AddRequest(web_contents()->GetMainFrame(),
                       &iframe_request_mic_other_domain_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_camera_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
}

// Only camera/ptz requests from the same origin should be grouped.
TEST_F(PermissionRequestManagerTest, CameraPtzGrouped) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_camera_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_ptz_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 2);

  Accept();
  EXPECT_TRUE(request_camera_.granted());
  EXPECT_TRUE(request_ptz_.granted());

  // If the requests come from different origins, they should not be grouped.
  manager_->AddRequest(web_contents()->GetMainFrame(),
                       &iframe_request_camera_other_domain_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_ptz_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
}

// Only mic/camera/ptz requests from the same origin should be grouped.
TEST_F(PermissionRequestManagerTest, MicCameraPtzGrouped) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_mic_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_camera_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_ptz_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 3);

  Accept();
  EXPECT_TRUE(request_mic_.granted());
  EXPECT_TRUE(request_camera_.granted());
  EXPECT_TRUE(request_ptz_.granted());

  // If the requests come from different origins, they should not be grouped.
  manager_->AddRequest(web_contents()->GetMainFrame(),
                       &iframe_request_mic_other_domain_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_camera_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_ptz_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
}

TEST_F(PermissionRequestManagerTest, TwoRequestsTabSwitch) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_mic_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_camera_);
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

TEST_F(PermissionRequestManagerTest, ThreeRequestsTabSwitch) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_mic_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_camera_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_ptz_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 3);

  MockTabSwitchAway();
#if defined(OS_ANDROID)
  EXPECT_TRUE(prompt_factory_->is_visible());
#else
  EXPECT_FALSE(prompt_factory_->is_visible());
#endif

  MockTabSwitchBack();
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 3);

  Accept();
  EXPECT_TRUE(request_mic_.granted());
  EXPECT_TRUE(request_camera_.granted());
  EXPECT_TRUE(request_ptz_.granted());
}

TEST_F(PermissionRequestManagerTest, NoRequests) {
  WaitForBubbleToBeShown();
  EXPECT_FALSE(prompt_factory_->is_visible());
}

TEST_F(PermissionRequestManagerTest, PermissionRequestWhileTabSwitchedAway) {
  MockTabSwitchAway();
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(prompt_factory_->is_visible());

  MockTabSwitchBack();
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
}

TEST_F(PermissionRequestManagerTest, TwoRequestsDoNotCoalesce) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  manager_->AddRequest(web_contents()->GetMainFrame(), &request2_);

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
}

TEST_F(PermissionRequestManagerTest, TwoRequestsShownInTwoBubbles) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  manager_->AddRequest(web_contents()->GetMainFrame(), &request2_);

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);

  Accept();
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
  ASSERT_EQ(prompt_factory_->show_count(), 2);
}

TEST_F(PermissionRequestManagerTest, TestAddDuplicateRequest) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);

  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
}

TEST_F(PermissionRequestManagerTest, SequentialRequests) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());

  Accept();
  EXPECT_TRUE(request1_.granted());

  EXPECT_FALSE(prompt_factory_->is_visible());

  manager_->AddRequest(web_contents()->GetMainFrame(), &request2_);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  Accept();
  EXPECT_FALSE(prompt_factory_->is_visible());
  EXPECT_TRUE(request2_.granted());
}

TEST_F(PermissionRequestManagerTest, SameRequestRejected) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  EXPECT_FALSE(request1_.finished());

  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
}

TEST_F(PermissionRequestManagerTest, DuplicateQueuedRequest) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  manager_->AddRequest(web_contents()->GetMainFrame(), &request2_);

  MockPermissionRequest dupe_request("test1");
  manager_->AddRequest(web_contents()->GetMainFrame(), &dupe_request);
  EXPECT_FALSE(dupe_request.finished());
  EXPECT_FALSE(request1_.finished());

  MockPermissionRequest dupe_request2("test2");
  manager_->AddRequest(web_contents()->GetMainFrame(), &dupe_request2);
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
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  manager_->AddRequest(web_contents()->GetMainFrame(), &request2_);
  manager_->AddRequest(web_contents()->GetMainFrame(),
                       &iframe_request_other_domain_);

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
  manager_->AddRequest(web_contents()->GetMainFrame(),
                       &iframe_request_same_domain_);
  WaitForBubbleToBeShown();
  WaitForFrameLoad();

  EXPECT_TRUE(prompt_factory_->is_visible());
  Closing();
  EXPECT_TRUE(iframe_request_same_domain_.finished());
}

TEST_F(PermissionRequestManagerTest, MainFrameAndIFrameRequestSameDomain) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  manager_->AddRequest(web_contents()->GetMainFrame(),
                       &iframe_request_same_domain_);
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
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  manager_->AddRequest(web_contents()->GetMainFrame(),
                       &iframe_request_other_domain_);
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
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());

  manager_->AddRequest(web_contents()->GetMainFrame(),
                       &iframe_request_same_domain_);
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
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());

  manager_->AddRequest(web_contents()->GetMainFrame(),
                       &iframe_request_other_domain_);
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
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  manager_->AddRequest(web_contents()->GetMainFrame(),
                       &iframe_request_other_domain_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request2_);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(prompt_factory_->is_visible());
}

// This code path (calling Accept on a non-merged bubble, with no accepted
// permission) would never be used in actual Chrome, but its still tested for
// completeness.
TEST_F(PermissionRequestManagerTest, UMAForSimpleDeniedBubbleAlternatePath) {
  base::HistogramTester histograms;

  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  // No need to test UMA for showing prompts again, they were tested in
  // UMAForSimpleAcceptedBubble.

  Deny();
  histograms.ExpectUniqueSample(
      PermissionUmaUtil::kPermissionsPromptDenied,
      static_cast<base::HistogramBase::Sample>(PermissionRequestType::QUOTA),
      1);
}

TEST_F(PermissionRequestManagerTest, UMAForTabSwitching) {
  base::HistogramTester histograms;

  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
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

// Simulate a NotificationPermissionUiSelector that simply returns a
// predefined |ui_to_use| every time.
class MockNotificationPermissionUiSelector
    : public NotificationPermissionUiSelector {
 public:
  explicit MockNotificationPermissionUiSelector(
      base::Optional<QuietUiReason> quiet_ui_reason,
      bool async) {
    quiet_ui_reason_ = quiet_ui_reason;
    async_ = async;
  }

  void SelectUiToUse(PermissionRequest* request,
                     DecisionMadeCallback callback) override {
    Decision decision(quiet_ui_reason_, Decision::ShowNoWarning());
    if (async_) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), decision));
    } else {
      std::move(callback).Run(decision);
    }
  }

  static void CreateForManager(PermissionRequestManager* manager,
                               base::Optional<QuietUiReason> quiet_ui_reason,
                               bool async) {
    manager->set_notification_permission_ui_selector_for_testing(
        std::make_unique<MockNotificationPermissionUiSelector>(quiet_ui_reason,
                                                               async));
  }

 private:
  base::Optional<QuietUiReason> quiet_ui_reason_;
  bool async_;
};

TEST_F(PermissionRequestManagerTest,
       UiSelectorNotUsedForPermissionsOtherThanNotification) {
  for (auto* request : {&request_mic_, &request_camera_, &request_ptz_}) {
    MockNotificationPermissionUiSelector::CreateForManager(
        manager_,
        NotificationPermissionUiSelector::QuietUiReason::kEnabledInPrefs,
        false /* async */);

    manager_->AddRequest(web_contents()->GetMainFrame(), request);
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
  const struct {
    base::Optional<NotificationPermissionUiSelector::QuietUiReason>
        quiet_ui_reason;
    bool async;
  } kTests[] = {
      {QuietUiReason::kEnabledInPrefs, true},
      {NotificationPermissionUiSelector::Decision::UseNormalUi(), true},
      {QuietUiReason::kEnabledInPrefs, false},
      {NotificationPermissionUiSelector::Decision::UseNormalUi(), false},
  };

  for (const auto& test : kTests) {
    MockNotificationPermissionUiSelector::CreateForManager(
        manager_, test.quiet_ui_reason, test.async);

    MockPermissionRequest request(
        "foo", PermissionRequestType::PERMISSION_NOTIFICATIONS,
        PermissionRequestGestureType::GESTURE);

    manager_->AddRequest(web_contents()->GetMainFrame(), &request);
    WaitForBubbleToBeShown();

    EXPECT_TRUE(prompt_factory_->is_visible());
    EXPECT_TRUE(
        prompt_factory_->RequestTypeSeen(request.GetPermissionRequestType()));
    EXPECT_EQ(!!test.quiet_ui_reason,
              manager_->ShouldCurrentRequestUseQuietUI());
    Accept();

    EXPECT_TRUE(request.granted());
  }
}

TEST_F(PermissionRequestManagerTest,
       UiSelectionHappensSeparatelyForEachRequest) {
  using QuietUiReason = NotificationPermissionUiSelector::QuietUiReason;
  MockNotificationPermissionUiSelector::CreateForManager(
      manager_, QuietUiReason::kEnabledInPrefs, true);
  MockPermissionRequest request1(
      "request1", PermissionRequestType::PERMISSION_NOTIFICATIONS,
      PermissionRequestGestureType::GESTURE);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(manager_->ShouldCurrentRequestUseQuietUI());
  Accept();

  MockPermissionRequest request2(
      "request2", PermissionRequestType::PERMISSION_NOTIFICATIONS,
      PermissionRequestGestureType::GESTURE);
  MockNotificationPermissionUiSelector::CreateForManager(
      manager_, NotificationPermissionUiSelector::Decision::UseNormalUi(),
      true);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request2);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Accept();
}

TEST_F(PermissionRequestManagerTest, RequestsNotSupported) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  Accept();
  EXPECT_TRUE(request1_.granted());

  manager_->set_web_contents_supports_permission_requests(false);

  manager_->AddRequest(web_contents()->GetMainFrame(), &request2_);
  EXPECT_TRUE(request2_.cancelled());
}
}  // namespace permissions
