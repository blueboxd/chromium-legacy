// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_browser_agent.h"

#import <Foundation/Foundation.h>

#include "base/strings/stringprintf.h"
#include "base/strings/string_util.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_tab_helper.h"
#import "ios/chrome/browser/main/browser.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BROWSER_USER_DATA_KEY_IMPL(BreadcrumbManagerBrowserAgent)

BreadcrumbManagerBrowserAgent::BreadcrumbManagerBrowserAgent(Browser* browser)
    : browser_(browser) {
  static int next_unique_id = 1;
  unique_id_ = next_unique_id++;

  browser_->AddObserver(this);
  browser_->GetWebStateList()->AddObserver(this);
}

BreadcrumbManagerBrowserAgent::~BreadcrumbManagerBrowserAgent() = default;

bool BreadcrumbManagerBrowserAgent::IsLoggingEnabled() {
  return logging_enabled_;
}

void BreadcrumbManagerBrowserAgent::SetLoggingEnabled(bool enabled) {
  logging_enabled_ = enabled;
}

void BreadcrumbManagerBrowserAgent::BrowserDestroyed(Browser* browser) {
  browser_->GetWebStateList()->RemoveObserver(this);
  browser_->RemoveObserver(this);
}

void BreadcrumbManagerBrowserAgent::LogEvent(const std::string& event) {
  if (!logging_enabled_) {
    return;
  }

  BreadcrumbManagerKeyedService* breadcrumb_manager =
      BreadcrumbManagerKeyedServiceFactory::GetInstance()->GetForBrowserState(
          browser_->GetBrowserState());
  breadcrumb_manager->AddEvent(
      base::StringPrintf("Browser%d %s", unique_id_, event.c_str()));
}

void BreadcrumbManagerBrowserAgent::WebStateInsertedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index,
    bool activating) {
  if (batch_operation_) {
    ++batch_operation_->insertion_count;
    return;
  }

  int web_state_id =
      BreadcrumbManagerTabHelper::FromWebState(web_state)->GetUniqueId();
  const char* activating_string = activating ? "active" : "inactive";
  LogEvent(base::StringPrintf("Insert %s Tab%d at %d", activating_string,
                              web_state_id, index));
}
void BreadcrumbManagerBrowserAgent::WebStateMoved(WebStateList* web_state_list,
                                                  web::WebState* web_state,
                                                  int from_index,
                                                  int to_index) {
  int web_state_id =
      BreadcrumbManagerTabHelper::FromWebState(web_state)->GetUniqueId();
  LogEvent(base::StringPrintf("Moved Tab%d from %d to %d", web_state_id,
                              from_index, to_index));
}
void BreadcrumbManagerBrowserAgent::WebStateReplacedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int index) {
  int old_web_state_id =
      BreadcrumbManagerTabHelper::FromWebState(old_web_state)->GetUniqueId();
  int new_web_state_id =
      BreadcrumbManagerTabHelper::FromWebState(new_web_state)->GetUniqueId();
  LogEvent(base::StringPrintf("Replaced Tab%d with Tab%d at %d",
                              old_web_state_id, new_web_state_id, index));
}
void BreadcrumbManagerBrowserAgent::WillCloseWebStateAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index,
    bool user_action) {
  if (batch_operation_) {
    ++batch_operation_->close_count;
    return;
  }

  int web_state_id =
      BreadcrumbManagerTabHelper::FromWebState(web_state)->GetUniqueId();
  LogEvent(base::StringPrintf("Close Tab%d at %d", web_state_id, index));
}
void BreadcrumbManagerBrowserAgent::WebStateActivatedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int active_index,
    ActiveWebStateChangeReason reason) {
  if (reason != ActiveWebStateChangeReason::Activated) {
    return;
  }

  std::vector<std::string> event = {"Switch"};
  if (old_web_state) {
    event.push_back(base::StringPrintf(
        "from Tab%d", BreadcrumbManagerTabHelper::FromWebState(old_web_state)
                          ->GetUniqueId()));
  }
  if (new_web_state) {
    event.push_back(base::StringPrintf(
        "to Tab%d at %d",
        BreadcrumbManagerTabHelper::FromWebState(new_web_state)->GetUniqueId(),
        active_index));
  }

  LogEvent(base::JoinString(event, " "));
}

void BreadcrumbManagerBrowserAgent::WillBeginBatchOperation(
    WebStateList* web_state_list) {
  batch_operation_ = std::make_unique<BatchOperation>();
}

void BreadcrumbManagerBrowserAgent::BatchOperationEnded(
    WebStateList* web_state_list) {
  if (batch_operation_) {
    if (batch_operation_->insertion_count > 0) {
      LogEvent(base::StringPrintf("Inserted %d tabs",
                                  batch_operation_->insertion_count));
    }
    if (batch_operation_->close_count > 0) {
      LogEvent(
          base::StringPrintf("Closed %d tabs", batch_operation_->close_count));
    }
  }
  batch_operation_.reset();
}
