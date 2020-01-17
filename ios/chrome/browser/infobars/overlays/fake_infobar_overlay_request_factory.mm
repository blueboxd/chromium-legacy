// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/fake_infobar_overlay_request_factory.h"

#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

FakeInfobarOverlayRequestFactory::FakeInfobarOverlayRequestFactory() = default;

FakeInfobarOverlayRequestFactory::~FakeInfobarOverlayRequestFactory() = default;

std::unique_ptr<OverlayRequest>
FakeInfobarOverlayRequestFactory::CreateInfobarRequest(
    infobars::InfoBar* infobar,
    InfobarOverlayType type) {
  return OverlayRequest::CreateWithConfig<InfobarOverlayRequestConfig>(
      static_cast<InfoBarIOS*>(infobar), type);
}
