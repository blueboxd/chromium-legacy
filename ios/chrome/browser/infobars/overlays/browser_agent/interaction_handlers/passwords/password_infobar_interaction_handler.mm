// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/passwords/password_infobar_interaction_handler.h"

#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/passwords/password_infobar_banner_interaction_handler.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/save_password_infobar_banner_overlay.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

PasswordInfobarInteractionHandler::PasswordInfobarInteractionHandler()
    : InfobarInteractionHandler(
          InfobarType::kInfobarTypePasswordSave,
          std::make_unique<PasswordInfobarBannerInteractionHandler>(),
          /*sheet_handler=*/nullptr,
          /*modal_handler=*/nullptr) {
  // TODO(crbug.com/1033154): Create interaction handlers for detail sheet and
  // modal.
}

PasswordInfobarInteractionHandler::~PasswordInfobarInteractionHandler() =
    default;
