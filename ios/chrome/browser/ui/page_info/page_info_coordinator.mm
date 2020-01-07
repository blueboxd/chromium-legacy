// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/page_info_coordinator.h"

#include "base/logging.h"
#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/page_info/page_info_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PageInfoCoordinator ()

@end

@implementation PageInfoCoordinator

@synthesize presentationProvider = _presentationProvider;

#pragma mark - ChromeCoordinator

- (void)start {
  // TODO(crbug.com/1038919): Implement this.
}

- (void)stop {
  // TODO(crbug.com/1038919): Implement this.
}

@end
