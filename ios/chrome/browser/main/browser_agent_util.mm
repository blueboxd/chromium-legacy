// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/browser_agent_util.h"

#include "base/feature_list.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_browser_agent.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/features.h"
#include "ios/chrome/browser/infobars/overlays/browser_agent/infobar_overlay_browser_agent_util.h"
#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#import "ios/chrome/browser/sessions/session_service_ios.h"
#import "ios/chrome/browser/web_state_list/tab_insertion_browser_agent.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void AttachBrowserAgents(Browser* browser) {
  if (base::FeatureList::IsEnabled(kLogBreadcrumbs)) {
    BreadcrumbManagerBrowserAgent::CreateForBrowser(browser);
  }
  TabInsertionBrowserAgent::CreateForBrowser(browser);
  AttachInfobarOverlayBrowserAgent(browser);

  SessionRestorationBrowserAgent::CreateForBrowser(
      browser, [SessionServiceIOS sharedService]);
  // This needs to be called last in case any downstream browser agents need to
  // access upstream agents created earlier in this function.
  ios::GetChromeBrowserProvider()->AttachBrowserAgents(browser);
}
