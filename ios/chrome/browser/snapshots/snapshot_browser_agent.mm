// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/snapshot_browser_agent.h"

#import "ios/chrome/browser/snapshots/snapshot_cache.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BROWSER_USER_DATA_KEY_IMPL(SnapshotBrowserAgent)

SnapshotBrowserAgent::SnapshotBrowserAgent(Browser* browser)
    : web_state_list_(browser->GetWebStateList()) {
  browser->AddObserver(this);
  web_state_list_->AddObserver(this);
  snapshot_cache_ = [[SnapshotCache alloc] init];
}

SnapshotBrowserAgent::~SnapshotBrowserAgent() = default;

// Browser Observer methods:
void SnapshotBrowserAgent::BrowserDestroyed(Browser* browser) {
  DCHECK_EQ(browser->GetWebStateList(), web_state_list_);
  web_state_list_->RemoveObserver(this);
  browser->RemoveObserver(this);
  web_state_list_ = nullptr;
  [snapshot_cache_ shutdown];
}

void SnapshotBrowserAgent::WebStateInsertedAt(WebStateList* web_state_list,
                                              web::WebState* web_state,
                                              int index,
                                              bool activating) {
  SnapshotTabHelper::FromWebState(web_state)->SetSnapshotCache(snapshot_cache_);
}

void SnapshotBrowserAgent::WebStateReplacedAt(WebStateList* web_state_list,
                                              web::WebState* old_web_state,
                                              web::WebState* new_web_state,
                                              int index) {
  SnapshotTabHelper::FromWebState(old_web_state)->SetSnapshotCache(nil);
  SnapshotTabHelper::FromWebState(new_web_state)
      ->SetSnapshotCache(snapshot_cache_);
}

void SnapshotBrowserAgent::WebStateDetachedAt(WebStateList* web_state_list,
                                              web::WebState* web_state,
                                              int index) {
  SnapshotTabHelper::FromWebState(web_state)->SetSnapshotCache(nil);
}

void SnapshotBrowserAgent::WillBeginBatchOperation(
    WebStateList* web_state_list) {
  for (int i = 0; i < web_state_list->count(); ++i) {
    web::WebState* web_state = web_state_list->GetWebStateAt(i);
    SnapshotTabHelper::FromWebState(web_state)->SetSnapshotCache(nil);
  }
}

void SnapshotBrowserAgent::BatchOperationEnded(WebStateList* web_state_list) {
  for (int i = 0; i < web_state_list->count(); ++i) {
    web::WebState* web_state = web_state_list->GetWebStateAt(i);
    SnapshotTabHelper::FromWebState(web_state)->SetSnapshotCache(
        snapshot_cache_);
  }
}

SnapshotCache* SnapshotBrowserAgent::GetSnapshotCache() {
  return snapshot_cache_;
}
