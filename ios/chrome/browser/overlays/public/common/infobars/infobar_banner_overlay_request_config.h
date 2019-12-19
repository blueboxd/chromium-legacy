// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_COMMON_INFOBARS_INFOBAR_BANNER_OVERLAY_REQUEST_CONFIG_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_COMMON_INFOBARS_INFOBAR_BANNER_OVERLAY_REQUEST_CONFIG_H_

#import <Foundation/Foundation.h>

#include "ios/chrome/browser/overlays/public/overlay_user_data.h"

// OverlayUserData used to configure an InfobarBannerConsumer.  Used as
// auxiliary data for OverlayRequests for InfoBars.
class InfobarBannerOverlayRequestConfig
    : public OverlayUserData<InfobarBannerOverlayRequestConfig> {
 public:
  ~InfobarBannerOverlayRequestConfig() override;

  // The optional accessibility label to use for the banner UI.
  NSString* banner_accessibility_label() const {
    return banner_accessibility_label_;
  }
  // The text to add to the button.
  NSString* button_text() const { return button_text_; }
  // The name of the image to use for the banner icon.
  NSString* icon_image_name() const { return icon_image_name_; }
  // Whether the banner can present a modal.
  bool presents_modal() const { return presents_modal_; }
  // The title text of the banner.
  NSString* title_text() const { return title_text_; }
  // The subtitle text of the banner.
  NSString* subtitle_text() const { return subtitle_text_; }

 private:
  OVERLAY_USER_DATA_SETUP(InfobarBannerOverlayRequestConfig);
  explicit InfobarBannerOverlayRequestConfig(
      NSString* banner_accessibility_label,
      NSString* button_text,
      NSString* icon_image_name,
      bool presents_modal,
      NSString* title_text,
      NSString* subtitle_text);

  NSString* banner_accessibility_label_;
  NSString* button_text_;
  NSString* icon_image_name_;
  bool presents_modal_ = false;
  NSString* title_text_;
  NSString* subtitle_text_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_COMMON_INFOBARS_INFOBAR_BANNER_OVERLAY_REQUEST_CONFIG_H_
