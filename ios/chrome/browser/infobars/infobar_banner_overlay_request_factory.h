// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_INFOBAR_BANNER_OVERLAY_REQUEST_FACTORY_H_
#define IOS_CHROME_BROWSER_INFOBARS_INFOBAR_BANNER_OVERLAY_REQUEST_FACTORY_H_

#include <memory>

namespace infobars {
class InfoBar;
}
class OverlayRequest;

// Factory object that converts InfoBars into OverlayRequests for banner UI.
class InfobarBannerOverlayRequestFactory {
 public:
  virtual ~InfobarBannerOverlayRequestFactory() = default;

  // Creates an OverlayRequest for an InfoBar banner configured using |infobar|,
  // or returns nullptr if a banner request cannot be created.
  virtual std::unique_ptr<OverlayRequest> CreateBannerRequest(
      infobars::InfoBar* infobar) = 0;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_INFOBAR_BANNER_OVERLAY_REQUEST_FACTORY_H_
