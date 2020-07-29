// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/delegated_ink_point_renderer.h"

#include <utility>

#include "base/trace_event/trace_event.h"

namespace viz {

DelegatedInkPointRendererImpl::DelegatedInkPointRendererImpl(
    mojo::PendingReceiver<mojom::DelegatedInkPointRenderer> receiver)
    : receiver_(this, std::move(receiver)) {}
DelegatedInkPointRendererImpl::~DelegatedInkPointRendererImpl() = default;

void DelegatedInkPointRendererImpl::StoreDelegatedInkPoint(
    const DelegatedInkPoint& point) {
  TRACE_EVENT_INSTANT1(
      "viz",
      "DelegatedInkPointRendererImpl::StoreDelegatedInkPoint - "
      "Point arrived in viz",
      TRACE_EVENT_SCOPE_THREAD, "point", point.ToString());
  // TODO(1052145): Start storing these points in something to be used during
  // |Display::DrawAndSwap()| to draw the delegated ink trail. These points will
  // need to be cleared when |device_scale_factor_| changes.
}

}  // namespace viz
