// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/fake_semantic_tree.h"

#include <lib/fidl/cpp/binding.h>
#include <zircon/types.h>

#include "base/auto_reset.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

FakeSemanticTree::FakeSemanticTree() : semantic_tree_binding_(this) {}
FakeSemanticTree::~FakeSemanticTree() = default;

void FakeSemanticTree::Bind(
    fidl::InterfaceRequest<fuchsia::accessibility::semantics::SemanticTree>
        semantic_tree_request) {
  semantic_tree_binding_.Bind(std::move(semantic_tree_request));
}

bool FakeSemanticTree::IsTreeValid(
    fuchsia::accessibility::semantics::Node* node,
    size_t* tree_size) {
  (*tree_size)++;

  if (!node->has_child_ids())
    return true;

  bool is_valid = true;
  for (auto c : node->child_ids()) {
    fuchsia::accessibility::semantics::Node* child = GetNodeWithId(c);
    if (!child)
      return false;
    is_valid &= IsTreeValid(child, tree_size);
  }
  return is_valid;
}

void FakeSemanticTree::Disconnect() {
  semantic_tree_binding_.Close(ZX_ERR_INTERNAL);
}

void FakeSemanticTree::RunUntilNodeCountAtLeast(size_t count) {
  DCHECK(!on_commit_updates_);
  if (nodes_.size() >= count)
    return;

  base::RunLoop run_loop;
  base::AutoReset<base::RepeatingClosure> auto_reset(
      &on_commit_updates_,
      base::BindLambdaForTesting([this, count, &run_loop]() {
        if (nodes_.size() >= count) {
          run_loop.Quit();
        }
      }));
  run_loop.Run();
}

fuchsia::accessibility::semantics::Node* FakeSemanticTree::GetNodeWithId(
    uint32_t id) {
  for (auto& node : nodes_) {
    if (node.has_node_id() && node.node_id() == id) {
      return &node;
    }
  }
  return nullptr;
}

fuchsia::accessibility::semantics::Node* FakeSemanticTree::GetNodeFromLabel(
    base::StringPiece label) {
  fuchsia::accessibility::semantics::Node* to_return = nullptr;
  for (auto& node : nodes_) {
    if (node.has_attributes() && node.attributes().has_label() &&
        node.attributes().label() == label) {
      // There are sometimes multiple semantic nodes with the same label. Hit
      // testing should return the node with the smallest node ID so behaviour
      // is consistent with the hit testing API being called.
      if (!to_return) {
        to_return = &node;
      } else if (node.node_id() < to_return->node_id()) {
        to_return = &node;
      }
    }
  }

  return to_return;
}

void FakeSemanticTree::UpdateSemanticNodes(
    std::vector<fuchsia::accessibility::semantics::Node> nodes) {
  nodes_.reserve(nodes.size() + nodes_.size());
  for (auto& node : nodes) {
    // Delete an existing node that's being updated to avoid having duplicate
    // nodes.
    DeleteSemanticNodes({node.node_id()});
    nodes_.push_back(std::move(node));
  }
}

void FakeSemanticTree::DeleteSemanticNodes(std::vector<uint32_t> node_ids) {
  for (auto id : node_ids) {
    for (uint i = 0; i < nodes_.size(); i++) {
      if (nodes_.at(i).node_id() == id)
        nodes_.erase(nodes_.begin() + i);
    }
  }
}

void FakeSemanticTree::CommitUpdates(CommitUpdatesCallback callback) {
  callback();
  if (on_commit_updates_)
    on_commit_updates_.Run();
  if (nodes_.size() > 0) {
    size_t tree_size = 0;
    EXPECT_TRUE(IsTreeValid(GetNodeWithId(0), &tree_size));
    EXPECT_EQ(tree_size, nodes_.size());
  }
}

void FakeSemanticTree::NotImplemented_(const std::string& name) {
  NOTIMPLEMENTED() << name;
}
