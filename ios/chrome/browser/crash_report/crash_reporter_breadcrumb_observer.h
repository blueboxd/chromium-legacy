// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_REPORTER_BREADCRUMB_OBSERVER_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_REPORTER_BREADCRUMB_OBSERVER_H_

#include <map>
#include <memory>

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_observer_bridge.h"

// The maximum string length for breadcrumbs data.
extern const NSUInteger kMaxBreadcrumbsDataLength;

// Combines breadcrumbs from multiple BreadcrumbManagers and sends the merged
// breadcrumb events to breakpad for attachment to crash reports.
@interface CrashReporterBreadcrumbObserver
    : NSObject <BreadcrumbManagerObserving> {
}

// Creates a singleton instance.
+ (CrashReporterBreadcrumbObserver*)uniqueInstance;

// Starts collecting breadcrumb events logged to |breadcrumbManager|.
- (void)observeBreadcrumbManager:(BreadcrumbManager*)breadcrumbManager;

// Stops collecting breadcrumb events logged to |breadcrumbManager|.
- (void)stopObservingBreadcrumbManager:(BreadcrumbManager*)breadcrumbManager;

// Starts collecting breadcrumb events logged to |breadcrumbManagerService|.
- (void)observeBreadcrumbManagerService:
    (BreadcrumbManagerKeyedService*)breadcrumbManagerService;

// Stops collecting breadcrumb events logged to |breadcrumbManagerService|.
- (void)stopObservingBreadcrumbManagerService:
    (BreadcrumbManagerKeyedService*)breadcrumbManagerService;

@end

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_REPORTER_BREADCRUMB_OBSERVER_H_
