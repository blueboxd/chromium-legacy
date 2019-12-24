// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/crash_reporter_breadcrumb_observer.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#include "ios/chrome/browser/crash_report/breakpad_helper.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The maximum string length of combined breadcrumb events to store at any given
// time. Breakpad truncates long values, so keeping the string stored here short
// will save on used memory. This will not affect the amount of data attached to
// crash reports (as long as the value matches or exceeds the breakpad maximum).
const int kMaxCombinedBreadcrumbLength = 255;
}

@interface CrashReporterBreadcrumbObserver () {
  // Map associating the observed ChromeBrowserStates with the corresponding
  // observer bridge instances.
  std::map<ios::ChromeBrowserState*,
           std::unique_ptr<BreadcrumbManagerObserverBridge>>
      _breadcrumbManagerObservers;
  // A string which stores the received breadcrumbs. Since breakpad will
  // truncate this string anyway, it is truncated when a new event is added in
  // order to reduce overall memory usage.
  NSMutableString* _breadcrumbsString;
}
@end

@implementation CrashReporterBreadcrumbObserver

+ (CrashReporterBreadcrumbObserver*)uniqueInstance {
  static CrashReporterBreadcrumbObserver* instance =
      [[CrashReporterBreadcrumbObserver alloc] init];
  return instance;
}

- (instancetype)init {
  if ((self = [super init])) {
    _breadcrumbsString = [[NSMutableString alloc] init];
  }
  return self;
}

- (void)observeBrowserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(!_breadcrumbManagerObservers[browserState]);

  BreadcrumbManagerKeyedService* service =
      BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(browserState);
  _breadcrumbManagerObservers[browserState] =
      std::make_unique<BreadcrumbManagerObserverBridge>(service, self);
}

- (void)stopObservingBrowserState:(ios::ChromeBrowserState*)browserState {
  _breadcrumbManagerObservers[browserState] = nullptr;
}

#pragma mark - BreadcrumbManagerObserving protocol

- (void)breadcrumbManager:(BreadcrumbManagerKeyedService*)manager
              didAddEvent:(NSString*)event {
  NSString* eventWithSeperator = [NSString stringWithFormat:@"%@\n", event];
  [_breadcrumbsString insertString:eventWithSeperator atIndex:0];

  if (_breadcrumbsString.length > kMaxCombinedBreadcrumbLength) {
    NSRange trimRange =
        NSMakeRange(kMaxCombinedBreadcrumbLength,
                    _breadcrumbsString.length - kMaxCombinedBreadcrumbLength);
    [_breadcrumbsString deleteCharactersInRange:trimRange];
  }
  breakpad_helper::SetBreadcrumbEvents(_breadcrumbsString);
}

@end
