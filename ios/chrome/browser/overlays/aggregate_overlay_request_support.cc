// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/public/aggregate_overlay_request_support.h"

#include "base/logging.h"

AggregateOverlayRequestSupport::AggregateOverlayRequestSupport(
    const std::vector<const OverlayRequestSupport*>& supports)
    : aggregated_supports_(supports) {
  DCHECK(!aggregated_supports_.empty());
}

AggregateOverlayRequestSupport::~AggregateOverlayRequestSupport() = default;

bool AggregateOverlayRequestSupport::IsRequestSupported(
    OverlayRequest* request) const {
  for (const OverlayRequestSupport* support : aggregated_supports_) {
    if (support->IsRequestSupported(request))
      return true;
  }
  return false;
}
