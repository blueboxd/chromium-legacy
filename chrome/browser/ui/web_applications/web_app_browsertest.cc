// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/ui/web_applications/web_app_menu_model.h"
#include "chrome/browser/web_applications/components/app_registry_controller.h"
#include "chrome/browser/web_applications/components/external_install_options.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/test/web_app_install_observer.h"
#include "chrome/common/web_application_info.h"
#include "components/sessions/core/tab_restore_service.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace {

constexpr const char kExampleURL[] = "http://example.org/";

constexpr char kLaunchWebAppDisplayModeHistogram[] = "Launch.WebAppDisplayMode";

// Performs a navigation and then checks that the toolbar visibility is as
// expected.
void NavigateAndCheckForToolbar(Browser* browser,
                                const GURL& url,
                                bool expected_visibility,
                                bool proceed_through_interstitial = false) {
  web_app::NavigateToURLAndWait(browser, url, proceed_through_interstitial);
  EXPECT_EQ(expected_visibility,
            browser->app_controller()->ShouldShowCustomTabBar());
}

// Opens |url| in a new popup window with the dimensions |popup_size|.
Browser* OpenPopupAndWait(Browser* browser,
                          const GURL& url,
                          const gfx::Size& popup_size) {
  content::WebContents* const web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  content::WebContentsAddedObserver new_contents_observer;
  std::string open_window_script = base::StringPrintf(
      "window.open('%s', '_blank', 'toolbar=none,width=%i,height=%i')",
      url.spec().c_str(), popup_size.width(), popup_size.height());

  EXPECT_TRUE(content::ExecJs(web_contents, open_window_script));

  content::WebContents* popup_contents = new_contents_observer.GetWebContents();
  content::WaitForLoadStop(popup_contents);
  Browser* popup_browser = chrome::FindBrowserWithWebContents(popup_contents);

  // The navigation should happen in a new window.
  EXPECT_NE(browser, popup_browser);

  return popup_browser;
}

}  // namespace

namespace web_app {

class WebAppBrowserTest : public WebAppControllerBrowserTest {
 public:
  GURL GetSecureAppURL() {
    return https_server()->GetURL("app.com", "/ssl/google.html");
  }

  GURL GetURLForPath(const std::string& path) {
    return https_server()->GetURL("app.com", path);
  }

  AppId InstallPwaForCurrentUrl() {
    chrome::SetAutoAcceptPWAInstallConfirmationForTesting(true);
    WebAppInstallObserver observer(profile());
    CHECK(chrome::ExecuteCommand(browser(), IDC_INSTALL_PWA));
    AppId app_id = observer.AwaitNextInstall();
    chrome::SetAutoAcceptPWAInstallConfirmationForTesting(false);
    return app_id;
  }
};

using WebAppTabRestoreBrowserTest = WebAppBrowserTest;

IN_PROC_BROWSER_TEST_P(WebAppBrowserTest, CreatedForInstalledPwaForPwa) {
  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->app_url = GURL(kExampleURL);
  web_app_info->scope = GURL(kExampleURL);
  AppId app_id = InstallWebApp(std::move(web_app_info));
  Browser* app_browser = LaunchWebAppBrowser(app_id);

  EXPECT_TRUE(app_browser->app_controller()->CreatedForInstalledPwa());
}

IN_PROC_BROWSER_TEST_P(WebAppBrowserTest, ThemeColor) {
  {
    const SkColor theme_color = SkColorSetA(SK_ColorBLUE, 0xF0);
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->app_url = GURL(kExampleURL);
    web_app_info->scope = GURL(kExampleURL);
    web_app_info->theme_color = theme_color;
    AppId app_id = InstallWebApp(std::move(web_app_info));
    Browser* app_browser = LaunchWebAppBrowser(app_id);

    EXPECT_EQ(GetAppIdFromApplicationName(app_browser->app_name()), app_id);
    EXPECT_EQ(SkColorSetA(theme_color, SK_AlphaOPAQUE),
              app_browser->app_controller()->GetThemeColor());
  }
  {
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->app_url = GURL("http://example.org/2");
    web_app_info->scope = GURL("http://example.org/");
    web_app_info->theme_color = base::Optional<SkColor>();
    AppId app_id = InstallWebApp(std::move(web_app_info));
    Browser* app_browser = LaunchWebAppBrowser(app_id);

    EXPECT_EQ(GetAppIdFromApplicationName(app_browser->app_name()), app_id);
    EXPECT_EQ(base::nullopt, app_browser->app_controller()->GetThemeColor());
  }
}

// This tests that we don't crash when launching a PWA window with an
// autogenerated user theme set.
IN_PROC_BROWSER_TEST_P(WebAppBrowserTest, AutoGeneratedUserThemeCrash) {
  ThemeServiceFactory::GetForProfile(browser()->profile())
      ->BuildAutogeneratedThemeFromColor(SK_ColorBLUE);

  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->app_url = GURL(kExampleURL);
  AppId app_id = InstallWebApp(std::move(web_app_info));

  LaunchWebAppBrowser(app_id);
}

IN_PROC_BROWSER_TEST_P(WebAppBrowserTest, HasMinimalUiButtons) {
  int index = 0;
  auto has_buttons = [this, &index](DisplayMode display_mode,
                                    bool open_as_window) -> bool {
    base::HistogramTester tester;
    const std::string base_url = "https://example.com/path";
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->app_url = GURL(base_url + base::NumberToString(index++));
    web_app_info->scope = web_app_info->app_url;
    web_app_info->display_mode = display_mode;
    web_app_info->open_as_window = open_as_window;
    AppId app_id = InstallWebApp(std::move(web_app_info));
    Browser* app_browser = LaunchWebAppBrowser(app_id);
    tester.ExpectUniqueSample(kLaunchWebAppDisplayModeHistogram, display_mode,
                              1);

    return app_browser->app_controller()->HasMinimalUiButtons();
  };

  EXPECT_TRUE(has_buttons(DisplayMode::kMinimalUi,
                          /*open_as_window=*/true));
  EXPECT_FALSE(has_buttons(DisplayMode::kStandalone,
                           /*open_as_window=*/true));
  EXPECT_FALSE(has_buttons(DisplayMode::kMinimalUi,
                           /*open_as_window=*/false));
}

// Tests that desktop PWAs open links in the browser.
IN_PROC_BROWSER_TEST_P(WebAppBrowserTest, DesktopPWAsOpenLinksInApp) {
  ASSERT_TRUE(https_server()->Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL app_url = GetSecureAppURL();
  const AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);
  NavigateToURLAndWait(app_browser, app_url);
  ASSERT_TRUE(app_browser->app_controller());
  NavigateAndCheckForToolbar(app_browser, GURL(kExampleURL), true);
}

// Tests that desktop PWAs are opened at the correct size.
IN_PROC_BROWSER_TEST_P(WebAppBrowserTest, PWASizeIsCorrectlyRestored) {
  ASSERT_TRUE(https_server()->Start());

  const GURL app_url = GetSecureAppURL();
  const AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);

  EXPECT_TRUE(AppBrowserController::IsForWebAppBrowser(app_browser));
  NavigateToURLAndWait(app_browser, app_url);

  const gfx::Rect bounds = gfx::Rect(50, 50, 500, 500);
  app_browser->window()->SetBounds(bounds);
  app_browser->window()->Close();

  Browser* const new_browser = LaunchWebAppBrowser(app_id);
  EXPECT_EQ(new_browser->window()->GetBounds(), bounds);
}

// Tests that desktop PWAs are reopened at the correct size.
IN_PROC_BROWSER_TEST_P(WebAppTabRestoreBrowserTest,
                       ReopenedPWASizeIsCorrectlyRestored) {
  ASSERT_TRUE(https_server()->Start());

  const GURL app_url = GetSecureAppURL();
  const AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);

  EXPECT_TRUE(AppBrowserController::IsForWebAppBrowser(app_browser));
  NavigateToURLAndWait(app_browser, app_url);

  const gfx::Rect bounds = gfx::Rect(50, 50, 500, 500);
  app_browser->window()->SetBounds(bounds);
  app_browser->window()->Close();

  content::WebContentsAddedObserver new_contents_observer;

  sessions::TabRestoreService* const service =
      TabRestoreServiceFactory::GetForProfile(profile());
  ASSERT_GT(service->entries().size(), 0U);
  service->RestoreMostRecentEntry(nullptr);

  content::WebContents* const restored_web_contents =
      new_contents_observer.GetWebContents();
  Browser* const restored_browser =
      chrome::FindBrowserWithWebContents(restored_web_contents);
  EXPECT_EQ(restored_browser->window()->GetBounds(), bounds);
}

// Tests that using window.open to create a popup window out of scope results in
// a correctly sized window.
IN_PROC_BROWSER_TEST_P(WebAppBrowserTest, OffScopePWAPopupsHaveCorrectSize) {
  ASSERT_TRUE(https_server()->Start());

  const GURL app_url = GetSecureAppURL();
  const AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowser(app_id);

  EXPECT_TRUE(AppBrowserController::IsForWebAppBrowser(app_browser));

  const GURL offscope_url("https://example.com");
  const gfx::Size size(500, 500);

  Browser* const popup_browser =
      OpenPopupAndWait(app_browser, offscope_url, size);

  // The navigation should have happened in a new window.
  EXPECT_NE(popup_browser, app_browser);

  // The popup browser should be a PWA.
  EXPECT_TRUE(AppBrowserController::IsForWebAppBrowser(popup_browser));

  // Toolbar should be shown, as the popup is out of scope.
  EXPECT_TRUE(popup_browser->app_controller()->ShouldShowCustomTabBar());

  // Skip animating the toolbar visibility.
  popup_browser->app_controller()->UpdateCustomTabBarVisibility(false);

  // The popup window should be the size we specified.
  EXPECT_EQ(size, popup_browser->window()->GetContentsSize());
}

// Tests that using window.open to create a popup window in scope results in
// a correctly sized window.
IN_PROC_BROWSER_TEST_P(WebAppBrowserTest, InScopePWAPopupsHaveCorrectSize) {
  ASSERT_TRUE(https_server()->Start());

  const GURL app_url = GetSecureAppURL();
  const AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowser(app_id);

  EXPECT_TRUE(AppBrowserController::IsForWebAppBrowser(app_browser));

  const gfx::Size size(500, 500);
  Browser* const popup_browser = OpenPopupAndWait(app_browser, app_url, size);

  // The navigation should have happened in a new window.
  EXPECT_NE(popup_browser, app_browser);

  // The popup browser should be a PWA.
  EXPECT_TRUE(AppBrowserController::IsForWebAppBrowser(popup_browser));

  // Toolbar should not be shown, as the popup is in scope.
  EXPECT_FALSE(popup_browser->app_controller()->ShouldShowCustomTabBar());

  // Skip animating the toolbar visibility.
  popup_browser->app_controller()->UpdateCustomTabBarVisibility(false);

  // The popup window should be the size we specified.
  EXPECT_EQ(size, popup_browser->window()->GetContentsSize());
}

// Tests that app windows are correctly restored.
IN_PROC_BROWSER_TEST_P(WebAppTabRestoreBrowserTest, RestoreAppWindow) {
  ASSERT_TRUE(https_server()->Start());

  const GURL app_url = GetSecureAppURL();
  const AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);

  ASSERT_TRUE(app_browser->is_type_app());
  app_browser->window()->Close();

  content::WebContentsAddedObserver new_contents_observer;

  sessions::TabRestoreService* const service =
      TabRestoreServiceFactory::GetForProfile(profile());
  service->RestoreMostRecentEntry(nullptr);

  content::WebContents* const restored_web_contents =
      new_contents_observer.GetWebContents();
  Browser* const restored_browser =
      chrome::FindBrowserWithWebContents(restored_web_contents);

  EXPECT_TRUE(restored_browser->is_type_app());
}

// Test navigating to an out of scope url on the same origin causes the url
// to be shown to the user.
IN_PROC_BROWSER_TEST_P(WebAppBrowserTest,
                       LocationBarIsVisibleOffScopeOnSameOrigin) {
  ASSERT_TRUE(https_server()->Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL app_url = GetSecureAppURL();
  const AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);

  // Toolbar should not be visible in the app.
  ASSERT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());

  // The installed PWA's scope is app.com:{PORT}/ssl,
  // so app.com:{PORT}/accessibility_fail.html is out of scope.
  const GURL out_of_scope = GetURLForPath("/accessibility_fail.html");
  NavigateToURLAndWait(app_browser, out_of_scope);

  // Location should be visible off scope.
  ASSERT_TRUE(app_browser->app_controller()->ShouldShowCustomTabBar());
}

IN_PROC_BROWSER_TEST_P(WebAppBrowserTest, OverscrollEnabled) {
  ASSERT_TRUE(https_server()->Start());

  const GURL app_url = GetSecureAppURL();
  const AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);

  // Overscroll is only enabled on Aura platforms currently.
#if defined(USE_AURA)
  EXPECT_TRUE(app_browser->CanOverscrollContent());
#else
  EXPECT_FALSE(app_browser->CanOverscrollContent());
#endif
}

// Check the 'Copy URL' menu button for Hosted App windows.
IN_PROC_BROWSER_TEST_P(WebAppBrowserTest, CopyURL) {
  const GURL app_url(kExampleURL);
  const AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);

  content::BrowserTestClipboardScope test_clipboard_scope;
  chrome::ExecuteCommand(app_browser, IDC_COPY_URL);

  ui::Clipboard* const clipboard = ui::Clipboard::GetForCurrentThread();
  base::string16 result;
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, &result);
  EXPECT_EQ(result, base::UTF8ToUTF16(kExampleURL));
}

// Tests that the command for popping a tab out to a PWA window is disabled in
// incognito.
IN_PROC_BROWSER_TEST_P(WebAppBrowserTest, PopOutDisabledInIncognito) {
  ASSERT_TRUE(https_server()->Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL app_url = GetSecureAppURL();
  const AppId app_id = InstallPWA(app_url);

  Browser* const incognito_browser = OpenURLOffTheRecord(profile(), app_url);
  auto app_menu_model =
      std::make_unique<AppMenuModel>(nullptr, incognito_browser);
  app_menu_model->Init();
  ui::MenuModel* model = app_menu_model.get();
  int index = -1;
  ASSERT_TRUE(app_menu_model->GetModelAndIndexForCommandId(
      IDC_OPEN_IN_PWA_WINDOW, &model, &index));
  EXPECT_FALSE(model->IsEnabledAt(index));
}

// Tests that PWA menus have an uninstall option.
IN_PROC_BROWSER_TEST_P(WebAppBrowserTest, UninstallMenuOption) {
  ASSERT_TRUE(https_server()->Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL app_url = GetSecureAppURL();
  const AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);

  auto app_menu_model = std::make_unique<WebAppMenuModel>(nullptr, app_browser);
  app_menu_model->Init();
  ui::MenuModel* model = app_menu_model.get();
  int index = -1;
  const bool found = app_menu_model->GetModelAndIndexForCommandId(
      WebAppMenuModel::kUninstallAppCommandId, &model, &index);
#if defined(OS_CHROMEOS)
  EXPECT_FALSE(found);
#else
  EXPECT_TRUE(found);
  EXPECT_TRUE(model->IsEnabledAt(index));
#endif  // defined(OS_CHROMEOS)
}

// Tests that both installing a PWA and creating a shortcut app are disabled for
// incognito windows.
IN_PROC_BROWSER_TEST_P(WebAppBrowserTest, ShortcutMenuOptionsInIncognito) {
  ASSERT_TRUE(https_server()->Start());

  Browser* const incognito_browser = CreateIncognitoBrowser(profile());
  EXPECT_FALSE(NavigateAndAwaitInstallabilityCheck(incognito_browser,
                                                   GetSecureAppURL()));

  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, incognito_browser),
            kDisabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, incognito_browser),
            kNotPresent);
}

// Tests that both installing a PWA and creating a shortcut app are available
// for an installable PWA.
IN_PROC_BROWSER_TEST_P(WebAppBrowserTest,
                       ShortcutMenuOptionsForInstallablePWA) {
  ASSERT_TRUE(https_server()->Start());

  EXPECT_TRUE(
      NavigateAndAwaitInstallabilityCheck(browser(), GetInstallableAppURL()));

  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, browser()), kEnabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, browser()), kEnabled);
}

// Tests that an installed PWA is not used when out of scope by one path level.
IN_PROC_BROWSER_TEST_P(WebAppBrowserTest, MenuOptionsOutsideInstalledPwaScope) {
  ASSERT_TRUE(https_server()->Start());

  NavigateToURLAndWait(
      browser(),
      https_server()->GetURL("/banners/scope_is_start_url/index.html"));
  InstallPwaForCurrentUrl();

  // Open a page that is one directory up from the installed PWA.
  Browser* const new_browser = NavigateInNewWindowAndAwaitInstallabilityCheck(
      https_server()->GetURL("/banners/no_manifest_test_page.html"));

  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, new_browser), kEnabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, new_browser), kNotPresent);
  EXPECT_EQ(GetAppMenuCommandState(IDC_OPEN_IN_PWA_WINDOW, new_browser),
            kNotPresent);
}

IN_PROC_BROWSER_TEST_P(WebAppBrowserTest, InstallInstallableSite) {
  base::UserActionTester user_action_tester;
  ASSERT_TRUE(https_server()->Start());
  NavigateToURLAndWait(browser(), GetInstallableAppURL());

  const AppId app_id = InstallPwaForCurrentUrl();
  auto* provider = WebAppProviderBase::GetProviderBase(profile());
  EXPECT_EQ(provider->registrar().GetAppShortName(app_id),
            GetInstallableAppName());

  // Installed PWAs should launch in their own window.
  EXPECT_EQ(provider->registrar().GetAppUserDisplayMode(app_id),
            blink::mojom::DisplayMode::kStandalone);

  EXPECT_EQ(1, user_action_tester.GetActionCount("InstallWebAppFromMenu"));
  EXPECT_EQ(0, user_action_tester.GetActionCount("CreateShortcut"));
}

IN_PROC_BROWSER_TEST_P(WebAppBrowserTest, CanInstallOverTabPwa) {
  ASSERT_TRUE(https_server()->Start());

  NavigateToURLAndWait(browser(), GetInstallableAppURL());
  const AppId app_id = InstallPwaForCurrentUrl();

  // Change display mode to open in tab.
  auto* provider = WebAppProviderBase::GetProviderBase(profile());
  provider->registry_controller().SetAppUserDisplayMode(
      app_id, blink::mojom::DisplayMode::kBrowser);

  Browser* const new_browser =
      NavigateInNewWindowAndAwaitInstallabilityCheck(GetInstallableAppURL());

  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, new_browser), kEnabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, new_browser), kEnabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_OPEN_IN_PWA_WINDOW, new_browser),
            kNotPresent);
}

IN_PROC_BROWSER_TEST_P(WebAppBrowserTest, CannotInstallOverWindowPwa) {
  ASSERT_TRUE(https_server()->Start());

  NavigateToURLAndWait(browser(), GetInstallableAppURL());
  InstallPwaForCurrentUrl();

  // Avoid any interference if active browser was changed by PWA install.
  Browser* const new_browser =
      NavigateInNewWindowAndAwaitInstallabilityCheck(GetInstallableAppURL());

  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, new_browser), kEnabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, new_browser), kNotPresent);
  EXPECT_EQ(GetAppMenuCommandState(IDC_OPEN_IN_PWA_WINDOW, new_browser),
            kEnabled);
}

IN_PROC_BROWSER_TEST_P(WebAppBrowserTest, CannotInstallOverPolicyPwa) {
  ASSERT_TRUE(https_server()->Start());

  ExternalInstallOptions options = CreateInstallOptions(GetInstallableAppURL());
  options.install_source = ExternalInstallSource::kExternalPolicy;
  PendingAppManagerInstall(profile(), options);

  // Avoid any interference if active browser was changed by PWA install.
  Browser* const new_browser =
      NavigateInNewWindowAndAwaitInstallabilityCheck(GetInstallableAppURL());

  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, new_browser),
            kDisabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, new_browser), kNotPresent);
  EXPECT_EQ(GetAppMenuCommandState(IDC_OPEN_IN_PWA_WINDOW, new_browser),
            kEnabled);
}

// Tests that the command for OpenActiveTabInPwaWindow is available for secure
// pages in an app's scope.
IN_PROC_BROWSER_TEST_P(WebAppBrowserTest, ReparentWebAppForSecureActiveTab) {
  ASSERT_TRUE(https_server()->Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL app_url = GetSecureAppURL();
  const AppId app_id = InstallPWA(app_url);

  NavigateToURLAndWait(browser(), app_url);
  content::WebContents* tab_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(tab_contents->GetLastCommittedURL(), app_url);

  EXPECT_EQ(GetAppMenuCommandState(IDC_OPEN_IN_PWA_WINDOW, browser()),
            kEnabled);

  Browser* const app_browser = ReparentWebAppForSecureActiveTab(browser());
  ASSERT_EQ(app_browser->app_controller()->GetAppId(), app_id);
}

// Tests that reparenting the last browser tab doesn't close the browser window.
IN_PROC_BROWSER_TEST_P(WebAppBrowserTest, ReparentLastBrowserTab) {
  ASSERT_TRUE(https_server()->Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL app_url = GetSecureAppURL();
  const AppId app_id = InstallPWA(app_url);
  NavigateToURLAndWait(browser(), app_url);

  Browser* const app_browser = ReparentWebAppForSecureActiveTab(browser());
  ASSERT_EQ(app_browser->app_controller()->GetAppId(), app_id);

  ASSERT_TRUE(IsBrowserOpen(browser()));
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
}

// Tests that the manifest name of the current installable site is used in the
// installation menu text.
IN_PROC_BROWSER_TEST_P(WebAppBrowserTest, InstallToShelfContainsAppName) {
  ASSERT_TRUE(https_server()->Start());

  EXPECT_TRUE(
      NavigateAndAwaitInstallabilityCheck(browser(), GetInstallableAppURL()));

  auto app_menu_model = std::make_unique<AppMenuModel>(nullptr, browser());
  app_menu_model->Init();
  ui::MenuModel* model = app_menu_model.get();
  int index = -1;
  EXPECT_TRUE(app_menu_model->GetModelAndIndexForCommandId(IDC_INSTALL_PWA,
                                                           &model, &index));
  EXPECT_EQ(app_menu_model.get(), model);
  EXPECT_EQ(model->GetLabelAt(index),
            base::UTF8ToUTF16("Install Manifest test app\xE2\x80\xA6"));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebAppBrowserTest,
    ::testing::Values(ControllerType::kHostedAppController,
                      ControllerType::kUnifiedControllerWithBookmarkApp,
                      ControllerType::kUnifiedControllerWithWebApp),
    ControllerTypeParamToString);

INSTANTIATE_TEST_SUITE_P(
    All,
    WebAppTabRestoreBrowserTest,
    ::testing::Values(ControllerType::kHostedAppController,
                      ControllerType::kUnifiedControllerWithBookmarkApp,
                      ControllerType::kUnifiedControllerWithWebApp),
    ControllerTypeParamToString);

}  // namespace web_app
