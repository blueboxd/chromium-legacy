// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/infobars/overlays/overlay_request_infobar_type_util.h"

#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

InfobarType GetOverlayRequestInfobarType(OverlayRequest* request) {
  return request->GetConfig<InfobarOverlayRequestConfig>()->infobar_type();
}
