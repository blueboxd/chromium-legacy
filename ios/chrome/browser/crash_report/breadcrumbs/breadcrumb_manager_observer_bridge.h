// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_MANAGER_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_MANAGER_OBSERVER_BRIDGE_H_

#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_observer.h"

class BreadcrumbManagerKeyedService;

// Protocol mirroring BreadcrumbManagerObserver
@protocol BreadcrumbManagerObserving
- (void)breadcrumbManager:(BreadcrumbManagerKeyedService*)manager
              didAddEvent:(NSString*)string;
@end

// A C++ bridge class to handle receiving notifications from the C++ class
// that observes the connection type.
class BreadcrumbManagerObserverBridge : public BreadcrumbManagerObserver {
 public:
  explicit BreadcrumbManagerObserverBridge(
      BreadcrumbManagerKeyedService* breadcrumb_manager_keyed_service,
      id<BreadcrumbManagerObserving> observer);
  ~BreadcrumbManagerObserverBridge() override;

 private:
  BreadcrumbManagerObserverBridge(const BreadcrumbManagerObserverBridge&) =
      delete;
  BreadcrumbManagerObserverBridge& operator=(
      const BreadcrumbManagerObserverBridge&) = delete;

  // BreadcrumbManagerObserver implementation:
  void EventAdded(BreadcrumbManagerKeyedService* manager,
                  const std::string& event) override;

  BreadcrumbManagerKeyedService* breadcrumb_manager_keyed_service_ = nullptr;
  __weak id<BreadcrumbManagerObserving> observer_ = nil;
};

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_MANAGER_OBSERVER_BRIDGE_H_
