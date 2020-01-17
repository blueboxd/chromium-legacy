// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_modal/infobar_modal_overlay_mediator.h"

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/overlays/public/infobar_modal/infobar_modal_overlay_responses.h"
#include "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_mediator+subclassing.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation InfobarModalOverlayMediator

#pragma mark - InfobarModalDelegate

- (void)dismissInfobarModal:(id)infobarModal {
  [self.delegate stopOverlayForMediator:self];
}

- (void)modalInfobarButtonWasAccepted:(id)infobarModal {
  [self dispatchResponse:OverlayResponse::CreateWithInfo<
                             InfobarModalMainActionResponse>()];
  [self dismissOverlay];
}

- (void)modalInfobarWasDismissed:(id)infobarModal {
  // Only needed in legacy implementation.  Dismissal completion cleanup occurs
  // in InfobarModalOverlayCoordinator.
  // TODO(crbug.com/1041917): Remove once non-overlay implementation is deleted.
}

@end
