// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"

#include "base/logging.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/ui/infobars/infobar_ui_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using infobars::InfoBar;

OVERLAY_USER_DATA_SETUP_IMPL(InfobarOverlayRequestConfig);

InfobarOverlayRequestConfig::InfobarOverlayRequestConfig(InfoBar* infobar)
    : infobar_(infobar),
      infobar_type_(
          static_cast<InfoBarIOS*>(infobar)->InfobarUIDelegate().infobarType),
      has_badge_(
          static_cast<InfoBarIOS*>(infobar)->InfobarUIDelegate().hasBadge) {}

InfobarOverlayRequestConfig::~InfobarOverlayRequestConfig() = default;
