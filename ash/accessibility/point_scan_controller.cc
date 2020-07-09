// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/point_scan_controller.h"

#include "ash/accessibility/point_scan_layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace ash {

PointScanController::PointScanController() = default;
PointScanController::~PointScanController() = default;

void PointScanController::Start() {
  point_scan_layer_.reset(new PointScanLayer(this));
  point_scan_layer_->StartHorizontalScanning();
}

void PointScanController::OnDeviceScaleFactorChanged() {}
void PointScanController::OnAnimationStep(base::TimeTicks timestamp) {}

}  // namespace ash
