// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_fragment_child_iterator.h"
#include "third_party/blink/renderer/core/layout/ng/ng_base_layout_algorithm_test.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {
namespace {

class NGFragmentChildIteratorTest
    : public NGBaseLayoutAlgorithmTest,
      private ScopedLayoutNGBlockFragmentationForTest,
      private ScopedLayoutNGFragmentItemForTest {
 protected:
  NGFragmentChildIteratorTest()
      : ScopedLayoutNGBlockFragmentationForTest(true),
        ScopedLayoutNGFragmentItemForTest(true) {}

  scoped_refptr<const NGPhysicalBoxFragment> RunBlockLayoutAlgorithm(
      Element* element) {
    NGBlockNode container(ToLayoutBox(element->GetLayoutObject()));
    NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
        WritingMode::kHorizontalTb, TextDirection::kLtr,
        LogicalSize(LayoutUnit(1000), kIndefiniteSize));
    return NGBaseLayoutAlgorithmTest::RunBlockLayoutAlgorithm(container, space);
  }
};

TEST_F(NGFragmentChildIteratorTest, Basic) {
  SetBodyInnerHTML(R"HTML(
    <div id="container">
      <div id="child1">
        <div id="grandchild"></div>
      </div>
      <div id="child2"></div>
    </div>
  )HTML");

  const LayoutObject* child1 = GetLayoutObjectByElementId("child1");
  const LayoutObject* child2 = GetLayoutObjectByElementId("child2");
  const LayoutObject* grandchild = GetLayoutObjectByElementId("grandchild");

  scoped_refptr<const NGPhysicalBoxFragment> container =
      RunBlockLayoutAlgorithm(GetElementById("container"));
  NGFragmentChildIterator iterator1(*container.get());
  EXPECT_FALSE(iterator1.IsAtEnd());

  const NGPhysicalBoxFragment* fragment = iterator1->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), child1);
  EXPECT_FALSE(iterator1.IsAtEnd());

  NGFragmentChildIterator iterator2 = iterator1.Descend();
  EXPECT_FALSE(iterator2.IsAtEnd());
  fragment = iterator2->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), grandchild);
  EXPECT_FALSE(iterator2.IsAtEnd());
  EXPECT_FALSE(iterator2.Advance());
  EXPECT_TRUE(iterator2.IsAtEnd());

  EXPECT_TRUE(iterator1.Advance());
  fragment = iterator1->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), child2);
  EXPECT_FALSE(iterator1.IsAtEnd());

  // #child2 has no children.
  EXPECT_TRUE(iterator1.Descend().IsAtEnd());

  // No more children left.
  EXPECT_FALSE(iterator1.Advance());
  EXPECT_TRUE(iterator1.IsAtEnd());
}

TEST_F(NGFragmentChildIteratorTest, BasicInline) {
  SetBodyInnerHTML(R"HTML(
    <div id="container">
      xxx
      <span id="span1" style="border:solid;">
        <div id="float1" style="float:left;"></div>
        xxx
      </span>
      xxx
    </div>
  )HTML");

  const LayoutObject* span1 = GetLayoutObjectByElementId("span1");
  const LayoutObject* float1 = GetLayoutObjectByElementId("float1");

  scoped_refptr<const NGPhysicalBoxFragment> container =
      RunBlockLayoutAlgorithm(GetElementById("container"));
  NGFragmentChildIterator iterator1(*container.get());

  EXPECT_FALSE(iterator1->BoxFragment());
  const NGFragmentItem* fragment_item = iterator1->FragmentItem();
  ASSERT_TRUE(fragment_item);
  EXPECT_EQ(fragment_item->Type(), NGFragmentItem::kLine);

  // Descend into the line box.
  NGFragmentChildIterator iterator2 = iterator1.Descend();
  fragment_item = iterator2->FragmentItem();
  ASSERT_TRUE(fragment_item);
  EXPECT_TRUE(fragment_item->IsText());

  EXPECT_TRUE(iterator2.Advance());
  const NGPhysicalBoxFragment* fragment = iterator2->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), span1);

  // Descend into children of #span1.
  NGFragmentChildIterator iterator3 = iterator2.Descend();
  fragment = iterator3->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), float1);

  EXPECT_TRUE(iterator3.Advance());
  fragment_item = iterator3->FragmentItem();
  ASSERT_TRUE(fragment_item);
  EXPECT_TRUE(fragment_item->IsText());
  EXPECT_FALSE(iterator3.Advance());

  // Continue with siblings of #span1.
  EXPECT_TRUE(iterator2.Advance());
  fragment_item = iterator2->FragmentItem();
  ASSERT_TRUE(fragment_item);
  EXPECT_TRUE(fragment_item->IsText());

  EXPECT_FALSE(iterator2.Advance());
  EXPECT_FALSE(iterator1.Advance());
}

TEST_F(NGFragmentChildIteratorTest, InlineBlock) {
  SetBodyInnerHTML(R"HTML(
    <div id="container">
      xxx
      <span id="inlineblock">
        <div id="float1" style="float:left;"></div>
      </span>
      xxx
    </div>
  )HTML");

  const LayoutObject* inlineblock = GetLayoutObjectByElementId("inlineblock");
  const LayoutObject* float1 = GetLayoutObjectByElementId("float1");

  scoped_refptr<const NGPhysicalBoxFragment> container =
      RunBlockLayoutAlgorithm(GetElementById("container"));
  NGFragmentChildIterator iterator1(*container.get());

  EXPECT_FALSE(iterator1->BoxFragment());
  const NGFragmentItem* fragment_item = iterator1->FragmentItem();
  ASSERT_TRUE(fragment_item);
  EXPECT_EQ(fragment_item->Type(), NGFragmentItem::kLine);

  // Descend into the line box.
  NGFragmentChildIterator iterator2 = iterator1.Descend();
  fragment_item = iterator2->FragmentItem();
  ASSERT_TRUE(fragment_item);
  EXPECT_TRUE(fragment_item->IsText());

  EXPECT_TRUE(iterator2.Advance());
  const NGPhysicalBoxFragment* fragment = iterator2->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), inlineblock);

  // Descend into children of #inlineblock.
  NGFragmentChildIterator iterator3 = iterator2.Descend();
  fragment = iterator3->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), float1);
  EXPECT_FALSE(iterator3.Advance());

  // Continue with siblings of #inlineblock.
  EXPECT_TRUE(iterator2.Advance());
  fragment_item = iterator2->FragmentItem();
  ASSERT_TRUE(fragment_item);
  EXPECT_TRUE(fragment_item->IsText());

  EXPECT_FALSE(iterator2.Advance());
  EXPECT_FALSE(iterator1.Advance());
}

TEST_F(NGFragmentChildIteratorTest, FloatsInInline) {
  SetBodyInnerHTML(R"HTML(
    <div id="container">
      <span id="span1" style="border:solid;">
        <div id="float1" style="float:left;">
          <div id="child"></div>
        </div>
      </span>
    </div>
  )HTML");

  const LayoutObject* span1 = GetLayoutObjectByElementId("span1");
  const LayoutObject* float1 = GetLayoutObjectByElementId("float1");
  const LayoutObject* child = GetLayoutObjectByElementId("child");

  scoped_refptr<const NGPhysicalBoxFragment> container =
      RunBlockLayoutAlgorithm(GetElementById("container"));
  NGFragmentChildIterator iterator1(*container.get());

  const NGPhysicalBoxFragment* fragment = iterator1->BoxFragment();
  EXPECT_FALSE(fragment);
  const NGFragmentItem* item = iterator1->FragmentItem();
  ASSERT_TRUE(item);
  EXPECT_EQ(item->Type(), NGFragmentItem::kLine);

  // Descend into the line box.
  NGFragmentChildIterator iterator2 = iterator1.Descend();
  fragment = iterator2->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), span1);

  // Descend into children of #span1.
  NGFragmentChildIterator iterator3 = iterator2.Descend();
  fragment = iterator3->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), float1);

  // Descend into children of #float1.
  NGFragmentChildIterator iterator4 = iterator3.Descend();
  fragment = iterator4->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), child);

  EXPECT_FALSE(iterator4.Advance());
  EXPECT_FALSE(iterator3.Advance());
  EXPECT_FALSE(iterator2.Advance());
  EXPECT_FALSE(iterator1.Advance());
}

TEST_F(NGFragmentChildIteratorTest, AbsposAndLine) {
  SetBodyInnerHTML(R"HTML(
    <div id="container" style="position:relative;">
      <div id="abspos" style="position:absolute;"></div>
      xxx
    </div>
  )HTML");

  const LayoutObject* abspos = GetLayoutObjectByElementId("abspos");

  scoped_refptr<const NGPhysicalBoxFragment> container =
      RunBlockLayoutAlgorithm(GetElementById("container"));
  NGFragmentChildIterator iterator1(*container.get());

  const NGPhysicalBoxFragment* fragment = iterator1->BoxFragment();
  EXPECT_FALSE(fragment);
  const NGFragmentItem* item = iterator1->FragmentItem();
  ASSERT_TRUE(item);
  EXPECT_EQ(item->Type(), NGFragmentItem::kLine);

  // Descend into the line box.
  NGFragmentChildIterator iterator2 = iterator1.Descend();

  fragment = iterator2->BoxFragment();
  EXPECT_FALSE(fragment);
  item = iterator2->FragmentItem();
  ASSERT_TRUE(item);
  EXPECT_TRUE(item->IsText());
  EXPECT_FALSE(iterator2.Advance());

  // The abspos is a sibling of the line box.
  EXPECT_TRUE(iterator1.Advance());
  fragment = iterator1->BoxFragment();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(fragment->GetLayoutObject(), abspos);
  EXPECT_FALSE(iterator1.Advance());
}

}  // anonymous namespace
}  // namespace blink
