// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_COMMON_INFOBARS_INFOBAR_OVERLAY_REQUEST_CONFIG_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_COMMON_INFOBARS_INFOBAR_OVERLAY_REQUEST_CONFIG_H_

#import "ios/chrome/browser/infobars/infobar_type.h"
#include "ios/chrome/browser/overlays/public/overlay_user_data.h"

namespace infobars {
class InfoBar;
}

// OverlayUserData used to hold a pointer to an InfoBar.  Used as auxiliary
// data for OverlayRequests for InfoBars.
class InfobarOverlayRequestConfig
    : public OverlayUserData<InfobarOverlayRequestConfig> {
 public:
  ~InfobarOverlayRequestConfig() override;

  // The InfoBar that triggered this OverlayRequest.
  infobars::InfoBar* infobar() const { return infobar_; }
  // |infobar_|'s type.
  InfobarType infobar_type() const { return infobar_type_; }
  // Whether |infobar_| has a badge.
  bool has_badge() const { return has_badge_; }

 private:
  OVERLAY_USER_DATA_SETUP(InfobarOverlayRequestConfig);
  explicit InfobarOverlayRequestConfig(infobars::InfoBar* infobar);

  infobars::InfoBar* infobar_ = nullptr;
  InfobarType infobar_type_;
  bool has_badge_ = false;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_COMMON_INFOBARS_INFOBAR_OVERLAY_REQUEST_CONFIG_H_
