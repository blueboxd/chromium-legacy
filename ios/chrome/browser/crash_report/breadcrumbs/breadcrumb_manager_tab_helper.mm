// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_tab_helper.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#include "ios/web/public/favicon/favicon_url.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BreadcrumbManagerTabHelper::BreadcrumbManagerTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  DCHECK(web_state_);
  web_state_->AddObserver(this);

  static int next_unique_id = 1;
  unique_id_ = next_unique_id++;
}

BreadcrumbManagerTabHelper::~BreadcrumbManagerTabHelper() = default;

void BreadcrumbManagerTabHelper::LogEvent(const std::string& event) {
  ios::ChromeBrowserState* chrome_browser_state =
      ios::ChromeBrowserState::FromBrowserState(web_state_->GetBrowserState());
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
  LogEvent("DidStartNavigation");
}

void BreadcrumbManagerTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  LogEvent("DidFinishNavigation");
}

void BreadcrumbManagerTabHelper::DidStartLoading(web::WebState* web_state) {
  LogEvent("DidStartLoading");
}

void BreadcrumbManagerTabHelper::DidStopLoading(web::WebState* web_state) {
  LogEvent("DidStopLoading");
}

void BreadcrumbManagerTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  switch (load_completion_status) {
    case web::PageLoadCompletionStatus::SUCCESS:
      LogEvent("PageLoaded: Success");
      break;
    case web::PageLoadCompletionStatus::FAILURE:
      LogEvent("PageLoaded: Failure");
      break;
  }
}

void BreadcrumbManagerTabHelper::DidChangeBackForwardState(
    web::WebState* web_state) {
  LogEvent("DidChangeBackForwardState");
}

void BreadcrumbManagerTabHelper::TitleWasSet(web::WebState* web_state) {
  LogEvent("TitleWasSet");
}

void BreadcrumbManagerTabHelper::DidChangeVisibleSecurityState(
    web::WebState* web_state) {
  LogEvent("DidChangeVisibleSecurityState");
}

void BreadcrumbManagerTabHelper::RenderProcessGone(web::WebState* web_state) {
  LogEvent("RenderProcessGone");
}

void BreadcrumbManagerTabHelper::WebStateDestroyed(web::WebState* web_state) {
  LogEvent("WebStateDestroyed");
  web_state->RemoveObserver(this);
}

WEB_STATE_USER_DATA_KEY_IMPL(BreadcrumbManagerTabHelper)
