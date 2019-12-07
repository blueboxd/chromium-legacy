// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_INFOBAR_BANNER_OVERLAY_REQUEST_FACTORY_IMPL_H_
#define IOS_CHROME_BROWSER_INFOBARS_INFOBAR_BANNER_OVERLAY_REQUEST_FACTORY_IMPL_H_

#import "ios/chrome/browser/infobars/infobar_banner_overlay_request_factory.h"

// Implementation of InfobarBannerOverlayRequestFactory.
class InfobarBannerOverlayRequestFactoryImpl
    : public InfobarBannerOverlayRequestFactory {
 public:
  explicit InfobarBannerOverlayRequestFactoryImpl();
  ~InfobarBannerOverlayRequestFactoryImpl() override;

 private:
  // InfobarBannerOverlayRequestFactory:
  std::unique_ptr<OverlayRequest> CreateBannerRequest(
      infobars::InfoBar* infobar) override;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_INFOBAR_BANNER_OVERLAY_REQUEST_FACTORY_IMPL_H_
