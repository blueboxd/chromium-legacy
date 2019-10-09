// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/app_browser_controller.h"

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/hosted_app_browser_controller.h"
#include "chrome/browser/ui/manifest_web_app_browser_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/ui/web_applications/web_app_browser_controller.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "components/security_state/core/security_state.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "net/base/escape.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/favicon_size.h"
#include "url/gurl.h"

namespace web_app {

// static
std::unique_ptr<web_app::AppBrowserController>
AppBrowserController::MaybeCreateWebAppController(Browser* browser) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  const AppId app_id =
      web_app::GetAppIdFromApplicationName(browser->app_name());
  if (base::FeatureList::IsEnabled(features::kDesktopPWAsWithoutExtensions)) {
    auto* provider = web_app::WebAppProvider::Get(browser->profile());
    if (provider && provider->registrar().IsInstalled(app_id))
      return std::make_unique<web_app::WebAppBrowserController>(browser);
  }
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(browser->profile())
          ->GetExtensionById(app_id, extensions::ExtensionRegistry::EVERYTHING);
  if (extension && extension->is_hosted_app()) {
    if (base::FeatureList::IsEnabled(
            features::kDesktopPWAsUnifiedUiController) &&
        extension->from_bookmark()) {
      return std::make_unique<web_app::WebAppBrowserController>(browser);
    }
    return std::make_unique<extensions::HostedAppBrowserController>(browser);
  }
#endif
  if (browser->is_focus_mode())
    return std::make_unique<ManifestWebAppBrowserController>(browser);
  return nullptr;
}

// static
bool AppBrowserController::IsForWebAppBrowser(const Browser* browser) {
  return browser && browser->app_controller();
}

// static
base::string16 AppBrowserController::FormatUrlOrigin(const GURL& url) {
  return url_formatter::FormatUrl(
      url.GetOrigin(),
      url_formatter::kFormatUrlOmitUsernamePassword |
          url_formatter::kFormatUrlOmitHTTPS |
          url_formatter::kFormatUrlOmitHTTP |
          url_formatter::kFormatUrlOmitTrailingSlashOnBareHostname |
          url_formatter::kFormatUrlOmitTrivialSubdomains,
      net::UnescapeRule::SPACES, nullptr, nullptr, nullptr);
}

AppBrowserController::AppBrowserController(Browser* browser)
    : content::WebContentsObserver(nullptr), browser_(browser) {
  browser->tab_strip_model()->AddObserver(this);
}

AppBrowserController::~AppBrowserController() {
  browser()->tab_strip_model()->RemoveObserver(this);
}

bool AppBrowserController::CreatedForInstalledPwa() const {
  return false;
}

bool AppBrowserController::HasTabStrip() const {
  // Show tabs for Terminal only.
  // TODO(crbug.com/846546): Generalise this as a SystemWebApp capability.
  return GetAppIdForSystemWebApp(browser()->profile(),
                                 SystemAppType::TERMINAL) == GetAppId();
}

bool AppBrowserController::HasTitlebarToolbar() const {
  // Show titlebar toolbar for Terminal System App, but not other system apps.
  // TODO(crbug.com/846546): Generalise this as a SystemWebApp capability.
  if (IsForSystemWebApp()) {
    return GetAppIdForSystemWebApp(browser()->profile(),
                                   SystemAppType::TERMINAL) == GetAppId();
  }
  // Show for all other apps.
  return true;
}

bool AppBrowserController::HasTitlebarAppOriginText() const {
  // Do not show origin text for System Apps.
  return !IsForSystemWebApp();
}

bool AppBrowserController::HasTitlebarContentSettings() const {
  // Do not show content settings for System Apps.
  return !IsForSystemWebApp();
}

#if defined(OS_CHROMEOS)
bool AppBrowserController::UseTitlebarTerminalSystemAppMenu() const {
  // Use the Terminal System App Menu for Terminal System App only.
  // TODO(crbug.com/846546): Generalise this as a SystemWebApp capability.
  if (IsForSystemWebApp()) {
    return GetAppIdForSystemWebApp(browser()->profile(),
                                   SystemAppType::TERMINAL) == GetAppId();
  }
  return false;
}
#endif

bool AppBrowserController::IsInstalled() const {
  return false;
}

bool AppBrowserController::IsHostedApp() const {
  return false;
}

web_app::WebAppBrowserController*
AppBrowserController::AsWebAppBrowserController() {
  return nullptr;
}

bool AppBrowserController::CanUninstall() const {
  return false;
}

void AppBrowserController::Uninstall() {
  NOTREACHED();
  return;
}

void AppBrowserController::UpdateCustomTabBarVisibility(bool animate) const {
  browser()->window()->UpdateCustomTabBarVisibility(ShouldShowCustomTabBar(),
                                                    animate);
}

bool AppBrowserController::IsForSystemWebApp() const {
  if (!GetAppId())
    return false;

  return web_app::WebAppProvider::Get(browser()->profile())
      ->system_web_app_manager()
      .IsSystemWebApp(*GetAppId());
}

void AppBrowserController::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!initial_url().is_empty())
    return;
  if (!navigation_handle->IsInMainFrame())
    return;
  if (navigation_handle->GetURL().is_empty())
    return;
  SetInitialURL(navigation_handle->GetURL());
}

void AppBrowserController::DidChangeThemeColor(
    base::Optional<SkColor> theme_color) {
  browser_->window()->UpdateFrameColor();
}

base::Optional<SkColor> AppBrowserController::GetThemeColor() const {
  base::Optional<SkColor> result;
  // HTML meta theme-color tag overrides manifest theme_color, see spec:
  // https://www.w3.org/TR/appmanifest/#theme_color-member
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  if (web_contents) {
    base::Optional<SkColor> color = web_contents->GetThemeColor();
    if (color)
      result = color;
  }

  if (!result)
    return base::nullopt;

  // The frame/tabstrip code expects an opaque color.
  return SkColorSetA(*result, SK_AlphaOPAQUE);
}

base::string16 AppBrowserController::GetTitle() const {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return base::string16();

  content::NavigationEntry* entry =
      web_contents->GetController().GetVisibleEntry();
  return entry ? entry->GetTitle() : base::string16();
}

void AppBrowserController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (selection.active_tab_changed()) {
    content::WebContentsObserver::Observe(selection.new_contents);
    DidChangeThemeColor(GetThemeColor());
  }
  if (change.type() == TabStripModelChange::kInserted) {
    for (const auto& contents : change.GetInsert()->contents)
      OnTabInserted(contents.contents);
  } else if (change.type() == TabStripModelChange::kRemoved) {
    for (const auto& contents : change.GetRemove()->contents)
      OnTabRemoved(contents.contents);
    // WebContents should be null when the last tab is closed.
    DCHECK_EQ(web_contents() == nullptr, tab_strip_model->empty());
  }
}

void AppBrowserController::OnTabInserted(content::WebContents* contents) {
  if (!contents->GetVisibleURL().is_empty() && initial_url_.is_empty())
    SetInitialURL(contents->GetVisibleURL());
}

void AppBrowserController::OnTabRemoved(content::WebContents* contents) {}

gfx::ImageSkia AppBrowserController::GetFallbackAppIcon() const {
  gfx::ImageSkia page_icon = browser()->GetCurrentPageIcon().AsImageSkia();
  if (!page_icon.isNull())
    return page_icon;

  // The icon may be loading still. Return a transparent icon rather
  // than using a placeholder to avoid flickering.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(gfx::kFaviconSize, gfx::kFaviconSize);
  bitmap.eraseColor(SK_ColorTRANSPARENT);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

void AppBrowserController::SetInitialURL(const GURL& initial_url) {
  DCHECK(initial_url_.is_empty());
  initial_url_ = initial_url;

  OnReceivedInitialURL();
}

}  // namespace web_app
