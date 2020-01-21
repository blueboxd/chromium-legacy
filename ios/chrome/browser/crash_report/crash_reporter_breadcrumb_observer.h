// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_REPORTER_BREADCRUMB_OBSERVER_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_REPORTER_BREADCRUMB_OBSERVER_H_

#include <map>
#include <memory>

#import <Foundation/Foundation.h>

#include "ios/chrome/browser/browser_state/chrome_browser_state_forward.h"
#import "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_observer_bridge.h"

// Combines breadcrumbs from multiple ChromeBrowserState instances and sends the
// merged breadcrumb events to breakpad for attachment to crash reports.
@interface CrashReporterBreadcrumbObserver
    : NSObject <BreadcrumbManagerObserving> {
}

// Creates a singleton instance.
+ (CrashReporterBreadcrumbObserver*)uniqueInstance;

// Starts collecting breadcrumb events associated with |browserState|.
- (void)observeBrowserState:(ios::ChromeBrowserState*)browserState;
// Stops collecting breadcrumb events associated with |browserState|.
- (void)stopObservingBrowserState:(ios::ChromeBrowserState*)browserState;

@end

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_REPORTER_BREADCRUMB_OBSERVER_H_
