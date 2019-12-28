// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_live_region_tracker.h"

#include "base/stl_util.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_role_properties.h"

namespace ui {

AXLiveRegionTracker::AXLiveRegionTracker(AXTree* tree) : tree_(tree) {
  InitializeLiveRegionNodeToRoot(tree_->root(), nullptr);
}

AXLiveRegionTracker::~AXLiveRegionTracker() {}

void AXLiveRegionTracker::TrackNode(AXNode* node) {
  LOG(ERROR) << "TrackNode " << node->data().ToString();
  AXNode* live_root = node;
  while (live_root && !live_root->HasStringAttribute(
                          ax::mojom::StringAttribute::kLiveStatus))
    live_root = live_root->parent();
  if (live_root) {
    LOG(ERROR) << "  Live root: " << live_root->data().ToString();
    live_region_node_to_root_[node] = live_root;
  } else
    LOG(ERROR) << "  No live root";
}

void AXLiveRegionTracker::OnNodeWillBeDeleted(AXNode* node) {
  LOG(ERROR) << "OnNodeWillBeDeleted " << node->data().ToString();
  live_region_node_to_root_.erase(node);
}

AXNode* AXLiveRegionTracker::GetLiveRoot(AXNode* node) {
  LOG(ERROR) << "GetLiveRoot for " << node->data().ToString();
  auto iter = live_region_node_to_root_.find(node);
  if (iter != live_region_node_to_root_.end())
    return iter->second;
  return nullptr;
}

void AXLiveRegionTracker::InitializeLiveRegionNodeToRoot(AXNode* node,
                                                         AXNode* current_root) {
  if (!current_root &&
      node->HasStringAttribute(ax::mojom::StringAttribute::kLiveStatus)) {
    current_root = node;
  }

  if (current_root)
    live_region_node_to_root_[node] = current_root;

  for (size_t i = 0; i < node->children().size(); i++)
    InitializeLiveRegionNodeToRoot(node->children()[i], current_root);
}

}  // namespace ui
