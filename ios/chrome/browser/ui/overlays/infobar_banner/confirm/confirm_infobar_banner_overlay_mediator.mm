// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_banner/confirm/confirm_infobar_banner_overlay_mediator.h"

#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/confirm_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_consumer.h"
#import "ios/chrome/browser/ui/overlays/infobar_banner/infobar_banner_overlay_mediator+consumer_support.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using confirm_infobar_overlays::ConfirmBannerRequestConfig;

@interface ConfirmInfobarBannerOverlayMediator ()
// The confirm banner config from the request.
@property(nonatomic, readonly) ConfirmBannerRequestConfig* config;
@end

@implementation ConfirmInfobarBannerOverlayMediator

#pragma mark - Accessors

- (ConfirmBannerRequestConfig*)config {
  return self.request ? self.request->GetConfig<ConfirmBannerRequestConfig>()
                      : nullptr;
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return ConfirmBannerRequestConfig::RequestSupport();
}

@end

@implementation ConfirmInfobarBannerOverlayMediator (ConsumerSupport)

- (void)configureConsumer {
  ConfirmBannerRequestConfig* config = self.config;
  if (!self.consumer || !config)
    return;

  [self.consumer setBannerAccessibilityLabel:base::SysUTF16ToNSString(
                                                 config->button_label_text())];
  [self.consumer
      setButtonText:base::SysUTF16ToNSString(config->button_label_text())];
  if (!config->icon_image().IsEmpty())
    [self.consumer setIconImage:config->icon_image().ToUIImage()];
  [self.consumer setPresentsModal:NO];
  [self.consumer setTitleText:base::SysUTF16ToNSString(config->message_text())];
}

@end
