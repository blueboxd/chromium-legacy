// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_PRESENTATION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_PRESENTATION_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol OverlayPresentationControllerObserver;

// Presentation controller used for overlays presented using custom
// UIViewController presentation.
@interface OverlayPresentationController : UIPresentationController

// Whether the presentation controller resizes the presentation container view.
// When set to YES, the overlay presentation context view will be resized to fit
// the presented view so that touches that fall outside of the overlay can be
// forwarded to the underlying browser UI.  Presentation controllers that return
// YES for this property must not lay out their presented views in relation to
// the presenter.  Returns NO by default.
@property(nonatomic, readonly) BOOL resizesPresentationContainer;

// Subclasses must notify the superclass when their container views lay out
// their subviews.
- (void)containerViewWillLayoutSubviews NS_REQUIRES_SUPER;

@end

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_PRESENTATION_CONTROLLER_H_
