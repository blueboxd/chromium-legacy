// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_INFOBAR_OVERLAY_REQUEST_FACTORY_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_INFOBAR_OVERLAY_REQUEST_FACTORY_H_

#include <memory>

#include "ios/chrome/browser/infobars/overlays/infobar_overlay_type.h"

namespace infobars {
class InfoBar;
}
class OverlayRequest;

// Factory object that converts InfoBars into OverlayRequests.
class InfobarOverlayRequestFactory {
 public:
  virtual ~InfobarOverlayRequestFactory() = default;

  // Creates an OverlayRequest configured to show the overlay of |type| for
  // |infobar|.
  virtual std::unique_ptr<OverlayRequest> CreateInfobarRequest(
      infobars::InfoBar* infobar,
      InfobarOverlayType type) = 0;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_INFOBAR_OVERLAY_REQUEST_FACTORY_H_
