// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/glass_browser_frame_view.h"

#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_frame_toolbar_view.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_navigation_observer.h"

class WebAppGlassBrowserFrameViewTest : public InProcessBrowserTest {
 public:
  WebAppGlassBrowserFrameViewTest() = default;
  ~WebAppGlassBrowserFrameViewTest() override = default;

  GURL GetAppURL() { return GURL("https://test.org"); }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    WebAppFrameToolbarView::DisableAnimationForTesting();
  }

  // Windows 7 does not use GlassBrowserFrameView when Aero glass is not
  // enabled. Skip testing in this scenario.
  // TODO(https://crbug.com/863278): Force Aero glass on Windows 7 for this
  // test.
  bool InstallAndLaunchWebApp() {
    WebApplicationInfo web_app_info;
    web_app_info.app_url = GetAppURL();
    web_app_info.scope = GetAppURL().GetWithoutFilename();
    if (theme_color_)
      web_app_info.theme_color = *theme_color_;

    // TODO(alancutter): Use web_app::InstallManager instead of Extensions
    // specific install path.
    const extensions::Extension* app =
        extensions::browsertest_util::InstallBookmarkApp(browser()->profile(),
                                                         web_app_info);
    content::TestNavigationObserver navigation_observer(GetAppURL());
    navigation_observer.StartWatchingNewWebContents();
    app_browser_ = extensions::browsertest_util::LaunchAppBrowser(
        browser()->profile(), app);
    navigation_observer.WaitForNavigationFinished();

    browser_view_ = BrowserView::GetBrowserViewForBrowser(app_browser_);
    views::NonClientFrameView* frame_view =
        browser_view_->GetWidget()->non_client_view()->frame_view();

    if (frame_view->GetClassName() != GlassBrowserFrameView::kClassName)
      return false;
    glass_frame_view_ = static_cast<GlassBrowserFrameView*>(frame_view);

    web_app_frame_toolbar_ =
        glass_frame_view_->web_app_frame_toolbar_for_testing();
    DCHECK(web_app_frame_toolbar_);
    DCHECK(web_app_frame_toolbar_->GetVisible());
    return true;
  }

  base::Optional<SkColor> theme_color_ = SK_ColorBLUE;
  Browser* app_browser_ = nullptr;
  BrowserView* browser_view_ = nullptr;
  GlassBrowserFrameView* glass_frame_view_ = nullptr;
  WebAppFrameToolbarView* web_app_frame_toolbar_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebAppGlassBrowserFrameViewTest);
};

IN_PROC_BROWSER_TEST_F(WebAppGlassBrowserFrameViewTest, ThemeColor) {
  if (!InstallAndLaunchWebApp())
    return;

  EXPECT_EQ(glass_frame_view_->GetTitlebarColor(), *theme_color_);
}

IN_PROC_BROWSER_TEST_F(WebAppGlassBrowserFrameViewTest, NoThemeColor) {
  theme_color_ = base::nullopt;
  if (!InstallAndLaunchWebApp())
    return;

  EXPECT_EQ(
      glass_frame_view_->GetTitlebarColor(),
      ThemeProperties::GetDefaultColor(ThemeProperties::COLOR_FRAME, false));
}

IN_PROC_BROWSER_TEST_F(WebAppGlassBrowserFrameViewTest, SpaceConstrained) {
  theme_color_ = base::nullopt;
  if (!InstallAndLaunchWebApp())
    return;

  views::View* page_action_icon_container =
      web_app_frame_toolbar_->GetPageActionIconContainerForTesting();
  EXPECT_EQ(page_action_icon_container->parent(), web_app_frame_toolbar_);

  views::View* menu_button =
      browser_view_->toolbar_button_provider()->GetAppMenuButton();
  EXPECT_EQ(menu_button->parent(), web_app_frame_toolbar_);

  // Initially the page action icons are not visible, just the menu button has
  // width.
  EXPECT_EQ(page_action_icon_container->width(), 0);
  int original_menu_button_width = menu_button->width();
  EXPECT_GT(original_menu_button_width, 0);

  // Cause the zoom page action icon to be visible.
  chrome::Zoom(app_browser_, content::PAGE_ZOOM_IN);

  // The page action icons should now take up width.
  EXPECT_GT(page_action_icon_container->width(), 0);
  EXPECT_EQ(menu_button->width(), original_menu_button_width);

  // Resize the WebAppFrameToolbarView just enough to clip out the page action
  // icons.
  web_app_frame_toolbar_->SetSize(
      gfx::Size(web_app_frame_toolbar_->width() -
                    page_action_icon_container->bounds().right(),
                web_app_frame_toolbar_->height()));
  web_app_frame_toolbar_->Layout();

  // The page action icons should be clipped to 0 width while the app menu
  // button retains its full width.
  EXPECT_EQ(page_action_icon_container->width(), 0);
  EXPECT_EQ(menu_button->width(), original_menu_button_width);
}

IN_PROC_BROWSER_TEST_F(WebAppGlassBrowserFrameViewTest, MaximizedLayout) {
  if (!InstallAndLaunchWebApp())
    return;

  glass_frame_view_->frame()->Maximize();
  static_cast<views::View*>(glass_frame_view_)->Layout();

  DCHECK_GT(glass_frame_view_->window_title_for_testing()->x(), 0);
  DCHECK_GT(glass_frame_view_->web_app_frame_toolbar_for_testing()->y(), 0);
}
