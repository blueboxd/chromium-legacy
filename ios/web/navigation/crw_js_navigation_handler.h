// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_CRW_JS_NAVIGATION_HANDLER_H_
#define IOS_WEB_NAVIGATION_CRW_JS_NAVIGATION_HANDLER_H_

#import <Foundation/Foundation.h>

namespace web {
class WebStateImpl;
}
@class CRWJSNavigationHandler;

@protocol CRWJSNavigationHandlerDelegate

// Returns the WebStateImpl associated with this handler.
- (web::WebStateImpl*)webStateImplForJSNavigationHandler:
    (CRWJSNavigationHandler*)navigationHandler;

@end

// Handles JS messages related to navigation(e.g. window.history.forward).
@interface CRWJSNavigationHandler : NSObject

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithDelegate:(id<CRWJSNavigationHandlerDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

// Set to YES when a hashchange event is manually dispatched for same-document
// history navigations.
@property(nonatomic, assign) BOOL dispatchingSameDocumentHashChangeEvent;

// Whether the web page is currently performing window.history.pushState or
// window.history.replaceState.
@property(nonatomic, assign) BOOL changingHistoryState;

// Instructs this handler to stop handling js navigation messages.
- (void)close;

@end

#endif  // IOS_WEB_NAVIGATION_CRW_JS_NAVIGATION_HANDLER_H_
