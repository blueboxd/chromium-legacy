// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_AGGREGATE_OVERLAY_REQUEST_SUPPORT_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_AGGREGATE_OVERLAY_REQUEST_SUPPORT_H_

#include <vector>

#include "ios/chrome/browser/overlays/public/overlay_request_support.h"

// Helper object that aggregates the request support for a list of
// OverlayRequestSupports.
class AggregateOverlayRequestSupport : public OverlayRequestSupport {
 public:
  // Constructor for an OverlayRequestSupport that supports requests that are
  // supported by at least one OverlayRequestSupport in |supports|.  |supports|
  // is expected to be non-empty.
  explicit AggregateOverlayRequestSupport(
      const std::vector<const OverlayRequestSupport*>& supports);
  ~AggregateOverlayRequestSupport() override;

  // OverlayRequestSupport:
  bool IsRequestSupported(OverlayRequest* request) const override;

 private:
  // The OverlayRequestSupport instances whose functionality is being
  // aggregated.
  const std::vector<const OverlayRequestSupport*> aggregated_supports_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_AGGREGATE_OVERLAY_REQUEST_SUPPORT_H_
