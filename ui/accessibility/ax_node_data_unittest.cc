// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_node_data.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_role_properties.h"

namespace ui {

TEST(AXNodeDataTest, GetAndSetCheckedState) {
  AXNodeData root;
  EXPECT_EQ(ax::mojom::CheckedState::kNone, root.GetCheckedState());
  EXPECT_FALSE(root.HasIntAttribute(ax::mojom::IntAttribute::kCheckedState));

  root.SetCheckedState(ax::mojom::CheckedState::kMixed);
  EXPECT_EQ(ax::mojom::CheckedState::kMixed, root.GetCheckedState());
  EXPECT_TRUE(root.HasIntAttribute(ax::mojom::IntAttribute::kCheckedState));

  root.SetCheckedState(ax::mojom::CheckedState::kFalse);
  EXPECT_EQ(ax::mojom::CheckedState::kFalse, root.GetCheckedState());
  EXPECT_TRUE(root.HasIntAttribute(ax::mojom::IntAttribute::kCheckedState));

  root.SetCheckedState(ax::mojom::CheckedState::kNone);
  EXPECT_EQ(ax::mojom::CheckedState::kNone, root.GetCheckedState());
  EXPECT_FALSE(root.HasIntAttribute(ax::mojom::IntAttribute::kCheckedState));
}

TEST(AXNodeDataTest, TextAttributes) {
  AXNodeData node_1;
  node_1.AddFloatAttribute(ax::mojom::FloatAttribute::kFontSize, 1.5);

  AXNodeData node_2;
  node_2.AddFloatAttribute(ax::mojom::FloatAttribute::kFontSize, 1.5);
  EXPECT_TRUE(node_1.GetTextStyles() == node_2.GetTextStyles());

  node_2.AddIntAttribute(ax::mojom::IntAttribute::kColor, 100);
  EXPECT_TRUE(node_1.GetTextStyles() != node_2.GetTextStyles());

  node_1.AddIntAttribute(ax::mojom::IntAttribute::kColor, 100);
  EXPECT_TRUE(node_1.GetTextStyles() == node_2.GetTextStyles());

  node_2.RemoveIntAttribute(ax::mojom::IntAttribute::kColor);
  EXPECT_TRUE(node_1.GetTextStyles() != node_2.GetTextStyles());

  node_2.AddIntAttribute(ax::mojom::IntAttribute::kColor, 100);
  EXPECT_TRUE(node_1.GetTextStyles() == node_2.GetTextStyles());

  node_1.AddStringAttribute(ax::mojom::StringAttribute::kFontFamily,
                            "test font");
  EXPECT_TRUE(node_1.GetTextStyles() != node_2.GetTextStyles());

  node_2.AddStringAttribute(ax::mojom::StringAttribute::kFontFamily,
                            "test font");
  EXPECT_TRUE(node_1.GetTextStyles() == node_2.GetTextStyles());

  node_2.RemoveStringAttribute(ax::mojom::StringAttribute::kFontFamily);
  EXPECT_TRUE(node_1.GetTextStyles() != node_2.GetTextStyles());

  node_2.AddStringAttribute(ax::mojom::StringAttribute::kFontFamily,
                            "test font");
  EXPECT_TRUE(node_1.GetTextStyles() == node_2.GetTextStyles());

  node_2.AddStringAttribute(ax::mojom::StringAttribute::kFontFamily,
                            "different font");
  EXPECT_TRUE(node_1.GetTextStyles() != node_2.GetTextStyles());

  std::string tooltip;
  node_2.AddStringAttribute(ax::mojom::StringAttribute::kTooltip,
                            "test tooltip");
  EXPECT_TRUE(node_2.GetStringAttribute(ax::mojom::StringAttribute::kTooltip,
                                        &tooltip));
  EXPECT_EQ(tooltip, "test tooltip");

  AXNodeTextStyles node1_styles = node_1.GetTextStyles();
  AXNodeTextStyles moved_styles = std::move(node1_styles);
  EXPECT_TRUE(node1_styles != moved_styles);
  EXPECT_TRUE(moved_styles == node_1.GetTextStyles());
}

TEST(AXNodeDataTest, TestIsClickable) {
  // Test for ax node data attribute with a custom default action verb.
  AXNodeData data_default_action_verb;

  for (int action_verb_idx =
           static_cast<int>(ax::mojom::DefaultActionVerb::kMinValue);
       action_verb_idx <=
       static_cast<int>(ax::mojom::DefaultActionVerb::kMaxValue);
       action_verb_idx++) {
    data_default_action_verb.SetDefaultActionVerb(
        static_cast<ax::mojom::DefaultActionVerb>(action_verb_idx));
    bool is_clickable = data_default_action_verb.IsClickable();

    SCOPED_TRACE(testing::Message()
                 << "ax::mojom::DefaultActionVerb="
                 << ToString(data_default_action_verb.GetDefaultActionVerb())
                 << ", Actual: isClickable=" << is_clickable
                 << ", Expected: isClickable=" << !is_clickable);

    if (data_default_action_verb.GetDefaultActionVerb() ==
            ax::mojom::DefaultActionVerb::kClickAncestor ||
        data_default_action_verb.GetDefaultActionVerb() ==
            ax::mojom::DefaultActionVerb::kNone)
      EXPECT_FALSE(is_clickable);
    else
      EXPECT_TRUE(is_clickable);
  }

  // Test for iterating through all roles and validate if a role is clickable.
  std::unordered_set<ax::mojom::Role> roles_expected_is_clickable = {
      ax::mojom::Role::kButton,
      ax::mojom::Role::kCheckBox,
      ax::mojom::Role::kColorWell,
      ax::mojom::Role::kDisclosureTriangle,
      ax::mojom::Role::kDocBackLink,
      ax::mojom::Role::kDocBiblioRef,
      ax::mojom::Role::kDocGlossRef,
      ax::mojom::Role::kDocNoteRef,
      ax::mojom::Role::kLink,
      ax::mojom::Role::kListBoxOption,
      ax::mojom::Role::kMenuButton,
      ax::mojom::Role::kMenuItem,
      ax::mojom::Role::kMenuItemCheckBox,
      ax::mojom::Role::kMenuItemRadio,
      ax::mojom::Role::kMenuListOption,
      ax::mojom::Role::kMenuListPopup,
      ax::mojom::Role::kPopUpButton,
      ax::mojom::Role::kRadioButton,
      ax::mojom::Role::kSwitch,
      ax::mojom::Role::kTab,
      ax::mojom::Role::kToggleButton};

  AXNodeData data;

  for (int role_idx = static_cast<int>(ax::mojom::Role::kMinValue);
       role_idx <= static_cast<int>(ax::mojom::Role::kMaxValue); role_idx++) {
    data.role = static_cast<ax::mojom::Role>(role_idx);
    bool is_clickable = data.IsClickable();

    SCOPED_TRACE(testing::Message()
                 << "ax::mojom::Role=" << ToString(data.role)
                 << ", Actual: isClickable=" << is_clickable
                 << ", Expected: isClickable=" << !is_clickable);

    if (roles_expected_is_clickable.find(data.role) !=
        roles_expected_is_clickable.end())
      EXPECT_TRUE(is_clickable);
    else
      EXPECT_FALSE(is_clickable);
  }
}

TEST(AXNodeDataTest, TestIsInvocable) {
  // Test for iterating through all roles and validate if a role is invocable.
  // A role is invocable if it is clickable and supports neither expand collpase
  // nor toggle.
  AXNodeData data;
  for (int role_idx = static_cast<int>(ax::mojom::Role::kMinValue);
       role_idx <= static_cast<int>(ax::mojom::Role::kMaxValue); role_idx++) {
    data.role = static_cast<ax::mojom::Role>(role_idx);
    bool supports_expand_collapse = data.SupportsExpandCollapse();
    bool supports_toggle = ui::SupportsToggle(data.role);
    bool is_clickable = data.IsClickable();
    bool is_invocable = data.IsInvocable();

    SCOPED_TRACE(testing::Message()
                 << "ax::mojom::Role=" << ToString(data.role)
                 << ", isClickable=" << is_clickable
                 << ", supportsToggle=" << supports_toggle
                 << ", supportsExpandCollapse=" << supports_expand_collapse
                 << ", Actual: isInvocable=" << is_invocable
                 << ", Expected: isInvocable=" << !is_invocable);

    if (is_clickable && !supports_toggle && !supports_expand_collapse)
      EXPECT_TRUE(is_invocable);
    else
      EXPECT_FALSE(is_invocable);
  }
}

TEST(AXNodeDataTest, TestSupportsExpandCollapse) {
  // Test for iterating through all hasPopup attributes and validate if a
  // hasPopup attribute supports expand collapse.
  AXNodeData data_has_popup;

  for (int has_popup_idx = static_cast<int>(ax::mojom::HasPopup::kMinValue);
       has_popup_idx <= static_cast<int>(ax::mojom::HasPopup::kMaxValue);
       has_popup_idx++) {
    data_has_popup.SetHasPopup(static_cast<ax::mojom::HasPopup>(has_popup_idx));
    bool supports_expand_collapse = data_has_popup.SupportsExpandCollapse();

    SCOPED_TRACE(testing::Message() << "ax::mojom::HasPopup="
                                    << ToString(data_has_popup.GetHasPopup())
                                    << ", Actual: supportsExpandCollapse="
                                    << supports_expand_collapse
                                    << ", Expected: supportsExpandCollapse="
                                    << !supports_expand_collapse);

    if (data_has_popup.GetHasPopup() == ax::mojom::HasPopup::kFalse)
      EXPECT_FALSE(supports_expand_collapse);
    else
      EXPECT_TRUE(supports_expand_collapse);
  }

  // Test for iterating through all states and validate if a state supports
  // expand collapse.
  AXNodeData data_state;

  for (int state_idx = static_cast<int>(ax::mojom::State::kMinValue);
       state_idx <= static_cast<int>(ax::mojom::State::kMaxValue);
       state_idx++) {
    ax::mojom::State state = static_cast<ax::mojom::State>(state_idx);

    // skipping kNone here because AXNodeData::AddState, RemoveState forbids
    // kNone to be added/removed and would fail DCHECK.
    if (state == ax::mojom::State::kNone)
      continue;

    data_state.AddState(state);

    bool supports_expand_collapse = data_state.SupportsExpandCollapse();

    SCOPED_TRACE(testing::Message() << "ax::mojom::State=" << ToString(state)
                                    << ", Actual: supportsExpandCollapse="
                                    << supports_expand_collapse
                                    << ", Expected: supportsExpandCollapse="
                                    << !supports_expand_collapse);

    if (data_state.HasState(ax::mojom::State::kExpanded) ||
        data_state.HasState(ax::mojom::State::kCollapsed))
      EXPECT_TRUE(supports_expand_collapse);
    else
      EXPECT_FALSE(supports_expand_collapse);

    data_state.RemoveState(state);
  }

  // Test for iterating through all roles and validate if a role supports expand
  // collapse.
  AXNodeData data;

  std::unordered_set<ax::mojom::Role> roles_expected_supports_expand_collapse =
      {ax::mojom::Role::kComboBoxGrouping, ax::mojom::Role::kComboBoxMenuButton,
       ax::mojom::Role::kDisclosureTriangle,
       ax::mojom::Role::kTextFieldWithComboBox, ax::mojom::Role::kTreeItem};

  for (int role_idx = static_cast<int>(ax::mojom::Role::kMinValue);
       role_idx <= static_cast<int>(ax::mojom::Role::kMaxValue); role_idx++) {
    data.role = static_cast<ax::mojom::Role>(role_idx);
    bool supports_expand_collapse = data.SupportsExpandCollapse();

    SCOPED_TRACE(testing::Message() << "ax::mojom::Role=" << ToString(data.role)
                                    << ", Actual: supportsExpandCollapse="
                                    << supports_expand_collapse
                                    << ", Expected: supportsExpandCollapse="
                                    << !supports_expand_collapse);

    if (roles_expected_supports_expand_collapse.find(data.role) !=
        roles_expected_supports_expand_collapse.end())
      EXPECT_TRUE(supports_expand_collapse);
    else
      EXPECT_FALSE(supports_expand_collapse);
  }
}

}  // namespace ui
