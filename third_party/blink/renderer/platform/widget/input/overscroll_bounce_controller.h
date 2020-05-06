// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_OVERSCROLL_BOUNCE_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_OVERSCROLL_BOUNCE_CONTROLLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "cc/input/overscroll_behavior.h"
#include "cc/input/scroll_elasticity_helper.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/platform/input/elastic_overscroll_controller.h"

namespace cc {
struct InputHandlerScrollResult;
}  // namespace cc

namespace blink {
// The overbounce version of elastic overscrolling mimics Windows style
// overscroll animations.
class OverscrollBounceController : public ElasticOverscrollController {
 public:
  explicit OverscrollBounceController(cc::ScrollElasticityHelper* helper);
  ~OverscrollBounceController() override;

  base::WeakPtr<ElasticOverscrollController> GetWeakPtr() override;

  void ObserveGestureEventAndResult(
      const blink::WebGestureEvent& gesture_event,
      const cc::InputHandlerScrollResult& scroll_result) override;
  void Animate(base::TimeTicks time) override;
  void ReconcileStretchAndScroll() override;

 private:
  base::WeakPtrFactory<OverscrollBounceController> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(OverscrollBounceController);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_OVERSCROLL_BOUNCE_CONTROLLER_H_
