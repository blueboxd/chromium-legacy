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
@class CRWLegacyNativeContentController;

namespace web {
class NavigationContextImpl;
class WebStateImpl;
class UserInteractionState;
}  // namespace web

@protocol CRWWebRequestControllerDelegate <NSObject>

// The delegate should stop loading the page.
- (void)webRequestControllerStopLoading:
    (CRWWebRequestController*)requestController;

// Asks proxy to disconnect scroll proxy if needed.
- (void)webRequestControllerDisconnectScrollViewProxy:
    (CRWWebRequestController*)requestController;

// Asks the delegate for the associated |UserInteractionState|.
- (web::UserInteractionState*)webRequestControllerUserInteractionState:
    (CRWWebRequestController*)requestController;

// Asks the delegate for the associated |CRWLegacyNativeContentController|.
- (CRWLegacyNativeContentController*)
    webRequestControllerLegacyNativeContentController:
        (CRWWebRequestController*)requestController;

// Tells the delegate to record the state (scroll position, form values,
// whatever can be harvested) from the current page into the current session
// entry.
- (void)webRequestControllerRecordStateInHistory:
    (CRWWebRequestController*)requestController;

// Tells the delegate to restores the state for current page from session
// history.
- (void)webRequestControllerRestoreStateFromHistory:
    (CRWWebRequestController*)requestController;

- (WKWebView*)webRequestControllerWebView:
    (CRWWebRequestController*)requestController;

- (CRWWKNavigationHandler*)webRequestControllerNavigationHandler:
    (CRWWebRequestController*)requestController;

@end

// Controller in charge of preparing and handling web requests for the delegate,
// which should be |CrwWebController|.
@interface CRWWebRequestController : NSObject

@property(nonatomic, weak) id<CRWWebRequestControllerDelegate> delegate;

- (instancetype)initWithWebState:(web::WebStateImpl*)webState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Instructs the receiver to close. This should be called when the receiver's
// owner is being destroyed. The state of the receiver will be set to
// "isBeingDestroyed" after this is called.
- (void)close;

// Checks if a load request of the current navigation item should proceed. If
// this returns |YES|, caller should create a webView and call
// |loadRequestForCurrentNavigationItem|. Otherwise this will set the request
// state to finished and call |didFinishWithURL| with failure.
- (BOOL)maybeLoadRequestForCurrentNavigationItem;

// Loads request for the URL of the current navigation item. Subclasses may
// choose to build a new NSURLRequest and call
// |loadRequestForCurrentNavigationItem| on the underlying web view, or use
// native web view navigation where possible (for example, going back and
// forward through the history stack).
- (void)loadRequestForCurrentNavigationItem;

// Should be called by owner component after URL is finished loading and
// self.navigationHandler.navigationState is set to FINISHED. |context| contains
// information about the navigation associated with the URL. It is nil if
// currentURL is invalid.
- (void)didFinishWithURL:(const GURL&)currentURL
             loadSuccess:(BOOL)loadSuccess
                 context:(const web::NavigationContextImpl*)context;

// Calls |registerLoadRequestForURL| with empty referrer and link or client
// redirect transition based on user interaction state. Returns navigation
// context for this request.
- (std::unique_ptr<web::NavigationContextImpl>)
    registerLoadRequestForURL:(const GURL&)URL
       sameDocumentNavigation:(BOOL)sameDocumentNavigation
               hasUserGesture:(BOOL)hasUserGesture
            rendererInitiated:(BOOL)rendererInitiated
        placeholderNavigation:(BOOL)placeholderNavigation;

// Creates a page change request and registers it with the navigation handler.
// Returns navigation context for this request.
- (std::unique_ptr<web::NavigationContextImpl>)
    registerLoadRequestForURL:(const GURL&)requestURL
                     referrer:(const web::Referrer&)referrer
                   transition:(ui::PageTransition)transition
       sameDocumentNavigation:(BOOL)sameDocumentNavigation
               hasUserGesture:(BOOL)hasUserGesture
            rendererInitiated:(BOOL)rendererInitiated
        placeholderNavigation:(BOOL)placeholderNavigation;

// Loads |data| of type |MIMEType| and replaces last committed URL with the
// given |URL|.
- (void)loadData:(NSData*)data
        MIMEType:(NSString*)MIMEType
          forURL:(const GURL&)URL;

// Loads |HTML| into the page and use |URL| to resolve relative URLs within the
// document.
- (void)loadHTML:(NSString*)HTML forURL:(const GURL&)URL;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_WEB_REQUEST_CONTROLLER_H_
