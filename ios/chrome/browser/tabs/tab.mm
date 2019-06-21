// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_private.h"

#import "ios/web/public/web_state/web_state_observer_bridge.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/web_state_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif


@interface Tab ()<CRWWebStateObserver> {
  // WebStateImpl for this tab.
  web::WebStateImpl* _webStateImpl;

  // Allows Tab to conform CRWWebStateDelegate protocol.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;

}

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
  [self.webController dismissModals];
}

#pragma mark - CRWWebStateObserver protocol

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webStateImpl, webState);

  _webStateImpl->RemoveObserver(_webStateObserver.get());
  _webStateObserver.reset();
  _webStateImpl = nullptr;
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
