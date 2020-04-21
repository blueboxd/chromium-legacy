// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/find_bar/find_bar_mediator.h"

#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/find_in_page_commands.h"
#import "ios/chrome/browser/ui/find_bar/find_bar_consumer.h"
#import "ios/chrome/browser/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FindBarMediator ()

// Handler for any FindInPageCommands.
@property(nonatomic, weak) id<FindInPageCommands> commandHandler;

@end

@implementation FindBarMediator

- (instancetype)initWithCommandHandler:(id<FindInPageCommands>)commandHandler {
  self = [super init];
  if (self) {
    _commandHandler = commandHandler;
  }
  return self;
}

#pragma mark - FindInPageResponseDelegate

- (void)findDidFinishWithUpdatedModel:(FindInPageModel*)model {
  [self.consumer updateResultsCount:model];
}

- (void)findDidStop {
  [self.commandHandler closeFindInPage];
}

@end
