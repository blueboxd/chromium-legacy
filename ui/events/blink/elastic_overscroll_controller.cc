// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/elastic_overscroll_controller.h"

#include "build/build_config.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/blink/input_scroll_elasticity_controller.h"
#include "ui/events/blink/overscroll_bounce_controller.h"

namespace ui {
// static
std::unique_ptr<ElasticOverscrollController>
ElasticOverscrollController::Create(cc::ScrollElasticityHelper* helper) {
#if defined(OS_WIN)
  if (base::FeatureList::IsEnabled(features::kElasticOverscrollWin)) {
    return std::make_unique<OverscrollBounceController>(helper);
  }
#endif
  return std::make_unique<InputScrollElasticityController>(helper);
}
}  // namespace ui
