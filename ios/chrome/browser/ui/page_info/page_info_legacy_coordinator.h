// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_LEGACY_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_LEGACY_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

@protocol PageInfoPresentation;

// Notification sent when the page info is shown.
extern NSString* const kPageInfoWillShowNotification;
// Notification sent when the page info is hidden.
extern NSString* const kPageInfoWillHideNotification;

// The coordinator that manages the display of the Page Info UI. When
// |dispatcher| is set, this coordinator uses |dispatcher| to register itself
// as the target for PageInfoCommmands. These commands can then trigger the
// showing/hiding of the Page Info UI. The PageInfo UI must have been hidden
// before |-stop| is called.
@interface PageInfoLegacyCoordinator : ChromeCoordinator

// Use -initWithBaseViewController:browser:
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
    NS_UNAVAILABLE;

// |presentationProvider| provides information and runs tasks necessary to
// present Page Info.
@property(nonatomic, weak) id<PageInfoPresentation> presentationProvider;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_LEGACY_COORDINATOR_H_
