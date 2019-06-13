// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_event_generator.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree_serializer.h"

namespace ui {

namespace {

std::string DumpEvents(AXEventGenerator* generator) {
  std::vector<std::string> event_strs;
  for (auto targeted_event : *generator) {
    const char* event_name;
    switch (targeted_event.event_params.event) {
      case AXEventGenerator::Event::ACCESS_KEY_CHANGED:
        event_name = "ACCESS_KEY_CHANGED";
        break;
      case AXEventGenerator::Event::ATOMIC_CHANGED:
        event_name = "ATOMIC_CHANGED";
        break;
      case AXEventGenerator::Event::ACTIVE_DESCENDANT_CHANGED:
        event_name = "ACTIVE_DESCENDANT_CHANGED";
        break;
      case AXEventGenerator::Event::ALERT:
        event_name = "ALERT";
        break;
      case AXEventGenerator::Event::BUSY_CHANGED:
        event_name = "BUSY_CHANGED";
        break;
      case AXEventGenerator::Event::CHECKED_STATE_CHANGED:
        event_name = "CHECKED_STATE_CHANGED";
        break;
      case AXEventGenerator::Event::CHILDREN_CHANGED:
        event_name = "CHILDREN_CHANGED";
        break;
      case AXEventGenerator::Event::CLASS_NAME_CHANGED:
        event_name = "CLASS_NAME_CHANGED";
        break;
      case AXEventGenerator::Event::COLLAPSED:
        event_name = "COLLAPSED";
        break;
      case AXEventGenerator::Event::CONTROLS_CHANGED:
        event_name = "CONTROLS_CHANGED";
        break;
      case AXEventGenerator::Event::DESCRIBED_BY_CHANGED:
        event_name = "DESCRIBED_BY_CHANGED";
        break;
      case AXEventGenerator::Event::DESCRIPTION_CHANGED:
        event_name = "DESCRIPTION_CHANGED";
        break;
      case AXEventGenerator::Event::DOCUMENT_SELECTION_CHANGED:
        event_name = "DOCUMENT_SELECTION_CHANGED";
        break;
      case AXEventGenerator::Event::DOCUMENT_TITLE_CHANGED:
        event_name = "DOCUMENT_TITLE_CHANGED";
        break;
      case AXEventGenerator::Event::DROPEFFECT_CHANGED:
        event_name = "DROPEFFECT_CHANGED";
        break;
      case AXEventGenerator::Event::ENABLED_CHANGED:
        event_name = "ENABLED_CHANGED";
        break;
      case AXEventGenerator::Event::EXPANDED:
        event_name = "EXPANDED";
        break;
      case AXEventGenerator::Event::FLOW_FROM_CHANGED:
        event_name = "FLOW_FROM_CHANGED";
        break;
      case AXEventGenerator::Event::FLOW_TO_CHANGED:
        event_name = "FLOW_TO_CHANGED";
        break;
      case AXEventGenerator::Event::GRABBED_CHANGED:
        event_name = "GRABBED_CHANGED";
        break;
      case AXEventGenerator::Event::HASPOPUP_CHANGED:
        event_name = "HASPOPUP_CHANGED";
        break;
      case AXEventGenerator::Event::HIERARCHICAL_LEVEL_CHANGED:
        event_name = "HIERARCHICAL_LEVEL_CHANGED";
        break;
      case ui::AXEventGenerator::Event::IGNORED_CHANGED:
        event_name = "IGNORED_CHANGED";
        break;
      case AXEventGenerator::Event::IMAGE_ANNOTATION_CHANGED:
        event_name = "IMAGE_ANNOTATION_CHANGED";
        break;
      case AXEventGenerator::Event::INVALID_STATUS_CHANGED:
        event_name = "INVALID_STATUS_CHANGED";
        break;
      case AXEventGenerator::Event::KEY_SHORTCUTS_CHANGED:
        event_name = "KEY_SHORTCUTS_CHANGED";
        break;
      case AXEventGenerator::Event::LABELED_BY_CHANGED:
        event_name = "LABELED_BY_CHANGED";
        break;
      case AXEventGenerator::Event::LANGUAGE_CHANGED:
        event_name = "LANGUAGE_CHANGED";
        break;
      case AXEventGenerator::Event::LAYOUT_INVALIDATED:
        event_name = "LAYOUT_INVALIDATED";
        break;
      case AXEventGenerator::Event::LIVE_REGION_CHANGED:
        event_name = "LIVE_REGION_CHANGED";
        break;
      case AXEventGenerator::Event::LIVE_REGION_CREATED:
        event_name = "LIVE_REGION_CREATED";
        break;
      case AXEventGenerator::Event::LIVE_REGION_NODE_CHANGED:
        event_name = "LIVE_REGION_NODE_CHANGED";
        break;
      case AXEventGenerator::Event::LIVE_RELEVANT_CHANGED:
        event_name = "LIVE_RELEVANT_CHANGED";
        break;
      case AXEventGenerator::Event::LIVE_STATUS_CHANGED:
        event_name = "LIVE_STATUS_CHANGED";
        break;
      case AXEventGenerator::Event::LOAD_COMPLETE:
        event_name = "LOAD_COMPLETE";
        break;
      case AXEventGenerator::Event::LOAD_START:
        event_name = "LOAD_START";
        break;
      case AXEventGenerator::Event::MENU_ITEM_SELECTED:
        event_name = "MENU_ITEM_SELECTED";
        break;
      case AXEventGenerator::Event::MULTILINE_STATE_CHANGED:
        event_name = "MULTILINE_STATE_CHANGED";
        break;
      case AXEventGenerator::Event::MULTISELECTABLE_STATE_CHANGED:
        event_name = "MULTISELECTABLE_STATE_CHANGED";
        break;
      case AXEventGenerator::Event::NAME_CHANGED:
        event_name = "NAME_CHANGED";
        break;
      case AXEventGenerator::Event::OTHER_ATTRIBUTE_CHANGED:
        event_name = "OTHER_ATTRIBUTE_CHANGED";
        break;
      case AXEventGenerator::Event::PLACEHOLDER_CHANGED:
        event_name = "PLACEHOLDER_CHANGED";
        break;
      case AXEventGenerator::Event::POSITION_IN_SET_CHANGED:
        event_name = "POSITION_IN_SET_CHANGED";
        break;
      case AXEventGenerator::Event::READONLY_CHANGED:
        event_name = "READONLY_CHANGED";
        break;
      case AXEventGenerator::Event::RELATED_NODE_CHANGED:
        event_name = "RELATED_NODE_CHANGED";
        break;
      case AXEventGenerator::Event::REQUIRED_STATE_CHANGED:
        event_name = "REQUIRED_STATE_CHANGED";
        break;
      case AXEventGenerator::Event::ROLE_CHANGED:
        event_name = "ROLE_CHANGED";
        break;
      case AXEventGenerator::Event::ROW_COUNT_CHANGED:
        event_name = "ROW_COUNT_CHANGED";
        break;
      case AXEventGenerator::Event::SCROLL_HORIZONTAL_POSITION_CHANGED:
        event_name = "SCROLL_HORIZONTAL_POSITION_CHANGED";
        break;
      case AXEventGenerator::Event::SCROLL_VERTICAL_POSITION_CHANGED:
        event_name = "SCROLL_VERTICAL_POSITION_CHANGED";
        break;
      case AXEventGenerator::Event::SELECTED_CHANGED:
        event_name = "SELECTED_CHANGED";
        break;
      case AXEventGenerator::Event::SELECTED_CHILDREN_CHANGED:
        event_name = "SELECTED_CHILDREN_CHANGED";
        break;
      case AXEventGenerator::Event::SET_SIZE_CHANGED:
        event_name = "SET_SIZE_CHANGED";
        break;
      case AXEventGenerator::Event::STATE_CHANGED:
        event_name = "STATE_CHANGED";
        break;
      case AXEventGenerator::Event::SUBTREE_CREATED:
        event_name = "SUBTREE_CREATED";
        break;
      case AXEventGenerator::Event::VALUE_CHANGED:
        event_name = "VALUE_CHANGED";
        break;
      case AXEventGenerator::Event::VALUE_MAX_CHANGED:
        event_name = "VALUE_MAX_CHANGED";
        break;
      case AXEventGenerator::Event::VALUE_MIN_CHANGED:
        event_name = "VALUE_MIN_CHANGED";
        break;
      case AXEventGenerator::Event::VALUE_STEP_CHANGED:
        event_name = "VALUE_STEP_CHANGED";
        break;
      default:
        NOTREACHED();
        event_name = "UNKNOWN";
        break;
    }
    event_strs.push_back(
        base::StringPrintf("%s on %d", event_name, targeted_event.node->id()));
  }

  // The order of events is arbitrary, so just sort the strings
  // alphabetically to make the test output predictable.
  std::sort(event_strs.begin(), event_strs.end());

  return base::JoinString(event_strs, ", ");
}

}  // namespace

TEST(AXEventGeneratorTest, LoadCompleteSameTree) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].relative_bounds.bounds = gfx::RectF(0, 0, 800, 600);
  initial_state.has_tree_data = true;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate load_complete_update = initial_state;
  load_complete_update.tree_data.loaded = true;
  ASSERT_TRUE(tree.Unserialize(load_complete_update));
  EXPECT_EQ("LOAD_COMPLETE on 1", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, LoadCompleteNewTree) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.has_tree_data = true;
  initial_state.tree_data.loaded = true;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate load_complete_update;
  load_complete_update.root_id = 2;
  load_complete_update.nodes.resize(1);
  load_complete_update.nodes[0].id = 2;
  load_complete_update.nodes[0].relative_bounds.bounds =
      gfx::RectF(0, 0, 800, 600);
  load_complete_update.has_tree_data = true;
  load_complete_update.tree_data.loaded = true;
  ASSERT_TRUE(tree.Unserialize(load_complete_update));
  EXPECT_EQ(
      "LOAD_COMPLETE on 2, "
      "SUBTREE_CREATED on 2",
      DumpEvents(&event_generator));

  // Load complete should not be emitted for sizeless roots.
  load_complete_update.root_id = 3;
  load_complete_update.nodes.resize(1);
  load_complete_update.nodes[0].id = 3;
  load_complete_update.nodes[0].relative_bounds.bounds = gfx::RectF(0, 0, 0, 0);
  load_complete_update.has_tree_data = true;
  load_complete_update.tree_data.loaded = true;
  ASSERT_TRUE(tree.Unserialize(load_complete_update));
  EXPECT_EQ("SUBTREE_CREATED on 3", DumpEvents(&event_generator));

  // TODO(accessibility): http://crbug.com/888758
  // Load complete should not be emitted for chrome-search URLs.
  load_complete_update.root_id = 4;
  load_complete_update.nodes.resize(1);
  load_complete_update.nodes[0].id = 4;
  load_complete_update.nodes[0].relative_bounds.bounds =
      gfx::RectF(0, 0, 800, 600);
  load_complete_update.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kUrl, "chrome-search://foo");
  load_complete_update.has_tree_data = true;
  load_complete_update.tree_data.loaded = true;
  ASSERT_TRUE(tree.Unserialize(load_complete_update));
  EXPECT_EQ(
      "LOAD_COMPLETE on 4, "
      "SUBTREE_CREATED on 4",
      DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, LoadStart) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].relative_bounds.bounds = gfx::RectF(0, 0, 800, 600);
  initial_state.has_tree_data = true;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate load_start_update;
  load_start_update.root_id = 2;
  load_start_update.nodes.resize(1);
  load_start_update.nodes[0].id = 2;
  load_start_update.nodes[0].relative_bounds.bounds =
      gfx::RectF(0, 0, 800, 600);
  load_start_update.has_tree_data = true;
  load_start_update.tree_data.loaded = false;
  ASSERT_TRUE(tree.Unserialize(load_start_update));
  EXPECT_EQ(
      "LOAD_START on 2, "
      "SUBTREE_CREATED on 2",
      DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, DocumentSelectionChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.has_tree_data = true;
  initial_state.tree_data.sel_focus_object_id = 1;
  initial_state.tree_data.sel_focus_offset = 1;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.tree_data.sel_focus_offset = 2;
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("DOCUMENT_SELECTION_CHANGED on 1", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, DocumentTitleChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.has_tree_data = true;
  initial_state.tree_data.title = "Before";
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.tree_data.title = "After";
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("DOCUMENT_TITLE_CHANGED on 1", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, ExpandedAndRowCount) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(4);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kRootWebArea;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(4);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kTable;
  initial_state.nodes[1].child_ids.push_back(3);
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kRow;
  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].role = ax::mojom::Role::kPopUpButton;
  initial_state.nodes[3].AddState(ax::mojom::State::kExpanded);
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[2].AddState(ax::mojom::State::kExpanded);
  update.nodes[3].state = 0;
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_EQ(
      "COLLAPSED on 4, "
      "EXPANDED on 3, "
      "ROW_COUNT_CHANGED on 2, "
      "STATE_CHANGED on 3, "
      "STATE_CHANGED on 4",
      DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, SelectedAndSelectedChildren) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(4);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kRootWebArea;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(4);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kMenu;
  initial_state.nodes[1].child_ids.push_back(3);
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kMenuItem;
  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].role = ax::mojom::Role::kListBoxOption;
  initial_state.nodes[3].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                                          true);
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[2].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes.pop_back();
  update.nodes.emplace_back();
  update.nodes[3].id = 4;
  update.nodes[3].role = ax::mojom::Role::kListBoxOption;
  update.nodes[3].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, false);
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_EQ(
      "SELECTED_CHANGED on 3, "
      "SELECTED_CHANGED on 4, "
      "SELECTED_CHILDREN_CHANGED on 2",
      DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, StringValueChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kTextField;
  initial_state.nodes[0].AddStringAttribute(ax::mojom::StringAttribute::kValue,
                                            "Before");
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].string_attributes.clear();
  update.nodes[0].AddStringAttribute(ax::mojom::StringAttribute::kValue,
                                     "After");
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("VALUE_CHANGED on 1", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, FloatValueChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kSlider;
  initial_state.nodes[0].AddFloatAttribute(
      ax::mojom::FloatAttribute::kValueForRange, 1.0);
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].float_attributes.clear();
  update.nodes[0].AddFloatAttribute(ax::mojom::FloatAttribute::kValueForRange,
                                    2.0);
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("VALUE_CHANGED on 1", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, InvalidStatusChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kTextField;
  initial_state.nodes[0].AddStringAttribute(ax::mojom::StringAttribute::kValue,
                                            "Text");
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].SetInvalidState(ax::mojom::InvalidState::kTrue);
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("INVALID_STATUS_CHANGED on 1", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, AddLiveRegionAttribute) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].AddStringAttribute(ax::mojom::StringAttribute::kLiveStatus,
                                     "polite");
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_EQ(
      "LIVE_REGION_CREATED on 1, "
      "LIVE_STATUS_CHANGED on 1",
      DumpEvents(&event_generator));

  event_generator.ClearEvents();
  update.nodes[0].AddStringAttribute(ax::mojom::StringAttribute::kLiveStatus,
                                     "assertive");
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("LIVE_STATUS_CHANGED on 1", DumpEvents(&event_generator));

  event_generator.ClearEvents();
  update.nodes[0].AddStringAttribute(ax::mojom::StringAttribute::kLiveStatus,
                                     "off");
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("LIVE_STATUS_CHANGED on 1", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, CheckedStateChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kCheckBox;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].SetCheckedState(ax::mojom::CheckedState::kTrue);
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("CHECKED_STATE_CHANGED on 1", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, ActiveDescendantChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kListBox;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(3);
  initial_state.nodes[0].AddIntAttribute(
      ax::mojom::IntAttribute::kActivedescendantId, 2);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kListBoxOption;
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kListBoxOption;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].int_attributes.clear();
  update.nodes[0].AddIntAttribute(ax::mojom::IntAttribute::kActivedescendantId,
                                  3);
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_EQ(
      "ACTIVE_DESCENDANT_CHANGED on 1, "
      "RELATED_NODE_CHANGED on 1",
      DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, CreateAlertAndLiveRegion) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes.resize(3);
  update.nodes[0].child_ids.push_back(2);
  update.nodes[0].child_ids.push_back(3);
  update.nodes[1].id = 2;
  update.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kLiveStatus,
                                     "polite");
  update.nodes[2].id = 3;
  update.nodes[2].role = ax::mojom::Role::kAlert;
  update.nodes[2].AddStringAttribute(ax::mojom::StringAttribute::kLiveStatus,
                                     "polite");

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_EQ(
      "ALERT on 3, "
      "CHILDREN_CHANGED on 1, "
      "LIVE_REGION_CREATED on 2, "
      "SUBTREE_CREATED on 2, "
      "SUBTREE_CREATED on 3",
      DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, LiveRegionChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kLiveStatus, "polite");
  initial_state.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(3);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                            "Before 1");
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[2].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[2].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                            "Before 2");
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[1].string_attributes.clear();
  update.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  update.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                     "After 1");
  update.nodes[2].string_attributes.clear();
  update.nodes[2].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  update.nodes[2].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                     "After 2");

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_EQ(
      "LIVE_REGION_CHANGED on 1, "
      "LIVE_REGION_NODE_CHANGED on 2, "
      "LIVE_REGION_NODE_CHANGED on 3, "
      "NAME_CHANGED on 2, "
      "NAME_CHANGED on 3",
      DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, LiveRegionOnlyTextChanges) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kLiveStatus, "polite");
  initial_state.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(3);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                            "Before 1");
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[2].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[2].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                            "Before 2");
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kDescription,
                                     "Description 1");
  update.nodes[2].SetCheckedState(ax::mojom::CheckedState::kTrue);

  // Note that we do NOT expect a LIVE_REGION_CHANGED event here, because
  // the name did not change.
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_EQ(
      "CHECKED_STATE_CHANGED on 3, "
      "DESCRIPTION_CHANGED on 2",
      DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, BusyLiveRegionChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kLiveStatus, "polite");
  initial_state.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[0].AddBoolAttribute(ax::mojom::BoolAttribute::kBusy,
                                          true);
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(3);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                            "Before 1");
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[2].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[2].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                            "Before 2");
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[1].string_attributes.clear();
  update.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  update.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                     "After 1");
  update.nodes[2].string_attributes.clear();
  update.nodes[2].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  update.nodes[2].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                     "After 2");

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_EQ(
      "NAME_CHANGED on 2, "
      "NAME_CHANGED on 3",
      DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, AddChild) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(2);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes.resize(3);
  update.nodes[0].child_ids.push_back(3);
  update.nodes[2].id = 3;

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_EQ(
      "CHILDREN_CHANGED on 1, "
      "SUBTREE_CREATED on 3",
      DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, RemoveChild) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(3);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[2].id = 3;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes.resize(2);
  update.nodes[0].child_ids.clear();
  update.nodes[0].child_ids.push_back(2);

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("CHILDREN_CHANGED on 1", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, ReorderChildren) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(3);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[2].id = 3;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].child_ids.clear();
  update.nodes[0].child_ids.push_back(3);
  update.nodes[0].child_ids.push_back(2);

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("CHILDREN_CHANGED on 1", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, ScrollHorizontalPositionChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].AddIntAttribute(ax::mojom::IntAttribute::kScrollX, 10);
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("SCROLL_HORIZONTAL_POSITION_CHANGED on 1",
            DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, ScrollVerticalPositionChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].AddIntAttribute(ax::mojom::IntAttribute::kScrollY, 10);
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("SCROLL_VERTICAL_POSITION_CHANGED on 1",
            DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, OtherAttributeChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(6);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(3);
  initial_state.nodes[0].child_ids.push_back(4);
  initial_state.nodes[0].child_ids.push_back(5);
  initial_state.nodes[0].child_ids.push_back(6);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[2].id = 3;
  initial_state.nodes[3].id = 4;
  initial_state.nodes[4].id = 5;
  initial_state.nodes[5].id = 6;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kLanguage,
                                     "de");
  update.nodes[2].AddIntAttribute(ax::mojom::IntAttribute::kAriaCellColumnIndex,
                                  7);
  update.nodes[3].AddFloatAttribute(ax::mojom::FloatAttribute::kFontSize,
                                    12.0f);
  update.nodes[4].AddBoolAttribute(ax::mojom::BoolAttribute::kModal, true);
  std::vector<int> ids = {2};
  update.nodes[5].AddIntListAttribute(ax::mojom::IntListAttribute::kControlsIds,
                                      ids);
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_EQ(
      "CONTROLS_CHANGED on 6, "
      "LANGUAGE_CHANGED on 2, "
      "OTHER_ATTRIBUTE_CHANGED on 3, "
      "OTHER_ATTRIBUTE_CHANGED on 4, "
      "OTHER_ATTRIBUTE_CHANGED on 5, "
      "RELATED_NODE_CHANGED on 6",
      DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, NameChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(2);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                     "Hello");
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("NAME_CHANGED on 2", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, DescriptionChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].AddStringAttribute(ax::mojom::StringAttribute::kDescription,
                                     "Hello");
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("DESCRIPTION_CHANGED on 1", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, RoleChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].role = ax::mojom::Role::kCheckBox;
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("ROLE_CHANGED on 1", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, MenuItemSelected) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kMenu;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(3);
  initial_state.nodes[0].AddIntAttribute(
      ax::mojom::IntAttribute::kActivedescendantId, 2);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kMenuListOption;
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kMenuListOption;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].int_attributes.clear();
  update.nodes[0].AddIntAttribute(ax::mojom::IntAttribute::kActivedescendantId,
                                  3);
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_EQ(
      "ACTIVE_DESCENDANT_CHANGED on 1, "
      "MENU_ITEM_SELECTED on 3, "
      "RELATED_NODE_CHANGED on 1",
      DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, NodeBecomesIgnored) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(5);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kRootWebArea;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kArticle;
  initial_state.nodes[1].child_ids.push_back(3);
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kGroup;
  initial_state.nodes[2].AddState(ax::mojom::State::kIgnored);
  initial_state.nodes[2].child_ids.push_back(4);
  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].role = ax::mojom::Role::kGroup;
  initial_state.nodes[3].child_ids.push_back(5);
  initial_state.nodes[4].id = 5;
  initial_state.nodes[4].role = ax::mojom::Role::kStaticText;

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[3].AddState(ax::mojom::State::kIgnored);
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_EQ(
      "CHILDREN_CHANGED on 2, "
      "IGNORED_CHANGED on 4, "
      "STATE_CHANGED on 4",
      DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, NodeBecomesUnignored) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(5);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kRootWebArea;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kArticle;
  initial_state.nodes[1].child_ids.push_back(3);
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kGroup;
  initial_state.nodes[2].AddState(ax::mojom::State::kIgnored);
  initial_state.nodes[2].child_ids.push_back(4);
  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].role = ax::mojom::Role::kGroup;
  initial_state.nodes[3].AddState(ax::mojom::State::kIgnored);
  initial_state.nodes[3].child_ids.push_back(5);
  initial_state.nodes[4].id = 5;
  initial_state.nodes[4].role = ax::mojom::Role::kStaticText;

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[3].state = 0;
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_EQ(
      "CHILDREN_CHANGED on 2, "
      "IGNORED_CHANGED on 4, "
      "STATE_CHANGED on 4",
      DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, ActiveDescendantChangeOnDescendant) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(5);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kRootWebArea;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kGenericContainer;
  initial_state.nodes[1].child_ids.push_back(3);
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kGroup;
  initial_state.nodes[2].AddIntAttribute(
      ax::mojom::IntAttribute::kActivedescendantId, 4);
  initial_state.nodes[2].child_ids.push_back(4);
  initial_state.nodes[2].child_ids.push_back(5);
  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].role = ax::mojom::Role::kGroup;
  initial_state.nodes[4].id = 5;
  initial_state.nodes[4].role = ax::mojom::Role::kGroup;

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  initial_state.nodes[2].RemoveIntAttribute(
      ax::mojom::IntAttribute::kActivedescendantId);
  initial_state.nodes[2].AddIntAttribute(
      ax::mojom::IntAttribute::kActivedescendantId, 5);
  AXTreeUpdate update = initial_state;
  update.node_id_to_clear = 2;
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_EQ(
      "ACTIVE_DESCENDANT_CHANGED on 3, "
      "RELATED_NODE_CHANGED on 3",
      DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, ImageAnnotationChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kImageAnnotation, "Hello");
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("IMAGE_ANNOTATION_CHANGED on 1", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, ImageAnnotationStatusChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded);

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("IMAGE_ANNOTATION_CHANGED on 1", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, StringPropertyChanges) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;

  struct {
    ax::mojom::StringAttribute id;
    std::string old_value;
    std::string new_value;
  } attributes[] = {
      {ax::mojom::StringAttribute::kAccessKey, "a", "b"},
      {ax::mojom::StringAttribute::kClassName, "a", "b"},
      {ax::mojom::StringAttribute::kKeyShortcuts, "a", "b"},
      {ax::mojom::StringAttribute::kLanguage, "a", "b"},
      {ax::mojom::StringAttribute::kPlaceholder, "a", "b"},
  };
  for (auto&& attrib : attributes) {
    initial_state.nodes.push_back({});
    initial_state.nodes.back().id = initial_state.nodes.size();
    initial_state.nodes.back().AddStringAttribute(attrib.id, attrib.old_value);
    initial_state.nodes[0].child_ids.push_back(initial_state.nodes.size());
  }

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  int index = 1;
  for (auto&& attrib : attributes) {
    initial_state.nodes[index++].AddStringAttribute(attrib.id,
                                                    attrib.new_value);
  }

  AXTreeUpdate update = initial_state;
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ(
      "ACCESS_KEY_CHANGED on 2, "
      "CLASS_NAME_CHANGED on 3, "
      "KEY_SHORTCUTS_CHANGED on 4, "
      "LANGUAGE_CHANGED on 5, "
      "PLACEHOLDER_CHANGED on 6",
      DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, IntPropertyChanges) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;

  struct {
    ax::mojom::IntAttribute id;
    int old_value;
    int new_value;
  } attributes[] = {
      {ax::mojom::IntAttribute::kHierarchicalLevel, 1, 2},
      {ax::mojom::IntAttribute::kPosInSet, 1, 2},
      {ax::mojom::IntAttribute::kSetSize, 1, 2},
  };
  for (auto&& attrib : attributes) {
    initial_state.nodes.push_back({});
    initial_state.nodes.back().id = initial_state.nodes.size();
    initial_state.nodes.back().AddIntAttribute(attrib.id, attrib.old_value);
    initial_state.nodes[0].child_ids.push_back(initial_state.nodes.size());
  }

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  int index = 1;
  for (auto&& attrib : attributes)
    initial_state.nodes[index++].AddIntAttribute(attrib.id, attrib.new_value);

  AXTreeUpdate update = initial_state;
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ(
      "HIERARCHICAL_LEVEL_CHANGED on 2, "
      "POSITION_IN_SET_CHANGED on 3, "
      "SET_SIZE_CHANGED on 4",
      DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, IntListPropertyChanges) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;

  struct {
    ax::mojom::IntListAttribute id;
    std::vector<int> old_value;
    std::vector<int> new_value;
  } attributes[] = {
      {ax::mojom::IntListAttribute::kDescribedbyIds, {1}, {2}},
      {ax::mojom::IntListAttribute::kFlowtoIds, {1}, {2}},
      {ax::mojom::IntListAttribute::kLabelledbyIds, {1}, {2}},
  };
  for (auto&& attrib : attributes) {
    initial_state.nodes.push_back({});
    initial_state.nodes.back().id = initial_state.nodes.size();
    initial_state.nodes.back().AddIntListAttribute(attrib.id, attrib.old_value);
    initial_state.nodes[0].child_ids.push_back(initial_state.nodes.size());
  }

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  int index = 1;
  for (auto&& attrib : attributes) {
    initial_state.nodes[index++].AddIntListAttribute(attrib.id,
                                                     attrib.new_value);
  }

  AXTreeUpdate update = initial_state;
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ(
      "DESCRIBED_BY_CHANGED on 2, "
      "FLOW_FROM_CHANGED on 1, "
      "FLOW_FROM_CHANGED on 2, "
      "FLOW_TO_CHANGED on 3, "
      "LABELED_BY_CHANGED on 4, "
      "RELATED_NODE_CHANGED on 2, "
      "RELATED_NODE_CHANGED on 3, "
      "RELATED_NODE_CHANGED on 4",
      DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, AriaBusyChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  AXTree tree(initial_state);
  initial_state.nodes[0].AddBoolAttribute(ax::mojom::BoolAttribute::kBusy,
                                          true);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].AddBoolAttribute(ax::mojom::BoolAttribute::kBusy, false);

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_EQ(
      "BUSY_CHANGED on 1, "
      "LAYOUT_INVALIDATED on 1",
      DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, MultiselectableStateChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kGrid;

  AXTree tree(initial_state);
  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;

  update.nodes[0].AddState(ax::mojom::State::kMultiselectable);
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("MULTISELECTABLE_STATE_CHANGED on 1, STATE_CHANGED on 1",
            DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, RequiredStateChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kTextField;

  AXTree tree(initial_state);
  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;

  update.nodes[0].AddState(ax::mojom::State::kRequired);
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("REQUIRED_STATE_CHANGED on 1, STATE_CHANGED on 1",
            DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, FlowToChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(6);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kGenericContainer;
  initial_state.nodes[0].child_ids.assign({2, 3, 4, 5, 6});
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kGenericContainer;
  initial_state.nodes[1].AddIntListAttribute(
      ax::mojom::IntListAttribute::kFlowtoIds, {3, 4});
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kGenericContainer;
  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].role = ax::mojom::Role::kGenericContainer;
  initial_state.nodes[4].id = 5;
  initial_state.nodes[4].role = ax::mojom::Role::kGenericContainer;
  initial_state.nodes[5].id = 6;
  initial_state.nodes[5].role = ax::mojom::Role::kGenericContainer;

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[1].AddIntListAttribute(ax::mojom::IntListAttribute::kFlowtoIds,
                                      {4, 5, 6});

  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ(
      "FLOW_FROM_CHANGED on 3, "
      "FLOW_FROM_CHANGED on 5, "
      "FLOW_FROM_CHANGED on 6, "
      "FLOW_TO_CHANGED on 2, "
      "RELATED_NODE_CHANGED on 2",
      DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, ControlsChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(2);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;

  AXTree tree(initial_state);
  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;

  std::vector<int> ids = {2};
  update.nodes[0].AddIntListAttribute(ax::mojom::IntListAttribute::kControlsIds,
                                      ids);
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ(
      "CONTROLS_CHANGED on 1, "
      "RELATED_NODE_CHANGED on 1",
      DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, AtomicChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;

  AXTree tree(initial_state);
  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;

  update.nodes[0].AddBoolAttribute(ax::mojom::BoolAttribute::kLiveAtomic, true);
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("ATOMIC_CHANGED on 1", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, DropeffectChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;

  AXTree tree(initial_state);
  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;

  update.nodes[0].AddDropeffect(ax::mojom::Dropeffect::kCopy);
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("DROPEFFECT_CHANGED on 1", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, GrabbedChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;

  AXTree tree(initial_state);
  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;

  update.nodes[0].AddBoolAttribute(ax::mojom::BoolAttribute::kGrabbed, true);
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("GRABBED_CHANGED on 1", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, HasPopupChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;

  AXTree tree(initial_state);
  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;

  update.nodes[0].SetHasPopup(ax::mojom::HasPopup::kTrue);
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("HASPOPUP_CHANGED on 1", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, LiveRelevantChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;

  AXTree tree(initial_state);
  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;

  update.nodes[0].AddStringAttribute(ax::mojom::StringAttribute::kLiveRelevant,
                                     "all");
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("LIVE_RELEVANT_CHANGED on 1", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, MultilineStateChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;

  AXTree tree(initial_state);
  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;

  update.nodes[0].AddState(ax::mojom::State::kMultiline);
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ(
      "MULTILINE_STATE_CHANGED on 1, "
      "STATE_CHANGED on 1",
      DumpEvents(&event_generator));
}

}  // namespace ui
