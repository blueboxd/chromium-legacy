// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CRW_WEB_REQUEST_CONTROLLER_H_
#define IOS_WEB_WEB_STATE_UI_CRW_WEB_REQUEST_CONTROLLER_H_

#import <WebKit/WebKit.h>

#include "ios/web/public/referrer.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

@class WKWebView;
@class CRWWebRequestController;
@class CRWWKNavigationHandler;

namespace web {
class NavigationContextImpl;
class WebStateImpl;
class WKBackForwardListItemHolder;
}  // namespace web

@protocol CRWWebRequestControllerDelegate <NSObject>

// The delegate should stop loading the page.
- (void)webRequestControllerStopLoading:
    (CRWWebRequestController*)requestController;

// Whether the delegate is being destroyed.
- (BOOL)webRequestControllerIsBeingDestroyed:
    (CRWWebRequestController*)requestController;

// Prepares web controller and delegates for anticipated page change.
// Allows several methods to invoke webWill/DidAddPendingURL on anticipated page
// change, using the same cached request and calculated transition types.
// Returns navigation context for this request.
- (std::unique_ptr<web::NavigationContextImpl>)
         webRequestController:(CRWWebRequestController*)requestController
    registerLoadRequestForURL:(const GURL&)requestURL
                     referrer:(const web::Referrer&)referrer
                   transition:(ui::PageTransition)transition
       sameDocumentNavigation:(BOOL)sameDocumentNavigation
               hasUserGesture:(BOOL)hasUserGesture
            rendererInitiated:(BOOL)rendererInitiated
        placeholderNavigation:(BOOL)placeholderNavigation;

// Asks proxy to disconnect scroll proxy if needed.
- (void)webRequestControllerDisconnectScrollViewProxy:
    (CRWWebRequestController*)requestController;

@end

// Controller in charge of preparing and handling web requests for the delegate,
// which should be |CrwWebController|.
@interface CRWWebRequestController : NSObject

@property(nonatomic, weak) id<CRWWebRequestControllerDelegate> delegate;

// The WKNavigationDelegate handler class.
@property(nonatomic, weak) CRWWKNavigationHandler* navigationHandler;

- (instancetype)initWithWebState:(web::WebStateImpl*)webState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Loads request for the URL of the current navigation item.
- (void)
    loadRequestForCurrentNavigationItemInWebView:(WKWebView*)webView
                                      itemHolder:
                                          (web::WKBackForwardListItemHolder*)
                                              holder;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_WEB_REQUEST_CONTROLLER_H_
