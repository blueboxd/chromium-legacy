// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_ELEMENTS_POPOVER_LABEL_VIEW_CONTROLLER_H_
#define IOS_CHROME_COMMON_UI_ELEMENTS_POPOVER_LABEL_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// Static popover presenting a simple message.
@interface PopoverLabelViewController : UIViewController

// |Presenter| is used to dismiss this view controller when done.
- (instancetype)initWithMessage:(NSString*)message NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
@end

#endif  // IOS_CHROME_COMMON_UI_ELEMENTS_POPOVER_LABEL_VIEW_CONTROLLER_H_
