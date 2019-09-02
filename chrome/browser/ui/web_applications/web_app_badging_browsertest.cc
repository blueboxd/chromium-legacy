// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/badging/badge_manager.h"
#include "chrome/browser/badging/badge_manager_factory.h"
#include "chrome/browser/badging/test_badge_manager_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "content/public/browser/web_contents.h"

using content::RenderFrameHost;
using content::WebContents;

namespace web_app {

class WebAppBadgingBrowserTest : public WebAppControllerBrowserTest {
 public:
  WebAppBadgingBrowserTest()
      : WebAppControllerBrowserTest(),
        cross_origin_https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebAppControllerBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII("enable-blink-features", "Badging");
  }

  void SetUpOnMainThread() override {
    WebAppControllerBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(cross_origin_https_server_.Start());
    ASSERT_TRUE(https_server()->Start());
    ASSERT_TRUE(embedded_test_server()->Start());

    GURL cross_site_frame_url =
        cross_origin_https_server_.GetURL("/ssl/google.html");
    // Note: The url for the cross site frame is embedded in the query string.
    GURL app_url = https_server()->GetURL(
        "/ssl/page_with_in_scope_and_cross_site_frame.html?url=" +
        cross_site_frame_url.spec());
    AppId app_id = InstallPWA(app_url);
    content::WebContents* web_contents = OpenApplication(app_id);
    // There should be exactly 3 frames:
    // 1) The main frame.
    // 2) A cross site frame, on |cross_site_frame_url|.
    // 3) A sub frame in the app's scope.
    auto frames = web_contents->GetAllFrames();
    ASSERT_EQ(3u, frames.size());

    main_frame_ = web_contents->GetMainFrame();
    for (auto* frame : frames) {
      if (url::IsSameOriginWith(frame->GetLastCommittedURL(),
                                main_frame_->GetLastCommittedURL())) {
        in_scope_frame_ = frame;
      } else if (frame != main_frame_) {
        cross_site_frame_ = frame;
      }
    }

    ASSERT_TRUE(main_frame_);
    ASSERT_TRUE(in_scope_frame_);
    ASSERT_TRUE(cross_site_frame_);

    awaiter_ = std::make_unique<base::RunLoop>();

    badging::BadgeManager* badge_manager =
        badging::BadgeManagerFactory::GetInstance()->GetForProfile(profile());

    // The delegate is owned by the badge manager. We hold a pointer to it for
    // the test.
    std::unique_ptr<badging::TestBadgeManagerDelegate> owned_delegate =
        std::make_unique<badging::TestBadgeManagerDelegate>(
            profile(), badge_manager,
            &WebAppProviderBase::GetProviderBase(profile())->registrar());
    owned_delegate->SetOnBadgeChanged(base::BindRepeating(
        &WebAppBadgingBrowserTest::OnBadgeChanged, base::Unretained(this)));
    delegate_ = owned_delegate.get();

    badge_manager->SetDelegate(std::move(owned_delegate));
  }

  void OnBadgeChanged() {
    // This is only set up to deal with one badge change at a time, in order to
    // make asserting the result of a badge change easier.
    int total_changes = delegate_->cleared_app_badges().size() +
                        delegate_->set_app_badges().size();
    EXPECT_LE(total_changes, 1);

    // A change is a failure if it doesn't set or clear any badges.
    change_failed_ = total_changes == 0;
    was_cleared_ = delegate_->cleared_app_badges().size();

    if (delegate_->set_app_badges().size() == 1) {
      last_badge_content_ = delegate_->set_app_badges()[0].second;
      was_flagged_ = last_badge_content_ == base::nullopt;
    }

    awaiter_->Quit();
  }

 protected:
  void ExecuteScriptAndWaitForBadgeChange(std::string script,
                                          RenderFrameHost* on) {
    was_cleared_ = false;
    was_flagged_ = false;
    change_failed_ = false;
    last_badge_content_ = base::nullopt;
    awaiter_ = std::make_unique<base::RunLoop>();
    delegate_->ResetBadges();

    ASSERT_TRUE(content::ExecuteScript(on, script));

    if (was_cleared_ || was_flagged_ || change_failed_ ||
        last_badge_content_ != base::nullopt)
      return;

    awaiter_->Run();
  }

  RenderFrameHost* main_frame_;
  RenderFrameHost* in_scope_frame_;
  RenderFrameHost* cross_site_frame_;

  bool was_cleared_ = false;
  bool was_flagged_ = false;
  bool change_failed_ = false;
  base::Optional<uint64_t> last_badge_content_ = base::nullopt;

 private:
  std::unique_ptr<base::RunLoop> awaiter_;
  badging::TestBadgeManagerDelegate* delegate_;
  net::EmbeddedTestServer cross_origin_https_server_;
};

// Tests that the badge for the main frame is not affected by changing the badge
// of a cross site subframe.
IN_PROC_BROWSER_TEST_P(WebAppBadgingBrowserTest,
                       BadgeCannotBeChangedFromCrossSiteFrame) {
  // Clearing from cross site frame should be a no-op.
  ExecuteScriptAndWaitForBadgeChange("ExperimentalBadge.clear()",
                                     cross_site_frame_);
  ASSERT_FALSE(was_cleared_);
  ASSERT_FALSE(was_flagged_);
  ASSERT_TRUE(change_failed_);

  // Setting from cross site frame should be a no-op.
  ExecuteScriptAndWaitForBadgeChange("ExperimentalBadge.set(77)",
                                     cross_site_frame_);
  ASSERT_FALSE(was_cleared_);
  ASSERT_FALSE(was_flagged_);
  ASSERT_TRUE(change_failed_);
}

// Tests that setting the badge to an integer will be propagated across
// processes.
IN_PROC_BROWSER_TEST_P(WebAppBadgingBrowserTest, BadgeCanBeSetToAnInteger) {
  ExecuteScriptAndWaitForBadgeChange("ExperimentalBadge.set(99)", main_frame_);
  ASSERT_FALSE(was_cleared_);
  ASSERT_FALSE(was_flagged_);
  ASSERT_FALSE(change_failed_);
  ASSERT_EQ(base::Optional<uint64_t>(99u), last_badge_content_);
}

// Tests that calls to |Badge.clear| are propagated across processes.
IN_PROC_BROWSER_TEST_P(WebAppBadgingBrowserTest,
                       BadgeCanBeClearedWithClearMethod) {
  ExecuteScriptAndWaitForBadgeChange("ExperimentalBadge.set(55)", main_frame_);
  ASSERT_FALSE(was_cleared_);
  ASSERT_FALSE(was_flagged_);
  ASSERT_FALSE(change_failed_);
  ASSERT_EQ(base::Optional<uint64_t>(55u), last_badge_content_);

  ExecuteScriptAndWaitForBadgeChange("ExperimentalBadge.clear()", main_frame_);
  ASSERT_TRUE(was_cleared_);
  ASSERT_FALSE(was_flagged_);
  ASSERT_FALSE(change_failed_);
  ASSERT_EQ(base::nullopt, last_badge_content_);
}

// Tests that calling Badge.set(0) is equivalent to calling |Badge.clear| and
// that it propagates across processes.
IN_PROC_BROWSER_TEST_P(WebAppBadgingBrowserTest, BadgeCanBeClearedWithZero) {
  ExecuteScriptAndWaitForBadgeChange("ExperimentalBadge.set(0)", main_frame_);
  ASSERT_TRUE(was_cleared_);
  ASSERT_FALSE(was_flagged_);
  ASSERT_FALSE(change_failed_);
  ASSERT_EQ(base::nullopt, last_badge_content_);
}

// Tests that setting the badge without content is propagated across processes.
IN_PROC_BROWSER_TEST_P(WebAppBadgingBrowserTest, BadgeCanBeSetWithoutAValue) {
  ExecuteScriptAndWaitForBadgeChange("ExperimentalBadge.set()", main_frame_);
  ASSERT_FALSE(was_cleared_);
  ASSERT_TRUE(was_flagged_);
  ASSERT_FALSE(change_failed_);
  ASSERT_EQ(base::nullopt, last_badge_content_);
}

// Tests that the badge can be set and cleared from an in scope frame.
IN_PROC_BROWSER_TEST_P(WebAppBadgingBrowserTest,
                       BadgeCanBeSetAndClearedFromInScopeFrame) {
  ExecuteScriptAndWaitForBadgeChange("ExperimentalBadge.set()",
                                     in_scope_frame_);
  ASSERT_FALSE(was_cleared_);
  ASSERT_TRUE(was_flagged_);
  ASSERT_FALSE(change_failed_);
  ASSERT_EQ(base::nullopt, last_badge_content_);

  ExecuteScriptAndWaitForBadgeChange("ExperimentalBadge.clear()",
                                     in_scope_frame_);
  ASSERT_TRUE(was_cleared_);
  ASSERT_FALSE(was_flagged_);
  ASSERT_FALSE(change_failed_);
  ASSERT_EQ(base::nullopt, last_badge_content_);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    WebAppBadgingBrowserTest,
    ::testing::Values(ControllerType::kHostedAppController,
                      ControllerType::kUnifiedControllerWithBookmarkApp,
                      ControllerType::kUnifiedControllerWithWebApp));

}  // namespace web_app
