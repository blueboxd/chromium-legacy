// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/crw_js_navigation_handler.h"

#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#include "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/web_state/web_state_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kCommandPrefix[] = "navigation";
}  // namespace

@interface CRWJSNavigationHandler ()

@property(nonatomic, weak) id<CRWJSNavigationHandlerDelegate> delegate;
@property(nonatomic, readonly, assign) web::WebStateImpl* webStateImpl;
@property(nonatomic, readonly, assign)
    web::NavigationManagerImpl* navigationManagerImpl;

@end

@implementation CRWJSNavigationHandler

#pragma mark - Public

- (instancetype)initWithDelegate:(id<CRWJSNavigationHandlerDelegate>)delegate {
  if (self = [super init]) {
    _delegate = delegate;

    __weak CRWJSNavigationHandler* weakSelf = self;
    auto navigationStateCallback =
        ^bool(const base::DictionaryValue& message, const GURL&,
              bool /* is_main_frame */, bool /* user_is_interacting */,
              web::WebFrame* senderFrame) {
          const std::string* command = message.FindStringKey("command");
          DCHECK(command);
          if (*command == "navigation.hashchange") {
            [weakSelf handleWindowHashChangeInFrame:senderFrame];
            return true;
          } else if (*command == "navigation.willChangeState") {
            [weakSelf handleWindowWillChangeStateInFrame:senderFrame];
            return true;
          }
          return false;
        };

    self.webStateImpl->AddScriptCommandCallback(
        base::BindRepeating(navigationStateCallback), kCommandPrefix);
  }
  return self;
}

- (void)close {
  self.webStateImpl->RemoveScriptCommandCallback(kCommandPrefix);
}

#pragma mark - Private

- (web::WebStateImpl*)webStateImpl {
  return [self.delegate webStateImplForJSNavigationHandler:self];
}

- (web::NavigationManagerImpl*)navigationManagerImpl {
  return &(self.webStateImpl->GetNavigationManagerImpl());
}

// Handles the navigation.hashchange event emitted from |senderFrame|.
- (void)handleWindowHashChangeInFrame:(web::WebFrame*)senderFrame {
  if (!senderFrame->IsMainFrame())
    return;

  // Record that the current NavigationItem was created by a hash change, but
  // ignore hashchange events that are manually dispatched for same-document
  // navigations.
  if (self.dispatchingSameDocumentHashChangeEvent) {
    self.dispatchingSameDocumentHashChangeEvent = NO;
  } else {
    web::NavigationItemImpl* item =
        self.navigationManagerImpl->GetCurrentItemImpl();
    item->SetIsCreatedFromHashChange(true);
  }
}

// Handles the navigation.willChangeState message sent from |senderFrame|.
- (void)handleWindowWillChangeStateInFrame:(web::WebFrame*)senderFrame {
  if (senderFrame->IsMainFrame()) {
    self.changingHistoryState = YES;
  }
}

@end
