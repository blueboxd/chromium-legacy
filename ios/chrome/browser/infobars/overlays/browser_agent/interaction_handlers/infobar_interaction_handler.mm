// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/infobar_interaction_handler.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

InfobarInteractionHandler::InfobarInteractionHandler(
    const OverlayRequestSupport* request_support,
    std::unique_ptr<InfobarBannerInteractionHandler> banner_handler,
    std::unique_ptr<InfobarDetailSheetInteractionHandler> sheet_handler,
    std::unique_ptr<InfobarModalInteractionHandler> modal_handler)
    : request_support_(request_support),
      banner_handler_(std::move(banner_handler)),
      sheet_handler_(std::move(sheet_handler)),
      modal_handler_(std::move(modal_handler)) {
  DCHECK(request_support_);
  DCHECK(banner_handler_.get());
}

InfobarInteractionHandler::~InfobarInteractionHandler() = default;
