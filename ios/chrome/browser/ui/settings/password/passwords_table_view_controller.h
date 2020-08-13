// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/password/legacy_password_details_table_view_controller_delegate.h"
#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

class Browser;
@protocol ReauthenticationProtocol;
@class PasswordExporter;

@interface PasswordsTableViewController
    : SettingsRootTableViewController <SettingsControllerProtocol>

// The designated initializer. |browser| must not be nil.
- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

// Starts password check.
- (void)startPasswordCheck;

@end

@interface PasswordsTableViewController (Testing) <
    LegacyPasswordDetailsTableViewControllerDelegate>

// Initializes the password exporter with a (fake) |reauthenticationModule|.
- (void)setReauthenticationModuleForExporter:
    (id<ReauthenticationProtocol>)reauthenticationModule;

// Returns the password exporter to allow setting fake testing objects on it.
- (PasswordExporter*)getPasswordExporter;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_TABLE_VIEW_CONTROLLER_H_
