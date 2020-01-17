// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/common/infobar_banner_interaction_handler.h"

#include "base/logging.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/common/infobar_banner_overlay_request_callback_installer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

InfobarBannerInteractionHandler::InfobarBannerInteractionHandler(
    const OverlayRequestSupport* request_support)
    : request_support_(request_support) {
  DCHECK(request_support_);
}

InfobarBannerInteractionHandler::~InfobarBannerInteractionHandler() = default;

std::unique_ptr<OverlayRequestCallbackInstaller>
InfobarBannerInteractionHandler::CreateInstaller() {
  return std::make_unique<InfobarBannerOverlayRequestCallbackInstaller>(
      request_support_, this);
}

void InfobarBannerInteractionHandler::InfobarVisibilityChanged(
    InfoBarIOS* infobar,
    bool visible) {
  BannerVisibilityChanged(infobar, visible);
}
