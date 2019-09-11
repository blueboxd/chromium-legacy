// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/accessibility_bridge.h"

#include <fuchsia/accessibility/semantics/cpp/fidl.h>
#include <fuchsia/accessibility/semantics/cpp/fidl_test_base.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/ui/scenic/cpp/view_ref_pair.h>
#include <zircon/types.h>

#include "base/fuchsia/default_context.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/service_directory_client.h"
#include "base/logging.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using fuchsia::accessibility::semantics::SemanticListener;
using fuchsia::accessibility::semantics::SemanticsManager;

namespace {

class FakeSemanticsManager : public fuchsia::accessibility::semantics::testing::
                                 SemanticsManager_TestBase {
 public:
  explicit FakeSemanticsManager() = default;
  ~FakeSemanticsManager() override = default;

  // fuchsia::accessibility::semantics::SemanticsManager implementation.
  void RegisterViewForSemantics(
      fuchsia::ui::views::ViewRef view_ref,
      fidl::InterfaceHandle<SemanticListener> listener,
      fidl::InterfaceRequest<fuchsia::accessibility::semantics::SemanticTree>
          semantic_tree) final {
    view_ref_ = std::move(view_ref);
    listener_ = std::move(listener);
    semantic_tree_ = std::move(semantic_tree);
  }

  bool is_view_registered() const { return view_ref_.reference.is_valid(); }

  bool is_listener_handle_valid() const { return listener_.is_valid(); }

  bool is_semantic_tree_handle_valid() const {
    return semantic_tree_.is_valid();
  }

  void NotImplemented_(const std::string& name) final {
    NOTIMPLEMENTED() << name;
  }

 private:
  fuchsia::ui::views::ViewRef view_ref_;
  fidl::InterfaceHandle<SemanticListener> listener_;
  fidl::InterfaceRequest<fuchsia::accessibility::semantics::SemanticTree>
      semantic_tree_;

  DISALLOW_COPY_AND_ASSIGN(FakeSemanticsManager);
};

}  // namespace

class AccessibilityBridgeTest : public testing::Test {
 public:
  AccessibilityBridgeTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        semantics_manager_binding_(&semantics_manager_) {}
  ~AccessibilityBridgeTest() override = default;

  void CreateAccessibilityBridge() {
    auto view_ref_pair = scenic::ViewRefPair::New();
    control_ref_ = std::move(view_ref_pair.control_ref);
    fuchsia::accessibility::semantics::SemanticsManagerPtr
        semantics_manager_ptr;
    semantics_manager_binding_.Bind(semantics_manager_ptr.NewRequest());
    accessibility_bridge_ = std::make_unique<AccessibilityBridge>(
        std::move(semantics_manager_ptr), std::move(view_ref_pair.view_ref));
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<AccessibilityBridge> accessibility_bridge_;
  FakeSemanticsManager semantics_manager_;
  fidl::Binding<fuchsia::accessibility::semantics::SemanticsManager>
      semantics_manager_binding_;
  fuchsia::ui::views::ViewRefControl control_ref_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityBridgeTest);
};

// Test registration to the SemanticsManager.
TEST_F(AccessibilityBridgeTest, RegisterViewRef) {
  CreateAccessibilityBridge();
  // Run loop so FIDL registration requests are processed.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(semantics_manager_.is_view_registered());
  EXPECT_TRUE(semantics_manager_.is_listener_handle_valid());
  EXPECT_TRUE(semantics_manager_.is_semantic_tree_handle_valid());
}
