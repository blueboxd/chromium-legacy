// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/web_state_list_metrics_browser_agent.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BROWSER_USER_DATA_KEY_IMPL(WebStateListMetricsBrowserAgent)

WebStateListMetricsBrowserAgent::WebStateListMetricsBrowserAgent(
    Browser* browser)
    : web_state_list_(browser->GetWebStateList()) {
  browser->AddObserver(this);
  DCHECK(web_state_list_);
  web_state_list_->AddObserver(this);
  SessionRestorationBrowserAgent* restoration_agent =
      SessionRestorationBrowserAgent::FromBrowser(browser);
  if (restoration_agent)
    restoration_agent->AddObserver(this);
}

WebStateListMetricsBrowserAgent::WebStateListMetricsBrowserAgent() {
  ResetSessionMetrics();
}

WebStateListMetricsBrowserAgent::~WebStateListMetricsBrowserAgent() = default;

void WebStateListMetricsBrowserAgent::RecordSessionMetrics() {
  UMA_HISTOGRAM_CUSTOM_COUNTS("Session.ClosedTabCounts",
                              detached_web_state_counter_, 1, 200, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Session.OpenedTabCounts",
                              activated_web_state_counter_, 1, 200, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Session.NewTabCounts",
                              inserted_web_state_counter_, 1, 200, 50);
  ResetSessionMetrics();
}

void WebStateListMetricsBrowserAgent::WillStartSessionRestoration() {
  metric_collection_paused_ = true;
}

void WebStateListMetricsBrowserAgent::SessionRestorationFinished(
    const std::vector<web::WebState*>& restored_web_states) {
  metric_collection_paused_ = false;
}

void WebStateListMetricsBrowserAgent::WebStateInsertedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index,
    bool activating) {
  if (metric_collection_paused_)
    return;
  base::RecordAction(base::UserMetricsAction("MobileNewTabOpened"));
  ++inserted_web_state_counter_;
}

void WebStateListMetricsBrowserAgent::WebStateDetachedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index) {
  if (metric_collection_paused_)
    return;
  base::RecordAction(base::UserMetricsAction("MobileTabClosed"));
  ++detached_web_state_counter_;
}

void WebStateListMetricsBrowserAgent::WebStateActivatedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int active_index,
    ActiveWebStateChangeReason reason) {
  if (metric_collection_paused_)
    return;
  ++activated_web_state_counter_;
  if (reason == ActiveWebStateChangeReason::Replaced)
    return;

  base::RecordAction(base::UserMetricsAction("MobileTabSwitched"));
}

void WebStateListMetricsBrowserAgent::ResetSessionMetrics() {
  inserted_web_state_counter_ = 0;
  detached_web_state_counter_ = 0;
  activated_web_state_counter_ = 0;
  metric_collection_paused_ = false;
}

void WebStateListMetricsBrowserAgent::BrowserDestroyed(Browser* browser) {
  DCHECK_EQ(browser->GetWebStateList(), web_state_list_);

  web_state_list_->RemoveObserver(this);
  browser->RemoveObserver(this);
  SessionRestorationBrowserAgent* restoration_agent =
      SessionRestorationBrowserAgent::FromBrowser(browser);
  if (restoration_agent)
    restoration_agent->RemoveObserver(this);
  web_state_list_ = nullptr;
}
