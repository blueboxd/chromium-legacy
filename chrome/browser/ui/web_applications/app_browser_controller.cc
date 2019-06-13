// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/app_browser_controller.h"

#include "base/feature_list.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "components/security_state/core/security_state.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/base/escape.h"
#include "url/gurl.h"

namespace web_app {

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

bool AppBrowserController::IsInstalled() const {
  return false;
}

bool AppBrowserController::IsHostedApp() const {
  return false;
}

bool AppBrowserController::CanUninstall() const {
  return false;
}

void AppBrowserController::Uninstall() {
  NOTREACHED();
  return;
}

void AppBrowserController::UpdateToolbarVisibility(bool animate) const {
  browser()->window()->UpdateToolbarVisibility(ShouldShowToolbar(), animate);
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
  if (change.type() == TabStripModelChange::kInserted) {
    for (const auto& contents : change.GetInsert()->contents)
      OnTabInserted(contents.contents);
  } else if (change.type() == TabStripModelChange::kRemoved) {
    for (const auto& contents : change.GetRemove()->contents)
      OnTabRemoved(contents.contents);
  }
}

void AppBrowserController::OnTabInserted(content::WebContents* contents) {
  DCHECK(!web_contents()) << " App windows are single tabbed only";
  content::WebContentsObserver::Observe(contents);
  DidChangeThemeColor(GetThemeColor());

  if (!contents->GetVisibleURL().is_empty())
    SetInitialURL(contents->GetVisibleURL());
}

void AppBrowserController::OnTabRemoved(content::WebContents* contents) {
  DCHECK_EQ(contents, web_contents());
  content::WebContentsObserver::Observe(nullptr);
}

void AppBrowserController::SetInitialURL(const GURL& initial_url) {
  DCHECK(initial_url_.is_empty());
  initial_url_ = initial_url;

  OnReceivedInitialURL();
}

}  // namespace web_app
