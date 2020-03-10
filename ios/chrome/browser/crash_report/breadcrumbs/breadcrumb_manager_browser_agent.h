// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_MANAGER_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_MANAGER_BROWSER_AGENT_H_

#include <string>

#include "ios/chrome/browser/main/browser_observer.h"
#include "ios/chrome/browser/main/browser_user_data.h"
#include "ios/chrome/browser/web_state_list/web_state_list_observer.h"

class Browser;
class WebStateList;

// Logs activity for the associated Browser's underlying WebStateList based on
// callbacks from WebStateListObserver. Event logs are sent to the
// BrowserState's BreadcrumbManagerKeyedService.
// For example:
//   Browser1 Insert active WebState2 at 0
// which indicates that a WebState with identifier 2 (from
// BreadcrumbManagerTabHelper) was inserted into the Browser with identifier 1
// (from BreadcrumbManagerBrowserAgent)
class BreadcrumbManagerBrowserAgent
    : BrowserObserver,
      public BrowserUserData<BreadcrumbManagerBrowserAgent>,
      WebStateListObserver {
 public:
  // Gets and Sets whether or not logging is enabled. Disabling logging be used
  // to prevent the over-collection of breadcrumb events during known states
  // such as a clean shutdown.
  // |IsLoggingEnabled()| defaults to true on initialization.
  bool IsLoggingEnabled();
  void SetLoggingEnabled(bool enabled);

  ~BreadcrumbManagerBrowserAgent() override;

 private:
  explicit BreadcrumbManagerBrowserAgent(Browser* browser);
  friend class BrowserUserData<BreadcrumbManagerBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  BreadcrumbManagerBrowserAgent(const BreadcrumbManagerBrowserAgent&) = delete;
  BreadcrumbManagerBrowserAgent& operator=(
      const BreadcrumbManagerBrowserAgent&) = delete;

  // Logs a breadcrumb event with message data |event| associated with
  // |browser_|. NOTE: |event| must not include newline characters as newlines
  // are used by BreadcrumbPersistentStore as a deliminator.
  void LogEvent(const std::string& event);

  // BrowserObserver
  void BrowserDestroyed(Browser* browser) override;

  // WebStateListObserver overrides
  void WebStateInsertedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index,
                          bool activating) override;
  void WebStateMoved(WebStateList* web_state_list,
                     web::WebState* web_state,
                     int from_index,
                     int to_index) override;
  void WebStateReplacedAt(WebStateList* web_state_list,
                          web::WebState* old_web_state,
                          web::WebState* new_web_state,
                          int index) override;
  void WillCloseWebStateAt(WebStateList* web_state_list,
                           web::WebState* web_state,
                           int index,
                           bool user_action) override;
  void WebStateActivatedAt(WebStateList* web_state_list,
                           web::WebState* old_web_state,
                           web::WebState* new_web_state,
                           int active_index,
                           ActiveWebStateChangeReason reason) override;
  void WillBeginBatchOperation(WebStateList* web_state_list) override;
  void BatchOperationEnded(WebStateList* web_state_list) override;

  // Unique (across this application run only) identifier for logs associated
  // with |browser_| instance. Used to differentiate logs associated with the
  // same underlying BrowserState.
  int unique_id_ = -1;
  // Whether or not events will be logged.
  bool logging_enabled_ = true;
  Browser* browser_ = nullptr;

  // Keeps track of WebState mutation count to avoid logging every event.
  // Created in WillBeginBatchOperation and destroyed in BatchOperationEnded.
  // Final mutation count is logged in BatchOperationEnded.
  struct BatchOperation {
    // Number of WebState objects inserted between WillBeginBatchOperation and
    // BatchOperationEnded callbacks.
    int insertion_count = 0;
    // Number of WebState objects closed between WillBeginBatchOperation and
    // BatchOperationEnded callbacks.
    int close_count = 0;
  };
  std::unique_ptr<BatchOperation> batch_operation_;
};

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_MANAGER_BROWSER_AGENT_H_
