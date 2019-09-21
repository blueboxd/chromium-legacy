// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/page_properties.h"
#include "content/renderer/render_widget_screen_metrics_emulator.h"

namespace content {
PageProperties::PageProperties() = default;
PageProperties::~PageProperties() = default;

void PageProperties::SetScreenMetricsEmulator(
    std::unique_ptr<RenderWidgetScreenMetricsEmulator> emulator) {
  screen_metrics_emulator_ = std::move(emulator);
}

}  // namespace content
