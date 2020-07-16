// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_H_

#import <Foundation/Foundation.h>

namespace autofill {
struct PasswordForm;
}

// Object which is used by |PasswordDetailsViewController| to show
// information about password.
@interface PasswordDetails : NSObject

// Short version of website.
@property(nonatomic, strong, readonly) NSString* origin;

// Associated website.
@property(nonatomic, strong, readonly) NSString* website;

// Associated username.
@property(nonatomic, strong, readonly) NSString* username;

// Associated password.
@property(nonatomic, strong) NSString* password;

- (instancetype)initWithPasswordForm:(const autofill::PasswordForm&)form
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_H_
