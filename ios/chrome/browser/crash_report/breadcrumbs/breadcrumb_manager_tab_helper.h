// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_MANAGER_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_MANAGER_TAB_HELPER_H_

#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {
class WebState;
}  // namespace web

// Name of DidStartNavigation event (see WebStateObserver::DidStartNavigation).
extern const char kBreadcrumbDidStartNavigation[];

// Name of DidStartNavigation event (see WebStateObserver::DidFinishNavigation).
extern const char kBreadcrumbDidFinishNavigation[];

// Name of PageLoaded event (see WebStateObserver::PageLoaded).
extern const char kBreadcrumbPageLoaded[];

// Constants below represent metadata for breadcrumb events.

// Appended to |kBreadcrumbDidFinishNavigation| event if
// navigation is a download.
extern const char kBreadcrumbDownload[];

// Appended to |kBreadcrumbPageLoaded| event if page load has
// failed.
extern const char kBreadcrumbPageLoadFailure[];

// Appended to |kBreadcrumbDidStartNavigation| and
// |kBreadcrumbPageLoaded| event if the navigation url was Chrome's New Tab
// Page.
extern const char kBreadcrumbNtpNavigation[];

// Appended to |kBreadcrumbPageLoaded| event if page load has
// succeeded.
extern const char kBreadcrumbPageLoadSuccess[];

// Appended to |kBreadcrumbDidStartNavigation| event if navigation
// was a client side redirect (f.e. window.open without user gesture).
extern const char kBreadcrumbRendererInitiatedByScript[];

// Appended to |kBreadcrumbDidStartNavigation| event if navigation
// was renderer-initiated navigation with user gesture (f.e. link tap or
// widow.open with user gesture).
extern const char kBreadcrumbRendererInitiatedByUser[];

// Handles logging of Breadcrumb events associated with |web_state_| based on
// calls from WebStateObserver.
class BreadcrumbManagerTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<BreadcrumbManagerTabHelper> {
 public:
  ~BreadcrumbManagerTabHelper() override;

  // Returns a unique identifier to be used in breadcrumb event logs to identify
  // events associated with the underlying WebState. This value is unique across
  // this application run only and is NOT persisted and will change across
  // launches.
  int GetUniqueId() const { return unique_id_; }

 private:
  friend class web::WebStateUserData<BreadcrumbManagerTabHelper>;

  explicit BreadcrumbManagerTabHelper(web::WebState* web_state);

  BreadcrumbManagerTabHelper(const BreadcrumbManagerTabHelper&) = delete;
  BreadcrumbManagerTabHelper& operator=(const BreadcrumbManagerTabHelper&) =
      delete;

  // Logs an event for the associated WebState.
  void LogEvent(const std::string& event);

  // web::WebStateObserver implementation.
  void WasShown(web::WebState* web_state) override;
  void WasHidden(web::WebState* web_state) override;
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void DidChangeBackForwardState(web::WebState* web_state) override;
  void DidChangeVisibleSecurityState(web::WebState* web_state) override;
  void RenderProcessGone(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // The webstate associated with this tab helper.
  web::WebState* web_state_ = nullptr;
  int unique_id_ = -1;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_MANAGER_TAB_HELPER_H_
