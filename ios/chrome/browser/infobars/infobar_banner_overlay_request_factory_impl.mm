// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/infobar_banner_overlay_request_factory_impl.h"

#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/save_password_infobar_banner_overlay.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using infobars::InfoBar;
using infobars::InfoBarDelegate;

InfobarBannerOverlayRequestFactoryImpl::
    InfobarBannerOverlayRequestFactoryImpl() = default;

InfobarBannerOverlayRequestFactoryImpl::
    ~InfobarBannerOverlayRequestFactoryImpl() = default;

std::unique_ptr<OverlayRequest>
InfobarBannerOverlayRequestFactoryImpl::CreateBannerRequest(InfoBar* infobar) {
  switch (infobar->delegate()->GetIdentifier()) {
    case InfoBarDelegate::SAVE_PASSWORD_INFOBAR_DELEGATE_MOBILE:
      return OverlayRequest::CreateWithConfig<
          SavePasswordInfobarBannerOverlayRequestConfig>(infobar);
    default:
      return nullptr;
  }
}
