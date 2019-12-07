// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_BANNER_SAVE_PASSWORD_INFOBAR_BANNER_OVERLAY_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_BANNER_SAVE_PASSWORD_INFOBAR_BANNER_OVERLAY_H_

#include "ios/chrome/browser/overlays/public/overlay_user_data.h"

namespace infobars {
class InfoBar;
}
class IOSChromeSavePasswordInfoBarDelegate;

// Configuration object for OverlayRequests for the banner UI for an InfoBar
// with a IOSChromeSavePasswordInfoBarDelegate.
class SavePasswordInfobarBannerOverlayRequestConfig
    : public OverlayUserData<SavePasswordInfobarBannerOverlayRequestConfig> {
 public:
  ~SavePasswordInfobarBannerOverlayRequestConfig() override;

  // The save password delegate used to configure the banner UI.
  IOSChromeSavePasswordInfoBarDelegate* save_password_delegate() const {
    return save_password_delegate_;
  }

 private:
  OVERLAY_USER_DATA_SETUP(SavePasswordInfobarBannerOverlayRequestConfig);
  explicit SavePasswordInfobarBannerOverlayRequestConfig(
      infobars::InfoBar* infobar);

  // OverlayUserData:
  void CreateAuxilliaryData(base::SupportsUserData* user_data) override;

  // The InfoBar causing this banner.
  infobars::InfoBar* infobar_ = nullptr;
  // The delegate used to configure the save passwords banner.
  IOSChromeSavePasswordInfoBarDelegate* save_password_delegate_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_BANNER_SAVE_PASSWORD_INFOBAR_BANNER_OVERLAY_H_
