// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/hosted_app_browser_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/installable/installable_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window_state.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_manager.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/api/url_handlers/url_handlers_parser.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/extensions/manifest_handlers/app_theme_color_info.h"
#include "components/security_state/core/security_state.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/web_preferences.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace extensions {

namespace {

// Gets the icon to use if the extension app icon is not available.
gfx::ImageSkia GetFallbackAppIcon(Browser* browser) {
  gfx::ImageSkia page_icon = browser->GetCurrentPageIcon().AsImageSkia();
  if (!page_icon.isNull())
    return page_icon;

  // The extension icon may be loading still. Return a transparent icon rather
  // than using a placeholder to avoid flickering.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(gfx::kFaviconSize, gfx::kFaviconSize);
  bitmap.eraseColor(SK_ColorTRANSPARENT);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

// Returns true if |app_url| and |page_url| are the same origin. To avoid
// breaking Hosted Apps and Bookmark Apps that might redirect to sites in the
// same domain but with "www.", this returns true if |page_url| is secure and in
// the same origin as |app_url| with "www.".
bool IsSameHostAndPort(const GURL& app_url, const GURL& page_url) {
  return (app_url.host_piece() == page_url.host_piece() ||
          std::string("www.") + app_url.host() == page_url.host_piece()) &&
         app_url.port() == page_url.port();
}

}  // namespace

// static
void HostedAppBrowserController::SetAppPrefsForWebContents(
    web_app::AppBrowserController* controller,
    content::WebContents* web_contents) {
  auto* rvh = web_contents->GetRenderViewHost();

  web_contents->GetMutableRendererPrefs()->can_accept_load_drops = false;
  rvh->SyncRendererPrefs();

  if (!controller)
    return;

  // All hosted apps should specify an app ID.
  DCHECK(controller->GetAppId());
  extensions::TabHelper::FromWebContents(web_contents)
      ->SetExtensionApp(ExtensionRegistry::Get(controller->browser()->profile())
                            ->GetExtensionById(*controller->GetAppId(),
                                               ExtensionRegistry::EVERYTHING));

  web_contents->NotifyPreferencesChanged();
}

// static
void HostedAppBrowserController::ClearAppPrefsForWebContents(
    content::WebContents* web_contents) {
  auto* rvh = web_contents->GetRenderViewHost();

  web_contents->GetMutableRendererPrefs()->can_accept_load_drops = true;
  rvh->SyncRendererPrefs();

  extensions::TabHelper::FromWebContents(web_contents)
      ->SetExtensionApp(nullptr);

  web_contents->NotifyPreferencesChanged();
}

HostedAppBrowserController::HostedAppBrowserController(Browser* browser)
    : AppBrowserController(browser),
      extension_id_(web_app::GetAppIdFromApplicationName(browser->app_name())),
      // If a bookmark app has a URL handler, then it is a PWA.
      // TODO(https://crbug.com/774918): Replace once there is a more explicit
      // indicator of a Bookmark App for an installable website.
      created_for_installed_pwa_(
          UrlHandlers::GetUrlHandlers(GetExtension())) {}

HostedAppBrowserController::~HostedAppBrowserController() = default;

base::Optional<std::string> HostedAppBrowserController::GetAppId() const {
  return extension_id_;
}

bool HostedAppBrowserController::CreatedForInstalledPwa() const {
  return created_for_installed_pwa_;
}

bool HostedAppBrowserController::IsHostedApp() const {
  return true;
}

bool HostedAppBrowserController::ShouldShowToolbar() const {
  const Extension* extension = GetExtension();
  if (!extension)
    return false;

  DCHECK(extension->is_hosted_app());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  if (!web_contents)
    return false;

  GURL launch_url = AppLaunchInfo::GetLaunchWebURL(extension);
  base::StringPiece launch_scheme = launch_url.scheme_piece();

  bool is_internal_launch_scheme = launch_scheme == kExtensionScheme ||
                                   launch_scheme == content::kChromeUIScheme;

  // The current page must be secure for us to hide the toolbar. However,
  // the chrome-extension:// and chrome:// launch URL apps can hide the toolbar,
  // if the current WebContents URLs are the same as the launch scheme.
  //
  // Note that the launch scheme may be insecure, but as long as the current
  // page's scheme is secure, we can hide the toolbar.
  base::StringPiece secure_page_scheme =
      is_internal_launch_scheme ? launch_scheme : url::kHttpsScheme;

  auto should_show_toolbar_for_url = [&](const GURL& url) -> bool {
    // If the url is unset, it doesn't give a signal as to whether the toolbar
    // should be shown or not. In lieu of more information, do not show the
    // toolbar.
    if (url.is_empty())
      return false;

    // Page URLs that are not within scope
    // (https://www.w3.org/TR/appmanifest/#dfn-within-scope) of the app
    // corresponding to |launch_url| show the toolbar.
    bool out_of_scope = !IsUrlInAppScope(url);

    if (url.scheme_piece() != secure_page_scheme) {
      // Some origins are (such as localhost) are considered secure even when
      // served over non-secure schemes. However, in order to hide the toolbar,
      // the 'considered secure' origin must also be in the app's scope.
      return out_of_scope || !InstallableManager::IsOriginConsideredSecure(url);
    }

    if (IsForSystemWebApp()) {
      DCHECK_EQ(url.scheme_piece(), content::kChromeUIScheme);
      return false;
    }

    return out_of_scope;
  };

  GURL visible_url = web_contents->GetVisibleURL();
  GURL last_committed_url = web_contents->GetLastCommittedURL();

  if (last_committed_url.is_empty() && visible_url.is_empty())
    return should_show_toolbar_for_url(initial_url());

  if (should_show_toolbar_for_url(visible_url) ||
      should_show_toolbar_for_url(last_committed_url)) {
    return true;
  }

  // Insecure external web sites show the toolbar.
  // Note: IsContentSecure is false until a navigation is committed.
  if (!last_committed_url.is_empty() && !is_internal_launch_scheme &&
      !InstallableManager::IsContentSecure(web_contents)) {
    return true;
  }

  return false;
}

bool HostedAppBrowserController::ShouldShowHostedAppButtonContainer() const {
  // System Web Apps don't get the Hosted App buttons.
  return IsForWebAppBrowser(browser()) && !IsForSystemWebApp();
}

gfx::ImageSkia HostedAppBrowserController::GetWindowAppIcon() const {
  // TODO(calamity): Use the app name to retrieve the app icon without using the
  // extensions tab helper to make icon load more immediate.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  if (!contents)
    return GetFallbackAppIcon(browser());

  extensions::TabHelper* extensions_tab_helper =
      extensions::TabHelper::FromWebContents(contents);
  if (!extensions_tab_helper)
    return GetFallbackAppIcon(browser());

  const SkBitmap* icon_bitmap = extensions_tab_helper->GetExtensionAppIcon();
  if (!icon_bitmap)
    return GetFallbackAppIcon(browser());

  return gfx::ImageSkia::CreateFrom1xBitmap(*icon_bitmap);
}

gfx::ImageSkia HostedAppBrowserController::GetWindowIcon() const {
  if (IsForWebAppBrowser(browser()))
    return GetWindowAppIcon();

  return browser()->GetCurrentPageIcon().AsImageSkia();
}

base::Optional<SkColor> HostedAppBrowserController::GetThemeColor() const {
  base::Optional<SkColor> web_theme_color =
      AppBrowserController::GetThemeColor();
  if (web_theme_color)
    return web_theme_color;

  const Extension* extension = GetExtension();
  if (!extension)
    return base::nullopt;

  base::Optional<SkColor> extension_theme_color =
      AppThemeColorInfo::GetThemeColor(extension);
  if (extension_theme_color)
    return SkColorSetA(*extension_theme_color, SK_AlphaOPAQUE);

  return base::nullopt;
}

base::string16 HostedAppBrowserController::GetTitle() const {
  // When showing the toolbar, display the name of the app, instead of the
  // current page as the title.
  if (ShouldShowToolbar()) {
    const Extension* extension = GetExtension();
    return base::UTF8ToUTF16(extension->name());
  }

  return AppBrowserController::GetTitle();
}

GURL HostedAppBrowserController::GetAppLaunchURL() const {
  const Extension* extension = GetExtension();
  if (!extension)
    return GURL();

  return AppLaunchInfo::GetLaunchWebURL(extension);
}

bool HostedAppBrowserController::IsUrlInAppScope(const GURL& url) const {
  const Extension* extension = GetExtension();
  const std::vector<UrlHandlerInfo>* url_handlers =
      UrlHandlers::GetUrlHandlers(extension);

  // We don't have a scope, fall back to same origin check.
  if (!url_handlers)
    return IsSameHostAndPort(GetAppLaunchURL(), url);

  return UrlHandlers::CanBookmarkAppHandleUrl(extension, url);
}

const Extension* HostedAppBrowserController::GetExtension() const {
  return ExtensionRegistry::Get(browser()->profile())
      ->GetExtensionById(extension_id_, ExtensionRegistry::EVERYTHING);
}

const Extension* HostedAppBrowserController::GetExtensionForTesting() const {
  return GetExtension();
}

std::string HostedAppBrowserController::GetAppShortName() const {
  const Extension* extension = GetExtension();
  return extension ? extension->short_name() : std::string();
}

base::string16 HostedAppBrowserController::GetFormattedUrlOrigin() const {
  return FormatUrlOrigin(AppLaunchInfo::GetLaunchWebURL(GetExtension()));
}

bool HostedAppBrowserController::CanUninstall() const {
  return web_app::WebAppProvider::Get(browser()->profile())
      ->ui_manager()
      .dialog_manager()
      .CanUninstallWebApp(extension_id_);
}

void HostedAppBrowserController::Uninstall() {
  web_app::WebAppProvider::Get(browser()->profile())
      ->ui_manager()
      .dialog_manager()
      .UninstallWebApp(extension_id_,
                       web_app::WebAppDialogManager::UninstallSource::kAppMenu,
                       browser()->window(), base::DoNothing());
}

bool HostedAppBrowserController::IsInstalled() const {
  return GetExtension();
}

void HostedAppBrowserController::OnReceivedInitialURL() {
  UpdateToolbarVisibility(false);

  // If the window bounds have not been overridden, there is no need to resize
  // the window.
  if (!browser()->bounds_overridden())
    return;

  // The saved bounds will only be wrong if they are content bounds.
  if (!chrome::SavedBoundsAreContentBounds(browser()))
    return;

  // TODO(crbug.com/964825): Correctly set the window size at creation time.
  // This is currently not possible because the current url is not easily known
  // at popup construction time.
  browser()->window()->SetContentsSize(browser()->override_bounds().size());
}

void HostedAppBrowserController::OnTabInserted(content::WebContents* contents) {
  AppBrowserController::OnTabInserted(contents);
  extensions::HostedAppBrowserController::SetAppPrefsForWebContents(this,
                                                                    contents);
}

void HostedAppBrowserController::OnTabRemoved(content::WebContents* contents) {
  AppBrowserController::OnTabRemoved(contents);
  extensions::HostedAppBrowserController::ClearAppPrefsForWebContents(contents);
}

}  // namespace extensions
