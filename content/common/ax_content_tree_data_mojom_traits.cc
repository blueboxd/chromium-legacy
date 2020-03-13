// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/ax_content_tree_data_mojom_traits.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/mojom/ax_tree_data_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<
    ax::mojom::AXContentTreeDataDataView,
    content::AXContentTreeData>::Read(ax::mojom::AXContentTreeDataDataView data,
                                      content::AXContentTreeData* out) {
  if (!data.ReadTreeData(static_cast<ui::AXTreeData*>(out)))
    return false;

  out->routing_id = data.routing_id();
  out->parent_routing_id = data.parent_routing_id();

  return true;
}

}  // namespace mojo
