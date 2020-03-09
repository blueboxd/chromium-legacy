// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_LEGACY_PAGE_INFO_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_LEGACY_PAGE_INFO_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include <memory>

#include "base/memory/weak_ptr.h"

@protocol BrowserCommands;
@class PageInfoSiteSecurityDescription;
@protocol PageInfoPresentation;

// The view controller for the page info view.
@interface LegacyPageInfoViewController : NSObject
// Designated initializer.
// The |sourcePoint| parameter should be in the coordinate system of
// |provider|'s view. Typically, |sourcePoint| would be the midpoint of a button
// that resulted in this popup being displayed.
- (id)initWithModel:(PageInfoSiteSecurityDescription*)model
             sourcePoint:(CGPoint)sourcePoint
    presentationProvider:(id<PageInfoPresentation>)provider
                 handler:(id<BrowserCommands>)handler;

// Dispatcher for this view controller.
@property(nonatomic, weak) id<BrowserCommands> handler;

// Dismisses the view.
- (void)dismiss;

// Layout the page info view.
- (void)performLayout;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_LEGACY_PAGE_INFO_VIEW_CONTROLLER_H_
