// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_event_generator.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree_serializer.h"

namespace ui {

namespace {

std::string DumpLiveRegionText(AXEventGenerator* generator) {
  std::vector<std::string> live_region_text_strs;
  for (auto targeted_event : *generator) {
    if (targeted_event.event_params.event !=
        AXEventGenerator::Event::LIVE_REGION_CHANGED)
      continue;

    if (!targeted_event.event_params.live_region_change_description.empty()) {
      live_region_text_strs.push_back(
          targeted_event.event_params.live_region_change_description);
    }
  }

  return base::JoinString(live_region_text_strs, ", ");
}

// Class that helps set live region attributes on a live region node
// and subtree.
class AXTreeUpdateLiveRegionHelper {
 public:
  AXTreeUpdateLiveRegionHelper(AXTreeUpdate* tree_update, int node_id)
      : tree_update_(tree_update), node_id_(node_id) {
    for (int i = 0; i < static_cast<int>(tree_update_->nodes.size()); i++)
      node_id_to_index_[tree_update_->nodes[i].id] = i;
    GetNodesInSubtree(node_id_, &nodes_in_live_region_);
    nodes_in_live_region_[0]->AddStringAttribute(
        ax::mojom::StringAttribute::kLiveStatus, "polite");
    for (AXNodeData* node_data : nodes_in_live_region_) {
      node_data->AddStringAttribute(
          ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
    }
  }

  void WithAriaRelevant(const std::string& relevant) {
    nodes_in_live_region_[0]->AddStringAttribute(
        ax::mojom::StringAttribute::kLiveRelevant, relevant);
    for (AXNodeData* node_data : nodes_in_live_region_) {
      node_data->AddStringAttribute(
          ax::mojom::StringAttribute::kContainerLiveRelevant, relevant);
    }
  }

  void WithAriaAtomicOnNodeId(int node_id) {
    std::vector<AXNodeData*> nodes_in_atomic_subtree;
    GetNodesInSubtree(node_id, &nodes_in_atomic_subtree);

    nodes_in_atomic_subtree[0]->AddBoolAttribute(
        ax::mojom::BoolAttribute::kLiveAtomic, true);
    for (AXNodeData* node_data : nodes_in_atomic_subtree) {
      node_data->AddBoolAttribute(
          ax::mojom::BoolAttribute::kContainerLiveAtomic, true);
    }
  }

 private:
  void GetNodesInSubtree(int node_id, std::vector<AXNodeData*>* nodes) {
    int node_index = node_id_to_index_[node_id];
    AXNodeData* node_data = &tree_update_->nodes[node_index];
    nodes->push_back(node_data);
    for (int child_id : node_data->child_ids)
      GetNodesInSubtree(child_id, nodes);
  }

  AXTreeUpdate* tree_update_;
  int node_id_;
  std::map<int, int> node_id_to_index_;
  std::vector<AXNodeData*> nodes_in_live_region_;
};

// Helper function to set live region attributes on nodes in a
// live region when constructing an AXTreeUpdate for a unit test.
//
// Can be chained to also set aria-relevant and aria-tomic, see
// the public member functions in AXTreeUpdateLiveRegionHelper.
std::unique_ptr<AXTreeUpdateLiveRegionHelper> SetLiveRegionAtNodeId(
    AXTreeUpdate* tree_update,
    int node_id) {
  std::unique_ptr<AXTreeUpdateLiveRegionHelper> helper =
      std::make_unique<AXTreeUpdateLiveRegionHelper>(tree_update, node_id);
  return helper;
}

}  // namespace

TEST(AXLiveRegionTrackerTest, TextChange) {
  // Simple tree with a live region whose text changes directly.
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(2);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kRootWebArea;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[1].SetName("Before");
  initial_state.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kLiveStatus, "polite");

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  event_generator.set_should_compute_live_region_change_description(true);
  AXTreeUpdate update = initial_state;
  update.nodes[1].SetName("After");
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("After", DumpLiveRegionText(&event_generator));
}

TEST(AXLiveRegionTrackerTest, TextAddition) {
  // Test adding two children to a live region.
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(2);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kRootWebArea;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kGroup;

  SetLiveRegionAtNodeId(&initial_state, 1);

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  event_generator.set_should_compute_live_region_change_description(true);
  AXTreeUpdate update = initial_state;
  update.nodes.resize(4);
  update.nodes[1].child_ids = {3, 4};
  update.nodes[2].id = 3;
  update.nodes[2].role = ax::mojom::Role::kStaticText;
  update.nodes[2].SetName("Added");
  update.nodes[3].id = 4;
  update.nodes[3].role = ax::mojom::Role::kStaticText;
  update.nodes[3].SetName("two nodes");

  SetLiveRegionAtNodeId(&update, 1);

  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("Added two nodes", DumpLiveRegionText(&event_generator));
}

TEST(AXLiveRegionTrackerTest, TextRemovals) {
  // Test removing children from a live region.
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(5);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kRootWebArea;
  initial_state.nodes[0].child_ids = {2};
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kGroup;
  initial_state.nodes[1].child_ids = {3, 4, 5};
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[2].SetName("One");
  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[3].SetName("Two");
  initial_state.nodes[4].id = 5;
  initial_state.nodes[4].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[4].SetName("Three");

  SetLiveRegionAtNodeId(&initial_state, 2)->WithAriaRelevant("removals");

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  event_generator.set_should_compute_live_region_change_description(true);
  AXTreeUpdate update = initial_state;
  update.nodes.resize(2);
  update.nodes[1].child_ids = {4};

  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("One Three removed", DumpLiveRegionText(&event_generator));
}

TEST(AXLiveRegionTrackerTest, TextAddSubtree) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(2);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kRootWebArea;
  initial_state.nodes[0].child_ids = {2};
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[1].SetName("Ignore");

  SetLiveRegionAtNodeId(&initial_state, 1);

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  event_generator.set_should_compute_live_region_change_description(true);
  AXTreeUpdate update = initial_state;
  update.nodes.resize(5);
  update.nodes[0].child_ids = {2, 3};
  update.nodes[2].id = 3;
  update.nodes[2].role = ax::mojom::Role::kGroup;
  update.nodes[2].child_ids = {4, 5};
  update.nodes[3].id = 4;
  update.nodes[3].role = ax::mojom::Role::kStaticText;
  update.nodes[3].SetName("Child 1");
  update.nodes[4].id = 5;
  update.nodes[4].role = ax::mojom::Role::kStaticText;
  update.nodes[4].SetName("Child 2");

  SetLiveRegionAtNodeId(&update, 1);

  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("Child 1 Child 2", DumpLiveRegionText(&event_generator));
}

TEST(AXLiveRegionTrackerTest, RemoveSubtree) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(4);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kRootWebArea;
  initial_state.nodes[0].child_ids = {2};
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kGroup;
  initial_state.nodes[1].child_ids = {3, 4};
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[2].SetName("Left");
  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[3].SetName("Right");

  SetLiveRegionAtNodeId(&initial_state, 1)->WithAriaRelevant("removals");

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  event_generator.set_should_compute_live_region_change_description(true);
  AXTreeUpdate update = initial_state;
  update.nodes.resize(1);
  update.nodes[0].child_ids = {};

  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("Left Right removed", DumpLiveRegionText(&event_generator));
}

TEST(AXLiveRegionTrackerTest, TextAtomic) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(5);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kRootWebArea;
  initial_state.nodes[0].child_ids = {2, 5};
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kGroup;
  initial_state.nodes[1].child_ids = {3, 4};
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[2].SetName("Top");
  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[3].SetName("Dog");
  initial_state.nodes[4].id = 5;
  initial_state.nodes[4].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[4].SetName("Before");

  SetLiveRegionAtNodeId(&initial_state, 1)->WithAriaAtomicOnNodeId(2);

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  event_generator.set_should_compute_live_region_change_description(true);
  AXTreeUpdate update = initial_state;
  update.nodes[3].SetName("Gun");

  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("Top Gun", DumpLiveRegionText(&event_generator));

  event_generator.ClearEvents();
  AXTreeUpdate update2 = update;
  update.nodes[4].SetName("After");

  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("After", DumpLiveRegionText(&event_generator));
}

}  // namespace ui
