// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/ax_content_tree_update_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<ax::mojom::AXContentTreeUpdateDataView,
                  content::AXContentTreeUpdate>::
    Read(ax::mojom::AXContentTreeUpdateDataView data,
         content::AXContentTreeUpdate* out) {
  if (!data.ReadTreeData(&out->tree_data))
    return false;

  if (!data.ReadNodes(&out->nodes))
    return false;

  out->has_tree_data = data.has_tree_data();
  out->node_id_to_clear = data.node_id_to_clear();
  out->root_id = data.root_id();
  out->event_from = data.event_from();

  return true;
}

}  // namespace mojo
