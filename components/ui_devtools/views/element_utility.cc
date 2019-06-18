// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/element_utility.h"
#include "ui/compositor/layer_owner.h"

namespace ui_devtools {

void AppendLayerProperties(
    const ui::Layer* layer,
    std::vector<std::pair<std::string, std::string>>* ret) {
  ret->emplace_back("layer-type", LayerTypeToString(layer->type()));
  ret->emplace_back("has-layer-mask",
                    layer->layer_mask_layer() ? "true" : "false");
  const cc::Layer* cc_layer = layer->cc_layer_for_testing();
  if (cc_layer) {
    // Property trees must be updated in order to get valid render surface
    // reasons.
    if (!cc_layer->layer_tree_host() ||
        cc_layer->layer_tree_host()->property_trees()->needs_rebuild)
      return;
    cc::RenderSurfaceReason render_surface = cc_layer->GetRenderSurfaceReason();
    if (render_surface != cc::RenderSurfaceReason::kNone) {
      ret->emplace_back("render-surface-reason",
                        cc::RenderSurfaceReasonToString(render_surface));
    }
  }
}

}  // namespace ui_devtools
