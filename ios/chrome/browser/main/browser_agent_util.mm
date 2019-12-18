// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/browser_agent_util.h"

#import "ios/chrome/browser/web_state_list/tab_insertion_browser_agent.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void AttachBrowserAgents(Browser* browser) {
  TabInsertionBrowserAgent::CreateForBrowser(browser);
}
