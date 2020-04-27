// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITY_SERVICE_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITY_SERVICE_CONTROLLER_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@protocol ActivityServicePositioner;
@protocol ActivityServicePresentation;
@protocol BrowserCommands;
class ChromeBrowserState;
@class ShareToData;
@protocol SnackbarCommands;

// Snackbar ID for any services that wish to show snackbars.
extern NSString* const kActivityServicesSnackbarCategory;

// Controller to show the built-in services (e.g. Copy, Printing) and services
// offered by iOS App Extensions (Share, Action).
@interface ActivityServiceController : NSObject

// Return the singleton ActivityServiceController.
+ (instancetype)sharedInstance;

// Returns YES if a share is currently in progress.
- (BOOL)isActive;

// Cancels share operation.
- (void)cancelShareAnimated:(BOOL)animated;

// Shares the given data. The given providers must not be nil.  On iPad, the
// |positionProvider| must return a non-nil view and a non-zero size.
- (void)shareWithData:(ShareToData*)data
            browserState:(ChromeBrowserState*)browserState
              dispatcher:(id<BrowserCommands, SnackbarCommands>)dispatcher
        positionProvider:(id<ActivityServicePositioner>)positionProvider
    presentationProvider:(id<ActivityServicePresentation>)presentationProvider;

@end

#endif  // IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITY_SERVICE_CONTROLLER_H_
