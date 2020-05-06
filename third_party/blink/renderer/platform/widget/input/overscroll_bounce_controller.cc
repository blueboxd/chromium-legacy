// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/overscroll_bounce_controller.h"

#include <algorithm>
#include <cmath>

// TODO(arakeri): This is where all the overscroll specific code will go.
namespace blink {

OverscrollBounceController::OverscrollBounceController(
    cc::ScrollElasticityHelper* helper)
    : weak_factory_(this) {}

OverscrollBounceController::~OverscrollBounceController() = default;

base::WeakPtr<ElasticOverscrollController>
OverscrollBounceController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void OverscrollBounceController::ObserveGestureEventAndResult(
    const blink::WebGestureEvent& gesture_event,
    const cc::InputHandlerScrollResult& scroll_result) {}

void OverscrollBounceController::Animate(base::TimeTicks time) {}

void OverscrollBounceController::ReconcileStretchAndScroll() {}

}  // namespace blink
