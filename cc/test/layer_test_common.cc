// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/layer_test_common.h"

#include <stddef.h>

#include "cc/animation/animation.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/base/math_util.h"
#include "cc/base/region.h"
#include "cc/layers/append_quads_data.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/test/mock_occlusion_tracker.h"
#include "cc/test/property_tree_test_utils.h"
#include "cc/trees/layer_tree_host_common.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/render_pass.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace cc {

// Align with expected and actual output.
const char* LayerTestCommon::quad_string = "    Quad: ";

RenderSurfaceImpl* GetRenderSurface(LayerImpl* layer_impl) {
  EffectTree& effect_tree =
      layer_impl->layer_tree_impl()->property_trees()->effect_tree;

  if (RenderSurfaceImpl* surface =
          effect_tree.GetRenderSurface(layer_impl->effect_tree_index()))
    return surface;

  return effect_tree.GetRenderSurface(
      effect_tree.Node(layer_impl->effect_tree_index())->target_id);
}

static bool CanRectFBeSafelyRoundedToRect(const gfx::RectF& r) {
  // Ensure that range of float values is not beyond integer range.
  if (!r.IsExpressibleAsRect())
    return false;

  // Ensure that the values are actually integers.
  gfx::RectF floored_rect(std::floor(r.x()), std::floor(r.y()),
                          std::floor(r.width()), std::floor(r.height()));
  return floored_rect == r;
}

void LayerTestCommon::VerifyQuadsExactlyCoverRect(const viz::QuadList& quads,
                                                  const gfx::Rect& rect) {
  Region remaining = rect;

  for (auto iter = quads.cbegin(); iter != quads.cend(); ++iter) {
    EXPECT_TRUE(iter->rect.Contains(iter->visible_rect));

    gfx::RectF quad_rectf = MathUtil::MapClippedRect(
        iter->shared_quad_state->quad_to_target_transform,
        gfx::RectF(iter->visible_rect));

    // Before testing for exact coverage in the integer world, assert that
    // rounding will not round the rect incorrectly.
    ASSERT_TRUE(CanRectFBeSafelyRoundedToRect(quad_rectf));

    gfx::Rect quad_rect = gfx::ToEnclosingRect(quad_rectf);

    EXPECT_TRUE(rect.Contains(quad_rect)) << quad_string << iter.index()
                                          << " rect: " << rect.ToString()
                                          << " quad: " << quad_rect.ToString();
    EXPECT_TRUE(remaining.Contains(quad_rect))
        << quad_string << iter.index() << " remaining: " << remaining.ToString()
        << " quad: " << quad_rect.ToString();
    remaining.Subtract(quad_rect);
  }

  EXPECT_TRUE(remaining.IsEmpty());
}

// static
void LayerTestCommon::VerifyQuadsAreOccluded(const viz::QuadList& quads,
                                             const gfx::Rect& occluded,
                                             size_t* partially_occluded_count) {
  // No quad should exist if it's fully occluded.
  for (auto* quad : quads) {
    gfx::Rect target_visible_rect = MathUtil::MapEnclosingClippedRect(
        quad->shared_quad_state->quad_to_target_transform, quad->visible_rect);
    EXPECT_FALSE(occluded.Contains(target_visible_rect));
  }

  // Quads that are fully occluded on one axis only should be shrunken.
  for (auto* quad : quads) {
    gfx::Rect target_rect = MathUtil::MapEnclosingClippedRect(
        quad->shared_quad_state->quad_to_target_transform, quad->rect);
    if (!quad->shared_quad_state->quad_to_target_transform
             .IsIdentityOrIntegerTranslation()) {
      DCHECK(quad->shared_quad_state->quad_to_target_transform
                 .IsPositiveScaleOrTranslation())
          << quad->shared_quad_state->quad_to_target_transform.ToString();
      gfx::RectF target_rectf = MathUtil::MapClippedRect(
          quad->shared_quad_state->quad_to_target_transform,
          gfx::RectF(quad->rect));
      // Scale transforms allowed, as long as the final transformed rect
      // ends up on integer boundaries for ease of testing.
      ASSERT_EQ(target_rectf, gfx::RectF(target_rect));
    }

    bool fully_occluded_horizontal = target_rect.x() >= occluded.x() &&
                                     target_rect.right() <= occluded.right();
    bool fully_occluded_vertical = target_rect.y() >= occluded.y() &&
                                   target_rect.bottom() <= occluded.bottom();
    bool should_be_occluded =
        target_rect.Intersects(occluded) &&
        (fully_occluded_vertical || fully_occluded_horizontal);
    if (!should_be_occluded) {
      EXPECT_EQ(quad->rect.ToString(), quad->visible_rect.ToString());
    } else {
      EXPECT_NE(quad->rect.ToString(), quad->visible_rect.ToString());
      EXPECT_TRUE(quad->rect.Contains(quad->visible_rect));
      ++(*partially_occluded_count);
    }
  }
}

LayerTestCommon::LayerImplTest::LayerImplTest()
    : LayerImplTest(LayerTreeSettings()) {}

LayerTestCommon::LayerImplTest::LayerImplTest(
    std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink)
    : LayerImplTest(LayerTreeSettings(), std::move(layer_tree_frame_sink)) {}

LayerTestCommon::LayerImplTest::LayerImplTest(const LayerTreeSettings& settings)
    : LayerImplTest(settings, FakeLayerTreeFrameSink::Create3d()) {}

LayerTestCommon::LayerImplTest::LayerImplTest(
    const LayerTreeSettings& settings,
    std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink)
    : layer_tree_frame_sink_(std::move(layer_tree_frame_sink)),
      animation_host_(AnimationHost::CreateForTesting(ThreadInstance::MAIN)),
      host_(FakeLayerTreeHost::Create(&client_,
                                      &task_graph_runner_,
                                      animation_host_.get(),
                                      settings)),
      render_pass_(viz::RenderPass::Create()),
      layer_impl_id_(2) {
  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_->host_impl()->active_tree(), 1);
  host_->host_impl()->active_tree()->SetRootLayerForTesting(std::move(root));
  host_->host_impl()->SetVisible(true);
  EXPECT_TRUE(
      host_->host_impl()->InitializeFrameSink(layer_tree_frame_sink_.get()));

  const int timeline_id = AnimationIdProvider::NextTimelineId();
  timeline_ = AnimationTimeline::Create(timeline_id);
  animation_host_->AddAnimationTimeline(timeline_);
  // Create impl-side instance.
  animation_host_->PushPropertiesTo(host_->host_impl()->animation_host());
  timeline_impl_ =
      host_->host_impl()->animation_host()->GetTimelineById(timeline_id);
}

LayerTestCommon::LayerImplTest::~LayerImplTest() {
  animation_host_->RemoveAnimationTimeline(timeline_);
  timeline_ = nullptr;
  host_->host_impl()->ReleaseLayerTreeFrameSink();
}

void LayerTestCommon::LayerImplTest::CalcDrawProps(
    const gfx::Size& viewport_size) {
  RenderSurfaceList render_surface_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root_layer_for_testing(), gfx::Rect(viewport_size), &render_surface_list);
  LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);
}

void LayerTestCommon::LayerImplTest::AppendQuadsWithOcclusion(
    LayerImpl* layer_impl,
    const gfx::Rect& occluded) {
  AppendQuadsData data;

  render_pass_->quad_list.clear();
  render_pass_->shared_quad_state_list.clear();

  Occlusion occlusion(layer_impl->DrawTransform(),
                      SimpleEnclosedRegion(occluded), SimpleEnclosedRegion());
  layer_impl->draw_properties().occlusion_in_content_space = occlusion;

  if (layer_impl->WillDraw(DRAW_MODE_HARDWARE, resource_provider())) {
    layer_impl->AppendQuads(render_pass_.get(), &data);
    layer_impl->DidDraw(resource_provider());
  }
}

void LayerTestCommon::LayerImplTest::AppendQuadsForPassWithOcclusion(
    LayerImpl* layer_impl,
    viz::RenderPass* given_render_pass,
    const gfx::Rect& occluded) {
  AppendQuadsData data;

  given_render_pass->quad_list.clear();
  given_render_pass->shared_quad_state_list.clear();

  Occlusion occlusion(layer_impl->DrawTransform(),
                      SimpleEnclosedRegion(occluded), SimpleEnclosedRegion());
  layer_impl->draw_properties().occlusion_in_content_space = occlusion;

  layer_impl->WillDraw(DRAW_MODE_HARDWARE, resource_provider());
  layer_impl->AppendQuads(given_render_pass, &data);
  layer_impl->DidDraw(resource_provider());
}

void LayerTestCommon::LayerImplTest::AppendSurfaceQuadsWithOcclusion(
    RenderSurfaceImpl* surface_impl,
    const gfx::Rect& occluded) {
  AppendQuadsData data;

  render_pass_->quad_list.clear();
  render_pass_->shared_quad_state_list.clear();

  surface_impl->set_occlusion_in_content_space(
      Occlusion(gfx::Transform(), SimpleEnclosedRegion(occluded),
                SimpleEnclosedRegion()));
  surface_impl->AppendQuads(DRAW_MODE_HARDWARE, render_pass_.get(), &data);
}

void LayerTestCommon::LayerImplTest::RequestCopyOfOutput() {
  root_layer_for_testing()->test_properties()->copy_requests.push_back(
      viz::CopyOutputRequest::CreateStubForTesting());
}

void LayerTestCommon::LayerImplTest::ExecuteCalculateDrawProperties(
    Layer* root_layer,
    float device_scale_factor,
    float page_scale_factor,
    Layer* page_scale_layer,
    Layer* inner_viewport_scroll_layer,
    Layer* outer_viewport_scroll_layer) {
  EXPECT_TRUE(page_scale_layer || (page_scale_factor == 1.f));
  gfx::Rect device_viewport_rect =
      gfx::Rect(root_layer->bounds().width() * device_scale_factor,
                root_layer->bounds().height() * device_scale_factor);

  root_layer->layer_tree_host()->SetViewportRectAndScale(
      device_viewport_rect, device_scale_factor,
      viz::LocalSurfaceIdAllocation());

  // We are probably not testing what is intended if the root_layer bounds are
  // empty.
  DCHECK(!root_layer->bounds().IsEmpty());
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      root_layer, device_viewport_rect);
  inputs.device_scale_factor = device_scale_factor;
  inputs.page_scale_factor = page_scale_factor;
  inputs.page_scale_layer = page_scale_layer;
  inputs.inner_viewport_scroll_layer = inner_viewport_scroll_layer;
  inputs.outer_viewport_scroll_layer = outer_viewport_scroll_layer;
  if (page_scale_layer) {
    PropertyTrees* property_trees =
        root_layer->layer_tree_host()->property_trees();
    inputs.page_scale_transform_node = property_trees->transform_tree.Node(
        page_scale_layer->transform_tree_index());
  }
  LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);
}

void LayerTestCommon::LayerImplTest::ExecuteCalculateDrawProperties(
    LayerImpl* root_layer,
    float device_scale_factor,
    float page_scale_factor,
    LayerImpl* page_scale_layer,
    LayerImpl* inner_viewport_scroll_layer,
    LayerImpl* outer_viewport_scroll_layer) {
  if (device_scale_factor !=
          root_layer->layer_tree_impl()->device_scale_factor() &&
      !root_layer->layer_tree_impl()->settings().use_layer_lists)
    root_layer->layer_tree_impl()->property_trees()->needs_rebuild = true;

  root_layer->layer_tree_impl()->SetDeviceScaleFactor(device_scale_factor);

  EXPECT_TRUE(page_scale_layer || (page_scale_factor == 1.f));

  gfx::Rect device_viewport_rect =
      gfx::Rect(root_layer->bounds().width() * device_scale_factor,
                root_layer->bounds().height() * device_scale_factor);

  render_surface_list_impl_.reset(new RenderSurfaceList);

  // We are probably not testing what is intended if the root_layer bounds are
  // empty.
  DCHECK(!root_layer->bounds().IsEmpty());
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root_layer, device_viewport_rect, render_surface_list_impl_.get());
  inputs.device_scale_factor = device_scale_factor;
  inputs.page_scale_factor = page_scale_factor;
  inputs.page_scale_layer = page_scale_layer;
  inputs.inner_viewport_scroll_layer = inner_viewport_scroll_layer;
  inputs.outer_viewport_scroll_layer = outer_viewport_scroll_layer;
  if (page_scale_layer) {
    PropertyTrees* property_trees =
        root_layer->layer_tree_impl()->property_trees();
    inputs.page_scale_transform_node = property_trees->transform_tree.Node(
        page_scale_layer->transform_tree_index());
  }

  LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);
}

void LayerTestCommon::LayerImplTest::
    ExecuteCalculateDrawPropertiesWithoutAdjustingRasterScales(
        LayerImpl* root_layer) {
  gfx::Rect device_viewport_rect =
      gfx::Rect(root_layer->bounds().width(), root_layer->bounds().height());
  render_surface_list_impl_.reset(new RenderSurfaceList);

  DCHECK(!root_layer->bounds().IsEmpty());
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root_layer, device_viewport_rect, render_surface_list_impl_.get());

  LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);
}

void LayerTestCommon::SetupBrowserControlsAndScrollLayerWithVirtualViewport(
    LayerTreeHostImpl* host_impl,
    LayerTreeImpl* tree_impl,
    float top_controls_height,
    const gfx::Size& inner_viewport_size,
    const gfx::Size& outer_viewport_size,
    const gfx::Size& scroll_layer_size) {
  tree_impl->SetRootLayerForTesting(nullptr);
  tree_impl->set_browser_controls_shrink_blink_size(true);
  tree_impl->SetTopControlsHeight(top_controls_height);
  tree_impl->SetCurrentBrowserControlsShownRatio(1.f);
  tree_impl->PushPageScaleFromMainThread(1.f, 1.f, 1.f);
  host_impl->DidChangeBrowserControlsPosition();

  std::unique_ptr<LayerImpl> root = LayerImpl::Create(tree_impl, 1);
  std::unique_ptr<LayerImpl> root_clip = LayerImpl::Create(tree_impl, 2);
  std::unique_ptr<LayerImpl> page_scale = LayerImpl::Create(tree_impl, 3);

  std::unique_ptr<LayerImpl> outer_scroll = LayerImpl::Create(tree_impl, 4);
  std::unique_ptr<LayerImpl> outer_clip = LayerImpl::Create(tree_impl, 5);

  root_clip->SetBounds(inner_viewport_size);
  root->SetScrollable(inner_viewport_size);
  root->SetElementId(LayerIdToElementIdForTesting(root->id()));
  root->SetBounds(outer_viewport_size);
  root->SetDrawsContent(false);
  root_clip->test_properties()->force_render_surface = true;
  root->test_properties()->is_container_for_fixed_position_layers = true;
  outer_clip->SetBounds(outer_viewport_size);
  outer_scroll->SetScrollable(outer_viewport_size);
  outer_scroll->SetElementId(LayerIdToElementIdForTesting(outer_scroll->id()));
  outer_scroll->SetBounds(scroll_layer_size);
  outer_scroll->SetDrawsContent(false);
  outer_scroll->test_properties()->is_container_for_fixed_position_layers =
      true;

  int inner_viewport_container_layer_id = root_clip->id();
  int outer_viewport_container_layer_id = outer_clip->id();
  int inner_viewport_scroll_layer_id = root->id();
  int outer_viewport_scroll_layer_id = outer_scroll->id();
  int page_scale_layer_id = page_scale->id();

  outer_clip->test_properties()->AddChild(std::move(outer_scroll));
  root->test_properties()->AddChild(std::move(outer_clip));
  page_scale->test_properties()->AddChild(std::move(root));
  root_clip->test_properties()->AddChild(std::move(page_scale));

  tree_impl->SetRootLayerForTesting(std::move(root_clip));
  LayerTreeImpl::ViewportLayerIds viewport_ids;
  viewport_ids.page_scale = page_scale_layer_id;
  viewport_ids.inner_viewport_container = inner_viewport_container_layer_id;
  viewport_ids.outer_viewport_container = outer_viewport_container_layer_id;
  viewport_ids.inner_viewport_scroll = inner_viewport_scroll_layer_id;
  viewport_ids.outer_viewport_scroll = outer_viewport_scroll_layer_id;
  tree_impl->SetViewportLayersFromIds(viewport_ids);
  tree_impl->BuildPropertyTreesForTesting();

  tree_impl->SetDeviceViewportRect(gfx::Rect(inner_viewport_size));
  LayerImpl* root_clip_ptr = tree_impl->root_layer_for_testing();
  EXPECT_EQ(inner_viewport_size, root_clip_ptr->bounds());
}

}  // namespace cc
