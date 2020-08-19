// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_BROWSER_AGENT_H_

#include "base/macros.h"
#import "ios/chrome/browser/main/browser_observer.h"
#import "ios/chrome/browser/main/browser_user_data.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer.h"

@class SnapshotCache;

// Associates a SnapshotCache to a Browser.
class SnapshotBrowserAgent : BrowserObserver,
                             public WebStateListObserver,
                             public BrowserUserData<SnapshotBrowserAgent> {
 public:
  SnapshotBrowserAgent();
  ~SnapshotBrowserAgent() override;

  // Returns the SnapshotCache
  SnapshotCache* GetSnapshotCache();

 private:
  explicit SnapshotBrowserAgent(Browser* browser);
  friend class BrowserUserData<SnapshotBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  // BrowserObserver methods
  void BrowserDestroyed(Browser* browser) override;

  // WebStateListObserver methods
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

  WebStateList* web_state_list_;   // unowned.
  SnapshotCache* snapshot_cache_;  // strong, owned.
};

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_BROWSER_AGENT_H_
