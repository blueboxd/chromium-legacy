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

void PageProperties::ConvertViewportToWindow(blink::WebRect* rect) {
  if (compositor_deps_->IsUseZoomForDSFEnabled()) {
    float reverse = 1 / GetOriginalScreenInfo().device_scale_factor;
    // TODO(oshima): We may need to allow pixel precision here as the the
    // anchor element can be placed at half pixel.
    gfx::Rect window_rect = gfx::ScaleToEnclosedRect(gfx::Rect(*rect), reverse);
    rect->x = window_rect.x();
    rect->y = window_rect.y();
    rect->width = window_rect.width();
    rect->height = window_rect.height();
  }
}

void PageProperties::ConvertViewportToWindow(blink::WebFloatRect* rect) {
  if (compositor_deps_->IsUseZoomForDSFEnabled()) {
    rect->x /= GetOriginalScreenInfo().device_scale_factor;
    rect->y /= GetOriginalScreenInfo().device_scale_factor;
    rect->width /= GetOriginalScreenInfo().device_scale_factor;
    rect->height /= GetOriginalScreenInfo().device_scale_factor;
  }
}

void PageProperties::ConvertWindowToViewport(blink::WebFloatRect* rect) {
  if (compositor_deps_->IsUseZoomForDSFEnabled()) {
    rect->x *= GetOriginalScreenInfo().device_scale_factor;
    rect->y *= GetOriginalScreenInfo().device_scale_factor;
    rect->width *= GetOriginalScreenInfo().device_scale_factor;
    rect->height *= GetOriginalScreenInfo().device_scale_factor;
  }
}

const ScreenInfo& PageProperties::GetOriginalScreenInfo() const {
  return ScreenMetricsEmulator()
             ? ScreenMetricsEmulator()->original_screen_info()
             : GetScreenInfo();
}

CompositorDependencies* PageProperties::GetCompositorDependencies() {
  return compositor_deps_;
}

}  // namespace content
