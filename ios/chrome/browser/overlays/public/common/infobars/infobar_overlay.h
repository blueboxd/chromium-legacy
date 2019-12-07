// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_COMMON_INFOBARS_INFOBAR_OVERLAY_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_COMMON_INFOBARS_INFOBAR_OVERLAY_H_

#include "ios/chrome/browser/overlays/public/overlay_user_data.h"

namespace infobars {
class InfoBar;
}

// OverlayUserData used to hold a pointer to an InfoBar.  Used as auxilliary
// data for OverlayRequests for InfoBars.
class InfobarOverlayData : public OverlayUserData<InfobarOverlayData> {
 public:
  ~InfobarOverlayData() override;

  // The InfoBar that triggered this OverlayRequest.
  infobars::InfoBar* infobar() const { return infobar_; }

 private:
  OVERLAY_USER_DATA_SETUP(InfobarOverlayData);
  explicit InfobarOverlayData(infobars::InfoBar* infobar);

  // The InfoBar causing this overlay.
  infobars::InfoBar* infobar_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_COMMON_INFOBARS_INFOBAR_OVERLAY_H_
