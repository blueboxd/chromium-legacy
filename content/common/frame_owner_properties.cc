// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/frame_owner_properties.h"

namespace content {

FrameOwnerProperties::FrameOwnerProperties()
    : scrollbar_mode(blink::mojom::ScrollbarMode::kAuto),
      margin_width(-1),
      margin_height(-1),
      allow_fullscreen(false),
      allow_payment_request(false),
      is_display_none(false) {}

FrameOwnerProperties::FrameOwnerProperties(const FrameOwnerProperties& other) =
    default;

FrameOwnerProperties::~FrameOwnerProperties() = default;

bool FrameOwnerProperties::operator==(const FrameOwnerProperties& other) const {
  return name == other.name && scrollbar_mode == other.scrollbar_mode &&
         margin_width == other.margin_width &&
         margin_height == other.margin_height &&
         allow_fullscreen == other.allow_fullscreen &&
         allow_payment_request == other.allow_payment_request &&
         is_display_none == other.is_display_none &&
         required_csp == other.required_csp;
}

}  // namespace content
