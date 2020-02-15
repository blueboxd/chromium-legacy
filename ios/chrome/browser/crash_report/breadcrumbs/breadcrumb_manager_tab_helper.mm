// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_tab_helper.h"

#import "base/ios/ns_error_util.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#import "ios/net/protocol_handler_util.h"
#include "ios/web/public/favicon/favicon_url.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/security/security_style.h"
#include "ios/web/public/security/ssl_status.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Returns true if navigation URL repesents Chrome's New Tab Page.
bool IsNptUrl(const GURL& url) {
  return url.GetOrigin() == kChromeUINewTabURL ||
         (url.SchemeIs(url::kAboutScheme) && url.path() == "//newtab");
}
}

const char kBreadcrumbDidStartNavigation[] = "StartNav";
const char kBreadcrumbDidFinishNavigation[] = "FinishNav";
const char kBreadcrumbPageLoaded[] = "PageLoad";
const char kBreadcrumbDidChangeVisibleSecurityState[] = "SecurityChange";

const char kBreadcrumbAuthenticationBroken[] = "#broken";
const char kBreadcrumbDownload[] = "#download";
const char kBreadcrumbMixedContent[] = "#mixed";
const char kBreadcrumbNtpNavigation[] = "#ntp";
const char kBreadcrumbPageLoadFailure[] = "#failure";
const char kBreadcrumbPageLoadSuccess[] = "#success";
const char kBreadcrumbRendererInitiatedByUser[] = "#renderer-user";
const char kBreadcrumbRendererInitiatedByScript[] = "#renderer-script";

BreadcrumbManagerTabHelper::BreadcrumbManagerTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  DCHECK(web_state_);
  web_state_->AddObserver(this);

  static int next_unique_id = 1;
  unique_id_ = next_unique_id++;
}

BreadcrumbManagerTabHelper::~BreadcrumbManagerTabHelper() = default;

void BreadcrumbManagerTabHelper::LogEvent(const std::string& event) {
  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(web_state_->GetBrowserState());
  std::string event_log =
      base::StringPrintf("Tab%d %s", unique_id_, event.c_str());
  BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(chrome_browser_state)
      ->AddEvent(event_log);
}

void BreadcrumbManagerTabHelper::WasShown(web::WebState* web_state) {
  LogEvent("WasShown");
}

void BreadcrumbManagerTabHelper::WasHidden(web::WebState* web_state) {
  LogEvent("WasHidden");
}

void BreadcrumbManagerTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  std::vector<std::string> event = {
      base::StringPrintf("%s%lld", kBreadcrumbDidStartNavigation,
                         navigation_context->GetNavigationId()),
  };

  if (IsNptUrl(navigation_context->GetUrl())) {
    event.push_back(kBreadcrumbNtpNavigation);
  }

  if (navigation_context->IsRendererInitiated()) {
    if (navigation_context->HasUserGesture()) {
      event.push_back(kBreadcrumbRendererInitiatedByUser);
    } else {
      event.push_back(kBreadcrumbRendererInitiatedByScript);
    }
  }

  event.push_back(
      base::StringPrintf("#%s", ui::PageTransitionGetCoreTransitionString(
                                    navigation_context->GetPageTransition())));

  LogEvent(base::JoinString(event, " "));
}

void BreadcrumbManagerTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  std::vector<std::string> event = {
      base::StringPrintf("%s%lld", kBreadcrumbDidFinishNavigation,
                         navigation_context->GetNavigationId()),
  };

  if (navigation_context->IsDownload()) {
    event.push_back(kBreadcrumbDownload);
  }

  NSError* error = navigation_context->GetError();
  if (error) {
    int code = net::ERR_FAILED;
    NSError* final_error = base::ios::GetFinalUnderlyingErrorFromError(error);
    // Only errors with net::kNSErrorDomain have correct net error code.
    if (final_error && [final_error.domain isEqual:net::kNSErrorDomain]) {
      code = final_error.code;
    }
    event.push_back(net::ErrorToShortString(code));
  }

  LogEvent(base::JoinString(event, " "));
}

void BreadcrumbManagerTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  std::vector<std::string> event = {kBreadcrumbPageLoaded};

  if (IsNptUrl(web_state->GetLastCommittedURL())) {
    // NTP load can't fail, so there is no need to report success/failure.
    event.push_back(kBreadcrumbNtpNavigation);
  } else {
    switch (load_completion_status) {
      case web::PageLoadCompletionStatus::SUCCESS:
        event.push_back(kBreadcrumbPageLoadSuccess);
        break;
      case web::PageLoadCompletionStatus::FAILURE:
        event.push_back(kBreadcrumbPageLoadFailure);
        break;
    }
  }

  LogEvent(base::JoinString(event, " "));
}

void BreadcrumbManagerTabHelper::DidChangeVisibleSecurityState(
    web::WebState* web_state) {
  web::NavigationItem* visible_item =
      web_state->GetNavigationManager()->GetVisibleItem();
  if (!visible_item) {
    return;
  }

  std::vector<std::string> event;
  const web::SSLStatus& ssl = visible_item->GetSSL();
  if (ssl.content_status & web::SSLStatus::DISPLAYED_INSECURE_CONTENT) {
    event.push_back(kBreadcrumbMixedContent);
  }

  if (ssl.security_style == web::SECURITY_STYLE_AUTHENTICATION_BROKEN) {
    event.push_back(kBreadcrumbAuthenticationBroken);
  }

  if (!event.empty()) {
    event.insert(event.begin(), kBreadcrumbDidChangeVisibleSecurityState);
    LogEvent(base::JoinString(event, " "));
  }
}

void BreadcrumbManagerTabHelper::RenderProcessGone(web::WebState* web_state) {
  LogEvent("RenderProcessGone");
}

void BreadcrumbManagerTabHelper::WebStateDestroyed(web::WebState* web_state) {
  LogEvent("WebStateDestroyed");
  web_state->RemoveObserver(this);
}

WEB_STATE_USER_DATA_KEY_IMPL(BreadcrumbManagerTabHelper)
