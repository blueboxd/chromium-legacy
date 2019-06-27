// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_icon_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_page_action_icon_container_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/common/autofill_payments_features.h"

// TODO(crbug.com/932818): Clean this and the same code in ukm_browsertest.
// Maybe move them to InProcessBrowserTest.
namespace {

void UnblockOnProfileCreation(base::RunLoop* run_loop,
                              Profile* profile,
                              Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_INITIALIZED)
    run_loop->Quit();
}

Profile* CreateGuestProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath new_path = profile_manager->GetGuestProfilePath();
  base::RunLoop run_loop;
  profile_manager->CreateProfileAsync(
      new_path, base::BindRepeating(&UnblockOnProfileCreation, &run_loop),
      base::string16(), std::string());
  run_loop.Run();
  return profile_manager->GetProfileByPath(new_path);
}

}  // namespace

// The param is whether to use the highlight in the container.
class ToolbarPageActionIconContainerViewBrowserTest
    : public InProcessBrowserTest {
 public:
  ToolbarPageActionIconContainerViewBrowserTest() {}
  ~ToolbarPageActionIconContainerViewBrowserTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        autofill::features::kAutofillEnableToolbarStatusChip);
    InProcessBrowserTest::SetUp();
  }

  void TestUsesHighlight(ToolbarPageActionIconContainerView* container,
                         bool expect_highlight) {
    DCHECK(container);

    // Make sure the save-card icon is visible so that at least two children are
    // visible. Otherwise the border highlight would never be drawn.
    container->save_card_icon_view()->SetVisible(true);

    EXPECT_EQ(container->uses_highlight(), expect_highlight);
    EXPECT_EQ(container->border(), nullptr);

    container->save_card_icon_view()->SetHighlighted(true);
    EXPECT_EQ(container->border() != nullptr, expect_highlight);

    container->save_card_icon_view()->SetHighlighted(false);
    EXPECT_EQ(container->border(), nullptr);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarPageActionIconContainerViewBrowserTest);
};

IN_PROC_BROWSER_TEST_F(ToolbarPageActionIconContainerViewBrowserTest,
                       ShouldUpdateHighlightInNormalWindow) {
  ToolbarPageActionIconContainerView* container_view =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar()
          ->toolbar_page_action_container();
  TestUsesHighlight(container_view, /*expect_highlight=*/true);
}

IN_PROC_BROWSER_TEST_F(ToolbarPageActionIconContainerViewBrowserTest,
                       ShouldUpdateHighlightInGuestWindow) {
  Profile* guest_profile = CreateGuestProfile();
  Browser* guest_browser = CreateIncognitoBrowser(guest_profile);
  ASSERT_TRUE(guest_browser->profile()->IsGuestSession());
  ToolbarPageActionIconContainerView* container_view =
      BrowserView::GetBrowserViewForBrowser(guest_browser)
          ->toolbar()
          ->toolbar_page_action_container();
  TestUsesHighlight(container_view, /*expect_highlight=*/true);
}

IN_PROC_BROWSER_TEST_F(ToolbarPageActionIconContainerViewBrowserTest,
                       ShouldNotUpdateHighlightInIncognitoWindow) {
  Browser* incognito_browser = CreateIncognitoBrowser();
  ToolbarPageActionIconContainerView* container_view =
      BrowserView::GetBrowserViewForBrowser(incognito_browser)
          ->toolbar()
          ->toolbar_page_action_container();
  TestUsesHighlight(container_view, /*expect_highlight=*/false);
}
