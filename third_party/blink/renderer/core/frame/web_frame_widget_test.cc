// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/web_frame_widget_base.h"

#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "third_party/blink/renderer/core/frame/web_view_frame_widget.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

namespace blink {

class WebFrameWidgetSimTest : public SimTest {};

// Tests that if a WebView is auto-resized, the associated
// WebViewFrameWidget requests a new viz::LocalSurfaceId to be allocated on the
// impl thread.
TEST_F(WebFrameWidgetSimTest, AutoResizeAllocatedLocalSurfaceId) {
  viz::ParentLocalSurfaceIdAllocator allocator;

  // Enable auto-resize.
  blink::VisualProperties visual_properties;
  visual_properties.auto_resize_enabled = true;
  visual_properties.min_size_for_auto_resize = gfx::Size(100, 100);
  visual_properties.max_size_for_auto_resize = gfx::Size(200, 200);
  allocator.GenerateId();
  visual_properties.local_surface_id_allocation =
      allocator.GetCurrentLocalSurfaceIdAllocation();
  WebView().MainFrameWidget()->ApplyVisualProperties(visual_properties);
  WebView().MainFrameWidget()->UpdateSurfaceAndScreenInfo(
      visual_properties.local_surface_id_allocation.value(),
      visual_properties.compositor_viewport_pixel_rect,
      visual_properties.screen_info);

  EXPECT_EQ(
      allocator.GetCurrentLocalSurfaceIdAllocation(),
      WebView().MainFrameWidgetBase()->LocalSurfaceIdAllocationFromParent());
  EXPECT_FALSE(WebView()
                   .MainFrameWidgetBase()
                   ->LayerTreeHost()
                   ->new_local_surface_id_request_for_testing());

  constexpr gfx::Size size(200, 200);
  static_cast<WebViewFrameWidget*>(WebView().MainFrameWidgetBase())
      ->DidAutoResize(size);
  EXPECT_EQ(
      allocator.GetCurrentLocalSurfaceIdAllocation(),
      WebView().MainFrameWidgetBase()->LocalSurfaceIdAllocationFromParent());
  EXPECT_TRUE(WebView()
                  .MainFrameWidgetBase()
                  ->LayerTreeHost()
                  ->new_local_surface_id_request_for_testing());
}

}  // namespace blink
