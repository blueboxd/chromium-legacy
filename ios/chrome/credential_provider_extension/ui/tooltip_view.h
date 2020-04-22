// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_TOOLTIP_VIEW_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_TOOLTIP_VIEW_H_

#import <UIKit/UIKit.h>

@interface TooltipView : UIView

// Init with the target and |action| parameter-less selector.
- (instancetype)initWithKeyWindow:(UIView*)keyWindow
                           target:(NSObject*)target
                           action:(SEL)action;

// Shows the tooltip with given |message| below the |view|.
- (void)showMessage:(NSString*)message atBottomOf:(UIView*)view;

// Hides this tooltip.
- (void)hide;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_TOOLTIP_VIEW_H_
