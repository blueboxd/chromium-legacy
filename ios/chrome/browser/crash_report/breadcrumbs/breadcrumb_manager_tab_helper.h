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
  void DidStartLoading(web::WebState* web_state) override;
  void DidStopLoading(web::WebState* web_state) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void DidChangeBackForwardState(web::WebState* web_state) override;
  void TitleWasSet(web::WebState* web_state) override;
  void DidChangeVisibleSecurityState(web::WebState* web_state) override;
  void RenderProcessGone(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // The webstate associated with this tab helper.
  web::WebState* web_state_ = nullptr;
  int unique_id_ = -1;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_MANAGER_TAB_HELPER_H_
