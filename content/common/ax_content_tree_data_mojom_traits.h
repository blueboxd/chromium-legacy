// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_AX_CONTENT_TREE_DATA_MOJOM_TRAITS_H_
#define CONTENT_COMMON_AX_CONTENT_TREE_DATA_MOJOM_TRAITS_H_

#include "content/common/ax_content_node_data.h"
#include "content/common/ax_content_tree_data.mojom-shared.h"
#include "ui/accessibility/ax_tree_data.h"

namespace mojo {

template <>
struct StructTraits<ax::mojom::AXContentTreeDataDataView,
                    content::AXContentTreeData> {
  static const ui::AXTreeData& tree_data(const content::AXContentTreeData& p) {
    return p;
  }
  static int32_t routing_id(const content::AXContentTreeData& p) {
    return p.routing_id;
  }
  static int32_t parent_routing_id(const content::AXContentTreeData& p) {
    return p.parent_routing_id;
  }

  static bool Read(ax::mojom::AXContentTreeDataDataView data,
                   content::AXContentTreeData* out);
};

}  // namespace mojo

#endif  // CONTENT_COMMON_AX_CONTENT_TREE_DATA_MOJOM_TRAITS_H_
