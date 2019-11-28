// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limits/web_time_limit_navigation_throttle.h"

#include <string>

#include "base/optional.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/child_accounts/child_user_service.h"
#include "chrome/browser/chromeos/child_accounts/child_user_service_factory.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/web_time_limit_error_page/web_time_limit_error_page.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace chromeos {

namespace {

bool IsWebBlocked(content::BrowserContext* context) {
  auto* child_user_service =
      ChildUserServiceFactory::GetForBrowserContext(context);
  if (!child_user_service)
    return false;

  return child_user_service->WebTimeLimitReached();
}

bool IsURLWhitelisted(const GURL& url, content::BrowserContext* context) {
  auto* child_user_service =
      ChildUserServiceFactory::GetForBrowserContext(context);
  if (!child_user_service)
    return false;

  return child_user_service->WebTimeLimitWhitelistedURL(url);
}

base::TimeDelta GetWebTimeLimit(content::BrowserContext* context) {
  auto* child_user_service =
      ChildUserServiceFactory::GetForBrowserContext(context);
  DCHECK(child_user_service);
  return child_user_service->GetWebTimeLimit();
}

Browser* FindBrowserForWebContents(content::WebContents* web_contents) {
  BrowserList* browser_list = BrowserList::GetInstance();
  for (Browser* browser : *browser_list) {
    TabStripModel* tab_strip_model = browser->tab_strip_model();
    for (int i = 0; i < tab_strip_model->count(); i++) {
      if (web_contents == tab_strip_model->GetWebContentsAt(i))
        return browser;
    }
  }

  return nullptr;
}

}  // namespace

using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;

// static
std::unique_ptr<WebTimeLimitNavigationThrottle>
WebTimeLimitNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* navigation_handle) {
  content::BrowserContext* browser_context =
      navigation_handle->GetWebContents()->GetBrowserContext();

  if (IsWebBlocked(browser_context)) {
    // Creating a throttle for both the main frame and sub frames. This prevents
    // kids from circumventing the app restrictions by using iframes in a local
    // html file.
    return base::WrapUnique(
        new WebTimeLimitNavigationThrottle(navigation_handle));
  }

  return nullptr;
}

WebTimeLimitNavigationThrottle::~WebTimeLimitNavigationThrottle() = default;

ThrottleCheckResult WebTimeLimitNavigationThrottle::WillStartRequest() {
  return WillStartOrRedirectRequest(/* proceed_if_no_browser */ true);
}

ThrottleCheckResult WebTimeLimitNavigationThrottle::WillRedirectRequest() {
  return WillStartOrRedirectRequest(/* proceed_if_no_browser */ true);
}

ThrottleCheckResult WebTimeLimitNavigationThrottle::WillProcessResponse() {
  return WillStartOrRedirectRequest(/* proceed_if_no_browser */ false);
}

const char* WebTimeLimitNavigationThrottle::GetNameForLogging() {
  return "WebTimeLimitNavigationThrottle";
}

WebTimeLimitNavigationThrottle::WebTimeLimitNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : NavigationThrottle(navigation_handle) {}

ThrottleCheckResult WebTimeLimitNavigationThrottle::WillStartOrRedirectRequest(
    bool proceed_if_no_browser) {
  content::BrowserContext* browser_context =
      navigation_handle()->GetWebContents()->GetBrowserContext();

  if (!IsWebBlocked(browser_context) ||
      IsURLWhitelisted(navigation_handle()->GetURL(), browser_context)) {
    return NavigationThrottle::PROCEED;
  }

  // Let's get the browser instance from the navigation handle.
  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  Browser* browser = FindBrowserForWebContents(web_contents);

  if (!browser && proceed_if_no_browser)
    return PROCEED;

  const std::string& app_locale = g_browser_process->GetApplicationLocale();

  Browser::Type type = browser->type();
  web_app::WebAppTabHelper* web_app_helper =
      web_app::WebAppTabHelper::FromWebContents(web_contents);

  bool is_windowed =
      (type == Browser::Type::TYPE_APP) || (type == Browser::Type::TYPE_POPUP);
  bool is_app = false;
  if (web_app_helper && !web_app_helper->app_id().empty())
    is_app = true;

  // Don't throttle windowed applications. We show a notification and close
  // them.
  if (is_windowed && is_app)
    return PROCEED;

  base::TimeDelta time_limit = GetWebTimeLimit(browser_context);
  std::string interstitial_html;
  if (is_app) {
    Profile* profile = Profile::FromBrowserContext(browser_context);
    web_app::WebAppProvider* web_app_provider =
        web_app::WebAppProvider::Get(profile);
    const web_app::AppRegistrar& registrar = web_app_provider->registrar();
    const std::string& app_name =
        registrar.GetAppShortName(web_app_helper->app_id());
    interstitial_html =
        GetWebTimeLimitAppErrorPage(time_limit, app_locale, app_name);
  } else {
    interstitial_html = GetWebTimeLimitChromeErrorPage(time_limit, app_locale);
  }

  return NavigationThrottle::ThrottleCheckResult(
      CANCEL, net::ERR_BLOCKED_BY_CLIENT, interstitial_html);
}

}  // namespace chromeos
