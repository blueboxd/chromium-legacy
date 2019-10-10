// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/page_properties.h"

#include "content/renderer/compositor/compositor_dependencies.h"
#include "content/renderer/render_widget_screen_metrics_emulator.h"
#include "third_party/blink/public/platform/web_float_rect.h"
#include "third_party/blink/public/platform/web_rect.h"

namespace content {
PageProperties::PageProperties(CompositorDependencies* compositor_deps)
    : compositor_deps_(compositor_deps) {}
PageProperties::~PageProperties() = default;

void PageProperties::SetScreenMetricsEmulator(
    std::unique_ptr<RenderWidgetScreenMetricsEmulator> emulator) {
  screen_metrics_emulator_ = std::move(emulator);
}

CompositorDependencies* PageProperties::GetCompositorDependencies() {
  return compositor_deps_;
}

}  // namespace content
