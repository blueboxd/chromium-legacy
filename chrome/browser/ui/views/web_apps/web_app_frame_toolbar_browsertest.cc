// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_frame_toolbar_view.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"
#include "url/gurl.h"

class WebAppFrameToolbarBrowserTest : public InProcessBrowserTest {
 public:
  WebAppFrameToolbarBrowserTest() {
    scoped_feature_list_.InitWithFeatures({features::kDesktopMinimalUI}, {});
  }
  ~WebAppFrameToolbarBrowserTest() override = default;

  WebAppFrameToolbarBrowserTest(const WebAppFrameToolbarBrowserTest&) = delete;
  WebAppFrameToolbarBrowserTest& operator=(
      const WebAppFrameToolbarBrowserTest&) = delete;

  GURL GetAppURL() { return GURL("https://test.org"); }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    WebAppFrameToolbarView::DisableAnimationForTesting();
  }

  void InstallAndLaunchWebApp() {
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->app_url = GetAppURL();
    web_app_info->scope = GetAppURL().GetWithoutFilename();
    web_app_info->title = base::ASCIIToUTF16("A minimal-ui app");
    web_app_info->display_mode = web_app::DisplayMode::kMinimalUi;
    web_app_info->open_as_window = true;

    web_app::AppId app_id =
        web_app::InstallWebApp(browser()->profile(), std::move(web_app_info));
    content::TestNavigationObserver navigation_observer(GetAppURL());
    navigation_observer.StartWatchingNewWebContents();
    app_browser_ = web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
    navigation_observer.WaitForNavigationFinished();

    browser_view_ = BrowserView::GetBrowserViewForBrowser(app_browser_);
    views::NonClientFrameView* frame_view =
        browser_view_->GetWidget()->non_client_view()->frame_view();
    frame_view_ = static_cast<BrowserNonClientFrameView*>(frame_view);

    web_app_frame_toolbar_ = frame_view_->web_app_frame_toolbar_for_testing();
    DCHECK(web_app_frame_toolbar_);
    DCHECK(web_app_frame_toolbar_->GetVisible());
  }

  Browser* app_browser_ = nullptr;
  BrowserView* browser_view_ = nullptr;
  BrowserNonClientFrameView* frame_view_ = nullptr;
  WebAppFrameToolbarView* web_app_frame_toolbar_ = nullptr;

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarBrowserTest, SpaceConstrained) {
  InstallAndLaunchWebApp();

  views::View* const toolbar_left_container =
      web_app_frame_toolbar_->GetLeftContainerForTesting();
  EXPECT_EQ(toolbar_left_container->parent(), web_app_frame_toolbar_);

  views::View* const window_title =
      frame_view_->GetViewByID(VIEW_ID_WINDOW_TITLE);
#if defined(OS_CHROMEOS)
  EXPECT_FALSE(window_title);
#else
  EXPECT_EQ(window_title->parent(), frame_view_);
#endif

  views::View* const toolbar_right_container =
      web_app_frame_toolbar_->GetRightContainerForTesting();
  EXPECT_EQ(toolbar_right_container->parent(), web_app_frame_toolbar_);

  views::View* const page_action_icon_container =
      web_app_frame_toolbar_->GetPageActionIconContainerForTesting();
  EXPECT_EQ(page_action_icon_container->parent(), toolbar_right_container);

  views::View* const menu_button =
      browser_view_->toolbar_button_provider()->GetAppMenuButton();
  EXPECT_EQ(menu_button->parent(), toolbar_right_container);

  // Ensure we initially have abundant space.
  frame_view_->SetSize(gfx::Size(1000, 1000));

  EXPECT_TRUE(toolbar_left_container->GetVisible());
  const int original_left_container_width = toolbar_left_container->width();
  EXPECT_GT(original_left_container_width, 0);

#if defined(OS_WIN) || (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  const int original_window_title_width = window_title->width();
  EXPECT_GT(original_window_title_width, 0);
#endif

  // Initially the page action icons are not visible.
  EXPECT_EQ(page_action_icon_container->width(), 0);
  const int original_menu_button_width = menu_button->width();
  EXPECT_GT(original_menu_button_width, 0);

  // Cause the zoom page action icon to be visible.
  chrome::Zoom(app_browser_, content::PAGE_ZOOM_IN);

  // The layout should be invalidated, but since we don't have the benefit of
  // the compositor to immediately kick a layout off, we have to do it manually.
  frame_view_->Layout();

  // The page action icons should now take up width, leaving less space on
  // Windows and Linux for the window title. (On Mac, the window title remains
  // centered - not tested here.)

  EXPECT_TRUE(toolbar_left_container->GetVisible());
  EXPECT_EQ(toolbar_left_container->width(), original_left_container_width);

#if defined(OS_WIN) || (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  EXPECT_GT(window_title->width(), 0);
  EXPECT_LT(window_title->width(), original_window_title_width);
#endif

  EXPECT_GT(page_action_icon_container->width(), 0);
  EXPECT_EQ(menu_button->width(), original_menu_button_width);

  // Resize the WebAppFrameToolbarView just enough to clip out the page action
  // icons (and toolbar contents left of them).
  const int original_toolbar_width = web_app_frame_toolbar_->width();
  web_app_frame_toolbar_->SetSize(
      gfx::Size(toolbar_right_container->width() -
                    page_action_icon_container->bounds().right(),
                web_app_frame_toolbar_->height()));
  frame_view_->SetSize(gfx::Size(frame_view_->width() - original_toolbar_width +
                                     web_app_frame_toolbar_->width(),
                                 frame_view_->height()));

  // The left container (containing Back and Reload) should be hidden.
  EXPECT_FALSE(toolbar_left_container->GetVisible());

  // The window title should be clipped to 0 width.
#if defined(OS_WIN) || (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  EXPECT_EQ(window_title->width(), 0);
#endif

  // The page action icons should be clipped to 0 width while the app menu
  // button retains its full width.
  EXPECT_EQ(page_action_icon_container->width(), 0);
  EXPECT_EQ(menu_button->width(), original_menu_button_width);
}
