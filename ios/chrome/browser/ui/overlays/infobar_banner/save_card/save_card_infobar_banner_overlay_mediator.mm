// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_banner/save_card/save_card_infobar_banner_overlay_mediator.h"

#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/save_card_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_consumer.h"
#import "ios/chrome/browser/ui/overlays/infobar_banner/infobar_banner_overlay_mediator+consumer_support.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using save_card_infobar_overlays::SaveCardBannerRequestConfig;

@interface SaveCardInfobarBannerOverlayMediator ()
// The save card banner config from the request.
@property(nonatomic, readonly) SaveCardBannerRequestConfig* config;
@end

@implementation SaveCardInfobarBannerOverlayMediator

#pragma mark - Accessors

- (SaveCardBannerRequestConfig*)config {
  return self.request ? self.request->GetConfig<SaveCardBannerRequestConfig>()
                      : nullptr;
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return SaveCardBannerRequestConfig::RequestSupport();
}

@end

@implementation SaveCardInfobarBannerOverlayMediator (ConsumerSupport)

- (void)configureConsumer {
  SaveCardBannerRequestConfig* config = self.config;
  if (!self.consumer || !config)
    return;

  [self.consumer setBannerAccessibilityLabel:base::SysUTF16ToNSString(
                                                 config->button_label_text())];
  [self.consumer
      setButtonText:base::SysUTF16ToNSString(self.config->button_label_text())];
  [self.consumer setIconImage:[UIImage imageNamed:config->icon_image_name()]];
  [self.consumer
      setTitleText:base::SysUTF16ToNSString(self.config->message_text())];
  [self.consumer
      setSubtitleText:base::SysUTF16ToNSString(self.config->card_label())];
}

@end
