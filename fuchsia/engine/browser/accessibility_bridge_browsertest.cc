// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/accessibility/semantics/cpp/fidl.h>
#include <lib/ui/scenic/cpp/view_ref_pair.h>
#include <zircon/types.h>

#include "content/public/test/browser_test.h"
#include "fuchsia/base/frame_test_util.h"
#include "fuchsia/base/test_navigation_listener.h"
#include "fuchsia/engine/browser/accessibility_bridge.h"
#include "fuchsia/engine/browser/fake_semantics_manager.h"
#include "fuchsia/engine/browser/frame_impl.h"
#include "fuchsia/engine/test/test_data.h"
#include "fuchsia/engine/test/web_engine_browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/switches.h"
#include "ui/ozone/public/ozone_switches.h"

namespace {

const char kPage1Path[] = "/ax1.html";
const char kPage2Path[] = "/batching.html";
const char kPage1Title[] = "accessibility 1";
const char kPage2Title[] = "lots of nodes!";
const char kButtonName1[] = "a button";
const char kButtonName2[] = "another button";
const char kButtonName3[] = "button 3";
const char kNodeName[] = "last node";
const char kParagraphName[] = "a third paragraph";
const size_t kPage1NodeCount = 9;
const size_t kPage2NodeCount = 190;


fuchsia::math::PointF GetCenterOfBox(fuchsia::ui::gfx::BoundingBox box) {
  fuchsia::math::PointF center;
  center.x = (box.min.x + box.max.x) / 2;
  center.y = (box.min.y + box.max.y) / 2;
  return center;
}

}  // namespace

class AccessibilityBridgeTest : public cr_fuchsia::WebEngineBrowserTest {
 public:
  AccessibilityBridgeTest() : semantics_manager_binding_(&semantics_manager_) {
    cr_fuchsia::WebEngineBrowserTest::set_test_server_root(
        base::FilePath(cr_fuchsia::kTestServerRoot));
  }

  ~AccessibilityBridgeTest() override = default;

  AccessibilityBridgeTest(const AccessibilityBridgeTest&) = delete;
  AccessibilityBridgeTest& operator=(const AccessibilityBridgeTest&) = delete;

  void SetUp() override {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchNative(switches::kOzonePlatform,
                                     switches::kHeadless);
    command_line->AppendSwitch(switches::kHeadless);
    cr_fuchsia::WebEngineBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    fuchsia::accessibility::semantics::SemanticsManagerPtr
        semantics_manager_ptr;
    semantics_manager_binding_.Bind(semantics_manager_ptr.NewRequest());

    frame_ptr_ =
        cr_fuchsia::WebEngineBrowserTest::CreateFrame(&navigation_listener_);
    frame_impl_ = context_impl()->GetFrameImplForTest(&frame_ptr_);
    frame_impl_->set_semantics_manager_for_test(
        std::move(semantics_manager_ptr));
    frame_ptr_->EnableHeadlessRendering();

    semantics_manager_.WaitUntilViewRegistered();
    ASSERT_TRUE(semantics_manager_.is_view_registered());
    ASSERT_TRUE(semantics_manager_.is_listener_valid());

    frame_ptr_->GetNavigationController(navigation_controller_.NewRequest());
    ASSERT_TRUE(embedded_test_server()->Start());
    semantics_manager_.SetSemanticsModeEnabled(true);
  }

 protected:
  fuchsia::web::FramePtr frame_ptr_;
  FrameImpl* frame_impl_;
  FakeSemanticsManager semantics_manager_;
  fidl::Binding<fuchsia::accessibility::semantics::SemanticsManager>
      semantics_manager_binding_;
  cr_fuchsia::TestNavigationListener navigation_listener_;
  fuchsia::web::NavigationControllerPtr navigation_controller_;
};

// Test registration to the SemanticsManager and accessibility mode on
// WebContents is set correctly.
IN_PROC_BROWSER_TEST_F(AccessibilityBridgeTest, RegisterViewRef) {
  // Change the accessibility mode on the Fuchsia side and check that it is
  // propagated correctly.
  ASSERT_FALSE(frame_impl_->web_contents_for_test()
                   ->IsWebContentsOnlyAccessibilityModeForTesting());
  semantics_manager_.SetSemanticsModeEnabled(true);

  // Spin the loop to let the FrameImpl receive the mode-change.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(frame_impl_->web_contents_for_test()
                  ->IsWebContentsOnlyAccessibilityModeForTesting());
}

IN_PROC_BROWSER_TEST_F(AccessibilityBridgeTest, CorrectDataSent) {
  GURL page_url(embedded_test_server()->GetURL(kPage1Path));
  ASSERT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      navigation_controller_.get(), fuchsia::web::LoadUrlParams(),
      page_url.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(page_url, kPage1Title);

  // Check that the data values are correct in the FakeSemanticTree.
  // TODO(fxb/18796): Test more fields once Chrome to Fuchsia conversions are
  // available.
  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage1NodeCount);
  EXPECT_TRUE(
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kPage1Title));
  EXPECT_TRUE(
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kButtonName1));
  EXPECT_TRUE(
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kParagraphName));
}

// Batching is performed when the number of nodes to send or delete exceeds the
// maximum, as set on the Fuchsia side. Check that all nodes are received by the
// Semantic Tree when batching is performed.
IN_PROC_BROWSER_TEST_F(AccessibilityBridgeTest, DataSentWithBatching) {
  GURL page_url(embedded_test_server()->GetURL(kPage2Path));
  ASSERT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      navigation_controller_.get(), fuchsia::web::LoadUrlParams(),
      page_url.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(page_url, kPage2Title);

  // Run until we expect more than a batch's worth of nodes to be present.
  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage2NodeCount);
  EXPECT_TRUE(semantics_manager_.semantic_tree()->GetNodeFromLabel(kNodeName));
}

// Check that semantics information is correctly sent when navigating from page
// to page.
IN_PROC_BROWSER_TEST_F(AccessibilityBridgeTest, TestNavigation) {
  GURL page_url1(embedded_test_server()->GetURL(kPage1Path));
  ASSERT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      navigation_controller_.get(), fuchsia::web::LoadUrlParams(),
      page_url1.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(page_url1, kPage1Title);

  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage1NodeCount);
  EXPECT_TRUE(
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kPage1Title));
  EXPECT_TRUE(
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kButtonName1));
  EXPECT_TRUE(
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kParagraphName));

  GURL page_url2(embedded_test_server()->GetURL(kPage2Path));
  ASSERT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      navigation_controller_.get(), fuchsia::web::LoadUrlParams(),
      page_url2.spec()));

  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage2NodeCount);
  EXPECT_TRUE(
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kPage2Title));
  EXPECT_TRUE(semantics_manager_.semantic_tree()->GetNodeFromLabel(kNodeName));

  // Check that data from the first page has been deleted successfully.
  EXPECT_FALSE(
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kButtonName1));
  EXPECT_FALSE(
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kParagraphName));
}

// Checks that the correct node ID is returned when performing hit testing.
// TODO(https://crbug.com/1050049): Re-enable once flake is fixed.
IN_PROC_BROWSER_TEST_F(AccessibilityBridgeTest, DISABLED_HitTest) {
  GURL page_url(embedded_test_server()->GetURL(kPage1Path));
  ASSERT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      navigation_controller_.get(), fuchsia::web::LoadUrlParams(),
      page_url.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(page_url, kPage1Title);

  fuchsia::accessibility::semantics::Node* hit_test_node =
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kParagraphName);
  EXPECT_TRUE(hit_test_node);

  fuchsia::math::PointF target_point =
      GetCenterOfBox(hit_test_node->location());

  EXPECT_EQ(hit_test_node->node_id(),
            semantics_manager_.HitTestAtPointSync(std::move(target_point)));

  // Expect hit testing to return the root when the point given is out of
  // bounds or there is no semantic node at that position.
  target_point.x = -1;
  target_point.y = -1;
  EXPECT_EQ(0u, semantics_manager_.HitTestAtPointSync(std::move(target_point)));
  target_point.x = 1;
  target_point.y = 1;
  EXPECT_EQ(0u, semantics_manager_.HitTestAtPointSync(std::move(target_point)));
}

IN_PROC_BROWSER_TEST_F(AccessibilityBridgeTest, PerformDefaultAction) {
  GURL page_url(embedded_test_server()->GetURL(kPage1Path));
  ASSERT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      navigation_controller_.get(), fuchsia::web::LoadUrlParams(),
      page_url.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(page_url, kPage1Title);
  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage1NodeCount);

  fuchsia::accessibility::semantics::Node* button1 =
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kButtonName1);
  EXPECT_TRUE(button1);
  fuchsia::accessibility::semantics::Node* button2 =
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kButtonName2);
  EXPECT_TRUE(button2);
  fuchsia::accessibility::semantics::Node* button3 =
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kButtonName3);
  EXPECT_TRUE(button3);

  // Perform the default action (click) on multiple buttons.
  semantics_manager_.RequestAccessibilityAction(
      button1->node_id(), fuchsia::accessibility::semantics::Action::DEFAULT);
  semantics_manager_.RequestAccessibilityAction(
      button2->node_id(), fuchsia::accessibility::semantics::Action::DEFAULT);
  semantics_manager_.RunUntilNumActionsHandledEquals(2);

  // Handle the case that actions are still in flight when AccessibilityBridge
  // gets torn down. The corresponding callbacks should still be run.
  frame_impl_->set_handle_actions_for_test(false);
  semantics_manager_.RequestAccessibilityAction(
      button3->node_id(), fuchsia::accessibility::semantics::Action::DEFAULT);
  frame_ptr_.Unbind();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, semantics_manager_.num_actions_handled());
  EXPECT_EQ(1, semantics_manager_.num_actions_unhandled());
}

IN_PROC_BROWSER_TEST_F(AccessibilityBridgeTest, PerformUnsupportedAction) {
  GURL page_url(embedded_test_server()->GetURL(kPage1Path));
  ASSERT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      navigation_controller_.get(), fuchsia::web::LoadUrlParams(),
      page_url.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(page_url, kPage1Title);
  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage1NodeCount);

  fuchsia::accessibility::semantics::Node* button1 =
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kButtonName1);
  EXPECT_TRUE(button1);
  fuchsia::accessibility::semantics::Node* button2 =
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kButtonName2);
  EXPECT_TRUE(button2);

  // Perform one supported action (DEFAULT) and one non-supported action
  // (SET_VALUE);
  semantics_manager_.RequestAccessibilityAction(
      button1->node_id(), fuchsia::accessibility::semantics::Action::DEFAULT);
  semantics_manager_.RequestAccessibilityAction(
      button2->node_id(), fuchsia::accessibility::semantics::Action::SET_VALUE);
  semantics_manager_.RunUntilNumActionsHandledEquals(2);

  EXPECT_EQ(1, semantics_manager_.num_actions_handled());
  EXPECT_EQ(1, semantics_manager_.num_actions_unhandled());
}
