// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITY_SERVICE_LEGACY_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITY_SERVICE_LEGACY_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

@protocol ActivityServicePositioner;
@protocol ActivityServicePresentation;

// ActivityServiceLegacyCoordinator provides a public interface for the share
// menu feature.
@interface ActivityServiceLegacyCoordinator : ChromeCoordinator

// Providers.
@property(nonatomic, readwrite, weak) id<ActivityServicePositioner>
    positionProvider;
@property(nonatomic, readwrite, weak) id<ActivityServicePresentation>
    presentationProvider;

// Cancels any in-progress share activities and dismisses the corresponding UI.
- (void)cancelShare;

// Shows an error alert with the given |title| and body |message|.
- (void)showErrorAlertWithStringTitle:(NSString*)title
                              message:(NSString*)message;

@end

#endif  // IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITY_SERVICE_LEGACY_COORDINATOR_H_
