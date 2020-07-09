// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_POINT_SCAN_CONTROLLER_H_
#define ASH_ACCESSIBILITY_POINT_SCAN_CONTROLLER_H_

#include "ash/accessibility/accessibility_layer.h"
#include "ash/ash_export.h"

namespace ash {

class PointScanLayer;

// PointScanController handles drawing and animating custom lines onscreen, for
// the purposes of selecting a point onscreen without using a traditional mouse
// or keyboard. Currently used by Switch Access.
class ASH_EXPORT PointScanController : public AccessibilityLayerDelegate {
 public:
  PointScanController();
  ~PointScanController() override;

  PointScanController(const PointScanController&) = delete;
  PointScanController& operator=(const PointScanController&) = delete;

  // Starts point scanning, by sweeping a line across the screen and waiting for
  // user input.
  // TODO(crbug/1061537): Animate the line across the screen.
  void Start();

 private:
  // AccessibilityLayerDelegate implementation:
  void OnDeviceScaleFactorChanged() override;
  void OnAnimationStep(base::TimeTicks timestamp) override;

  std::unique_ptr<PointScanLayer> point_scan_layer_;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_POINT_SCAN_CONTROLLER_H_
