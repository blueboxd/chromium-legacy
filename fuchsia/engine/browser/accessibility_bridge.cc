// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/accessibility_bridge.h"

#include "base/logging.h"

AccessibilityBridge::AccessibilityBridge(
    fuchsia::accessibility::semantics::SemanticsManagerPtr semantics_manager,
    fuchsia::ui::views::ViewRef view_ref)
    : binding_(this) {
  semantics_manager->RegisterViewForSemantics(
      std::move(view_ref), binding_.NewBinding(), tree_ptr_.NewRequest());
}

AccessibilityBridge::~AccessibilityBridge() = default;

void AccessibilityBridge::OnAccessibilityActionRequested(
    uint32_t node_id,
    fuchsia::accessibility::semantics::Action action,
    OnAccessibilityActionRequestedCallback callback) {
  NOTIMPLEMENTED();
}

void AccessibilityBridge::HitTest(fuchsia::math::PointF local_point,
                                  HitTestCallback callback) {
  NOTIMPLEMENTED();
}

void AccessibilityBridge::OnSemanticsModeChanged(
    bool updates_enabled,
    OnSemanticsModeChangedCallback callback) {
  NOTIMPLEMENTED();
}
