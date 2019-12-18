// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_FAKE_INFOBAR_OVERLAY_REQUEST_FACTORY_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_FAKE_INFOBAR_OVERLAY_REQUEST_FACTORY_H_

#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_factory.h"

class OverlayRequest;

// Fake version of InfobarOverlayRequestFactory.  Creates OverlayRequests that
// are only configured with an InfobarOverlayRequestConfig.
class FakeInfobarOverlayRequestFactory : public InfobarOverlayRequestFactory {
 public:
  FakeInfobarOverlayRequestFactory();
  ~FakeInfobarOverlayRequestFactory() override;

  // InfobarOverlayRequestFactory:
  std::unique_ptr<OverlayRequest> CreateInfobarRequest(
      infobars::InfoBar* infobar,
      InfobarOverlayType type) override;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_FAKE_INFOBAR_OVERLAY_REQUEST_FACTORY_H_
