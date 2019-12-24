// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/browser_agent_util.h"

#include "base/feature_list.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_browser_agent.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/features.h"
#import "ios/chrome/browser/infobars/infobar_badge_browser_agent.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"
#import "ios/chrome/browser/web_state_list/tab_insertion_browser_agent.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void AttachBrowserAgents(Browser* browser) {
  if (base::FeatureList::IsEnabled(kLogBreadcrumbs)) {
    BreadcrumbManagerBrowserAgent::CreateForBrowser(browser);
  }
  TabInsertionBrowserAgent::CreateForBrowser(browser);

  if (base::FeatureList::IsEnabled(kInfobarOverlayUI)) {
    InfobarBadgeBrowserAgent::CreateForBrowser(browser);
  }
}
