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

AXLiveRegionTracker::~AXLiveRegionTracker() = default;

void AXLiveRegionTracker::TrackNode(AXNode* node) {
  AXNode* live_root = node;
  while (live_root && !IsLiveRegionRoot(live_root))
    live_root = live_root->parent();
  if (live_root)
    live_region_node_to_root_id_[node] = live_root->id();
}

void AXLiveRegionTracker::OnNodeWillBeDeleted(AXNode* node) {
  live_region_node_to_root_id_.erase(node);
  deleted_node_ids_.insert(node->id());
}

void AXLiveRegionTracker::OnAtomicUpdateFinished() {
  deleted_node_ids_.clear();
}

AXNode* AXLiveRegionTracker::GetLiveRoot(AXNode* node) {
  auto iter = live_region_node_to_root_id_.find(node);
  if (iter != live_region_node_to_root_id_.end()) {
    int32_t id = iter->second;
    if (deleted_node_ids_.find(id) != deleted_node_ids_.end())
      return nullptr;

    return tree_->GetFromId(id);
  }
  return nullptr;
}

AXNode* AXLiveRegionTracker::GetLiveRootIfNotBusy(AXNode* node) {
  AXNode* live_root = GetLiveRoot(node);
  if (!live_root)
    return nullptr;

  // Don't return the live root if the live region is busy.
  if (live_root->data().GetBoolAttribute(ax::mojom::BoolAttribute::kBusy))
    return nullptr;

  return live_root;
}

void AXLiveRegionTracker::InitializeLiveRegionNodeToRoot(AXNode* node,
                                                         AXNode* current_root) {
  if (!current_root && IsLiveRegionRoot(node))
    current_root = node;

  if (current_root)
    live_region_node_to_root_id_[node] = current_root->id();

  for (AXNode* child : node->children())
    InitializeLiveRegionNodeToRoot(child, current_root);
}

// static
bool AXLiveRegionTracker::IsLiveRegionRoot(const AXNode* node) {
  return node->HasStringAttribute(ax::mojom::StringAttribute::kLiveStatus) &&
         node->data().GetStringAttribute(
             ax::mojom::StringAttribute::kLiveStatus) != "off";
}

}  // namespace ui
