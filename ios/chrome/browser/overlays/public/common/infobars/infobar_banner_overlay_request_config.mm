// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/common/infobars/infobar_banner_overlay_request_config.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

OVERLAY_USER_DATA_SETUP_IMPL(InfobarBannerOverlayRequestConfig);

InfobarBannerOverlayRequestConfig::InfobarBannerOverlayRequestConfig(
    NSString* banner_accessibility_label,
    NSString* button_text,
    NSString* icon_image_name,
    bool presents_modal,
    NSString* title_text,
    NSString* subtitle_text)
    : banner_accessibility_label_(banner_accessibility_label),
      button_text_(button_text),
      icon_image_name_(icon_image_name),
      presents_modal_(presents_modal),
      title_text_(title_text),
      subtitle_text_(subtitle_text) {}

InfobarBannerOverlayRequestConfig::~InfobarBannerOverlayRequestConfig() =
    default;
