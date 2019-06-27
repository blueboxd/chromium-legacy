// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_range.h"

#include <memory>
#include <vector>

#include "base/strings/string16.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/accessibility/platform/test_ax_node_wrapper.h"

namespace ui {

using TestPositionInstance =
    std::unique_ptr<AXPosition<AXNodePosition, AXNode>>;
using TestPositionRange = AXRange<AXPosition<AXNodePosition, AXNode>>;

#define EXPECT_RANGE_VECTOR_EQ(actual_vector, expected_vector)  \
  {                                                             \
    EXPECT_EQ(expected_vector.size(), actual_vector.size());    \
    size_t element_count =                                      \
        std::min(expected_vector.size(), actual_vector.size()); \
    for (size_t i = 0; i < element_count; ++i)                  \
      EXPECT_EQ(expected_vector[i], actual_vector[i]);          \
  }

namespace {

constexpr int32_t ROOT_ID = 1;
constexpr int32_t BUTTON_ID = 2;
constexpr int32_t CHECK_BOX_ID = 3;
constexpr int32_t TEXT_FIELD_ID = 4;
constexpr int32_t STATIC_TEXT1_ID = 5;
constexpr int32_t INLINE_BOX1_ID = 6;
constexpr int32_t LINE_BREAK_ID = 7;
constexpr int32_t STATIC_TEXT2_ID = 8;
constexpr int32_t INLINE_BOX2_ID = 9;

class AXRangeTest : public testing::Test, public AXTreeManager {
 public:
  const base::string16 LINE_1 = base::ASCIIToUTF16("Line 1");
  const base::string16 LINE_2 = base::ASCIIToUTF16("Line 2");
  const base::string16 TEXT_VALUE = AXRangeTest::LINE_1.substr()
                                        .append(base::ASCIIToUTF16("\n"))
                                        .append(AXRangeTest::LINE_2);
  const base::string16 BUTTON_TEXT = base::ASCIIToUTF16("Button");

  AXRangeTest() = default;
  ~AXRangeTest() override = default;

 protected:
  void SetUp() override;
  void TearDown() override;

  AXNode* GetRootNode() const { return tree_->root(); }

  // AXTreeManager implementation.
  AXNode* GetNodeFromTree(const AXTreeID tree_id,
                          const int32_t node_id) const override;
  AXPlatformNodeDelegate* GetDelegate(const AXTreeID tree_id,
                                      const int32_t node_id) const override;
  AXPlatformNodeDelegate* GetRootDelegate(
      const AXTreeID tree_id) const override;
  AXTreeID GetTreeID() const override;
  AXTreeID GetParentTreeID() const override;
  AXNode* GetRootAsAXNode() const override;
  AXNode* GetParentNodeFromParentTreeAsAXNode() const override;

  AXNodeData root_;
  AXNodeData button_;
  AXNodeData check_box_;
  AXNodeData text_field_;
  AXNodeData static_text1_;
  AXNodeData line_break_;
  AXNodeData static_text2_;
  AXNodeData inline_box1_;
  AXNodeData inline_box2_;

  std::unique_ptr<AXTree> tree_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AXRangeTest);
};

void AXRangeTest::SetUp() {
  root_.id = ROOT_ID;
  button_.id = BUTTON_ID;
  check_box_.id = CHECK_BOX_ID;
  text_field_.id = TEXT_FIELD_ID;
  static_text1_.id = STATIC_TEXT1_ID;
  inline_box1_.id = INLINE_BOX1_ID;
  line_break_.id = LINE_BREAK_ID;
  static_text2_.id = STATIC_TEXT2_ID;
  inline_box2_.id = INLINE_BOX2_ID;

  root_.role = ax::mojom::Role::kDialog;
  root_.AddState(ax::mojom::State::kFocusable);
  root_.SetName(base::string16(base::ASCIIToUTF16("ButtonCheck box")) +
                TEXT_VALUE);
  root_.relative_bounds.bounds = gfx::RectF(0, 0, 800, 600);

  button_.role = ax::mojom::Role::kButton;
  button_.SetHasPopup(ax::mojom::HasPopup::kMenu);
  button_.SetName(BUTTON_TEXT);
  button_.SetValue(BUTTON_TEXT);
  button_.relative_bounds.bounds = gfx::RectF(20, 20, 200, 30);
  button_.AddIntAttribute(ax::mojom::IntAttribute::kNextOnLineId,
                          check_box_.id);
  root_.child_ids.push_back(button_.id);

  check_box_.role = ax::mojom::Role::kCheckBox;
  check_box_.SetCheckedState(ax::mojom::CheckedState::kTrue);
  check_box_.SetName("Check box");
  check_box_.relative_bounds.bounds = gfx::RectF(20, 50, 200, 30);
  check_box_.AddIntAttribute(ax::mojom::IntAttribute::kPreviousOnLineId,
                             button_.id);
  root_.child_ids.push_back(check_box_.id);

  text_field_.role = ax::mojom::Role::kTextField;
  text_field_.AddState(ax::mojom::State::kEditable);
  text_field_.SetValue(TEXT_VALUE);
  text_field_.AddIntListAttribute(
      ax::mojom::IntListAttribute::kCachedLineStarts,
      std::vector<int32_t>{0, 7});
  text_field_.child_ids.push_back(static_text1_.id);
  text_field_.child_ids.push_back(line_break_.id);
  text_field_.child_ids.push_back(static_text2_.id);
  root_.child_ids.push_back(text_field_.id);

  static_text1_.role = ax::mojom::Role::kStaticText;
  static_text1_.AddState(ax::mojom::State::kEditable);
  static_text1_.SetName("Line 1");
  static_text1_.child_ids.push_back(inline_box1_.id);

  inline_box1_.role = ax::mojom::Role::kInlineTextBox;
  inline_box1_.AddState(ax::mojom::State::kEditable);
  inline_box1_.SetName("Line 1");
  inline_box1_.relative_bounds.bounds = gfx::RectF(220, 20, 30, 30);
  std::vector<int32_t> character_offsets1;
  // The width of each character is 5px.
  character_offsets1.push_back(225);  // "L" {220, 20, 5x30}
  character_offsets1.push_back(230);  // "i" {225, 20, 5x30}
  character_offsets1.push_back(235);  // "n" {230, 20, 5x30}
  character_offsets1.push_back(240);  // "e" {235, 20, 5x30}
  character_offsets1.push_back(245);  // " " {240, 20, 5x30}
  character_offsets1.push_back(250);  // "1" {245, 20, 5x30}
  inline_box1_.AddIntListAttribute(
      ax::mojom::IntListAttribute::kCharacterOffsets, character_offsets1);
  inline_box1_.AddIntListAttribute(ax::mojom::IntListAttribute::kWordStarts,
                                   std::vector<int32_t>{0, 5});
  inline_box1_.AddIntListAttribute(ax::mojom::IntListAttribute::kWordEnds,
                                   std::vector<int32_t>{4, 6});
  inline_box1_.AddIntAttribute(ax::mojom::IntAttribute::kNextOnLineId,
                               line_break_.id);

  line_break_.role = ax::mojom::Role::kLineBreak;
  line_break_.AddState(ax::mojom::State::kEditable);
  line_break_.SetName("\n");
  line_break_.relative_bounds.bounds = gfx::RectF(250, 20, 0, 30);
  line_break_.AddIntAttribute(ax::mojom::IntAttribute::kPreviousOnLineId,
                              inline_box1_.id);

  static_text2_.role = ax::mojom::Role::kStaticText;
  static_text2_.AddState(ax::mojom::State::kEditable);
  static_text2_.SetName("Line 2");
  static_text2_.child_ids.push_back(inline_box2_.id);

  inline_box2_.role = ax::mojom::Role::kInlineTextBox;
  inline_box2_.AddState(ax::mojom::State::kEditable);
  inline_box2_.SetName("Line 2");
  inline_box2_.relative_bounds.bounds = gfx::RectF(220, 50, 42, 30);
  std::vector<int32_t> character_offsets2;
  // The width of each character is 7 px.
  character_offsets2.push_back(227);  // "L" {220, 50, 7x30}
  character_offsets2.push_back(234);  // "i" {227, 50, 7x30}
  character_offsets2.push_back(241);  // "n" {234, 50, 7x30}
  character_offsets2.push_back(248);  // "e" {241, 50, 7x30}
  character_offsets2.push_back(255);  // " " {248, 50, 7x30}
  character_offsets2.push_back(262);  // "2" {255, 50, 7x30}
  inline_box2_.AddIntListAttribute(
      ax::mojom::IntListAttribute::kCharacterOffsets, character_offsets2);
  inline_box2_.AddIntListAttribute(ax::mojom::IntListAttribute::kWordStarts,
                                   std::vector<int32_t>{0, 5});
  inline_box2_.AddIntListAttribute(ax::mojom::IntListAttribute::kWordEnds,
                                   std::vector<int32_t>{4, 6});

  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes = {root_,       button_,       check_box_,
                         text_field_, static_text1_, inline_box1_,
                         line_break_, static_text2_, inline_box2_};
  initial_state.has_tree_data = true;
  initial_state.tree_data.tree_id = AXTreeID::CreateNewAXTreeID();
  initial_state.tree_data.title = "Dialog title";

  tree_.reset(new AXTree(initial_state));
  AXNodePosition::SetTreeForTesting(tree_.get());
  AXTreeManagerMap::GetInstance().AddTreeManager(
      initial_state.tree_data.tree_id, this);
}

void AXRangeTest::TearDown() {
  AXNodePosition::SetTreeForTesting(nullptr);
  AXTreeManagerMap::GetInstance().RemoveTreeManager(tree_->data().tree_id);
}

AXNode* AXRangeTest::GetNodeFromTree(const AXTreeID tree_id,
                                     const int32_t node_id) const {
  if (GetTreeID() == tree_id)
    return tree_->GetFromId(node_id);

  return nullptr;
}

AXPlatformNodeDelegate* AXRangeTest::GetDelegate(const AXTreeID tree_id,
                                                 const int32_t node_id) const {
  AXNode* node = GetNodeFromTree(tree_id, node_id);
  if (node) {
    TestAXNodeWrapper* wrapper =
        TestAXNodeWrapper::GetOrCreate(tree_.get(), node);

    return wrapper;
  }
  return nullptr;
}

AXPlatformNodeDelegate* AXRangeTest::GetRootDelegate(
    const AXTreeID tree_id) const {
  if (GetTreeID() == tree_id) {
    AXNode* root_node = GetRootNode();

    if (root_node) {
      TestAXNodeWrapper* wrapper =
          TestAXNodeWrapper::GetOrCreate(tree_.get(), root_node);
      return wrapper;
    }
  }

  return nullptr;
}

AXTreeID AXRangeTest::GetTreeID() const {
  return tree_->data().tree_id;
}

AXTreeID AXRangeTest::GetParentTreeID() const {
  return GetTreeID();
}

ui::AXNode* AXRangeTest::GetRootAsAXNode() const {
  return GetRootNode();
}

ui::AXNode* AXRangeTest::GetParentNodeFromParentTreeAsAXNode() const {
  return nullptr;
}

}  // namespace

TEST_F(AXRangeTest, EqualityOperators) {
  TestPositionInstance null_position = AXNodePosition::CreateNullPosition();
  TestPositionInstance test_position1 = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, button_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  TestPositionInstance test_position2 = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, line_break_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  TestPositionInstance test_position3 = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, inline_box2_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);

  // Invalid ranges (with at least one null endpoint).
  TestPositionRange null_position_and_nullptr(null_position->Clone(), nullptr);
  TestPositionRange nullptr_and_test_position(nullptr, test_position1->Clone());
  TestPositionRange test_position_and_null_position(test_position2->Clone(),
                                                    null_position->Clone());

  TestPositionRange test_positions_1_and_2(test_position1->Clone(),
                                           test_position2->Clone());
  TestPositionRange test_positions_2_and_1(test_position2->Clone(),
                                           test_position1->Clone());
  TestPositionRange test_positions_1_and_3(test_position1->Clone(),
                                           test_position3->Clone());
  TestPositionRange test_positions_2_and_3(test_position2->Clone(),
                                           test_position3->Clone());
  TestPositionRange test_positions_3_and_2(test_position3->Clone(),
                                           test_position2->Clone());

  EXPECT_EQ(null_position_and_nullptr, nullptr_and_test_position);
  EXPECT_EQ(nullptr_and_test_position, test_position_and_null_position);
  EXPECT_NE(null_position_and_nullptr, test_positions_2_and_1);
  EXPECT_NE(test_positions_2_and_1, test_position_and_null_position);
  EXPECT_EQ(test_positions_1_and_2, test_positions_1_and_2);
  EXPECT_NE(test_positions_2_and_1, test_positions_1_and_2);
  EXPECT_EQ(test_positions_3_and_2, test_positions_2_and_3);
  EXPECT_NE(test_positions_1_and_2, test_positions_2_and_3);
  EXPECT_EQ(test_positions_1_and_2, test_positions_1_and_3);
}

TEST_F(AXRangeTest, GetTextWithWholeObjects) {
  base::string16 all_text = BUTTON_TEXT.substr(0).append(TEXT_VALUE);
  // Create a range starting from the button object and ending at the last
  // character of the root, i.e. at the last character of the second line in the
  // text field.
  TestPositionInstance start = AXNodePosition::CreateTreePosition(
      tree_->data().tree_id, root_.id, 0 /* child_index */);
  TestPositionInstance end = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, root_.id, all_text.length() /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_TRUE(end->IsTextPosition());
  TestPositionRange forward_range(start->Clone(), end->Clone());
  EXPECT_EQ(all_text, forward_range.GetText());
  TestPositionRange backward_range(std::move(end), std::move(start));
  EXPECT_EQ(all_text, backward_range.GetText());

  // Button
  start = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, button_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_TRUE(start->IsTextPosition());
  end = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, button_.id, BUTTON_TEXT.length() /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_TRUE(end->IsTextPosition());
  TestPositionRange button_range(start->Clone(), end->Clone());
  EXPECT_EQ(BUTTON_TEXT, button_range.GetText());
  TestPositionRange button_range_backward(std::move(end), std::move(start));
  EXPECT_EQ(BUTTON_TEXT, button_range_backward.GetText());

  // text_field_
  start = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, text_field_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  end =
      AXNodePosition::CreateTextPosition(tree_->data().tree_id, text_field_.id,
                                         TEXT_VALUE.length() /* text_offset */,
                                         ax::mojom::TextAffinity::kDownstream);
  ASSERT_TRUE(start->IsTextPosition());
  ASSERT_TRUE(end->IsTextPosition());
  TestPositionRange text_field_range(start->Clone(), end->Clone());
  EXPECT_EQ(TEXT_VALUE, text_field_range.GetText());
  TestPositionRange text_field_range_backward(std::move(end), std::move(start));
  EXPECT_EQ(TEXT_VALUE, text_field_range_backward.GetText());

  // static_text1_
  start = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, static_text1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_TRUE(start->IsTextPosition());
  end = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, static_text1_.id,
      LINE_1.length() /* text_offset */, ax::mojom::TextAffinity::kDownstream);
  ASSERT_TRUE(end->IsTextPosition());
  TestPositionRange static_text1_range(start->Clone(), end->Clone());
  EXPECT_EQ(LINE_1, static_text1_range.GetText());
  TestPositionRange static_text1_range_backward(std::move(end),
                                                std::move(start));
  EXPECT_EQ(LINE_1, static_text1_range_backward.GetText());

  // static_text2_
  start = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, static_text2_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_TRUE(start->IsTextPosition());
  end = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, static_text2_.id,
      LINE_2.length() /* text_offset */, ax::mojom::TextAffinity::kDownstream);
  ASSERT_TRUE(end->IsTextPosition());
  TestPositionRange static_text2_range(start->Clone(), end->Clone());
  EXPECT_EQ(LINE_2, static_text2_range.GetText());
  TestPositionRange static_text2_range_backward(std::move(end),
                                                std::move(start));
  EXPECT_EQ(LINE_2, static_text2_range_backward.GetText());

  // static_text1_ to static_text2_
  start = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, static_text1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_TRUE(start->IsTextPosition());
  end = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, static_text2_.id,
      LINE_2.length() /* text_offset */, ax::mojom::TextAffinity::kDownstream);
  ASSERT_TRUE(end->IsTextPosition());
  TestPositionRange static_text_range(start->Clone(), end->Clone());
  EXPECT_EQ(TEXT_VALUE, static_text_range.GetText());
  TestPositionRange static_text_range_backward(std::move(end),
                                               std::move(start));
  EXPECT_EQ(TEXT_VALUE, static_text_range_backward.GetText());

  // root_ to static_text2_'s end
  start = AXNodePosition::CreateTreePosition(tree_->data().tree_id, root_.id,
                                             0 /* child_index */);
  end = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, static_text2_.id,
      LINE_2.length() /* text_offset */, ax::mojom::TextAffinity::kDownstream);
  ASSERT_TRUE(end->IsTextPosition());
  TestPositionRange root_to_static2_text_range(start->Clone(), end->Clone());
  EXPECT_EQ(all_text, root_to_static2_text_range.GetText());
  TestPositionRange root_to_static2_text_range_backward(std::move(end),
                                                        std::move(start));
  EXPECT_EQ(all_text, root_to_static2_text_range_backward.GetText());

  // root_ to static_text2_'s start
  base::string16 text_up_to_text2_tree_start =
      BUTTON_TEXT.substr(0).append(LINE_1).append(base::ASCIIToUTF16("\n"));
  start = AXNodePosition::CreateTreePosition(tree_->data().tree_id, root_.id,
                                             0 /* child_index */);
  end = AXNodePosition::CreateTreePosition(
      tree_->data().tree_id, static_text2_.id, 0 /* child_index */);
  TestPositionRange root_to_static2_tree_range(start->Clone(), end->Clone());
  EXPECT_EQ(text_up_to_text2_tree_start, root_to_static2_tree_range.GetText());
  TestPositionRange root_to_static2_tree_range_backward(std::move(end),
                                                        std::move(start));
  EXPECT_EQ(text_up_to_text2_tree_start,
            root_to_static2_tree_range_backward.GetText());
}

TEST_F(AXRangeTest, GetTextWithTextOffsets) {
  base::string16 most_text =
      BUTTON_TEXT.substr(2).append(TEXT_VALUE).substr(0, 15);
  // Create a range starting from the button object and ending two characters
  // before the end of the root.
  TestPositionInstance start = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, button_.id, 2 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_TRUE(start->IsTextPosition());
  TestPositionInstance end = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, static_text2_.id, 4 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_TRUE(end->IsTextPosition());
  TestPositionRange forward_range(start->Clone(), end->Clone());
  EXPECT_EQ(most_text, forward_range.GetText());
  TestPositionRange backward_range(std::move(end), std::move(start));
  EXPECT_EQ(most_text, backward_range.GetText());

  // root_ to static_text2_'s start with offsets
  base::string16 text_up_to_text2_tree_start =
      BUTTON_TEXT.substr(0).append(TEXT_VALUE).substr(0, 16);
  start = AXNodePosition::CreateTreePosition(tree_->data().tree_id, root_.id,
                                             0 /* child_index */);
  end = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, static_text2_.id, 3 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_TRUE(end->IsTextPosition());
  TestPositionRange root_to_static2_tree_range(start->Clone(), end->Clone());
  EXPECT_EQ(text_up_to_text2_tree_start, root_to_static2_tree_range.GetText());
  TestPositionRange root_to_static2_tree_range_backward(std::move(end),
                                                        std::move(start));
  EXPECT_EQ(text_up_to_text2_tree_start,
            root_to_static2_tree_range_backward.GetText());
}

TEST_F(AXRangeTest, GetTextWithEmptyRanges) {
  // empty string with non-leaf tree position
  base::string16 empty_string(base::ASCIIToUTF16(""));
  TestPositionInstance start = AXNodePosition::CreateTreePosition(
      tree_->data().tree_id, root_.id, 0 /* child_index */);
  TestPositionRange non_leaf_tree_range(start->Clone(), start->Clone());
  EXPECT_EQ(empty_string, non_leaf_tree_range.GetText());

  // empty string with leaf tree position
  start = AXNodePosition::CreateTreePosition(
      tree_->data().tree_id, inline_box1_.id, 0 /* child_index */);
  TestPositionRange leaf_empty_range(start->Clone(), start->Clone());
  EXPECT_EQ(empty_string, leaf_empty_range.GetText());

  // empty string with leaf text position and no offset
  start = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, inline_box1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  TestPositionRange leaf_text_no_offset(start->Clone(), start->Clone());
  EXPECT_EQ(empty_string, leaf_text_no_offset.GetText());

  // empty string with leaf text position with offset
  start = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, inline_box1_.id, 3 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  TestPositionRange leaf_text_offset(start->Clone(), start->Clone());
  EXPECT_EQ(empty_string, leaf_text_offset.GetText());

  // empty string with non-leaf text with no offset
  start = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, root_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  TestPositionRange non_leaf_text_no_offset(start->Clone(), start->Clone());
  EXPECT_EQ(empty_string, non_leaf_text_no_offset.GetText());

  // empty string with non-leaf text position with offset
  start = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, root_.id, 3 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  TestPositionRange non_leaf_text_offset(start->Clone(), start->Clone());
  EXPECT_EQ(empty_string, non_leaf_text_offset.GetText());

  // empty string with same position between two anchors, but different offsets
  TestPositionInstance after_end = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, line_break_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  TestPositionInstance before_start = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, static_text2_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);

  AXRange<AXPosition<AXNodePosition, AXNode>>
      same_position_different_anchors_forward(after_end->Clone(),
                                              before_start->Clone());
  EXPECT_EQ(empty_string, same_position_different_anchors_forward.GetText());
  AXRange<AXPosition<AXNodePosition, AXNode>>
      same_position_different_anchors_backward(before_start->Clone(),
                                               after_end->Clone());
  EXPECT_EQ(empty_string, same_position_different_anchors_backward.GetText());
}

TEST_F(AXRangeTest, GetScreenRects) {
  // Setting up ax ranges for testing.
  TestPositionInstance button = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, button_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);

  TestPositionInstance check_box = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, check_box_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);

  TestPositionInstance line1_start = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, inline_box1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  TestPositionInstance line1_second_char = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, inline_box1_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  TestPositionInstance line1_middle = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, inline_box1_.id, 3 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  TestPositionInstance line1_second_to_last_char =
      AXNodePosition::CreateTextPosition(tree_->data().tree_id, inline_box1_.id,
                                         5 /* text_offset */,
                                         ax::mojom::TextAffinity::kDownstream);
  TestPositionInstance line1_end = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, inline_box1_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);

  TestPositionInstance line2_start = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, inline_box2_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  TestPositionInstance line2_second_char = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, inline_box2_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  TestPositionInstance line2_middle = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, inline_box2_.id, 3 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  TestPositionInstance line2_second_to_last_char =
      AXNodePosition::CreateTextPosition(tree_->data().tree_id, inline_box2_.id,
                                         5 /* text_offset */,
                                         ax::mojom::TextAffinity::kDownstream);
  TestPositionInstance line2_end = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, inline_box2_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);

  // Since a button is not visible to the text representation, it spans an
  // empty anchor whose start and end positions are the same.
  TestPositionRange button_range(button->Clone(), button->Clone());
  std::vector<gfx::Rect> expected_screen_rects = {gfx::Rect(20, 20, 200, 30)};
  EXPECT_THAT(button_range.GetScreenRects(),
              testing::ContainerEq(expected_screen_rects));

  // Since a check box is not visible to the text representation, it spans an
  // empty anchor whose start and end positions are the same.
  TestPositionRange check_box_range(check_box->Clone(), check_box->Clone());
  expected_screen_rects = {gfx::Rect(20, 50, 200, 30)};
  EXPECT_THAT(check_box_range.GetScreenRects(),
              testing::ContainerEq(expected_screen_rects));

  // Retrieving bounding boxes of both, button and check box.
  TestPositionRange button_check_box_range(button->Clone(), check_box->Clone());
  expected_screen_rects = {gfx::Rect(20, 20, 200, 30),
                           gfx::Rect(20, 50, 200, 30)};
  EXPECT_THAT(button_check_box_range.GetScreenRects(),
              testing::ContainerEq(expected_screen_rects));

  // Retrieving bounding box of text line 1, its whole range.
  //  0 1 2 3 4 5
  // |L|i|n|e| |1|
  // |-----------|
  TestPositionRange line1_whole_range(line1_start->Clone(), line1_end->Clone());
  expected_screen_rects = {gfx::Rect(220, 20, 30, 30)};  //
  EXPECT_THAT(line1_whole_range.GetScreenRects(),
              testing::ContainerEq(expected_screen_rects));

  // Retrieving bounding box of text line 1, its first half range.
  //  0 1 2 3 4 5
  // |L|i|n|e| |1|
  // |-----|
  TestPositionRange line1_first_half_range(line1_start->Clone(),
                                           line1_middle->Clone());
  expected_screen_rects = {gfx::Rect(220, 20, 15, 30)};
  EXPECT_THAT(line1_first_half_range.GetScreenRects(),
              testing::ContainerEq(expected_screen_rects));

  // Retrieving bounding box of text line 1, its second half range.
  //  0 1 2 3 4 5
  // |L|i|n|e| |1|
  //       |-----|
  TestPositionRange line1_second_half_range(line1_middle->Clone(),
                                            line1_end->Clone());
  expected_screen_rects = {gfx::Rect(235, 20, 15, 30)};
  EXPECT_THAT(line1_second_half_range.GetScreenRects(),
              testing::ContainerEq(expected_screen_rects));

  // Retrieving bounding box of text line 1, its mid range.
  //  0 1 2 3 4 5
  // |L|i|n|e| |1|
  //   |-------|
  TestPositionRange line1_mid_range(line1_second_char->Clone(),
                                    line1_second_to_last_char->Clone());
  expected_screen_rects = {gfx::Rect(225, 20, 20, 30)};
  EXPECT_THAT(line1_mid_range.GetScreenRects(),
              testing::ContainerEq(expected_screen_rects));

  // Retrieving bounding box of text line 2, its whole range.
  //  0 1 2 3 4 5
  // |L|i|n|e| |2|
  // |-----------|
  TestPositionRange line2_whole_range(line2_start->Clone(), line2_end->Clone());
  expected_screen_rects = {gfx::Rect(220, 50, 42, 30)};
  EXPECT_THAT(line2_whole_range.GetScreenRects(),
              testing::ContainerEq(expected_screen_rects));

  // Retrieving bounding box of text line 2, its first half range.
  //  0 1 2 3 4 5
  // |L|i|n|e| |2|
  // |-----|
  TestPositionRange line2_first_half_range(line2_start->Clone(),
                                           line2_middle->Clone());
  expected_screen_rects = {gfx::Rect(220, 50, 21, 30)};
  EXPECT_THAT(line2_first_half_range.GetScreenRects(),
              testing::ContainerEq(expected_screen_rects));

  // Retrieving bounding box of text line 2, its second half range.
  //  0 1 2 3 4 5
  // |L|i|n|e| |2|
  //       |-----|
  TestPositionRange line2_second_half_range(line2_middle->Clone(),
                                            line2_end->Clone());
  expected_screen_rects = {gfx::Rect(241, 50, 21, 30)};
  EXPECT_THAT(line2_second_half_range.GetScreenRects(),
              testing::ContainerEq(expected_screen_rects));

  // Retrieving bounding box of text line 2, its mid range.
  //  0 1 2 3 4 5
  // |L|i|n|e| |2|
  //   |-------|
  TestPositionRange line2_mid_range(line2_second_char->Clone(),
                                    line2_second_to_last_char->Clone());
  expected_screen_rects = {gfx::Rect(227, 50, 28, 30)};
  EXPECT_THAT(line2_mid_range.GetScreenRects(),
              testing::ContainerEq(expected_screen_rects));

  // Retrieving bounding boxes of text line 1 and line 2, the entire range.
  // |L|i|n|e| |1|\n|L|i|n|e| |2|
  // |--------------------------|
  TestPositionRange line1_line2_whole_range(line1_start->Clone(),
                                            line2_end->Clone());
  expected_screen_rects = {gfx::Rect(220, 20, 30, 30),
                           gfx::Rect(220, 50, 42, 30)};
  EXPECT_THAT(line1_line2_whole_range.GetScreenRects(),
              testing::ContainerEq(expected_screen_rects));

  // Retrieving bounding boxes of the range that spans from the middle of text
  // line 1 to the middle of text line 2.
  // |L|i|n|e| |1|\n|L|i|n|e| |2|
  //       |--------------|
  TestPositionRange line1_line2_mid_range(line1_middle->Clone(),
                                          line2_middle->Clone());
  expected_screen_rects = {gfx::Rect(235, 20, 15, 30),
                           gfx::Rect(220, 50, 21, 30)};
  EXPECT_THAT(line1_line2_mid_range.GetScreenRects(),
              testing::ContainerEq(expected_screen_rects));

  // Retrieving bounding boxes of the range that spans from the check box
  // ("invisible" in the text representation) to the middle of text line 2.
  // |[Button][Check box]L|i|n|e| |1|\n|L|i|n|e| |2|
  //          |------------------------------|
  TestPositionRange check_box_line2_mid_range(check_box->Clone(),
                                              line2_middle->Clone());
  expected_screen_rects = {gfx::Rect(20, 50, 200, 30),
                           gfx::Rect(220, 20, 30, 30),
                           gfx::Rect(220, 50, 21, 30)};
  EXPECT_THAT(check_box_line2_mid_range.GetScreenRects(),
              testing::ContainerEq(expected_screen_rects));

  // Retrieving bounding boxes of the range spanning the entire document.
  // |[Button][Check box]L|i|n|e| |1|\n|L|i|n|e| |2|
  // |---------------------------------------------|
  TestPositionRange entire_document_range(button->Clone(), line2_end->Clone());
  expected_screen_rects = {
      gfx::Rect(20, 20, 200, 30), gfx::Rect(20, 50, 200, 30),
      gfx::Rect(220, 20, 30, 30), gfx::Rect(220, 50, 42, 30)};
  EXPECT_THAT(entire_document_range.GetScreenRects(),
              testing::ContainerEq(expected_screen_rects));
}

TEST_F(AXRangeTest, GetAnchors) {
  TestPositionInstance button_start = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, button_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  TestPositionInstance button_middle = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, button_.id, 3 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  TestPositionInstance button_end = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, button_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);

  // Since a check box is not visible to the text representation, it spans an
  // empty anchor whose start and end positions are the same.
  TestPositionInstance check_box = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, check_box_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);

  TestPositionInstance line1_start = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, inline_box1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  TestPositionInstance line1_middle = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, inline_box1_.id, 3 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  TestPositionInstance line1_end = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, inline_box1_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);

  TestPositionInstance line_break_start = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, line_break_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  TestPositionInstance line_break_end = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, line_break_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);

  TestPositionInstance line2_start = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, inline_box2_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  TestPositionInstance line2_middle = AXNodePosition::CreateTextPosition(
      tree_->data().tree_id, inline_box2_.id, 3 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);

  TestPositionRange whole_anchor_range(button_start->Clone(),
                                       button_end->Clone());
  std::vector<TestPositionRange> range_anchors =
      whole_anchor_range.GetAnchors();

  std::vector<TestPositionRange> expected_anchors;
  expected_anchors.emplace_back(button_start->Clone(), button_end->Clone());
  EXPECT_RANGE_VECTOR_EQ(expected_anchors, range_anchors);

  TestPositionRange non_null_degenerate_range(check_box->Clone(),
                                              check_box->Clone());
  range_anchors = non_null_degenerate_range.GetAnchors();

  expected_anchors.clear();
  expected_anchors.emplace_back(check_box->Clone(), check_box->Clone());
  EXPECT_RANGE_VECTOR_EQ(expected_anchors, range_anchors);

  TestPositionRange across_anchors_range(button_middle->Clone(),
                                         line1_middle->Clone());
  range_anchors = across_anchors_range.GetAnchors();

  expected_anchors.clear();
  expected_anchors.emplace_back(button_middle->Clone(), button_end->Clone());
  expected_anchors.emplace_back(check_box->Clone(), check_box->Clone());
  expected_anchors.emplace_back(line1_start->Clone(), line1_middle->Clone());
  EXPECT_RANGE_VECTOR_EQ(expected_anchors, range_anchors);

  TestPositionRange across_anchors_backward_range(line1_middle->Clone(),
                                                  button_middle->Clone());
  range_anchors = across_anchors_backward_range.GetAnchors();
  EXPECT_RANGE_VECTOR_EQ(expected_anchors, range_anchors);

  TestPositionRange starting_at_end_position_range(line1_end->Clone(),
                                                   line2_middle->Clone());
  range_anchors = starting_at_end_position_range.GetAnchors();

  expected_anchors.clear();
  expected_anchors.emplace_back(line1_end->Clone(), line1_end->Clone());
  expected_anchors.emplace_back(line_break_start->Clone(),
                                line_break_end->Clone());
  expected_anchors.emplace_back(line2_start->Clone(), line2_middle->Clone());
  EXPECT_RANGE_VECTOR_EQ(expected_anchors, range_anchors);

  TestPositionRange ending_at_start_position_range(line1_middle->Clone(),
                                                   line2_start->Clone());
  range_anchors = ending_at_start_position_range.GetAnchors();

  expected_anchors.clear();
  expected_anchors.emplace_back(line1_middle->Clone(), line1_end->Clone());
  expected_anchors.emplace_back(line_break_start->Clone(),
                                line_break_end->Clone());
  expected_anchors.emplace_back(line2_start->Clone(), line2_start->Clone());
  EXPECT_RANGE_VECTOR_EQ(expected_anchors, range_anchors);
}

}  // namespace ui
