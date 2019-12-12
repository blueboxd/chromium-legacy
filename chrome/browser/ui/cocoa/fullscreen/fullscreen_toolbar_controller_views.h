// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_FULLSCREEN_FULLSCREEN_TOOLBAR_CONTROLLER_VIEWS_H_
#define CHROME_BROWSER_UI_COCOA_FULLSCREEN_FULLSCREEN_TOOLBAR_CONTROLLER_VIEWS_H_

#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_controller.h"

#include "base/mac/scoped_nsobject.h"

@class BridgedContentView;
class BrowserView;

// Provides a controller to the fullscreen toolbar for a single Views based
// browser window.
@interface FullscreenToolbarControllerViews
    : FullscreenToolbarController<FullscreenToolbarContextDelegate> {
 @private
  BrowserView* browserView_;  // weak

  // Since dealloc() may need to access the native view, we retain it here.
  base::scoped_nsobject<BridgedContentView> ns_view_;
}

// Designated initializer.
- (id)initWithBrowserView:(BrowserView*)browserView;

// Updates the toolbar by updating the layout.
- (void)layoutToolbar;

@end

#endif  //
CHROME_BROWSER_UI_COCOA_FULLSCREEN_FULLSCREEN_TOOLBAR_CONTROLLER_VIEWS_H_
