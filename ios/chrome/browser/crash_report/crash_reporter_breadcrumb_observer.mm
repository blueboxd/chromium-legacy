// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/crash_reporter_breadcrumb_observer.h"

#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager.h"
#import "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_observer_bridge.h"
#include "ios/chrome/browser/crash_report/breakpad_helper.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const int kMaxProductDataLength = 255;

@interface CrashReporterBreadcrumbObserver () {
  // Map associating the observed BreadcrumbManager with the corresponding
  // observer bridge instances.
  std::map<BreadcrumbManager*, std::unique_ptr<BreadcrumbManagerObserverBridge>>
      _breadcrumbManagerObservers;

  // Map associating the observed BreadcrumbManagerKeyedServices with the
  // corresponding observer bridge instances.
  std::map<BreadcrumbManagerKeyedService*,
           std::unique_ptr<BreadcrumbManagerObserverBridge>>
      _breadcrumbManagerServiceObservers;

  // A string which stores the received breadcrumbs. Since breakpad limits
  // product data string length, it may be truncated when a new event is added
  // in order to reduce overall memory usage.
  NSMutableString* _breadcrumbs;
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
    _breadcrumbs = [[NSMutableString alloc] init];
  }
  return self;
}

- (void)observeBreadcrumbManager:(BreadcrumbManager*)breadcrumbManager {
  DCHECK(!_breadcrumbManagerObservers[breadcrumbManager]);

  _breadcrumbManagerObservers[breadcrumbManager] =
      std::make_unique<BreadcrumbManagerObserverBridge>(breadcrumbManager,
                                                        self);
}

- (void)stopObservingBreadcrumbManager:(BreadcrumbManager*)breadcrumbManager {
  _breadcrumbManagerObservers.erase(breadcrumbManager);
}

- (void)observeBreadcrumbManagerService:
    (BreadcrumbManagerKeyedService*)breadcrumbManagerService {
  DCHECK(!_breadcrumbManagerServiceObservers[breadcrumbManagerService]);

  _breadcrumbManagerServiceObservers[breadcrumbManagerService] =
      std::make_unique<BreadcrumbManagerObserverBridge>(
          breadcrumbManagerService, self);
}

- (void)stopObservingBreadcrumbManagerService:
    (BreadcrumbManagerKeyedService*)breadcrumbManagerService {
  _breadcrumbManagerServiceObservers[breadcrumbManagerService] = nullptr;
}

#pragma mark - BreadcrumbManagerObserving protocol

- (void)breadcrumbManager:(BreadcrumbManager*)manager
              didAddEvent:(NSString*)event {
  NSString* eventWithSeperator = [NSString stringWithFormat:@"%@\n", event];
  [_breadcrumbs insertString:eventWithSeperator atIndex:0];

  NSUInteger maxBreadcrumbsLength =
      self.breadcrumbsKeyCount * kMaxProductDataLength;
  if (_breadcrumbs.length > maxBreadcrumbsLength) {
    NSRange trimRange = NSMakeRange(maxBreadcrumbsLength,
                                    _breadcrumbs.length - maxBreadcrumbsLength);
    [_breadcrumbs deleteCharactersInRange:trimRange];
  }

  // Cut breadcrumbs strings into multiple pieces and upload with separate keys.
  NSMutableArray* breadcrumbs =
      [[NSMutableArray alloc] initWithCapacity:self.breadcrumbsKeyCount];
  for (NSUInteger i = 0; i < self.breadcrumbsKeyCount &&
                         (i * kMaxProductDataLength) < _breadcrumbs.length;
       i++) {
    NSUInteger location = i * kMaxProductDataLength;
    NSRange range = NSMakeRange(
        location, MIN(kMaxProductDataLength, _breadcrumbs.length - location));
    [breadcrumbs addObject:[_breadcrumbs substringWithRange:range]];
  }
  breakpad_helper::SetBreadcrumbEvents(breadcrumbs);
}

@end
