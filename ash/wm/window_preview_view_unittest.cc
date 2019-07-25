// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_preview_view.h"

#include "ash/test/ash_test_base.h"
#include "ash/wm/window_preview_view_test_api.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

using WindowPreviewViewTest = AshTestBase;

// Test that if we have two widgets whos windows are linked together by
// transience, WindowPreviewView's internal collection will contain both those
// two windows.
TEST_F(WindowPreviewViewTest, Basic) {
  auto widget1 = CreateTestWidget();
  auto widget2 = CreateTestWidget();

  ::wm::AddTransientChild(widget1->GetNativeWindow(),
                          widget2->GetNativeWindow());
  auto preview_view = std::make_unique<WindowPreviewView>(
      widget1->GetNativeWindow(), /*trilinear_filtering_on_init=*/false);
  WindowPreviewViewTestApi test_api(preview_view.get());
  EXPECT_EQ(2u, test_api.GetMirrorViews().size());
  EXPECT_TRUE(test_api.GetMirrorViews().contains(widget1->GetNativeWindow()));
  EXPECT_TRUE(test_api.GetMirrorViews().contains(widget2->GetNativeWindow()));
}

// Tests that WindowPreviewView behaves as expected when we add or remove
// transient children.
TEST_F(WindowPreviewViewTest, TransientChildAddedAndRemoved) {
  auto widget1 = CreateTestWidget();
  auto widget2 = CreateTestWidget();
  auto widget3 = CreateTestWidget();

  ::wm::AddTransientChild(widget1->GetNativeWindow(),
                          widget2->GetNativeWindow());
  auto preview_view = std::make_unique<WindowPreviewView>(
      widget1->GetNativeWindow(), /*trilinear_filtering_on_init=*/false);
  WindowPreviewViewTestApi test_api(preview_view.get());
  ASSERT_EQ(2u, test_api.GetMirrorViews().size());

  ::wm::AddTransientChild(widget1->GetNativeWindow(),
                          widget3->GetNativeWindow());
  EXPECT_EQ(3u, test_api.GetMirrorViews().size());

  ::wm::RemoveTransientChild(widget1->GetNativeWindow(),
                             widget3->GetNativeWindow());
  EXPECT_EQ(2u, test_api.GetMirrorViews().size());
}

// Test that WindowPreviewView layouts the transient tree correctly when each
// transient child is within the bounds of its transient parent.
TEST_F(WindowPreviewViewTest, LayoutChildWithinParentBounds) {
  UpdateDisplay("1000x1000");

  // Create two widgets linked transiently. The child window is within the
  // bounds of the parent window.
  auto widget1 = CreateTestWidget();
  auto widget2 = CreateTestWidget();
  widget1->GetNativeWindow()->SetBounds(gfx::Rect(100, 100));
  widget2->GetNativeWindow()->SetBounds(gfx::Rect(25, 25, 50, 50));
  ::wm::AddTransientChild(widget1->GetNativeWindow(),
                          widget2->GetNativeWindow());

  auto preview_view = std::make_unique<WindowPreviewView>(
      widget1->GetNativeWindow(), /*trilinear_filtering_on_init=*/false);
  WindowPreviewViewTestApi test_api(preview_view.get());
  EXPECT_EQ(gfx::RectF(100.f, 100.f), test_api.GetUnionRect());

  // Test that the ratio between the two windows is maintained.
  preview_view->SetBoundsRect(gfx::Rect(500, 500));
  EXPECT_EQ(gfx::Rect(500, 500),
            test_api.GetMirrorViewForWidget(widget1.get())->bounds());
  EXPECT_EQ(gfx::Rect(125, 125, 250, 250),
            test_api.GetMirrorViewForWidget(widget2.get())->bounds());
}

// Test that WindowPreviewView layouts the transient tree correctly when each
// transient child is outside the bounds of its transient parent.
TEST_F(WindowPreviewViewTest, LayoutChildOutsideParentBounds) {
  UpdateDisplay("1000x1000");

  // Create two widgets linked transiently. The child window is outside of the
  // bounds of the parent window.
  auto widget1 = CreateTestWidget();
  auto widget2 = CreateTestWidget();
  widget1->GetNativeWindow()->SetBounds(gfx::Rect(200, 200));
  widget2->GetNativeWindow()->SetBounds(gfx::Rect(300, 300, 100, 100));
  ::wm::AddTransientChild(widget1->GetNativeWindow(),
                          widget2->GetNativeWindow());

  auto preview_view = std::make_unique<WindowPreviewView>(
      widget1->GetNativeWindow(), /*trilinear_filtering_on_init=*/false);
  WindowPreviewViewTestApi test_api(preview_view.get());
  EXPECT_EQ(gfx::RectF(400.f, 400.f), test_api.GetUnionRect());

  // Test that the ratio between the two windows, relative to the smallest
  // rectangle which encompasses them both (0,0, 400, 400)  is maintained.
  preview_view->SetBoundsRect(gfx::Rect(500, 500));
  EXPECT_EQ(gfx::Rect(250, 250),
            test_api.GetMirrorViewForWidget(widget1.get())->bounds());
  EXPECT_EQ(gfx::Rect(375, 375, 125, 125),
            test_api.GetMirrorViewForWidget(widget2.get())->bounds());
}

}  // namespace
}  // namespace ash
