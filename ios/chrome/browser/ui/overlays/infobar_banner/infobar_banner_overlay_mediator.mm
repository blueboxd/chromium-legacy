// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_banner/infobar_banner_overlay_mediator.h"

#import <UIKit/UIKit.h>

#include "base/logging.h"
#import "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/overlays/public/infobar_banner/infobar_banner_overlay_responses.h"
#include "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#include "ios/chrome/browser/overlays/public/overlay_request_support.h"
#include "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_consumer.h"
#import "ios/chrome/browser/ui/overlays/infobar_banner/infobar_banner_overlay_mediator+consumer_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation InfobarBannerOverlayMediator

- (instancetype)initWithRequest:(OverlayRequest*)request {
  if (self = [super initWithRequest:request]) {
    DCHECK([self class].requestSupport->IsRequestSupported(request));
  }
  return self;
}

#pragma mark - Accessors

- (void)setConsumer:(id<InfobarBannerConsumer>)consumer {
  if (_consumer == consumer)
    return;
  _consumer = consumer;
  if (_consumer)
    [self configureConsumer];
}

#pragma mark - InfobarBannerDelegate

- (void)bannerInfobarButtonWasPressed:(UIButton*)sender {
  // Notify the model layer to perform the infobar's main action before
  // dismissing the banner.
  [self dispatchResponseAndStopOverlay:OverlayResponse::CreateWithInfo<
                                           InfobarBannerMainActionResponse>()];
}

- (void)dismissInfobarBannerForUserInteraction:(BOOL)userInitiated {
  if (userInitiated) {
    // Notify the model layer of user-initiated banner dismissal before
    // dismissing the banner.
    [self dispatchResponseAndStopOverlay:
              OverlayResponse::CreateWithInfo<
                  InfobarBannerUserInitiatedDismissalResponse>()];
  } else {
    [self.delegate stopOverlayForMediator:self];
  }
}

- (void)presentInfobarModalFromBanner {
  // Notify the model layer to show the infobar modal before dismissing the
  // banner.
  [self dispatchResponseAndStopOverlay:OverlayResponse::CreateWithInfo<
                                           InfobarBannerShowModalResponse>()];
}

- (void)infobarBannerWasDismissed {
  // Only needed in legacy implementation.  Dismissal completion cleanup occurs
  // in InfobarBannerOverlayCoordinator.
}

#pragma mark - Private

// Dispatches |response| through the OverlayRequest, then stops the overlay UI.
- (void)dispatchResponseAndStopOverlay:
    (std::unique_ptr<OverlayResponse>)response {
  if (self.request)
    self.request->GetCallbackManager()->DispatchResponse(std::move(response));
  [self.delegate stopOverlayForMediator:self];
}

@end

@implementation InfobarBannerOverlayMediator (ConsumerSupport)

- (void)configureConsumer {
  NOTREACHED() << "Subclasses must implement.";
}

@end
