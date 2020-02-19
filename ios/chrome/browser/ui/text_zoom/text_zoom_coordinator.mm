// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/text_zoom/text_zoom_coordinator.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/text_zoom_commands.h"
#include "ios/chrome/browser/ui/presenters/contained_presenter_delegate.h"
#import "ios/chrome/browser/ui/text_zoom/text_zoom_mediator.h"
#import "ios/chrome/browser/ui/text_zoom/text_zoom_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/accessory/toolbar_accessory_coordinator_delegate.h"
#import "ios/chrome/browser/ui/toolbar/accessory/toolbar_accessory_presenter.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/common/colors/dynamic_color_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TextZoomCoordinator () <ContainedPresenterDelegate>

// The view controller managed by this coordinator.
@property(nonatomic, strong, readwrite)
    TextZoomViewController* textZoomViewController;

@property(nonatomic, strong) TextZoomMediator* mediator;

// Allows simplified access to the TextZoomCommands handler.
@property(nonatomic, readonly) id<TextZoomCommands> textZoomCommandHandler;

@end

@implementation TextZoomCoordinator

#pragma mark - ChromeCoordinator

- (void)start {
  DCHECK(self.browser);
  DCHECK(self.browserState);

  self.mediator = [[TextZoomMediator alloc]
      initWithWebStateList:self.browser->GetWebStateList()
            commandHandler:self.textZoomCommandHandler];

  self.textZoomViewController = [[TextZoomViewController alloc]
      initWithDarkAppearance:self.browserState->IsOffTheRecord()];
  self.textZoomViewController.commandHandler = self.textZoomCommandHandler;

  self.textZoomViewController.zoomHandler = self.mediator;
  self.mediator.consumer = self.textZoomViewController;

  [self showAnimated:YES];
}

- (void)stop {
  [self.presenter dismissAnimated:YES];
  self.textZoomViewController = nil;

  [self.mediator disconnect];
}

- (void)showAnimated:(BOOL)animated {
  self.presenter.presentedViewController = self.textZoomViewController;
  self.presenter.delegate = self;

  [self.presenter prepareForPresentation];
  [self.presenter presentAnimated:animated];
}

#pragma mark - ContainedPresenterDelegate

- (void)containedPresenterDidDismiss:(id<ContainedPresenter>)presenter {
  [self.delegate toolbarAccessoryCoordinatorDidDismissUI:self];
}

#pragma mark - Private

- (id<TextZoomCommands>)textZoomCommandHandler {
  return HandlerForProtocol(self.browser->GetCommandDispatcher(),
                            TextZoomCommands);
}

@end
