// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/find_bar/find_bar_mediator.h"

#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FindBarMediator () <CRWWebStateObserver> {
  std::unique_ptr<web::WebStateObserverBridge> _observer;
  std::unique_ptr<ActiveWebStateObservationForwarder> _forwarder;
}

// Handler for any BrowserCommands.
@property(nonatomic, weak) id<BrowserCommands> commandHandler;

@end

@implementation FindBarMediator

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                      commandHandler:(id<BrowserCommands>)commandHandler {
  self = [super init];
  if (self) {
    DCHECK(webStateList);

    _commandHandler = commandHandler;

    // Set up the WebState and its observer.
    _observer = std::make_unique<web::WebStateObserverBridge>(self);
    _forwarder = std::make_unique<ActiveWebStateObservationForwarder>(
        webStateList, _observer.get());
  }
  return self;
}

- (void)dealloc {
  _forwarder = nullptr;
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  [self.commandHandler closeFindInPage];
}

@end
