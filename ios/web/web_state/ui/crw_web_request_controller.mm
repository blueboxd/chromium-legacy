// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_web_request_controller.h"

#import <WebKit/WebKit.h>

#include "base/feature_list.h"
#import "base/ios/block_types.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/web/common/features.h"
#include "ios/web/common/referrer_util.h"
#import "ios/web/navigation/crw_wk_navigation_handler.h"
#import "ios/web/navigation/crw_wk_navigation_states.h"
#import "ios/web/navigation/navigation_context_impl.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/navigation/wk_back_forward_list_item_holder.h"
#import "ios/web/navigation/wk_navigation_util.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state/web_state.h"
#import "ios/web/web_state/web_state_impl.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using web::wk_navigation_util::ExtractUrlFromPlaceholderUrl;
using web::wk_navigation_util::IsPlaceholderUrl;
using web::wk_navigation_util::kReferrerHeaderName;

namespace {
// Values for the histogram that counts slow/fast back/forward navigations.
enum class BackForwardNavigationType {
  // Fast back navigation through WKWebView back-forward list.
  FAST_BACK = 0,
  // Slow back navigation when back-forward list navigation is not possible.
  SLOW_BACK = 1,
  // Fast forward navigation through WKWebView back-forward list.
  FAST_FORWARD = 2,
  // Slow forward navigation when back-forward list navigation is not possible.
  SLOW_FORWARD = 3,
  BACK_FORWARD_NAVIGATION_TYPE_COUNT
};
}

@interface CRWWebRequestController ()

@property(nonatomic, readonly) web::WebStateImpl* webState;

@end

@implementation CRWWebRequestController

- (instancetype)initWithWebState:(web::WebStateImpl*)webState {
  self = [super init];
  if (self) {
    _webState = webState;
  }
  return self;
}

- (void)
    loadRequestForCurrentNavigationItemInWebView:(WKWebView*)webView
                                      itemHolder:
                                          (web::WKBackForwardListItemHolder*)
                                              holder {
  DCHECK(webView);
  DCHECK(self.currentNavItem);
  // If a load is kicked off on a WKWebView with a frame whose size is {0, 0} or
  // that has a negative dimension for a size, rendering issues occur that
  // manifest in erroneous scrolling and tap handling (crbug.com/574996,
  // crbug.com/577793).
  DCHECK_GT(CGRectGetWidth(webView.frame), 0.0);
  DCHECK_GT(CGRectGetHeight(webView.frame), 0.0);

  // If the current item uses a different user agent from that is currently used
  // in the web view, update |customUserAgent| property, which will be used by
  // the next request sent by this web view.
  web::UserAgentType itemUserAgentType =
      self.currentNavItem->GetUserAgentType();
  if (itemUserAgentType != web::UserAgentType::NONE) {
    NSString* userAgentString = base::SysUTF8ToNSString(
        web::GetWebClient()->GetUserAgent(itemUserAgentType));
    if (![webView.customUserAgent isEqualToString:userAgentString]) {
      webView.customUserAgent = userAgentString;
    }
  }

  BOOL repostedForm =
      [holder->http_method() isEqual:@"POST"] &&
      (holder->navigation_type() == WKNavigationTypeFormResubmitted ||
       holder->navigation_type() == WKNavigationTypeFormSubmitted);
  web::NavigationItemImpl* currentItem = self.currentNavItem;
  NSData* POSTData = currentItem->GetPostData();
  NSMutableURLRequest* request = [self requestForCurrentNavigationItem];

  BOOL sameDocumentNavigation = currentItem->IsCreatedFromPushState() ||
                                currentItem->IsCreatedFromHashChange();

  if (holder->back_forward_list_item()) {
    // Check if holder's WKBackForwardListItem still correctly represents
    // navigation item. With LegacyNavigationManager, replaceState operation
    // creates a new navigation item, leaving the old item committed. That
    // old committed item will be associated with WKBackForwardListItem whose
    // state was replaced. So old item won't have correct WKBackForwardListItem.
    if (net::GURLWithNSURL(holder->back_forward_list_item().URL) !=
        currentItem->GetURL()) {
      // The state was replaced for this item. The item should not be a part of
      // committed items, but it's too late to remove the item. Cleaup
      // WKBackForwardListItem and mark item with "state replaced" flag.
      currentItem->SetHasStateBeenReplaced(true);
      holder->set_back_forward_list_item(nil);
    }
  }

  // If the request has POST data and is not a repost form, configure the POST
  // request.
  if (POSTData.length && !repostedForm) {
    [request setHTTPMethod:@"POST"];
    [request setHTTPBody:POSTData];
    [request setAllHTTPHeaderFields:self.currentHTTPHeaders];
  }

  ProceduralBlock defaultNavigationBlock = ^{
    web::NavigationItem* item = self.currentNavItem;
    GURL navigationURL = item ? item->GetURL() : GURL::EmptyGURL();
    GURL virtualURL = item ? item->GetVirtualURL() : GURL::EmptyGURL();
    // Set |item| to nullptr here to avoid any use-after-free issues, as it can
    // be cleared by the call to -registerLoadRequestForURL below.
    item = nullptr;
    GURL contextURL = IsPlaceholderUrl(navigationURL)
                          ? ExtractUrlFromPlaceholderUrl(navigationURL)
                          : navigationURL;
    std::unique_ptr<web::NavigationContextImpl> navigationContext =
        [_delegate webRequestController:self
              registerLoadRequestForURL:contextURL
                               referrer:self.currentNavItemReferrer
                             transition:self.currentTransition
                 sameDocumentNavigation:sameDocumentNavigation
                         hasUserGesture:YES
                      rendererInitiated:NO
                  placeholderNavigation:IsPlaceholderUrl(navigationURL)];

    if (web::GetWebClient()->IsSlimNavigationManagerEnabled() &&
        self.navigationManagerImpl->IsRestoreSessionInProgress()) {
      [_delegate webRequestControllerDisconnectScrollViewProxy:self];
    }

    WKNavigation* navigation = nil;
    if (navigationURL.SchemeIsFile() &&
        web::GetWebClient()->IsAppSpecificURL(virtualURL)) {
      // file:// URL navigations are allowed for app-specific URLs, which
      // already have elevated privileges.
      NSURL* navigationNSURL = net::NSURLWithGURL(navigationURL);
      navigation = [webView loadFileURL:navigationNSURL
                allowingReadAccessToURL:navigationNSURL];
    } else {
      navigation = [webView loadRequest:request];
    }
    [self.navigationHandler.navigationStates
             setState:web::WKNavigationState::REQUESTED
        forNavigation:navigation];
    [self.navigationHandler.navigationStates
           setContext:std::move(navigationContext)
        forNavigation:navigation];
    [self reportBackForwardNavigationTypeForFastNavigation:NO];
  };

  // When navigating via WKBackForwardListItem to pages created or updated by
  // calls to pushState() and replaceState(), sometimes web_bundle.js is not
  // injected correctly.  This means that calling window.history navigation
  // functions will invoke WKWebView's non-overridden implementations, causing a
  // mismatch between the WKBackForwardList and NavigationManager.
  // TODO(crbug.com/659816): Figure out how to prevent web_bundle.js injection
  // flake.
  if (currentItem->HasStateBeenReplaced() ||
      currentItem->IsCreatedFromPushState()) {
    defaultNavigationBlock();
    return;
  }

  // If there is no corresponding WKBackForwardListItem, or the item is not in
  // the current WKWebView's back-forward list, navigating using WKWebView API
  // is not possible. In this case, fall back to the default navigation
  // mechanism.
  if (!holder->back_forward_list_item() ||
      ![self isBackForwardListItemValid:holder->back_forward_list_item()
                                webView:webView]) {
    defaultNavigationBlock();
    return;
  }

  ProceduralBlock webViewNavigationBlock = ^{
    // If the current navigation URL is the same as the URL of the visible
    // page, that means the user requested a reload. |goToBackForwardListItem|
    // will be a no-op when it is passed the current back forward list item,
    // so |reload| must be explicitly called.
    web::NavigationItem* item = self.currentNavItem;
    GURL navigationURL = item ? item->GetURL() : GURL::EmptyGURL();
    std::unique_ptr<web::NavigationContextImpl> navigationContext =
        [_delegate webRequestController:self
              registerLoadRequestForURL:navigationURL
                               referrer:self.currentNavItemReferrer
                             transition:self.currentTransition
                 sameDocumentNavigation:sameDocumentNavigation
                         hasUserGesture:YES
                      rendererInitiated:NO
                  placeholderNavigation:NO];
    WKNavigation* navigation = nil;
    if (navigationURL == net::GURLWithNSURL(webView.URL)) {
      navigation = [webView reload];
    } else {
      // |didCommitNavigation:| may not be called for fast navigation, so update
      // the navigation type now as it is already known.
      navigationContext->SetWKNavigationType(WKNavigationTypeBackForward);
      navigationContext->SetMimeType(holder->mime_type());
      holder->set_navigation_type(WKNavigationTypeBackForward);
      navigation =
          [webView goToBackForwardListItem:holder->back_forward_list_item()];
      [self reportBackForwardNavigationTypeForFastNavigation:YES];
    }
    [self.navigationHandler.navigationStates
             setState:web::WKNavigationState::REQUESTED
        forNavigation:navigation];
    [self.navigationHandler.navigationStates
           setContext:std::move(navigationContext)
        forNavigation:navigation];
  };

  // If the request is not a form submission or resubmission, or the user
  // doesn't need to confirm the load, then continue right away.

  if (!repostedForm || currentItem->ShouldSkipRepostFormConfirmation()) {
    webViewNavigationBlock();
    return;
  }

  // If the request is form submission or resubmission, then prompt the
  // user before proceeding.
  DCHECK(repostedForm);
  DCHECK(!web::GetWebClient()->IsSlimNavigationManagerEnabled());
  self.webState->ShowRepostFormWarningDialog(
      base::BindOnce(^(bool shouldContinue) {
        if ([_delegate webRequestControllerIsBeingDestroyed:self])
          return;

        if (shouldContinue)
          webViewNavigationBlock();
        else
          [_delegate webRequestControllerStopLoading:self];
      }));
}

// Reports Navigation.IOSWKWebViewSlowFastBackForward UMA. No-op if pending
// navigation is not back forward navigation.
- (void)reportBackForwardNavigationTypeForFastNavigation:(BOOL)isFast {
  web::NavigationManager* navigationManager = self.navigationManagerImpl;
  int pendingIndex = navigationManager->GetPendingItemIndex();
  if (pendingIndex == -1) {
    // Pending navigation is not a back forward navigation.
    return;
  }

  BOOL isBack = pendingIndex < navigationManager->GetLastCommittedItemIndex();
  BackForwardNavigationType type = BackForwardNavigationType::FAST_BACK;
  if (isBack) {
    type = isFast ? BackForwardNavigationType::FAST_BACK
                  : BackForwardNavigationType::SLOW_BACK;
  } else {
    type = isFast ? BackForwardNavigationType::FAST_FORWARD
                  : BackForwardNavigationType::SLOW_FORWARD;
  }

  UMA_HISTOGRAM_ENUMERATION(
      "Navigation.IOSWKWebViewSlowFastBackForward", type,
      BackForwardNavigationType::BACK_FORWARD_NAVIGATION_TYPE_COUNT);
}

// Returns a NSMutableURLRequest that represents the current NavigationItem.
- (NSMutableURLRequest*)requestForCurrentNavigationItem {
  web::NavigationItem* item = self.currentNavItem;
  const GURL currentNavigationURL = item ? item->GetURL() : GURL::EmptyGURL();
  NSMutableURLRequest* request = [NSMutableURLRequest
      requestWithURL:net::NSURLWithGURL(currentNavigationURL)];
  const web::Referrer referrer(self.currentNavItemReferrer);
  if (referrer.url.is_valid()) {
    std::string referrerValue =
        web::ReferrerHeaderValueForNavigation(currentNavigationURL, referrer);
    if (!referrerValue.empty()) {
      [request setValue:base::SysUTF8ToNSString(referrerValue)
          forHTTPHeaderField:kReferrerHeaderName];
    }
  }

  // If there are headers in the current session entry add them to |request|.
  // Headers that would overwrite fields already present in |request| are
  // skipped.
  NSDictionary* headers = self.currentHTTPHeaders;
  for (NSString* headerName in headers) {
    if (![request valueForHTTPHeaderField:headerName]) {
      [request setValue:[headers objectForKey:headerName]
          forHTTPHeaderField:headerName];
    }
  }

  return request;
}

// Returns YES if the given WKBackForwardListItem is valid to use for
// navigation.
- (BOOL)isBackForwardListItemValid:(WKBackForwardListItem*)item
                           webView:(WKWebView*)webView {
  // The current back-forward list item MUST be in the WKWebView's back-forward
  // list to be valid.
  WKBackForwardList* list = webView.backForwardList;
  return list.currentItem == item ||
         [list.forwardList indexOfObject:item] != NSNotFound ||
         [list.backList indexOfObject:item] != NSNotFound;
}

#pragma mark Navigation and Session Information

// Returns the associated NavigationManagerImpl.
- (web::NavigationManagerImpl*)navigationManagerImpl {
  return self.webState ? &(self.webState->GetNavigationManagerImpl()) : nil;
}

// Returns the navigation item for the current page.
- (web::NavigationItemImpl*)currentNavItem {
  return self.navigationManagerImpl
             ? self.navigationManagerImpl->GetCurrentItemImpl()
             : nullptr;
}

// Returns the current transition type.
- (ui::PageTransition)currentTransition {
  if (self.currentNavItem)
    return self.currentNavItem->GetTransitionType();
  else
    return ui::PageTransitionFromInt(0);
}

// Returns the referrer for current navigation item. May be empty.
- (web::Referrer)currentNavItemReferrer {
  web::NavigationItem* currentItem = self.currentNavItem;
  return currentItem ? currentItem->GetReferrer() : web::Referrer();
}

// The HTTP headers associated with the current navigation item. These are nil
// unless the request was a POST.
- (NSDictionary*)currentHTTPHeaders {
  web::NavigationItem* currentItem = self.currentNavItem;
  return currentItem ? currentItem->GetHttpRequestHeaders() : nil;
}

@end
