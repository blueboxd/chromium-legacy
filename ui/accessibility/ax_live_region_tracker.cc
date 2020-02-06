// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_live_region_tracker.h"

#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_role_properties.h"

namespace ui {
namespace {

void GetTextFromSubtreeHelper(AXNode* node, std::vector<std::string>* strings) {
  if (node->data().role == ax::mojom::Role::kInlineTextBox)
    return;

  std::string name =
      node->GetStringAttribute(ax::mojom::StringAttribute::kName);
  if (!name.empty())
    strings->push_back(name);
  for (AXNode* child : node->children())
    GetTextFromSubtreeHelper(child, strings);
}

std::string GetTextFromSubtree(AXNode* node) {
  std::vector<std::string> strings;
  GetTextFromSubtreeHelper(node, &strings);
  return base::JoinString(strings, " ");
}
}  // namespace

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

void AXLiveRegionTracker::ComputeTextForChangedNode(AXNode* node,
                                                    ChangeType type) {
  AXNode* live_root = GetLiveRoot(node);
  if (!live_root)
    return;

  LiveRegionChange change;
  change.node_id = node->id();
  change.data = node->data();
  change.type = type;
  live_region_changes_[live_root].push_back(change);
}

void AXLiveRegionTracker::ComputeLiveRegionChangeDescription(
    std::map<AXNode*, std::string>* live_region_change_description) {
  for (auto iter : live_region_changes_) {
    AXNode* live_root = iter.first;
    std::set<int32_t> already_handled_node_ids;
    std::vector<std::string> additions_vector;
    std::vector<std::string> removals_vector;

    for (LiveRegionChange change : iter.second) {
      int32_t node_id = change.node_id;
      ui::AXNodeData data = change.data;
      bool is_deleted =
          deleted_node_ids_.find(node_id) != deleted_node_ids_.end();
      if (!is_deleted) {
        AXNode* node = tree_->GetFromId(node_id);
        DCHECK(node);
        if (!node)
          continue;

        while (node->data().GetBoolAttribute(
                   ax::mojom::BoolAttribute::kContainerLiveAtomic) &&
               !node->data().GetBoolAttribute(
                   ax::mojom::BoolAttribute::kLiveAtomic)) {
          node = node->parent();
        }
        if (node) {
          node_id = node->id();
          data = node->data();
        }
      }

      if (already_handled_node_ids.find(node_id) !=
          already_handled_node_ids.end())
        continue;
      already_handled_node_ids.insert(node_id);

      bool explore_subtree =
          data.GetBoolAttribute(ax::mojom::BoolAttribute::kLiveAtomic);
      std::string relevant;
      if (data.GetStringAttribute(
              ax::mojom::StringAttribute::kContainerLiveRelevant, &relevant)) {
        relevant = base::ToLowerASCII(relevant);
      } else {
        relevant = "additions text";
      }

      bool all = relevant.find("all") != std::string::npos;
      bool additions = all || relevant.find("additions") != std::string::npos;
      bool removals = all || relevant.find("removals") != std::string::npos;
      bool text = all || relevant.find("text") != std::string::npos;

      std::string live_text;
      if (explore_subtree && !is_deleted) {
        AXNode* node = tree_->GetFromId(node_id);
        DCHECK(node);
        if (node)
          live_text = GetTextFromSubtree(node);
      } else {
        live_text = data.GetStringAttribute(ax::mojom::StringAttribute::kName);
      }
      if (live_text.empty())
        continue;

      if (change.type == ChangeType::kNodeAdded && additions)
        additions_vector.push_back(live_text);
      else if (change.type == ChangeType::kTextChanged && text)
        additions_vector.push_back(live_text);
      else if (change.type == ChangeType::kNodeRemoved && removals)
        removals_vector.push_back(live_text);
    }

    if (additions_vector.empty() && removals_vector.empty())
      continue;

    std::string additions_text = base::JoinString(additions_vector, " ");
    std::string removals_text = base::JoinString(removals_vector, " ");

    std::string final_text = additions_text;
    if (!additions_text.empty() && !removals_text.empty())
      final_text += ", ";
    if (!removals_text.empty())
      final_text += removals_text + " removed";  // TODO: i18n

    (*live_region_change_description)[live_root] = final_text;
  }

  live_region_changes_.clear();
}

}  // namespace ui
