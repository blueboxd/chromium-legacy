// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_AX_CONTENT_NODE_DATA_MOJOM_TRAITS_H_
#define CONTENT_COMMON_AX_CONTENT_NODE_DATA_MOJOM_TRAITS_H_

#include "content/common/ax_content_node_data.h"
#include "content/common/ax_content_node_data.mojom-shared.h"
#include "ui/accessibility/ax_node_data.h"

namespace mojo {

template <>
struct StructTraits<ax::mojom::AXContentNodeDataDataView,
                    content::AXContentNodeData> {
  static const ui::AXNodeData& node_data(const content::AXContentNodeData& p) {
    return p;
  }
  static int32_t child_routing_id(const content::AXContentNodeData& p) {
    return p.child_routing_id;
  }

  static bool Read(ax::mojom::AXContentNodeDataDataView data,
                   content::AXContentNodeData* out);
};

}  // namespace mojo

#endif  // CONTENT_COMMON_AX_CONTENT_NODE_DATA_MOJOM_TRAITS_H_
