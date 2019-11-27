// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BREACH_LEARN_MORE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BREACH_LEARN_MORE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol PasswordBreachPresenter;

// Static view controller with the password breach learn more information.
@interface PasswordBreachLearnMoreViewController : UIViewController

// |Presenter| is used to dismiss this view controller when done.
- (instancetype)initWithPresenter:(id<PasswordBreachPresenter>)presenter
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
@end

#endif  // IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BREACH_LEARN_MORE_VIEW_CONTROLLER_H_
