// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_observer_bridge.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BreadcrumbManagerObserverBridge::BreadcrumbManagerObserverBridge(
    BreadcrumbManager* breadcrumb_manager,
    id<BreadcrumbManagerObserving> observer)
    : breadcrumb_manager_(breadcrumb_manager), observer_(observer) {
  DCHECK(observer_);
  breadcrumb_manager_->AddObserver(this);
}

BreadcrumbManagerObserverBridge::BreadcrumbManagerObserverBridge(
    BreadcrumbManagerKeyedService* breadcrumb_manager_service,
    id<BreadcrumbManagerObserving> observer)
    : breadcrumb_manager_service_(breadcrumb_manager_service),
      observer_(observer) {
  DCHECK(observer_);
  breadcrumb_manager_service_->AddObserver(this);
}

BreadcrumbManagerObserverBridge::~BreadcrumbManagerObserverBridge() {
  if (breadcrumb_manager_) {
    breadcrumb_manager_->RemoveObserver(this);
  }
  if (breadcrumb_manager_service_) {
    breadcrumb_manager_service_->RemoveObserver(this);
  }
}

void BreadcrumbManagerObserverBridge::EventAdded(BreadcrumbManager* manager,
                                                 const std::string& event) {
  [observer_ breadcrumbManager:manager
                   didAddEvent:base::SysUTF8ToNSString(event)];
}
