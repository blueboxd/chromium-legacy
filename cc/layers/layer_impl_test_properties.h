// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_LAYER_IMPL_TEST_PROPERTIES_H_
#define CC_LAYERS_LAYER_IMPL_TEST_PROPERTIES_H_

#include <set>
#include <vector>

#include "cc/input/scroll_snap_data.h"
#include "cc/layers/layer_collections.h"
#include "cc/paint/element_id.h"
#include "cc/paint/filter_operations.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/rrect_f.h"
#include "ui/gfx/transform.h"

namespace viz {
class CopyOutputRequest;
}

namespace cc {

class LayerImpl;

struct CC_EXPORT LayerImplTestProperties {
  explicit LayerImplTestProperties(LayerImpl* owning_layer);
  ~LayerImplTestProperties();

  void AddChild(std::unique_ptr<LayerImpl> child);
  std::unique_ptr<LayerImpl> RemoveChild(LayerImpl* child);
  void SetMaskLayer(std::unique_ptr<LayerImpl> mask);

  LayerImpl* owning_layer;
  bool double_sided;
  bool trilinear_filtering;
  bool cache_render_surface;
  bool force_render_surface;
  bool is_container_for_fixed_position_layers;
  bool should_flatten_transform;
  bool hide_layer_and_subtree;
  bool opacity_can_animate;
  bool subtree_has_copy_request;
  int sorting_context_id;
  float opacity;
  FilterOperations filters;
  FilterOperations backdrop_filters;
  base::Optional<gfx::RRectF> backdrop_filter_bounds;
  ElementId backdrop_mask_element_id;
  float backdrop_filter_quality;
  gfx::PointF filters_origin;
  SkBlendMode blend_mode;
  gfx::Point3F transform_origin;
  gfx::Transform transform;
  gfx::PointF position;
  std::vector<std::unique_ptr<viz::CopyOutputRequest>> copy_requests;
  LayerImplList children;
  LayerImpl* mask_layer;
  LayerImpl* parent;
  uint32_t main_thread_scrolling_reasons = 0;
  bool user_scrollable_horizontal = true;
  bool user_scrollable_vertical = true;
  base::Optional<SnapContainerData> snap_container_data;
  gfx::RRectF rounded_corner_bounds;
};

}  // namespace cc

#endif  // CC_LAYERS_LAYER_IMPL_TEST_PROPERTIES_H_
