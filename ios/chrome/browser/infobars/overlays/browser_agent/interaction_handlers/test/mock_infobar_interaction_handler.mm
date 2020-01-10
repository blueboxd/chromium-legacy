// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/test/mock_infobar_interaction_handler.h"

#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - MockInfobarBannerInteractionHandler

MockInfobarBannerInteractionHandler::MockInfobarBannerInteractionHandler() =
    default;

MockInfobarBannerInteractionHandler::~MockInfobarBannerInteractionHandler() =
    default;

#pragma mark - MockInfobarDetailSheetInteractionHandler

MockInfobarDetailSheetInteractionHandler::
    MockInfobarDetailSheetInteractionHandler() = default;

MockInfobarDetailSheetInteractionHandler::
    ~MockInfobarDetailSheetInteractionHandler() = default;

#pragma mark - MockInfobarModalInteractionHandler

MockInfobarModalInteractionHandler::MockInfobarModalInteractionHandler() =
    default;

MockInfobarModalInteractionHandler::~MockInfobarModalInteractionHandler() =
    default;

#pragma mark - MockInfobarModalInteractionHandler

MockInfobarInteractionHandler::MockInfobarInteractionHandler()
    : InfobarInteractionHandler(
          InfobarOverlayRequestConfig::RequestSupport(),
          std::make_unique<MockInfobarBannerInteractionHandler>(),
          std::make_unique<MockInfobarDetailSheetInteractionHandler>(),
          std::make_unique<MockInfobarModalInteractionHandler>()) {
  mock_banner_handler_ =
      static_cast<MockInfobarBannerInteractionHandler*>(banner_handler());
  mock_sheet_handler_ =
      static_cast<MockInfobarDetailSheetInteractionHandler*>(sheet_handler());
  mock_modal_handler_ =
      static_cast<MockInfobarModalInteractionHandler*>(modal_handler());
}

MockInfobarInteractionHandler::~MockInfobarInteractionHandler() = default;
