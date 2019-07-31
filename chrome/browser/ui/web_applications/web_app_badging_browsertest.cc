// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/badging/badge_manager.h"
#include "chrome/browser/badging/badge_manager_delegate.h"
#include "chrome/browser/badging/badge_manager_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "content/public/browser/web_contents.h"

using content::RenderFrameHost;
using content::WebContents;

namespace web_app {

class WebAppBadgingBrowserTest : public WebAppControllerBrowserTest {
 public:
  // Listens to BadgeManager events and forwards them to the test class.
  class TestBadgeManagerDelegate : public badging::BadgeManagerDelegate {
   public:
    using SetBadgeCallback =
        base::RepeatingCallback<void(const AppId&, base::Optional<uint64_t>)>;
    using ClearBadgeCallback = base::RepeatingCallback<void(const AppId&)>;

    using ChangeFailedCallback = base::RepeatingCallback<void()>;

    TestBadgeManagerDelegate(Profile* profile,
                             SetBadgeCallback on_set_badge,
                             ClearBadgeCallback on_clear_badge,
                             ChangeFailedCallback on_change_failed)
        : badging::BadgeManagerDelegate(profile),
          on_set_badge_(on_set_badge),
          on_clear_badge_(on_clear_badge),
          on_change_failed_(on_change_failed) {}

    void OnBadgeSet(const AppId& app_id,
                    base::Optional<uint64_t> contents) override {
      on_set_badge_.Run(app_id, contents);
    }

    void OnBadgeCleared(const AppId& app_id) override {
      on_clear_badge_.Run(app_id);
    }

    void OnBadgeChangeIgnoredForTesting() override { on_change_failed_.Run(); }

   private:
    SetBadgeCallback on_set_badge_;
    ClearBadgeCallback on_clear_badge_;
    ChangeFailedCallback on_change_failed_;
  };

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebAppControllerBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII("enable-blink-features", "Badging");
  }

  void SetUpOnMainThread() override {
    WebAppControllerBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(https_server()->Start());
    ASSERT_TRUE(embedded_test_server()->Start());

    AppId app_id = InstallPWA(https_server()->GetURL(
        "/ssl/page_with_in_scope_and_cross_site_frame.html"));
    content::WebContents* web_contents = OpenApplication(app_id);
    // There should be exactly 3 frames:
    // 1) The main frame.
    // 2) A cross site frame, on https://example.com/
    // 3) A sub frame in the app's scope.
    auto frames = web_contents->GetAllFrames();
    ASSERT_EQ(3u, frames.size());

    main_frame_ = web_contents->GetMainFrame();
    for (auto* frame : frames) {
      if (frame->GetLastCommittedURL() == "https://example.com/")
        cross_site_frame_ = frame;
      else if (frame != main_frame_)
        in_scope_frame_ = frame;
    }

    ASSERT_TRUE(main_frame_);
    ASSERT_TRUE(in_scope_frame_);
    ASSERT_TRUE(cross_site_frame_);

    awaiter_ = std::make_unique<base::RunLoop>();

    std::unique_ptr<badging::BadgeManagerDelegate> delegate =
        std::make_unique<TestBadgeManagerDelegate>(
            profile(),
            base::BindRepeating(&WebAppBadgingBrowserTest::OnBadgeSet,
                                base::Unretained(this)),
            base::BindRepeating(&WebAppBadgingBrowserTest::OnBadgeCleared,
                                base::Unretained(this)),
            base::BindRepeating(&WebAppBadgingBrowserTest::OnBadgeChangeFailed,
                                base::Unretained(this)));
    badging::BadgeManagerFactory::GetInstance()
        ->GetForProfile(profile())
        ->SetDelegate(std::move(delegate));
  }

  void OnBadgeSet(const AppId& app_id, base::Optional<uint64_t> badge_content) {
    if (badge_content.has_value())
      last_badge_content_ = badge_content;
    else
      was_flagged_ = true;

    awaiter_->Quit();
  }

  void OnBadgeCleared(const AppId& app_id) {
    was_cleared_ = true;
    awaiter_->Quit();
  }

  void OnBadgeChangeFailed() {
    change_failed_ = true;
    awaiter_->Quit();
  }

 protected:
  void ExecuteScriptAndWaitForBadgeChange(std::string script,
                                          RenderFrameHost* on) {
    was_cleared_ = false;
    was_flagged_ = false;
    change_failed_ = false;
    last_badge_content_ = base::nullopt;
    awaiter_.reset(new base::RunLoop());

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
};

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

// Tests that the badge cannot be set and cleared from a cross site frame.
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

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    WebAppBadgingBrowserTest,
    ::testing::Values(ControllerType::kHostedAppController,
                      ControllerType::kUnifiedControllerWithBookmarkApp,
                      ControllerType::kUnifiedControllerWithWebApp));

}  // namespace web_app
