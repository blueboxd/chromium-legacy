// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITY_SERVICE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITY_SERVICE_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

@protocol ActivityServicePositioner;
@protocol ActivityServicePresentation;

// ActivityServiceCoordinator provides a public interface for the share
// menu feature.
@interface ActivityServiceCoordinator : ChromeCoordinator

// Provider of the share action location.
@property(nonatomic, readwrite, weak) id<ActivityServicePositioner>
    positionProvider;

// Image that should be shared via the activity view. When set, will trigger
// the share image experience.
@property(nonatomic, strong) UIImage* image;

// Title of the content that will be shared.
@property(nonatomic, strong) NSString* title;

// Provider of share action presentation.
@property(nonatomic, readwrite, weak) id<ActivityServicePresentation>
    presentationProvider;

@end

#endif  // IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITY_SERVICE_COORDINATOR_H_
