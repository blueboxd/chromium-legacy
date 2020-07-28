// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_FAKE_SEMANTIC_TREE_H_
#define FUCHSIA_ENGINE_BROWSER_FAKE_SEMANTIC_TREE_H_

#include <fuchsia/accessibility/semantics/cpp/fidl.h>
#include <fuchsia/accessibility/semantics/cpp/fidl_test_base.h>
#include <lib/fidl/cpp/binding.h>

#include "base/callback.h"

class FakeSemanticTree
    : public fuchsia::accessibility::semantics::testing::SemanticTree_TestBase {
 public:
  FakeSemanticTree();
  ~FakeSemanticTree() override;

  FakeSemanticTree(const FakeSemanticTree&) = delete;
  FakeSemanticTree& operator=(const FakeSemanticTree&) = delete;

  // Binds |semantic_tree_request| to |this|.
  void Bind(
      fidl::InterfaceRequest<fuchsia::accessibility::semantics::SemanticTree>
          semantic_tree_request);

  // Checks that the tree is complete and that there are no dangling nodes by
  // traversing the tree starting at the root. Keeps track of how many nodes are
  // visited to make sure there aren't dangling nodes in |nodes_|.
  bool IsTreeValid(fuchsia::accessibility::semantics::Node* node,
                   size_t* tree_size);

  // Disconnects the SemanticTree binding.
  void Disconnect();

  void RunUntilNodeCountAtLeast(size_t count);
  fuchsia::accessibility::semantics::Node* GetNodeWithId(uint32_t id);
  fuchsia::accessibility::semantics::Node* GetNodeFromLabel(
      base::StringPiece label);

  // fuchsia::accessibility::semantics::SemanticTree implementation.
  void UpdateSemanticNodes(
      std::vector<fuchsia::accessibility::semantics::Node> nodes) final;
  void DeleteSemanticNodes(std::vector<uint32_t> node_ids) final;
  void CommitUpdates(CommitUpdatesCallback callback) final;

  void NotImplemented_(const std::string& name) final;

 private:
  fidl::Binding<fuchsia::accessibility::semantics::SemanticTree>
      semantic_tree_binding_;
  std::vector<fuchsia::accessibility::semantics::Node> nodes_;
  base::RepeatingClosure on_commit_updates_;
};

#endif  // FUCHSIA_ENGINE_BROWSER_FAKE_SEMANTIC_TREE_H_
