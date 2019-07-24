// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/sync/test/fake_server/fake_server_network_resources.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_constants.h"
#include "ui/views/test/widget_test.h"

namespace {

using password_manager::metrics_util::PasswordType;

// The ClickEvent class is copied from the PageInfoBubbleViewBrowserTest.iden
class ClickEvent : public ui::Event {
 public:
  ClickEvent() : ui::Event(ui::ET_UNKNOWN, base::TimeTicks(), 0) {}
};

void PerformMouseClickOnView(views::View* view) {
  ui::AXActionData data;
  data.action = ax::mojom::Action::kDoDefault;
  view->HandleAccessibleAction(data);
}

// Clicks the location icon to open the page info bubble.
void OpenPageInfoBubble(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  LocationIconView* location_icon_view =
      browser_view->toolbar()->location_bar()->location_icon_view();
  ASSERT_TRUE(location_icon_view);
  ClickEvent event;
  location_icon_view->ShowBubble(event);
  views::BubbleDialogDelegateView* page_info =
      PageInfoBubbleView::GetPageInfoBubble();
  EXPECT_NE(nullptr, page_info);
  page_info->set_close_on_deactivate(false);
}

// Opens the Page Info bubble and retrieves the UI view identified by
// |view_id|.
views::View* GetView(Browser* browser, int view_id) {
  views::Widget* page_info_bubble =
      PageInfoBubbleView::GetPageInfoBubble()->GetWidget();
  EXPECT_TRUE(page_info_bubble);

  views::View* view = page_info_bubble->GetRootView()->GetViewByID(view_id);
  EXPECT_TRUE(view);
  return view;
}

}  // namespace

// This test suite tests functionality that requires Sync to be active.
class PageInfoBubbleViewSyncBrowserTest : public SyncTest {
 public:
  PageInfoBubbleViewSyncBrowserTest() : SyncTest(SINGLE_CLIENT) {}

 protected:
  void SetupSyncForAccount(Profile* profile) {
    syncer::ProfileSyncService* sync_service =
        ProfileSyncServiceFactory::GetAsProfileSyncServiceForProfile(profile);

    sync_service->OverrideNetworkResourcesForTest(
        std::make_unique<fake_server::FakeServerNetworkResources>(
            GetFakeServer()->AsWeakPtr()));

    std::string username;
#if defined(OS_CHROMEOS)
    // In browser tests, the profile may already by authenticated with stub
    // account |user_manager::kStubUserEmail|.
    CoreAccountInfo info =
        IdentityManagerFactory::GetForProfile(profile)->GetPrimaryAccountInfo();
    username = info.email;
#endif
    if (username.empty()) {
      browser()->profile()->GetPrefs()->SetString(
          prefs::kGoogleServicesUsername, "user@domain.com");
      username = "user@domain.com";
    }

    std::unique_ptr<ProfileSyncServiceHarness> harness =
        ProfileSyncServiceHarness::Create(
            profile, username, "password",
            ProfileSyncServiceHarness::SigninType::FAKE_SIGNIN);
    EXPECT_TRUE(harness->SetupSync());
  }

  DISALLOW_COPY_AND_ASSIGN(PageInfoBubbleViewSyncBrowserTest);
};

// Test opening page info bubble that matches
// SB_THREAT_TYPE_SIGN_IN_PASSWORD_REUSE threat type.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewSyncBrowserTest,
                       VerifySignInPasswordReusePageInfoBubble) {
  Profile* profile = browser()->profile();
  SetupSyncForAccount(profile);

  ASSERT_TRUE(embedded_test_server()->Start());
  base::HistogramTester histograms;
  histograms.ExpectTotalCount(safe_browsing::kSyncPasswordPageInfoHistogram, 0);
  ui_test_utils::NavigateToURL(browser(), embedded_test_server()->GetURL("/"));
  // Update security state of the current page to match
  // SB_THREAT_TYPE_SIGN_IN_PASSWORD_REUSE.
  safe_browsing::ChromePasswordProtectionService* service = safe_browsing::
      ChromePasswordProtectionService::GetPasswordProtectionService(profile);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  service->ShowModalWarning(contents, "token",
                            PasswordType::PRIMARY_ACCOUNT_PASSWORD);

  OpenPageInfoBubble(browser());
  views::View* change_password_button = GetView(
      browser(), PageInfoBubbleView::VIEW_ID_PAGE_INFO_BUTTON_CHANGE_PASSWORD);
  views::View* safelist_password_reuse_button = GetView(
      browser(),
      PageInfoBubbleView::VIEW_ID_PAGE_INFO_BUTTON_WHITELIST_PASSWORD_REUSE);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  std::unique_ptr<security_state::VisibleSecurityState> visible_security_state =
      helper->GetVisibleSecurityState();
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_SIGN_IN_PASSWORD_REUSE,
            visible_security_state->malicious_content_status);

  // Verify these two buttons are showing.
  EXPECT_TRUE(change_password_button->GetVisible());
  EXPECT_TRUE(safelist_password_reuse_button->GetVisible());

  // Verify clicking on button will increment corresponding bucket of
  // PasswordProtection.PageInfoAction.SyncPasswordEntry histogram.
  PerformMouseClickOnView(change_password_button);
  EXPECT_THAT(
      histograms.GetAllSamples(safe_browsing::kSyncPasswordPageInfoHistogram),
      testing::ElementsAre(
          base::Bucket(static_cast<int>(safe_browsing::WarningAction::SHOWN),
                       1),
          base::Bucket(
              static_cast<int>(safe_browsing::WarningAction::CHANGE_PASSWORD),
              1)));

  PerformMouseClickOnView(safelist_password_reuse_button);
  EXPECT_THAT(
      histograms.GetAllSamples(safe_browsing::kSyncPasswordPageInfoHistogram),
      testing::ElementsAre(
          base::Bucket(static_cast<int>(safe_browsing::WarningAction::SHOWN),
                       1),
          base::Bucket(
              static_cast<int>(safe_browsing::WarningAction::CHANGE_PASSWORD),
              1),
          base::Bucket(static_cast<int>(
                           safe_browsing::WarningAction::MARK_AS_LEGITIMATE),
                       1)));
  // Security state will change after whitelisting.
  visible_security_state = helper->GetVisibleSecurityState();
  EXPECT_EQ(security_state::MALICIOUS_CONTENT_STATUS_NONE,
            visible_security_state->malicious_content_status);
}
