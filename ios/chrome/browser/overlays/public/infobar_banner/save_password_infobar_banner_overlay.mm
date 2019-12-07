// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/infobar_banner/save_password_infobar_banner_overlay.h"

#include "base/logging.h"
#include "components/infobars/core/infobar.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay.h"
#import "ios/chrome/browser/passwords/ios_chrome_save_password_infobar_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using infobars::InfoBar;

OVERLAY_USER_DATA_SETUP_IMPL(SavePasswordInfobarBannerOverlayRequestConfig);

SavePasswordInfobarBannerOverlayRequestConfig::
    SavePasswordInfobarBannerOverlayRequestConfig(InfoBar* infobar)
    : infobar_(infobar),
      save_password_delegate_(
          IOSChromeSavePasswordInfoBarDelegate::FromInfobarDelegate(
              infobar->delegate())) {
  DCHECK(infobar_);
  DCHECK(save_password_delegate_);
}

SavePasswordInfobarBannerOverlayRequestConfig::
    ~SavePasswordInfobarBannerOverlayRequestConfig() = default;

void SavePasswordInfobarBannerOverlayRequestConfig::CreateAuxilliaryData(
    base::SupportsUserData* user_data) {
  InfobarOverlayData::CreateForUserData(user_data, infobar_);
}
