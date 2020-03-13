// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/ax_content_node_data_mojom_traits.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/mojom/ax_node_data_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<
    ax::mojom::AXContentNodeDataDataView,
    content::AXContentNodeData>::Read(ax::mojom::AXContentNodeDataDataView data,
                                      content::AXContentNodeData* out) {
  if (!data.ReadNodeData(static_cast<ui::AXNodeData*>(out)))
    return false;

  out->child_routing_id = data.child_routing_id();

  return true;
}

}  // namespace mojo
