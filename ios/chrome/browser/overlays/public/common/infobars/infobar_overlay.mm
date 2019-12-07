// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using infobars::InfoBar;

OVERLAY_USER_DATA_SETUP_IMPL(InfobarOverlayData);

InfobarOverlayData::InfobarOverlayData(InfoBar* infobar) : infobar_(infobar) {
  DCHECK(infobar_);
}

InfobarOverlayData::~InfobarOverlayData() = default;
