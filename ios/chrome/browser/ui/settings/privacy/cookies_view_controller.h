// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_COOKIES_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_COOKIES_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

#import "ios/chrome/browser/ui/settings/privacy/cookies_consumer.h"

@class PrivacyCookiesViewController;

@protocol PrivacyCookiesCommands;

// Delegate for presentation events related to
// PrivacyCookiesViewController.
@protocol PrivacyCookiesViewControllerPresentationDelegate

// Called when the view controller is removed from its parent.
- (void)privacyCookiesViewControllerWasRemoved:
    (PrivacyCookiesViewController*)controller;

@end

// View Controller for displaying the Cookies screen.
@interface PrivacyCookiesViewController
    : SettingsRootTableViewController <PrivacyCookiesConsumer>

// Presentation delegate.
@property(nonatomic, weak) id<PrivacyCookiesViewControllerPresentationDelegate>
    presentationDelegate;

// Handler used to update Cookies settings.
@property(nonatomic, weak) id<PrivacyCookiesCommands> handler;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_COOKIES_VIEW_CONTROLLER_H_
