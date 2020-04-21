// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/metric_kit_subscribing_util.h"

#import <Foundation/Foundation.h>
#import <MetricKit/MetricKit.h>

#import "ios/chrome/app/application_delegate/metric_kit_subscriber.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The Experimental setting that enables the collection of MetricKit reports.
NSString* const kEnableMetricKit = @"EnableMetricKit";

}  // namespace

void EnableMetricKitReportCollection() {
  static dispatch_once_t once_token;
  dispatch_once(&once_token, ^{
    if (@available(iOS 13, *)) {
      NSUserDefaults* standard_defaults = [NSUserDefaults standardUserDefaults];
      if ([standard_defaults boolForKey:kEnableMetricKit]) {
        [[MXMetricManager sharedManager]
            addSubscriber:[MetricKitSubscriber sharedInstance]];
      }
    }
  });
}
