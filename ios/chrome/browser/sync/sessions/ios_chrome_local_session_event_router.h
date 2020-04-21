// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_SESSIONS_IOS_CHROME_LOCAL_SESSION_EVENT_ROUTER_H_
#define IOS_CHROME_BROWSER_SYNC_SESSIONS_IOS_CHROME_LOCAL_SESSION_EVENT_ROUTER_H_

#include <stddef.h>

#include <memory>
#include <set>

#include "base/callback_list.h"
#include "base/macros.h"
#include "components/sync/model/syncable_service.h"
#include "components/sync_sessions/local_session_event_router.h"
#import "ios/chrome/browser/tabs/tab_model_list_observer.h"
#include "ios/chrome/browser/web_state_list/web_state_list_observer.h"
#include "ios/web/public/web_state_observer.h"

class ChromeBrowserState;

namespace sync_sessions {
class SyncSessionsClient;
}

// A LocalEventRouter that drives session sync via observation of
// web::WebState-related events.
class IOSChromeLocalSessionEventRouter
    : public sync_sessions::LocalSessionEventRouter,
      public web::WebStateObserver,
      public WebStateListObserver,
      public TabModelListObserver {
 public:
  IOSChromeLocalSessionEventRouter(
      ChromeBrowserState* browser_state,
      sync_sessions::SyncSessionsClient* sessions_client_,
      const syncer::SyncableService::StartSyncFlare& flare);
  ~IOSChromeLocalSessionEventRouter() override;

  // LocalEventRouter:
  void StartRoutingTo(
      sync_sessions::LocalSessionEventHandler* handler) override;
  void Stop() override;

  // TabModelListObserver:
  void TabModelRegisteredWithBrowserState(
      TabModel* tab_model,
      ChromeBrowserState* browser_state) override;
  void TabModelUnregisteredFromBrowserState(
      TabModel* tab_model,
      ChromeBrowserState* browser_state) override;

  // web::WebStateObserver:
  void TitleWasSet(web::WebState* web_state) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void DidChangeBackForwardState(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // web::WebStateListObserver:
  void WebStateInsertedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index,
                          bool activating) override;
  void WebStateReplacedAt(WebStateList* web_state_list,
                          web::WebState* old_web_state,
                          web::WebState* new_web_state,
                          int index) override;
  void WebStateDetachedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index) override;
  void WillBeginBatchOperation(WebStateList* web_state_list) override;
  void BatchOperationEnded(WebStateList* web_state_list) override;

 private:
  // Methods to add and remove WebStateList observer.
  void StartObservingWebStateList(WebStateList* web_state_list);
  void StopObservingWebStateList(WebStateList* web_state_list);

  // Called when a tab is parented.
  void OnTabParented(web::WebState* web_state);

  // Called on observation of a change in |web_state|.
  void OnWebStateChange(web::WebState* web_state);

  sync_sessions::LocalSessionEventHandler* handler_;
  ChromeBrowserState* const browser_state_;
  sync_sessions::SyncSessionsClient* const sessions_client_;
  syncer::SyncableService::StartSyncFlare flare_;

  std::unique_ptr<base::CallbackList<void(web::WebState*)>::Subscription>
      tab_parented_subscription_;

  // Track the number of WebStateList we are observing that are in a batch
  // operation.
  int batch_in_progress_ = 0;

  DISALLOW_COPY_AND_ASSIGN(IOSChromeLocalSessionEventRouter);
};

#endif  // IOS_CHROME_BROWSER_SYNC_SESSIONS_IOS_CHROME_LOCAL_SESSION_EVENT_ROUTER_H_
