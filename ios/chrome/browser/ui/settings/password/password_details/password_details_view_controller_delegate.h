// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_VIEW_CONTROLLER_DELEGATE_H_

@class PasswordDetails;
@class PasswordDetailsViewController;

@protocol PasswordDetailsViewControllerDelegate

// Called when user finished editing a password.
- (void)passwordDetailsViewController:
            (PasswordDetailsViewController*)viewController
               didEditPasswordDetails:(PasswordDetails*)password;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_VIEW_CONTROLLER_DELEGATE_H_
