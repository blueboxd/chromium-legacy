// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/accessibility_bridge.h"

#include <lib/sys/cpp/component_context.h>
#include <lib/ui/scenic/cpp/view_ref_pair.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "fuchsia/engine/browser/ax_tree_converter.h"
#include "ui/accessibility/ax_action_data.h"

using fuchsia::accessibility::semantics::SemanticTree;

namespace {

constexpr uint32_t kSemanticNodeRootId = 0;

// TODO(https://crbug.com/973095): Update this value based on average and
// maximum sizes of serialized Semantic Nodes.
constexpr size_t kMaxNodesPerUpdate = 16;

// Template function to handle batching and sending of FIDL messages when
// updating or deleting nodes.
template <typename T>
void SendBatches(std::vector<T> pending_items,
                 base::RepeatingCallback<void(std::vector<T>)> callback) {
  std::vector<T> nodes_to_send;
  for (size_t i = 0; i < pending_items.size(); i++) {
    nodes_to_send.push_back(std::move(pending_items.at(i)));
    if (nodes_to_send.size() == kMaxNodesPerUpdate) {
      callback.Run(std::move(nodes_to_send));
      nodes_to_send.clear();
    }
  }

  if (!nodes_to_send.empty()) {
    callback.Run(std::move(nodes_to_send));
  }
}

}  // namespace

AccessibilityBridge::AccessibilityBridge(
    fuchsia::accessibility::semantics::SemanticsManagerPtr semantics_manager,
    fuchsia::ui::views::ViewRef view_ref,
    content::WebContents* web_contents)
    : binding_(this), web_contents_(web_contents) {
  DCHECK(web_contents_);
  Observe(web_contents_);
  tree_.AddObserver(this);

  semantics_manager->RegisterViewForSemantics(
      std::move(view_ref), binding_.NewBinding(), tree_ptr_.NewRequest());
  tree_ptr_.set_error_handler([](zx_status_t status) {
    ZX_LOG_IF(ERROR, status != ZX_ERR_PEER_CLOSED, status)
        << "Semantic Tree disconnected.";
  });
}

AccessibilityBridge::~AccessibilityBridge() = default;

void AccessibilityBridge::TryCommit() {
  if (commit_inflight_ || (to_send_.empty() && to_delete_.empty()))
    return;

  if (!to_send_.empty()) {
    SendBatches<fuchsia::accessibility::semantics::Node>(
        std::move(to_send_),
        base::BindRepeating(
            [](SemanticTree* tree,
               std::vector<fuchsia::accessibility::semantics::Node> nodes) {
              tree->UpdateSemanticNodes(std::move(nodes));
            },
            base::Unretained(tree_ptr_.get())));
  }

  if (!to_delete_.empty()) {
    SendBatches<uint32_t>(
        std::move(to_delete_),
        base::BindRepeating(
            [](SemanticTree* tree, std::vector<uint32_t> nodes) {
              tree->DeleteSemanticNodes(std::move(nodes));
            },
            base::Unretained(tree_ptr_.get())));
  }

  tree_ptr_->CommitUpdates(
      fit::bind_member(this, &AccessibilityBridge::OnCommitComplete));
  commit_inflight_ = true;
}

void AccessibilityBridge::OnCommitComplete() {
  commit_inflight_ = false;
  TryCommit();
}

uint32_t AccessibilityBridge::ConvertToFuchsiaNodeId(int32_t ax_node_id) {
  if (ax_node_id == root_id_) {
    // On the Fuchsia side, the root node is indicated by id
    // |kSemanticNodeRootId|, which is 0.
    return kSemanticNodeRootId;
  } else {
    // AXNode ids are signed, Semantic Node ids are unsigned.
    return bit_cast<uint32_t>(ax_node_id);
  }
}

void AccessibilityBridge::AccessibilityEventReceived(
    const content::AXEventNotificationDetails& details) {
  // Updates to AXTree must be applied first.
  for (const ui::AXTreeUpdate& update : details.updates) {
    tree_.Unserialize(update);
  }

  // Events to fire after tree has been updated.
  for (const ui::AXEvent& event : details.events) {
    if (event.event_type == ax::mojom::Event::kHitTestResult) {
      if (pending_hit_test_callbacks_.find(event.action_request_id) !=
          pending_hit_test_callbacks_.end()) {
        fuchsia::accessibility::semantics::Hit hit;
        hit.set_node_id(ConvertToFuchsiaNodeId(event.id));

        // Run the pending callback with the hit.
        pending_hit_test_callbacks_[event.action_request_id](std::move(hit));
        pending_hit_test_callbacks_.erase(event.action_request_id);
      }
    }
  }
}

void AccessibilityBridge::OnAccessibilityActionRequested(
    uint32_t node_id,
    fuchsia::accessibility::semantics::Action action,
    OnAccessibilityActionRequestedCallback callback) {
  NOTIMPLEMENTED();
}

void AccessibilityBridge::HitTest(fuchsia::math::PointF local_point,
                                  HitTestCallback callback) {
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kHitTest;
  gfx::Point point;
  point.set_x(local_point.x);
  point.set_y(local_point.y);
  action_data.target_point = point;
  action_data.hit_test_event_to_fire = ax::mojom::Event::kHitTestResult;
  pending_hit_test_callbacks_[action_data.request_id] = std::move(callback);

  web_contents_->GetMainFrame()->AccessibilityPerformAction(action_data);
}

void AccessibilityBridge::OnSemanticsModeChanged(
    bool updates_enabled,
    OnSemanticsModeChangedCallback callback) {
  if (updates_enabled) {
    // The first call to AccessibilityEventReceived after this call will be the
    // entire semantic tree.
    web_contents_->EnableWebContentsOnlyAccessibilityMode();
  } else {
    // The SemanticsManager will clear all state in this case, which is mirrored
    // here.
    to_send_.clear();
    to_delete_.clear();
    commit_inflight_ = false;
  }

  // Notify the SemanticsManager that semantics mode has been updated.
  callback();
}

void AccessibilityBridge::OnNodeWillBeDeleted(ui::AXTree* tree,
                                              ui::AXNode* node) {
  to_delete_.push_back(ConvertToFuchsiaNodeId(node->id()));
  TryCommit();
}

void AccessibilityBridge::OnAtomicUpdateFinished(
    ui::AXTree* tree,
    bool root_changed,
    const std::vector<ui::AXTreeObserver::Change>& changes) {
  root_id_ = tree_.root()->id();
  for (const ui::AXTreeObserver::Change& change : changes) {
    // Reparent changes aren't included here because they consist of a delete
    // and create change, which are already being handled.
    if (change.type == ui::AXTreeObserver::NODE_CREATED ||
        change.type == ui::AXTreeObserver::SUBTREE_CREATED ||
        change.type == ui::AXTreeObserver::NODE_CHANGED) {
      ui::AXNodeData ax_data = change.node->data();
      if (change.node->id() == root_id_) {
        ax_data.id = kSemanticNodeRootId;
      }
      to_send_.push_back(AXNodeDataToSemanticNode(ax_data));
    }
  }
  TryCommit();
}
