// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_ACCESSIBILITY_BRIDGE_H_
#define FUCHSIA_ENGINE_BROWSER_ACCESSIBILITY_BRIDGE_H_

#include <fuchsia/accessibility/semantics/cpp/fidl.h>
#include <fuchsia/math/cpp/fidl.h>
#include <fuchsia/ui/views/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>

#include "base/macros.h"
#include "fuchsia/engine/web_engine_export.h"

// This class is the intermediate for accessibility between Chrome and Fuchsia.
// It handles registration to the Fuchsia Semantics Manager, translating events
// and data structures between the two services, and forwarding actions and
// events.
// The lifetime of an instance of AccessibilityBridge is the same as that of a
// View created by FrameImpl. This class refers to the View via the
// caller-supplied ViewRef.
class WEB_ENGINE_EXPORT AccessibilityBridge
    : public fuchsia::accessibility::semantics::SemanticListener {
 public:
  AccessibilityBridge(
      fuchsia::accessibility::semantics::SemanticsManagerPtr semantics_manager,
      fuchsia::ui::views::ViewRef view_ref);
  ~AccessibilityBridge() final;

 private:
  // fuchsia::accessibility::semantics::SemanticListener implementation.
  void OnAccessibilityActionRequested(
      uint32_t node_id,
      fuchsia::accessibility::semantics::Action action,
      OnAccessibilityActionRequestedCallback callback) final;
  void HitTest(fuchsia::math::PointF local_point,
               HitTestCallback callback) final;
  void OnSemanticsModeChanged(bool updates_enabled,
                              OnSemanticsModeChangedCallback callback) final;

  fuchsia::accessibility::semantics::SemanticTreePtr tree_ptr_;
  fidl::Binding<fuchsia::accessibility::semantics::SemanticListener> binding_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityBridge);
};

#endif  // FUCHSIA_ENGINE_BROWSER_ACCESSIBILITY_BRIDGE_H_
