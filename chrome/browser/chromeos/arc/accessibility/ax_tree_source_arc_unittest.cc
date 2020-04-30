// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/accessibility/ax_tree_source_arc.h"

#include "base/stl_util.h"
#include "chrome/browser/chromeos/arc/accessibility/accessibility_node_info_data_wrapper.h"
#include "chrome/browser/chromeos/arc/accessibility/accessibility_window_info_data_wrapper.h"
#include "components/arc/mojom/accessibility_helper.mojom.h"
#include "extensions/browser/api/automation_internal/automation_event_router.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/platform/ax_android_constants.h"

namespace arc {

using AXActionType = mojom::AccessibilityActionType;
using AXBooleanProperty = mojom::AccessibilityBooleanProperty;
using AXCollectionInfoData = mojom::AccessibilityCollectionInfoData;
using AXCollectionItemInfoData = mojom::AccessibilityCollectionItemInfoData;
using AXEventData = mojom::AccessibilityEventData;
using AXEventIntProperty = mojom::AccessibilityEventIntProperty;
using AXEventType = mojom::AccessibilityEventType;
using AXIntListProperty = mojom::AccessibilityIntListProperty;
using AXIntProperty = mojom::AccessibilityIntProperty;
using AXNodeInfoData = mojom::AccessibilityNodeInfoData;
using AXRangeInfoData = mojom::AccessibilityRangeInfoData;
using AXStringListProperty = mojom::AccessibilityStringListProperty;
using AXStringProperty = mojom::AccessibilityStringProperty;
using AXWindowInfoData = mojom::AccessibilityWindowInfoData;
using AXWindowIntListProperty = mojom::AccessibilityWindowIntListProperty;
using AXWindowStringProperty = mojom::AccessibilityWindowStringProperty;

void SetProperty(AXNodeInfoData* node, AXBooleanProperty prop, bool value) {
  if (!node->boolean_properties) {
    node->boolean_properties = base::flat_map<AXBooleanProperty, bool>();
  }
  auto& prop_map = node->boolean_properties.value();
  base::EraseIf(prop_map, [prop](auto it) { return it.first == prop; });
  prop_map.insert(std::make_pair(prop, value));
}

void SetProperty(AXNodeInfoData* node,
                 AXStringProperty prop,
                 const std::string& value) {
  if (!node->string_properties) {
    node->string_properties = base::flat_map<AXStringProperty, std::string>();
  }
  auto& prop_map = node->string_properties.value();
  base::EraseIf(prop_map, [prop](auto it) { return it.first == prop; });
  prop_map.insert(std::make_pair(prop, value));
}

void SetProperty(AXNodeInfoData* node, AXIntProperty prop, int32_t value) {
  if (!node->int_properties) {
    node->int_properties = base::flat_map<AXIntProperty, int>();
  }
  auto& prop_map = node->int_properties.value();
  base::EraseIf(prop_map, [prop](auto it) { return it.first == prop; });
  prop_map.insert(std::make_pair(prop, value));
}

void SetProperty(AXWindowInfoData* window,
                 AXWindowStringProperty prop,
                 const std::string& value) {
  if (!window->string_properties) {
    window->string_properties =
        base::flat_map<AXWindowStringProperty, std::string>();
  }
  auto& prop_map = window->string_properties.value();
  base::EraseIf(prop_map, [prop](auto it) { return it.first == prop; });
  prop_map.insert(std::make_pair(prop, value));
}

void SetProperty(AXNodeInfoData* node,
                 AXIntListProperty prop,
                 const std::vector<int>& value) {
  if (!node->int_list_properties) {
    node->int_list_properties =
        base::flat_map<AXIntListProperty, std::vector<int>>();
  }
  auto& prop_map = node->int_list_properties.value();
  base::EraseIf(prop_map, [prop](auto it) { return it.first == prop; });
  prop_map.insert(std::make_pair(prop, value));
}

void SetProperty(AXWindowInfoData* window,
                 AXWindowIntListProperty prop,
                 const std::vector<int>& value) {
  if (!window->int_list_properties) {
    window->int_list_properties =
        base::flat_map<AXWindowIntListProperty, std::vector<int>>();
  }
  auto& prop_map = window->int_list_properties.value();
  base::EraseIf(prop_map, [prop](auto it) { return it.first == prop; });
  prop_map.insert(std::make_pair(prop, value));
}

void SetProperty(AXEventData* event, AXEventIntProperty prop, int32_t value) {
  if (!event->int_properties) {
    event->int_properties = base::flat_map<AXEventIntProperty, int>();
  }
  auto& prop_map = event->int_properties.value();
  base::EraseIf(prop_map, [prop](auto it) { return it.first == prop; });
  prop_map.insert(std::make_pair(prop, value));
}

class MockAutomationEventRouter
    : public extensions::AutomationEventRouterInterface {
 public:
  MockAutomationEventRouter() {}
  ~MockAutomationEventRouter() override = default;

  ui::AXTree* tree() { return &tree_; }

  void DispatchAccessibilityEvents(
      const ExtensionMsg_AccessibilityEventBundleParams& events) override {
    for (auto&& event : events.events)
      event_count_[event.event_type]++;

    for (const auto& update : events.updates)
      tree_.Unserialize(update);
  }

  void DispatchAccessibilityLocationChange(
      const ExtensionMsg_AccessibilityLocationChangeParams& params) override {}

  void DispatchTreeDestroyedEvent(
      ui::AXTreeID tree_id,
      content::BrowserContext* browser_context) override {}

  void DispatchActionResult(
      const ui::AXActionData& data,
      bool result,
      content::BrowserContext* browser_context = nullptr) override {}

  void DispatchGetTextLocationDataResult(
      const ui::AXActionData& data,
      const base::Optional<gfx::Rect>& rect) override {}

  std::map<ax::mojom::Event, int> event_count_;
  ui::AXTree tree_;
};

class AXTreeSourceArcTest : public testing::Test,
                            public AXTreeSourceArc::Delegate {
 public:
  class TestAXTreeSourceArc : public AXTreeSourceArc {
   public:
    TestAXTreeSourceArc(AXTreeSourceArc::Delegate* delegate,
                        MockAutomationEventRouter* router)
        : AXTreeSourceArc(delegate, 1.0), router_(router) {}

   private:
    extensions::AutomationEventRouterInterface* GetAutomationEventRouter()
        const override {
      return router_;
    }

    MockAutomationEventRouter* const router_;
  };

  AXTreeSourceArcTest()
      : router_(new MockAutomationEventRouter()),
        tree_source_(new TestAXTreeSourceArc(this, router_.get())) {}

 protected:
  void CallNotifyAccessibilityEvent(AXEventData* event_data) {
    tree_source_->NotifyAccessibilityEvent(event_data);
  }

  void GetChildren(AXNodeInfoData* node,
                   std::vector<ui::AXNode*>& out_children) {
    ui::AXNode* ax_node = tree()->GetFromId(node->id);
    ASSERT_TRUE(ax_node);
    out_children = ax_node->children();
  }

  void GetSerializedNode(AXNodeInfoData* node, ui::AXNodeData& out_data) {
    ui::AXNode* ax_node = tree()->GetFromId(node->id);
    ASSERT_TRUE(ax_node);
    out_data = ax_node->data();
  }

  void GetSerializedWindow(AXWindowInfoData* window, ui::AXNodeData& out_data) {
    ui::AXNode* ax_node = tree()->GetFromId(window->window_id);
    ASSERT_TRUE(ax_node);
    out_data = ax_node->data();
  }

  bool CallGetTreeData(ui::AXTreeData* data) {
    return tree_source_->GetTreeData(data);
  }

  MockAutomationEventRouter* GetRouter() const { return router_.get(); }

  int GetDispatchedEventCount(ax::mojom::Event type) {
    return router_->event_count_[type];
  }

  ui::AXTree* tree() { return router_->tree(); }

  void ExpectTree(const std::string& expected) {
    const std::string& tree_text = tree()->ToString();
    size_t first_new_line = tree_text.find("\n");
    ASSERT_NE(std::string::npos, first_new_line);
    ASSERT_GT(tree_text.size(), ++first_new_line);

    // Omit the first line, which contains an unguessable ax tree id.
    EXPECT_EQ(expected, tree_text.substr(first_new_line));
  }

 private:
  void OnAction(const ui::AXActionData& data) const override {}

  const std::unique_ptr<MockAutomationEventRouter> router_;
  const std::unique_ptr<AXTreeSourceArc> tree_source_;

  DISALLOW_COPY_AND_ASSIGN(AXTreeSourceArcTest);
};

TEST_F(AXTreeSourceArcTest, ReorderChildrenByLayout) {
  auto event = AXEventData::New();
  event->source_id = 0;
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;

  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* root_window = event->window_data->back().get();
  root_window->window_id = 100;
  root_window->root_node_id = 10;

  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* root = event->node_data.back().get();
  root->id = 10;
  SetProperty(root, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(root, AXIntListProperty::CHILD_NODE_IDS,
              std::vector<int>({1, 2}));

  // Add child button.
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* button1 = event->node_data.back().get();
  button1->id = 1;
  SetProperty(button1, AXStringProperty::CLASS_NAME, ui::kAXButtonClassname);
  SetProperty(button1, AXBooleanProperty::VISIBLE_TO_USER, true);
  SetProperty(button1, AXBooleanProperty::FOCUSABLE, true);
  SetProperty(button1, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(button1, AXStringProperty::CONTENT_DESCRIPTION, "button1");

  // Add another child button.
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* button2 = event->node_data.back().get();
  button2->id = 2;
  SetProperty(button2, AXStringProperty::CLASS_NAME, ui::kAXButtonClassname);
  SetProperty(button2, AXBooleanProperty::VISIBLE_TO_USER, true);
  SetProperty(button2, AXBooleanProperty::FOCUSABLE, true);
  SetProperty(button2, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(button2, AXStringProperty::CONTENT_DESCRIPTION, "button2");

  // Non-overlapping, bottom to top.
  button1->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  button2->bounds_in_screen = gfx::Rect(0, 0, 50, 50);

  // Trigger an update which refreshes the computed bounds used for reordering.
  CallNotifyAccessibilityEvent(event.get());
  std::vector<ui::AXNode*> top_to_bottom;
  GetChildren(root, top_to_bottom);
  ASSERT_EQ(2U, top_to_bottom.size());
  EXPECT_EQ(2, top_to_bottom[0]->id());
  EXPECT_EQ(1, top_to_bottom[1]->id());

  // Non-overlapping, top to bottom.
  button1->bounds_in_screen = gfx::Rect(0, 0, 50, 50);
  button2->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  CallNotifyAccessibilityEvent(event.get());
  top_to_bottom.clear();
  GetChildren(event->node_data[0].get(), top_to_bottom);
  ASSERT_EQ(2U, top_to_bottom.size());
  EXPECT_EQ(1, top_to_bottom[0]->id());
  EXPECT_EQ(2, top_to_bottom[1]->id());

  // Overlapping; right to left.
  button1->bounds_in_screen = gfx::Rect(101, 100, 99, 100);
  button2->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  CallNotifyAccessibilityEvent(event.get());
  std::vector<ui::AXNode*> left_to_right;
  GetChildren(root, left_to_right);
  ASSERT_EQ(2U, left_to_right.size());
  EXPECT_EQ(2, left_to_right[0]->id());
  EXPECT_EQ(1, left_to_right[1]->id());

  // Overlapping; left to right.
  button1->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  button2->bounds_in_screen = gfx::Rect(101, 100, 99, 100);
  CallNotifyAccessibilityEvent(event.get());
  left_to_right.clear();
  GetChildren(event->node_data[0].get(), left_to_right);
  ASSERT_EQ(2U, left_to_right.size());
  EXPECT_EQ(1, left_to_right[0]->id());
  EXPECT_EQ(2, left_to_right[1]->id());

  // Overlapping, bottom to top.
  button1->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  button2->bounds_in_screen = gfx::Rect(100, 99, 100, 100);
  CallNotifyAccessibilityEvent(event.get());
  top_to_bottom.clear();
  GetChildren(event->node_data[0].get(), top_to_bottom);
  ASSERT_EQ(2U, top_to_bottom.size());
  EXPECT_EQ(2, top_to_bottom[0]->id());
  EXPECT_EQ(1, top_to_bottom[1]->id());

  // Overlapping, top to bottom.
  button1->bounds_in_screen = gfx::Rect(100, 99, 100, 100);
  button2->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  CallNotifyAccessibilityEvent(event.get());
  top_to_bottom.clear();
  GetChildren(event->node_data[0].get(), top_to_bottom);
  ASSERT_EQ(2U, top_to_bottom.size());
  EXPECT_EQ(1, top_to_bottom[0]->id());
  EXPECT_EQ(2, top_to_bottom[1]->id());

  // Identical. smaller to larger.
  button1->bounds_in_screen = gfx::Rect(100, 100, 100, 10);
  button2->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  CallNotifyAccessibilityEvent(event.get());
  std::vector<ui::AXNode*> dimension;
  GetChildren(event->node_data[0].get(), dimension);
  ASSERT_EQ(2U, dimension.size());
  EXPECT_EQ(2, dimension[0]->id());
  EXPECT_EQ(1, dimension[1]->id());

  button1->bounds_in_screen = gfx::Rect(100, 100, 10, 100);
  button2->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  CallNotifyAccessibilityEvent(event.get());
  dimension.clear();
  GetChildren(event->node_data[0].get(), dimension);
  ASSERT_EQ(2U, dimension.size());
  EXPECT_EQ(2, dimension[0]->id());
  EXPECT_EQ(1, dimension[1]->id());

  // Identical. Larger to smaller.
  button1->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  button2->bounds_in_screen = gfx::Rect(100, 100, 100, 10);
  CallNotifyAccessibilityEvent(event.get());
  dimension.clear();
  GetChildren(event->node_data[0].get(), dimension);
  ASSERT_EQ(2U, dimension.size());
  EXPECT_EQ(1, dimension[0]->id());
  EXPECT_EQ(2, dimension[1]->id());

  button1->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  button2->bounds_in_screen = gfx::Rect(100, 100, 10, 100);
  CallNotifyAccessibilityEvent(event.get());
  dimension.clear();
  GetChildren(event->node_data[0].get(), dimension);
  ASSERT_EQ(2U, dimension.size());
  EXPECT_EQ(1, dimension[0]->id());
  EXPECT_EQ(2, dimension[1]->id());

  EXPECT_EQ(10, GetDispatchedEventCount(ax::mojom::Event::kFocus));

  // Sanity check tree output.
  ExpectTree(
      "id=100 window FOCUSABLE (0, 0)-(0, 0) modal=true child_ids=10\n"
      "  id=10 genericContainer INVISIBLE (0, 0)-(0, 0) restriction=disabled "
      "child_ids=1,2\n"
      "    id=1 button FOCUSABLE (100, 100)-(100, 100) restriction=disabled "
      "class_name=android.widget.Button name=button1\n"
      "    id=2 button FOCUSABLE (100, 100)-(10, 100) restriction=disabled "
      "class_name=android.widget.Button name=button2\n");
}

TEST_F(AXTreeSourceArcTest, AccessibleNameComputation) {
  auto event = AXEventData::New();
  event->source_id = 0;
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;

  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* root_window = event->window_data->back().get();
  root_window->window_id = 100;
  root_window->root_node_id = 10;

  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* root = event->node_data.back().get();
  root->id = 10;
  SetProperty(root, AXStringProperty::CLASS_NAME, "");
  SetProperty(root, AXIntListProperty::CHILD_NODE_IDS,
              std::vector<int>({1, 2}));

  // Add child node.
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* child1 = event->node_data.back().get();
  child1->id = 1;

  // Add another child.
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* child2 = event->node_data.back().get();
  child2->id = 2;

  // Populate the tree source with the data.
  CallNotifyAccessibilityEvent(event.get());

  // No attributes.
  ui::AXNodeData data;
  GetSerializedNode(root, data);
  std::string name;
  ASSERT_FALSE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));

  // Text (empty).
  SetProperty(root, AXStringProperty::TEXT, "");

  CallNotifyAccessibilityEvent(event.get());
  GetSerializedNode(root, data);
  // With crrev/1786363, empty text on node will not set the name.
  ASSERT_FALSE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));

  // Text (non-empty).
  SetProperty(root, AXStringProperty::TEXT, "label text");

  CallNotifyAccessibilityEvent(event.get());
  GetSerializedNode(root, data);
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("label text", name);

  // Content description (empty), text (non-empty).
  SetProperty(root, AXStringProperty::CONTENT_DESCRIPTION, "");

  CallNotifyAccessibilityEvent(event.get());
  GetSerializedNode(root, data);
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("label text", name);

  // Content description (non-empty), text (empty).
  SetProperty(root, AXStringProperty::TEXT, "");
  SetProperty(root, AXStringProperty::CONTENT_DESCRIPTION,
              "label content description");

  CallNotifyAccessibilityEvent(event.get());
  GetSerializedNode(root, data);
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("label content description", name);

  // Content description (non-empty), text (non-empty).
  SetProperty(root, AXStringProperty::TEXT, "label text");

  CallNotifyAccessibilityEvent(event.get());
  GetSerializedNode(root, data);
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("label content description label text", name);

  // Name from contents.

  // Root node has no name, but has descendants with name.
  root->string_properties->clear();
  // Name from contents only happens if a node is clickable.
  SetProperty(root, AXBooleanProperty::CLICKABLE, true);
  SetProperty(child1, AXStringProperty::TEXT, "child1 label text");
  SetProperty(child2, AXStringProperty::TEXT, "child2 label text");

  CallNotifyAccessibilityEvent(event.get());
  GetSerializedNode(root, data);
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  ASSERT_EQ("child1 label text child2 label text", name);

  // If a child is also clickable, do not use child property.
  SetProperty(child1, AXBooleanProperty::CLICKABLE, true);
  SetProperty(child2, AXBooleanProperty::CLICKABLE, true);

  CallNotifyAccessibilityEvent(event.get());
  GetSerializedNode(root, data);
  ASSERT_FALSE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));

  // If the node has a name, it should override the contents.
  child1->boolean_properties->clear();
  child2->boolean_properties->clear();
  SetProperty(root, AXStringProperty::TEXT, "root label text");

  CallNotifyAccessibilityEvent(event.get());
  GetSerializedNode(root, data);
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  ASSERT_EQ("root label text", name);

  // The placeholder text on the node, should also be appended to the name.
  SetProperty(child2, AXStringProperty::HINT_TEXT, "child2 hint text");

  CallNotifyAccessibilityEvent(event.get());
  GetSerializedNode(child2, data);
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  ASSERT_EQ("child2 label text child2 hint text", name);

  // Clearing both clickable and name from root, the name should not be
  // populated.
  root->boolean_properties->clear();
  root->string_properties->clear();
  CallNotifyAccessibilityEvent(event.get());
  GetSerializedNode(root, data);
  ASSERT_FALSE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
}

TEST_F(AXTreeSourceArcTest, AccessibleNameComputationTextField) {
  auto event = AXEventData::New();
  event->source_id = 1;
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* root = event->node_data.back().get();
  root->id = 1;

  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* root_window = event->window_data->back().get();
  root_window->window_id = 100;
  root_window->root_node_id = 1;

  ui::AXNodeData data;
  SetProperty(root, AXBooleanProperty::EDITABLE, true);

  struct AndroidState {
    std::string content_description, text, hint_text;
    bool showingHint = false;
  };
  struct ChromeState {
    std::string name, value;
  };

  std::vector<std::pair<AndroidState, ChromeState>> test_cases = {
      {
          {"email", "editing_text", "", false},
          {"email", "editing_text"},
      },
      {
          {"email", "", "", false},
          {"email", ""},
      },
      {
          {"", "editing_text", "", false},
          {"", "editing_text"},
      },
      {
          // User input and hint text.
          {"", "editing_text", "hint@example.com", false},
          {"hint@example.com", "editing_text"},
      },
      {
          // No user input. Hint text is non-empty.
          {"", "hint@example.com", "hint@example.com", true},
          {"hint@example.com", ""},
      },
      {
          // User input is the same as hint text.
          {"", "example@example.com", "example@example.com", false},
          {"example@example.com", "example@example.com"},
      },
      {
          // No user input. Content description and hint tex are non-empty.
          {"email", "hint@example.com", "hint@example.com", true},
          {"email hint@example.com", ""},
      },
      {
          {"email", "editing_text", "hint@example.com", false},
          {"email hint@example.com", "editing_text"},
      },
      {
          {"", "", "", false},
          {"", ""},
      },
  };

  for (const auto& test_case : test_cases) {
    SetProperty(root, AXStringProperty::CONTENT_DESCRIPTION,
                test_case.first.content_description);
    SetProperty(root, AXStringProperty::TEXT, test_case.first.text);
    SetProperty(root, AXStringProperty::HINT_TEXT, test_case.first.hint_text);
    SetProperty(root, AXBooleanProperty::SHOWING_HINT_TEXT,
                test_case.first.showingHint);

    CallNotifyAccessibilityEvent(event.get());
    GetSerializedNode(root, data);

    std::string prop;
    ASSERT_EQ(
        !test_case.second.name.empty(),
        data.GetStringAttribute(ax::mojom::StringAttribute::kName, &prop));
    if (!test_case.second.name.empty())
      EXPECT_EQ(test_case.second.name, prop);

    ASSERT_EQ(
        !test_case.second.value.empty(),
        data.GetStringAttribute(ax::mojom::StringAttribute::kValue, &prop));
    if (!test_case.second.value.empty())
      EXPECT_EQ(test_case.second.value, prop);
  }
}

TEST_F(AXTreeSourceArcTest, AccessibleNameComputationWindow) {
  auto event = AXEventData::New();
  event->source_id = 1;
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;

  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* node = event->node_data.back().get();
  node->id = 10;

  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* root = event->window_data->back().get();
  root->window_id = 1;
  root->root_node_id = node->id;

  // Live edit name related attributes.

  ui::AXNodeData data;

  // No attributes.
  CallNotifyAccessibilityEvent(event.get());
  GetSerializedWindow(root, data);
  std::string name;
  ASSERT_FALSE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));

  // Title attribute
  SetProperty(root, AXWindowStringProperty::TITLE, "window title");
  CallNotifyAccessibilityEvent(event.get());
  GetSerializedWindow(root, data);
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("window title", name);

  EXPECT_EQ(2, GetDispatchedEventCount(ax::mojom::Event::kFocus));
}

TEST_F(AXTreeSourceArcTest, AccessibleNameComputationWindowWithChildren) {
  auto event = AXEventData::New();
  event->source_id = 3;
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;
  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* root = event->window_data->back().get();
  root->window_id = 100;
  root->root_node_id = 3;
  SetProperty(root, AXWindowIntListProperty::CHILD_WINDOW_IDS, {2, 5});
  SetProperty(root, AXWindowStringProperty::TITLE, "window title");

  // Add a child window.
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* child = event->window_data->back().get();
  child->window_id = 2;
  child->root_node_id = 4;
  SetProperty(child, AXWindowStringProperty::TITLE, "child window title");

  // Add a child node.
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* node = event->node_data.back().get();
  node->id = 3;
  SetProperty(node, AXStringProperty::TEXT, "node text");
  SetProperty(node, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(node, AXBooleanProperty::VISIBLE_TO_USER, true);

  // Add a child node to the child window as well.
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* child_node = event->node_data.back().get();
  child_node->id = 4;
  SetProperty(child_node, AXStringProperty::TEXT, "child node text");
  SetProperty(child_node, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(child_node, AXBooleanProperty::VISIBLE_TO_USER, true);

  // Add a child window with no children as well.
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* child2 = event->window_data->back().get();
  child2->window_id = 5;
  SetProperty(child2, AXWindowStringProperty::TITLE, "child2 window title");

  CallNotifyAccessibilityEvent(event.get());
  ui::AXNodeData data;
  std::string name;

  GetSerializedWindow(root, data);
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("window title", name);
  EXPECT_NE(ax::mojom::Role::kRootWebArea, data.role);
  EXPECT_TRUE(data.GetBoolAttribute(ax::mojom::BoolAttribute::kModal));

  GetSerializedWindow(child, data);
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("child window title", name);
  EXPECT_NE(ax::mojom::Role::kRootWebArea, data.role);

  GetSerializedNode(node, data);
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("node text", name);
  EXPECT_EQ(ax::mojom::Role::kStaticText, data.role);
  ASSERT_FALSE(data.IsIgnored());

  GetSerializedNode(child_node, data);
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("child node text", name);
  EXPECT_NE(ax::mojom::Role::kRootWebArea, data.role);
  ASSERT_FALSE(data.IsIgnored());

  GetSerializedWindow(child2, data);
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("child2 window title", name);
  EXPECT_NE(ax::mojom::Role::kRootWebArea, data.role);

  EXPECT_EQ(1, GetDispatchedEventCount(ax::mojom::Event::kFocus));
}

TEST_F(AXTreeSourceArcTest, StringPropertiesComputations) {
  auto event = AXEventData::New();
  event->source_id = 1;
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* root = event->node_data.back().get();
  root->id = 1;

  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* root_window = event->window_data->back().get();
  root_window->window_id = 100;
  root_window->root_node_id = 1;

  // Add a child node.
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* child = event->node_data.back().get();
  child->id = 2;

  // Set properties to the root.
  SetProperty(root, AXIntListProperty::CHILD_NODE_IDS, std::vector<int>({2}));
  SetProperty(root, AXStringProperty::PACKAGE_NAME, "com.android.vending");

  // Set properties to the child.
  SetProperty(child, AXStringProperty::TOOLTIP, "tooltip text");

  // Populate the tree source with the data.
  CallNotifyAccessibilityEvent(event.get());

  ui::AXNodeData data;
  GetSerializedNode(root, data);

  std::string prop;
  // Url includes AXTreeId, which is unguessable. Just verifies the prefix.
  ASSERT_TRUE(data.GetStringAttribute(ax::mojom::StringAttribute::kUrl, &prop));
  EXPECT_EQ(0U, prop.find("com.android.vending/"));

  GetSerializedNode(child, data);
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kTooltip, &prop));
  ASSERT_EQ("tooltip text", prop);
}

TEST_F(AXTreeSourceArcTest, StateComputations) {
  auto event = AXEventData::New();
  event->source_id = 1;
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;

  // Window.
  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* root_window = event->window_data->back().get();
  root_window->window_id = 100;
  root_window->root_node_id = 1;

  // Node.
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* node = event->node_data.back().get();
  node->id = 1;

  // Node is checkable, but not checked.
  SetProperty(node, AXBooleanProperty::CHECKABLE, true);
  SetProperty(node, AXBooleanProperty::CHECKED, false);
  CallNotifyAccessibilityEvent(event.get());

  ui::AXNodeData data;
  GetSerializedNode(node, data);
  EXPECT_EQ(ax::mojom::CheckedState::kFalse, data.GetCheckedState());

  // Make the node checked.
  SetProperty(node, AXBooleanProperty::CHECKED, true);

  CallNotifyAccessibilityEvent(event.get());
  GetSerializedNode(node, data);
  EXPECT_EQ(ax::mojom::CheckedState::kTrue, data.GetCheckedState());

  // Make the node expandable (i.e. collapsed).
  SetProperty(node, AXIntListProperty::STANDARD_ACTION_IDS,
              std::vector<int>({static_cast<int>(AXActionType::EXPAND)}));

  CallNotifyAccessibilityEvent(event.get());
  GetSerializedNode(node, data);
  EXPECT_TRUE(data.HasState(ax::mojom::State::kCollapsed));
  EXPECT_FALSE(data.HasState(ax::mojom::State::kExpanded));

  // Make the node collapsible (i.e. expanded).
  SetProperty(node, AXIntListProperty::STANDARD_ACTION_IDS,
              std::vector<int>({static_cast<int>(AXActionType::COLLAPSE)}));

  CallNotifyAccessibilityEvent(event.get());
  GetSerializedNode(node, data);
  EXPECT_FALSE(data.HasState(ax::mojom::State::kCollapsed));
  EXPECT_TRUE(data.HasState(ax::mojom::State::kExpanded));
}

TEST_F(AXTreeSourceArcTest, RoleComputationEditText) {
  auto event = AXEventData::New();
  event->source_id = 1;
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* root = event->node_data.back().get();
  SetProperty(root, AXIntListProperty::CHILD_NODE_IDS, std::vector<int>({2}));
  root->id = 1;

  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* root_window = event->window_data->back().get();
  root_window->window_id = 100;
  root_window->root_node_id = 1;

  // Add a child node.
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* node = event->node_data.back().get();
  node->id = 2;

  ui::AXNodeData data;

  // Editable node is textField.
  SetProperty(node, AXStringProperty::CLASS_NAME, ui::kAXEditTextClassname);
  SetProperty(node, AXBooleanProperty::EDITABLE, true);

  CallNotifyAccessibilityEvent(event.get());
  GetSerializedNode(node, data);
  EXPECT_EQ(ax::mojom::Role::kTextField, data.role);

  // Non-editable node is not textField even if it has EditTextClassname.
  // When it has text and no children, it is staticText. Otherwise, it's
  // genericContainer.
  SetProperty(node, AXBooleanProperty::EDITABLE, false);
  SetProperty(node, AXStringProperty::TEXT, "text");

  CallNotifyAccessibilityEvent(event.get());
  GetSerializedNode(node, data);
  EXPECT_EQ(ax::mojom::Role::kStaticText, data.role);

  // Add a child.
  SetProperty(node, AXIntListProperty::CHILD_NODE_IDS, std::vector<int>({3}));
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* child = event->node_data.back().get();
  child->id = 3;

  CallNotifyAccessibilityEvent(event.get());
  GetSerializedNode(node, data);
  EXPECT_EQ(ax::mojom::Role::kGenericContainer, data.role);
}

TEST_F(AXTreeSourceArcTest, ComplexTreeStructure) {
  int tree_size = 4;
  int num_trees = 3;

  auto event = AXEventData::New();
  event->source_id = 4;
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;
  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* root_window = event->window_data->back().get();
  // Pick large numbers for the IDs so as not to overlap.
  root_window->window_id = 1000;
  SetProperty(root_window, AXWindowIntListProperty::CHILD_WINDOW_IDS,
              {100, 200, 300});

  // Make three non-overlapping trees rooted at the same window. One tree has
  // the source_id of interest. Each subtree has a root window, which has a
  // root node with one child, and that child has two leaf children.
  for (int i = 0; i < num_trees; i++) {
    event->window_data->push_back(AXWindowInfoData::New());
    AXWindowInfoData* child_window = event->window_data->back().get();
    child_window->window_id = (i + 1) * 100;
    child_window->root_node_id = i * tree_size + 1;

    event->node_data.push_back(AXNodeInfoData::New());
    AXNodeInfoData* root = event->node_data.back().get();
    root->id = i * tree_size + 1;
    root->window_id = (i + 1) * 100;
    SetProperty(root, AXIntListProperty::CHILD_NODE_IDS,
                std::vector<int>({i * tree_size + 2}));

    event->node_data.push_back(AXNodeInfoData::New());
    AXNodeInfoData* child1 = event->node_data.back().get();
    child1->id = i * tree_size + 2;
    SetProperty(child1, AXIntListProperty::CHILD_NODE_IDS,
                std::vector<int>({i * tree_size + 3, i * tree_size + 4}));

    event->node_data.push_back(AXNodeInfoData::New());
    AXNodeInfoData* child2 = event->node_data.back().get();
    child2->id = i * tree_size + 3;

    event->node_data.push_back(AXNodeInfoData::New());
    AXNodeInfoData* child3 = event->node_data.back().get();
    child3->id = i * tree_size + 4;
  }

  CallNotifyAccessibilityEvent(event.get());

  // Check that each node subtree tree was added, and that it is correct.
  std::vector<ui::AXNode*> children;
  for (int i = 0; i < num_trees; i++) {
    GetChildren(event->node_data.at(i * tree_size).get(), children);
    ASSERT_EQ(1U, children.size());
    EXPECT_EQ(i * tree_size + 2, children[0]->id());
    children.clear();
    GetChildren(event->node_data.at(i * tree_size + 1).get(), children);
    ASSERT_EQ(2U, children.size());
    EXPECT_EQ(i * tree_size + 3, children[0]->id());
    EXPECT_EQ(i * tree_size + 4, children[1]->id());
    children.clear();
  }
  EXPECT_EQ(1, GetDispatchedEventCount(ax::mojom::Event::kFocus));
}

TEST_F(AXTreeSourceArcTest, GetTreeDataAppliesFocus) {
  auto event = AXEventData::New();
  event->source_id = 5;
  event->task_id = 1;
  event->event_type = AXEventType::WINDOW_CONTENT_CHANGED;
  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* root = event->window_data->back().get();
  root->window_id = 5;
  SetProperty(root, AXWindowIntListProperty::CHILD_WINDOW_IDS, {1});

  // Add a child window.
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* child = event->window_data->back().get();
  child->window_id = 1;

  // Add a child node.
  root->root_node_id = 2;
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* node = event->node_data.back().get();
  node->id = 2;
  SetProperty(node, AXBooleanProperty::FOCUSED, true);

  CallNotifyAccessibilityEvent(event.get());

  ui::AXTreeData data;
  EXPECT_TRUE(CallGetTreeData(&data));
  EXPECT_EQ(root->window_id, data.focus_id);

  EXPECT_EQ(1, GetDispatchedEventCount(ax::mojom::Event::kLayoutComplete));
}

TEST_F(AXTreeSourceArcTest, OnViewSelectedEvent) {
  auto event = AXEventData::New();
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_SELECTED;

  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->emplace_back(AXWindowInfoData::New());
  AXWindowInfoData* root_window = event->window_data->back().get();
  root_window->window_id = 100;
  root_window->root_node_id = 10;

  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* root = event->node_data.back().get();
  root->id = 10;
  SetProperty(root, AXIntListProperty::CHILD_NODE_IDS, std::vector<int>({1}));

  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* list = event->node_data.back().get();
  list->id = 1;
  SetProperty(list, AXBooleanProperty::FOCUSABLE, true);
  SetProperty(list, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(list, AXBooleanProperty::VISIBLE_TO_USER, true);
  SetProperty(list, AXIntListProperty::CHILD_NODE_IDS,
              std::vector<int>({2, 3, 4}));

  // Slider.
  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* slider = event->node_data.back().get();
  slider->id = 2;
  SetProperty(slider, AXBooleanProperty::FOCUSABLE, true);
  SetProperty(slider, AXBooleanProperty::IMPORTANCE, true);
  slider->range_info = AXRangeInfoData::New();

  // Simple list item.
  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* simple_item = event->node_data.back().get();
  simple_item->id = 3;
  SetProperty(simple_item, AXBooleanProperty::FOCUSABLE, true);
  SetProperty(simple_item, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(simple_item, AXBooleanProperty::VISIBLE_TO_USER, true);
  simple_item->collection_item_info = AXCollectionItemInfoData::New();

  // This node is not focusable.
  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* wrap_node = event->node_data.back().get();
  wrap_node->id = 4;
  SetProperty(wrap_node, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(wrap_node, AXBooleanProperty::VISIBLE_TO_USER, true);
  SetProperty(wrap_node, AXIntListProperty::CHILD_NODE_IDS,
              std::vector<int>({5}));
  wrap_node->collection_item_info = AXCollectionItemInfoData::New();

  // A list item expected to get the focus.
  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* item = event->node_data.back().get();
  item->id = 5;
  SetProperty(item, AXBooleanProperty::FOCUSABLE, true);
  SetProperty(item, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(item, AXBooleanProperty::VISIBLE_TO_USER, true);

  // A selected event from Slider is kValueChanged.
  event->source_id = slider->id;
  CallNotifyAccessibilityEvent(event.get());
  EXPECT_EQ(1, GetDispatchedEventCount(ax::mojom::Event::kValueChanged));

  // A selected event from a collection. In Android, these event properties are
  // populated by AdapterView.
  event->source_id = list->id;
  SetProperty(event.get(), AXEventIntProperty::ITEM_COUNT, 3);
  SetProperty(event.get(), AXEventIntProperty::FROM_INDEX, 0);
  SetProperty(event.get(), AXEventIntProperty::CURRENT_ITEM_INDEX, 2);
  CallNotifyAccessibilityEvent(event.get());
  EXPECT_EQ(1, GetDispatchedEventCount(ax::mojom::Event::kFocus));

  ui::AXTreeData data;
  EXPECT_TRUE(CallGetTreeData(&data));
  EXPECT_EQ(item->id, data.focus_id);

  // A selected event from a collection item.
  event->source_id = simple_item->id;
  event->int_properties->clear();
  CallNotifyAccessibilityEvent(event.get());
  EXPECT_EQ(2, GetDispatchedEventCount(ax::mojom::Event::kFocus));

  EXPECT_TRUE(CallGetTreeData(&data));
  EXPECT_EQ(simple_item->id, data.focus_id);

  // An event from an invisible node is dropped.
  SetProperty(simple_item, AXBooleanProperty::VISIBLE_TO_USER, false);
  CallNotifyAccessibilityEvent(event.get());
  EXPECT_EQ(2,
            GetDispatchedEventCount(ax::mojom::Event::kFocus));  // not changed

  // A selected event from non collection node is dropped.
  SetProperty(simple_item, AXBooleanProperty::VISIBLE_TO_USER, true);
  event->source_id = item->id;
  event->int_properties->clear();
  CallNotifyAccessibilityEvent(event.get());
  EXPECT_EQ(2,
            GetDispatchedEventCount(ax::mojom::Event::kFocus));  // not changed
}

TEST_F(AXTreeSourceArcTest, OnWindowStateChangedEvent) {
  auto event = AXEventData::New();
  event->source_id = 1;  // node1.
  event->task_id = 1;
  event->event_type = AXEventType::WINDOW_STATE_CHANGED;

  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->emplace_back(AXWindowInfoData::New());
  AXWindowInfoData* root_window = event->window_data->back().get();
  root_window->window_id = 100;
  root_window->root_node_id = 10;

  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* root = event->node_data.back().get();
  root->id = 10;

  SetProperty(root, AXIntListProperty::CHILD_NODE_IDS, std::vector<int>({1}));
  SetProperty(root, AXBooleanProperty::IMPORTANCE, true);

  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* node1 = event->node_data.back().get();
  node1->id = 1;
  SetProperty(node1, AXIntListProperty::CHILD_NODE_IDS, std::vector<int>({2}));
  SetProperty(node1, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(node1, AXBooleanProperty::VISIBLE_TO_USER, true);

  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* node2 = event->node_data.back().get();
  node2->id = 2;
  SetProperty(node2, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(node2, AXBooleanProperty::VISIBLE_TO_USER, true);
  SetProperty(node2, AXStringProperty::TEXT, "sample string.");

  CallNotifyAccessibilityEvent(event.get());
  ui::AXTreeData data;

  // Focus is now at the first accessible node (node2).
  EXPECT_TRUE(CallGetTreeData(&data));
  EXPECT_EQ(node2->id, data.focus_id);

  EXPECT_EQ(1, GetDispatchedEventCount(ax::mojom::Event::kFocus));
}

TEST_F(AXTreeSourceArcTest, OnFocusEvent) {
  auto event = AXEventData::New();
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;

  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->emplace_back(AXWindowInfoData::New());
  AXWindowInfoData* root_window = event->window_data->back().get();
  root_window->window_id = 100;
  root_window->root_node_id = 10;

  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* root = event->node_data.back().get();
  root->id = 10;
  SetProperty(root, AXIntListProperty::CHILD_NODE_IDS,
              std::vector<int>({1, 2}));
  SetProperty(root, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(root, AXBooleanProperty::VISIBLE_TO_USER, true);
  root->collection_info = AXCollectionInfoData::New();
  root->collection_info->row_count = 2;
  root->collection_info->column_count = 1;

  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* node1 = event->node_data.back().get();
  node1->id = 1;
  SetProperty(node1, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(node1, AXBooleanProperty::ACCESSIBILITY_FOCUSED, true);
  SetProperty(node1, AXBooleanProperty::VISIBLE_TO_USER, true);
  SetProperty(node1, AXStringProperty::TEXT, "sample string1.");

  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* node2 = event->node_data.back().get();
  node2->id = 2;
  SetProperty(node2, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(node2, AXBooleanProperty::VISIBLE_TO_USER, true);
  SetProperty(node2, AXStringProperty::TEXT, "sample string2.");

  // Chrome should focus to node2, even if node1 has 'focus' in Android.
  event->source_id = node2->id;
  CallNotifyAccessibilityEvent(event.get());

  ui::AXTreeData data;
  EXPECT_TRUE(CallGetTreeData(&data));
  EXPECT_EQ(node2->id, data.focus_id);

  // Chrome should focus to node2, even if Android sends focus on List.
  event->source_id = root->id;
  CallNotifyAccessibilityEvent(event.get());

  EXPECT_TRUE(CallGetTreeData(&data));
  EXPECT_EQ(node1->id, data.focus_id);

  EXPECT_EQ(2, GetDispatchedEventCount(ax::mojom::Event::kFocus));
}

TEST_F(AXTreeSourceArcTest, OnDrawerOpened) {
  auto event = AXEventData::New();
  event->source_id = 10;  // root
  event->task_id = 1;
  event->event_type = AXEventType::WINDOW_STATE_CHANGED;
  event->event_text = std::vector<std::string>({"Navigation"});

  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->emplace_back(AXWindowInfoData::New());
  AXWindowInfoData* root_window = event->window_data->back().get();
  root_window->window_id = 100;
  root_window->root_node_id = 10;

  /* AXTree of this test:
    [10] root (DrawerLayout)
    --[1] node1 (not-importantForAccessibility) hidden node
    --[2] node2 visible node
    ----[3] node3 node with text
  */
  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* root = event->node_data.back().get();
  root->id = 10;
  SetProperty(root, AXIntListProperty::CHILD_NODE_IDS,
              std::vector<int>({1, 2}));
  SetProperty(root, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(root, AXStringProperty::CLASS_NAME,
              "androidx.drawerlayout.widget.DrawerLayout");

  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* node1 = event->node_data.back().get();
  node1->id = 1;
  SetProperty(node1, AXBooleanProperty::VISIBLE_TO_USER, true);

  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* node2 = event->node_data.back().get();
  node2->id = 2;
  SetProperty(node2, AXIntListProperty::CHILD_NODE_IDS, std::vector<int>({3}));
  SetProperty(node2, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(node2, AXBooleanProperty::VISIBLE_TO_USER, true);

  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* node3 = event->node_data.back().get();
  node3->id = 3;
  SetProperty(node3, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(node3, AXBooleanProperty::VISIBLE_TO_USER, true);
  SetProperty(node3, AXStringProperty::TEXT, "sample string.");

  CallNotifyAccessibilityEvent(event.get());

  ui::AXNodeData data;
  std::string name;
  GetSerializedNode(node2, data);
  ASSERT_EQ(ax::mojom::Role::kMenu, data.role);
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("Navigation", name);

  // Validate that the drawer title is cached.
  event->event_text.reset();
  event->event_type = AXEventType::WINDOW_CONTENT_CHANGED;
  CallNotifyAccessibilityEvent(event.get());

  data.RemoveStringAttribute(ax::mojom::StringAttribute::kName);
  GetSerializedNode(node2, data);
  ASSERT_EQ(ax::mojom::Role::kMenu, data.role);
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("Navigation", name);
}

TEST_F(AXTreeSourceArcTest, SerializeAndUnserialize) {
  auto event = AXEventData::New();
  event->source_id = 10;
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;

  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->emplace_back(AXWindowInfoData::New());
  AXWindowInfoData* root_window = event->window_data->back().get();
  root_window->window_id = 100;
  root_window->root_node_id = 10;

  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* root = event->node_data.back().get();
  root->id = 10;
  SetProperty(root, AXIntListProperty::CHILD_NODE_IDS, std::vector<int>({1}));
  SetProperty(root, AXBooleanProperty::IMPORTANCE, true);

  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* node1 = event->node_data.back().get();
  node1->id = 1;
  SetProperty(node1, AXIntListProperty::CHILD_NODE_IDS, std::vector<int>({2}));

  // An ignored node.
  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* node2 = event->node_data.back().get();
  node2->id = 2;

  // |node2| is ignored by default because
  // AXBooleanProperty::IMPORTANCE has a default false value.

  CallNotifyAccessibilityEvent(event.get());
  EXPECT_EQ(1, GetDispatchedEventCount(ax::mojom::Event::kFocus));
  ExpectTree(
      "id=100 window FOCUSABLE (0, 0)-(0, 0) modal=true child_ids=10\n"
      "  id=10 genericContainer IGNORED INVISIBLE (0, 0)-(0, 0) "
      "restriction=disabled child_ids=1\n"
      "    id=1 genericContainer IGNORED INVISIBLE (0, 0)-(0, 0) "
      "restriction=disabled child_ids=2\n"
      "      id=2 genericContainer IGNORED INVISIBLE (0, 0)-(0, 0) "
      "restriction=disabled\n");

  EXPECT_EQ(0U, tree()->GetFromId(10)->GetUnignoredChildCount());

  // An unignored node.
  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* node3 = event->node_data.back().get();
  node3->id = 3;
  SetProperty(node3, AXStringProperty::CONTENT_DESCRIPTION, "some text");
  SetProperty(node3, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(node2, AXIntListProperty::CHILD_NODE_IDS, std::vector<int>({3}));

  // |node3| is unignored since it has some text.

  CallNotifyAccessibilityEvent(event.get());
  ExpectTree(
      "id=100 window FOCUSABLE (0, 0)-(0, 0) modal=true child_ids=10\n"
      "  id=10 genericContainer INVISIBLE (0, 0)-(0, 0) "
      "restriction=disabled child_ids=1\n"
      "    id=1 genericContainer IGNORED INVISIBLE (0, 0)-(0, 0) "
      "restriction=disabled child_ids=2\n"
      "      id=2 genericContainer IGNORED INVISIBLE (0, 0)-(0, 0) "
      "restriction=disabled child_ids=3\n"
      "        id=3 genericContainer INVISIBLE (0, 0)-(0, 0) "
      "restriction=disabled name=some text\n");
  EXPECT_EQ(1U, tree()->GetFromId(10)->GetUnignoredChildCount());
}

TEST_F(AXTreeSourceArcTest, SerializeVirtualNode) {
  auto event = AXEventData::New();
  event->source_id = 10;
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;

  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->emplace_back(AXWindowInfoData::New());
  AXWindowInfoData* root_window = event->window_data->back().get();
  root_window->window_id = 100;
  root_window->root_node_id = 10;

  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* root = event->node_data.back().get();
  root->id = 10;
  SetProperty(root, AXIntListProperty::CHILD_NODE_IDS, std::vector<int>({1}));
  SetProperty(root, AXBooleanProperty::IMPORTANCE, true);

  // Add a webview node.
  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* webview = event->node_data.back().get();
  webview->id = 1;
  SetProperty(webview, AXBooleanProperty::VISIBLE_TO_USER, true);
  SetProperty(webview, AXIntListProperty::CHILD_NODE_IDS,
              std::vector<int>({2, 3}));
  SetProperty(webview, AXStringProperty::CHROME_ROLE, "rootWebArea");

  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* button1 = event->node_data.back().get();
  button1->id = 2;
  button1->bounds_in_screen = gfx::Rect(0, 0, 50, 50);
  button1->is_virtual_node = true;
  SetProperty(button1, AXStringProperty::CLASS_NAME, ui::kAXButtonClassname);
  SetProperty(button1, AXBooleanProperty::VISIBLE_TO_USER, true);
  SetProperty(
      button1, AXIntListProperty::STANDARD_ACTION_IDS,
      std::vector<int>({static_cast<int>(AXActionType::NEXT_HTML_ELEMENT),
                        static_cast<int>(AXActionType::FOCUS)}));
  SetProperty(button1, AXStringProperty::CONTENT_DESCRIPTION, "button1");

  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* button2 = event->node_data.back().get();
  button2->id = 3;
  button2->bounds_in_screen = gfx::Rect(0, 0, 100, 100);
  button2->is_virtual_node = true;
  SetProperty(button2, AXStringProperty::CLASS_NAME, ui::kAXButtonClassname);
  SetProperty(button2, AXBooleanProperty::VISIBLE_TO_USER, true);
  SetProperty(
      button2, AXIntListProperty::STANDARD_ACTION_IDS,
      std::vector<int>({static_cast<int>(AXActionType::NEXT_HTML_ELEMENT),
                        static_cast<int>(AXActionType::FOCUS)}));
  SetProperty(button2, AXStringProperty::CONTENT_DESCRIPTION, "button2");

  CallNotifyAccessibilityEvent(event.get());

  ui::AXNodeData data;
  GetSerializedNode(webview, data);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer, data.role);

  // Node inside a WebView is not ignored even if it's not set importance.
  GetSerializedNode(button1, data);
  ASSERT_FALSE(data.IsIgnored());

  GetSerializedNode(button2, data);
  ASSERT_FALSE(data.IsIgnored());

  // Children are not reordered under WebView.
  std::vector<ui::AXNode*> children;
  GetChildren(webview, children);
  ASSERT_EQ(2U, children.size());
  EXPECT_EQ(button1->id, children[0]->id());
  EXPECT_EQ(button2->id, children[1]->id());
}

TEST_F(AXTreeSourceArcTest, SyncFocus) {
  auto event = AXEventData::New();
  event->source_id = 1;
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;

  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->emplace_back(AXWindowInfoData::New());
  AXWindowInfoData* root_window = event->window_data->back().get();
  root_window->window_id = 100;
  root_window->root_node_id = 10;

  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* root = event->node_data.back().get();
  root->id = 10;
  SetProperty(root, AXIntListProperty::CHILD_NODE_IDS,
              std::vector<int>({1, 2}));

  // Add child nodes.
  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* node1 = event->node_data.back().get();
  node1->id = 1;
  SetProperty(node1, AXBooleanProperty::FOCUSABLE, true);
  SetProperty(node1, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(node1, AXBooleanProperty::VISIBLE_TO_USER, true);
  node1->bounds_in_screen = gfx::Rect(0, 0, 50, 50);

  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* node2 = event->node_data.back().get();
  node2->id = 2;
  SetProperty(node2, AXBooleanProperty::FOCUSABLE, true);
  SetProperty(node2, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(node2, AXBooleanProperty::VISIBLE_TO_USER, true);

  // Add a child node to |node1|, but it's not an important node.
  SetProperty(node1, AXIntListProperty::CHILD_NODE_IDS, std::vector<int>({3}));
  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* node3 = event->node_data.back().get();
  node3->id = 3;

  // Initially |node1| has a focus.
  CallNotifyAccessibilityEvent(event.get());
  ui::AXTreeData data;
  EXPECT_TRUE(CallGetTreeData(&data));
  EXPECT_EQ(node1->id, data.focus_id);

  // Focus event to a non-important node. The descendant important node |node1|
  // gets focus instead.
  event->source_id = node3->id;
  event->event_type = AXEventType::VIEW_FOCUSED;
  CallNotifyAccessibilityEvent(event.get());

  EXPECT_TRUE(CallGetTreeData(&data));
  EXPECT_EQ(node1->id, data.focus_id);

  // When the focused node disappeared from the tree, move the focus to the
  // root.
  root->int_list_properties->clear();
  event->node_data.resize(1);

  event->event_type = AXEventType::WINDOW_CONTENT_CHANGED;
  CallNotifyAccessibilityEvent(event.get());

  EXPECT_TRUE(CallGetTreeData(&data));
  EXPECT_EQ(root_window->window_id, data.focus_id);
}

TEST_F(AXTreeSourceArcTest, LiveRegion) {
  auto event = AXEventData::New();
  event->source_id = 1;
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;

  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->emplace_back(AXWindowInfoData::New());
  AXWindowInfoData* root_window = event->window_data->back().get();
  root_window->window_id = 100;
  root_window->root_node_id = 10;

  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* root = event->node_data.back().get();
  root->id = 10;
  SetProperty(root, AXIntListProperty::CHILD_NODE_IDS,
              std::vector<int>({1, 2}));
  SetProperty(root, AXIntProperty::LIVE_REGION,
              static_cast<int32_t>(mojom::AccessibilityLiveRegionType::POLITE));

  // Add child nodes.
  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* node1 = event->node_data.back().get();
  node1->id = 1;
  SetProperty(node1, AXStringProperty::TEXT, "text 1");

  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* node2 = event->node_data.back().get();
  node2->id = 2;
  SetProperty(node2, AXStringProperty::TEXT, "text 2");

  CallNotifyAccessibilityEvent(event.get());

  ui::AXNodeData data;
  GetSerializedNode(root, data);
  std::string status;
  ASSERT_TRUE(data.GetStringAttribute(ax::mojom::StringAttribute::kLiveStatus,
                                      &status));
  ASSERT_EQ(status, "polite");
  for (AXNodeInfoData* node : {root, node1, node2}) {
    GetSerializedNode(node, data);
    ASSERT_TRUE(data.GetStringAttribute(
        ax::mojom::StringAttribute::kContainerLiveStatus, &status));
    ASSERT_EQ(status, "polite");
  }

  EXPECT_EQ(0, GetDispatchedEventCount(ax::mojom::Event::kLiveRegionChanged));

  // Modify text of node1.
  SetProperty(node1, AXStringProperty::TEXT, "modified text 1");
  CallNotifyAccessibilityEvent(event.get());

  EXPECT_EQ(1, GetDispatchedEventCount(ax::mojom::Event::kLiveRegionChanged));
}

}  // namespace arc
