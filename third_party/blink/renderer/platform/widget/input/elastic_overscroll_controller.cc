// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/input/elastic_overscroll_controller.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/widget/input/input_scroll_elasticity_controller.h"
#include "third_party/blink/renderer/platform/widget/input/overscroll_bounce_controller.h"
#include "ui/base/ui_base_features.h"

namespace blink {
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
}  // namespace blink
