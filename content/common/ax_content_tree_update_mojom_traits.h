// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_AX_CONTENT_TREE_UPDATE_MOJOM_TRAITS_H_
#define CONTENT_COMMON_AX_CONTENT_TREE_UPDATE_MOJOM_TRAITS_H_

#include "content/common/ax_content_node_data.h"
#include "content/common/ax_content_node_data_mojom_traits.h"
#include "content/common/ax_content_tree_data_mojom_traits.h"
#include "content/common/ax_content_tree_update.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<ax::mojom::AXContentTreeUpdateDataView,
                    content::AXContentTreeUpdate> {
  static bool has_tree_data(const content::AXContentTreeUpdate& p) {
    return p.has_tree_data;
  }
  static const content::AXContentTreeData& tree_data(
      const content::AXContentTreeUpdate& p) {
    return p.tree_data;
  }
  static int32_t node_id_to_clear(const content::AXContentTreeUpdate& p) {
    return p.node_id_to_clear;
  }
  static int32_t root_id(const content::AXContentTreeUpdate& p) {
    return p.root_id;
  }
  static const std::vector<content::AXContentNodeData>& nodes(
      const content::AXContentTreeUpdate& p) {
    return p.nodes;
  }

  static ax::mojom::EventFrom event_from(
      const content::AXContentTreeUpdate& p) {
    return p.event_from;
  }

  static bool Read(ax::mojom::AXContentTreeUpdateDataView data,
                   content::AXContentTreeUpdate* out);
};

}  // namespace mojo

#endif  // CONTENT_COMMON_AX_CONTENT_TREE_UPDATE_MOJOM_TRAITS_H_
