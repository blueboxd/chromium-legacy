// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/screenshot/screenshot_notification_listener.h"

#import <UIKit/UIKit.h>

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
char const* singleScreenUserActionName = "MobileSingleScreenScreenshot";
}  // namespace

@implementation ScreenshotNotificationListener

#pragma mark - Public

- (void)startListening {
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(collectMetricFromNotification:)
             name:@"UIApplicationUserDidTakeScreenshotNotification"
           object:nil];
}

#pragma mark - Private
/**
 *  If the device does not support multiple scenes or if the iOS version is
 * bellow iOS13 it will record the SingleScreenUserActionName metric.
 *
 *  Otherwise it will record the SingleScreenUserActionName metric only if there
 * is a single window in the foreground.
 */
- (void)collectMetricFromNotification:(NSNotification*)notification {
  UIApplication* sharedApplication = [UIApplication sharedApplication];

  if (@available(iOS 13, *)) {
    if (!sharedApplication.supportsMultipleScenes) {
      // Multi-Window is not supported, thus there is only a single screen
      base::RecordAction(base::UserMetricsAction(singleScreenUserActionName));
      return;
    }

    // Inspect the connectScenes and map the current window scenario
    NSInteger foreground = 0;
    // NSLog(@"I got the following: %@", [sharedApplication connectedScenes]);
    for (UIScene* scene in [sharedApplication connectedScenes]) {
      switch (scene.activationState) {
        case UISceneActivationStateForegroundActive:
          foreground++;
          break;
        case UISceneActivationStateForegroundInactive:
          foreground++;
          break;
        default:
          // Catch for UISceneActivationStateUnattached
          // and UISceneActivationStateBackground
          // TODO (crbug.com/1091818): Add state inpection to identify other
          // scenarios
          break;
      }
    }
    // Only register screenshots taken of chrome in a single screen in the
    // foreground.
    if (foreground == 1) {
      base::RecordAction(base::UserMetricsAction(singleScreenUserActionName));
    }
    return;
  }
  // If we reach this line, then it means the iOS < 13 thus there is only a
  // single window
  base::RecordAction(base::UserMetricsAction(singleScreenUserActionName));
}
@end
