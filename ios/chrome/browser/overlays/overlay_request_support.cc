// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/public/overlay_request_support.h"

#include "base/logging.h"
#include "base/no_destructor.h"

namespace {
// OverlayRequestSupport that returns a constant value for IsRequestSupported()
// regardless of the request type.
class ConstantOverlayRequestSupport : public OverlayRequestSupport {
 public:
  ConstantOverlayRequestSupport(bool should_support)
      : supports_requests_(should_support) {}

  bool IsRequestSupported(OverlayRequest* request) const override {
    return supports_requests_;
  }

 private:
  // Whether requests should be supported.
  bool supports_requests_ = false;
};
}  // namespace

OverlayRequestSupport::OverlayRequestSupport() = default;

OverlayRequestSupport::~OverlayRequestSupport() = default;

bool OverlayRequestSupport::IsRequestSupported(OverlayRequest* request) const {
  NOTREACHED() << "Subclasses implement.";
  return false;
}

// static
const OverlayRequestSupport* OverlayRequestSupport::All() {
  static base::NoDestructor<ConstantOverlayRequestSupport> support_all(true);
  return support_all.get();
}

// static
const OverlayRequestSupport* OverlayRequestSupport::None() {
  static base::NoDestructor<ConstantOverlayRequestSupport> support_none(false);
  return support_none.get();
}
