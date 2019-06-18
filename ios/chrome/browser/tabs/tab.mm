// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_private.h"

#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/open_in/open_in_controller.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/web_state_impl.h"
#include "net/base/filename_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif


@interface Tab ()<CRWWebStateObserver> {
  // Browser state associated with this Tab.
  ios::ChromeBrowserState* _browserState;

  OpenInController* _openInController;

  // WebStateImpl for this tab.
  web::WebStateImpl* _webStateImpl;

  // Allows Tab to conform CRWWebStateDelegate protocol.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;

}

// Returns the OpenInController for this tab.
- (OpenInController*)openInController;

// Handles exportable files if possible.
- (void)handleExportableFile:(net::HttpResponseHeaders*)headers;

@end

@implementation Tab

#pragma mark - Initializers

- (instancetype)initWithWebState:(web::WebState*)webState {
  DCHECK(webState);
  self = [super init];
  if (self) {
    // TODO(crbug.com/620465): Tab should only use public API of WebState.
    // Remove this cast once this is the case.
    _webStateImpl = static_cast<web::WebStateImpl*>(webState);
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    _webStateImpl->AddObserver(_webStateObserver.get());

    _browserState =
        ios::ChromeBrowserState::FromBrowserState(webState->GetBrowserState());
  }
  return self;
}

#pragma mark - NSObject protocol

- (void)dealloc {
  // The WebState owns the Tab, so -webStateDestroyed: should be called before
  // -dealloc and _webStateImpl set to nullptr.
  DCHECK(!_webStateImpl);
}

- (NSString*)description {
  return
      [NSString stringWithFormat:@"%p ... - %s", self,
                                 self.webState->GetVisibleURL().spec().c_str()];
}

#pragma mark - Properties

- (web::WebState*)webState {
  return _webStateImpl;
}

#pragma mark - Public API

- (UIView*)viewForPrinting {
  return self.webController.viewForPrinting;
}

// Halt the tab, which amounts to halting its webController.
- (void)terminateNetworkActivity {
  [self.webController terminateNetworkActivity];
}

- (void)dismissModals {
  [_openInController disable];
  [self.webController dismissModals];
}

#pragma mark - CRWWebStateObserver protocol

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  [_openInController disable];
}

- (void)webState:(web::WebState*)webState
    didLoadPageWithSuccess:(BOOL)loadSuccess {
  if (loadSuccess) {
    scoped_refptr<net::HttpResponseHeaders> headers =
        _webStateImpl->GetHttpResponseHeaders();
    [self handleExportableFile:headers.get()];
  }
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webStateImpl, webState);

  [_openInController detachFromWebState];
  _openInController = nil;

  _webStateImpl->RemoveObserver(_webStateObserver.get());
  _webStateObserver.reset();
  _webStateImpl = nullptr;
}

#pragma mark - Private methods

- (OpenInController*)openInController {
  if (!_openInController) {
    _openInController = [[OpenInController alloc]
        initWithURLLoaderFactory:_browserState->GetSharedURLLoaderFactory()
                        webState:self.webState];
    // Previously evicted tabs should be reloaded before this method is called.
    DCHECK(!self.webState->IsEvicted());
    self.webState->GetNavigationManager()->LoadIfNecessary();
    _openInController.baseView = self.webState->GetView();
  }
  return _openInController;
}

- (void)handleExportableFile:(net::HttpResponseHeaders*)headers {
  // Only "application/pdf" is supported for now.
  if (self.webState->GetContentsMimeType() != "application/pdf")
    return;

  // Try to generate a filename by first looking at |content_disposition_|, then
  // at the last component of WebState's last committed URL and if both of these
  // fail use the default filename "document".
  std::string contentDisposition;
  if (headers)
    headers->GetNormalizedHeader("content-disposition", &contentDisposition);
  std::string defaultFilename =
      l10n_util::GetStringUTF8(IDS_IOS_OPEN_IN_FILE_DEFAULT_TITLE);
  web::NavigationItem* item =
      self.webState->GetNavigationManager()->GetLastCommittedItem();
  const GURL& lastCommittedURL = item ? item->GetURL() : GURL::EmptyGURL();
  base::string16 filename =
      net::GetSuggestedFilename(lastCommittedURL, contentDisposition,
                                "",                 // referrer-charset
                                "",                 // suggested-name
                                "application/pdf",  // mime-type
                                defaultFilename);
  [[self openInController]
      enableWithDocumentURL:lastCommittedURL
          suggestedFilename:base::SysUTF16ToNSString(filename)];
}

@end

#pragma mark - TestingSupport

@implementation Tab (TestingSupport)

// TODO(crbug.com/620465): this require the Tab's WebState to be a WebStateImpl,
// remove this helper once this is no longer true (and fix the unit tests).
- (CRWWebController*)webController {
  return _webStateImpl ? _webStateImpl->GetWebController() : nil;
}

@end
