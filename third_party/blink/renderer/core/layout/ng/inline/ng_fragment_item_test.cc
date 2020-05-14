// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

using testing::ElementsAre;

namespace blink {

// We enable LayoutNGFragmentTraversal here, so that we get the "first/last for
// node" bits set as appropriate.
class NGFragmentItemTest : public NGLayoutTest,
                           ScopedLayoutNGFragmentItemForTest,
                           ScopedLayoutNGFragmentTraversalForTest {
 public:
  NGFragmentItemTest()
      : ScopedLayoutNGFragmentItemForTest(true),
        ScopedLayoutNGFragmentTraversalForTest(true) {}

  LayoutBlockFlow* GetLayoutBlockFlowByElementId(const char* id) {
    return To<LayoutBlockFlow>(GetLayoutObjectByElementId(id));
  }

  const NGFragmentItems* GetFragmentItemsByElementId(const char* id) {
    const auto* block_flow =
        To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));
    return block_flow->FragmentItems();
  }

  Vector<NGInlineCursorPosition> GetLines(NGInlineCursor* cursor) {
    Vector<NGInlineCursorPosition> lines;
    for (cursor->MoveToFirstLine(); *cursor; cursor->MoveToNextLine())
      lines.push_back(cursor->Current());
    return lines;
  }

  wtf_size_t IndexOf(const Vector<NGInlineCursorPosition>& items,
                     const NGFragmentItem* target) {
    wtf_size_t index = 0;
    for (const auto& item : items) {
      if (item.Item() == target)
        return index;
      ++index;
    }
    return kNotFound;
  }

  void TestFirstDirtyLineIndex(const char* id, wtf_size_t expected_index) {
    LayoutBlockFlow* block_flow = GetLayoutBlockFlowByElementId(id);
    const NGFragmentItems* items = block_flow->FragmentItems();
    items->DirtyLinesFromNeedsLayout(block_flow);
    const NGFragmentItem* end_reusable_item = items->EndOfReusableItems();

    NGInlineCursor cursor(*items);
    const auto lines = GetLines(&cursor);
    EXPECT_EQ(IndexOf(lines, end_reusable_item), expected_index);
  }

  Vector<const NGFragmentItem*> ItemsForAsVector(
      const LayoutObject& layout_object) {
    Vector<const NGFragmentItem*> list;
    NGInlineCursor cursor;
    for (cursor.MoveTo(layout_object); cursor;
         cursor.MoveToNextForSameLayoutObject()) {
      DCHECK(cursor.Current().Item());
      const NGFragmentItem& item = *cursor.Current().Item();
      EXPECT_EQ(item.GetLayoutObject(), &layout_object);
      list.push_back(&item);
    }
    return list;
  }
};

TEST_F(NGFragmentItemTest, BasicText) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0;
      font-family: Ahem;
      font-size: 10px;
      line-height: 1;
    }
    div {
      width: 10ch;
    }
    </style>
    <div id="container">
      1234567 98765
    </div>
  )HTML");

  LayoutBlockFlow* container =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));
  LayoutText* layout_text = ToLayoutText(container->FirstChild());
  const NGPhysicalBoxFragment* box = container->CurrentFragment();
  EXPECT_NE(box, nullptr);
  const NGFragmentItems* items = box->Items();
  EXPECT_NE(items, nullptr);
  EXPECT_EQ(items->Items().size(), 4u);

  // The text node wraps, produces two fragments.
  Vector<const NGFragmentItem*> items_for_text = ItemsForAsVector(*layout_text);
  EXPECT_EQ(items_for_text.size(), 2u);

  const NGFragmentItem& text1 = *items_for_text[0];
  EXPECT_EQ(text1.Type(), NGFragmentItem::kText);
  EXPECT_EQ(text1.GetLayoutObject(), layout_text);
  EXPECT_EQ(text1.OffsetInContainerBlock(), PhysicalOffset());
  EXPECT_TRUE(text1.IsFirstForNode());
  EXPECT_FALSE(text1.IsLastForNode());

  const NGFragmentItem& text2 = *items_for_text[1];
  EXPECT_EQ(text2.Type(), NGFragmentItem::kText);
  EXPECT_EQ(text2.GetLayoutObject(), layout_text);
  EXPECT_EQ(text2.OffsetInContainerBlock(), PhysicalOffset(0, 10));
  EXPECT_FALSE(text2.IsFirstForNode());
  EXPECT_TRUE(text2.IsLastForNode());

  EXPECT_EQ(IntRect(0, 0, 70, 20),
            layout_text->FragmentsVisualRectBoundingBox());
}

TEST_F(NGFragmentItemTest, RtlText) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    div {
      font-family: Ahem;
      font-size: 10px;
      width: 10ch;
      direction: rtl;
    }
    </style>
    <div id="container">
      <span id="span" style="background:hotpink;">
        11111. 22222.
      </span>
    </div>
  )HTML");

  LayoutBlockFlow* container =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));
  LayoutObject* span = GetLayoutObjectByElementId("span");
  LayoutText* layout_text = ToLayoutText(span->SlowFirstChild());
  const NGPhysicalBoxFragment* box = container->CurrentFragment();
  EXPECT_NE(box, nullptr);
  const NGFragmentItems* items = box->Items();
  EXPECT_NE(items, nullptr);
  EXPECT_EQ(items->Items().size(), 8u);

  Vector<const NGFragmentItem*> items_for_span = ItemsForAsVector(*span);
  EXPECT_EQ(items_for_span.size(), 2u);
  const NGFragmentItem* item = items_for_span[0];
  EXPECT_TRUE(item->IsFirstForNode());
  EXPECT_FALSE(item->IsLastForNode());

  item = items_for_span[1];
  EXPECT_FALSE(item->IsFirstForNode());
  EXPECT_TRUE(item->IsLastForNode());

  Vector<const NGFragmentItem*> items_for_text = ItemsForAsVector(*layout_text);
  EXPECT_EQ(items_for_text.size(), 4u);

  item = items_for_text[0];
  EXPECT_EQ(item->Text(*items).ToString(), String("."));
  EXPECT_TRUE(item->IsFirstForNode());
  EXPECT_FALSE(item->IsLastForNode());

  item = items_for_text[1];
  EXPECT_EQ(item->Text(*items).ToString(), String("11111"));
  EXPECT_FALSE(item->IsFirstForNode());
  EXPECT_FALSE(item->IsLastForNode());

  item = items_for_text[2];
  EXPECT_EQ(item->Text(*items).ToString(), String("."));
  EXPECT_FALSE(item->IsFirstForNode());
  EXPECT_FALSE(item->IsLastForNode());

  item = items_for_text[3];
  EXPECT_EQ(item->Text(*items).ToString(), String("22222"));
  EXPECT_FALSE(item->IsFirstForNode());
  EXPECT_TRUE(item->IsLastForNode());
}

TEST_F(NGFragmentItemTest, BasicInlineBox) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0;
      font-family: Ahem;
      font-size: 10px;
      line-height: 1;
    }
    #container {
      width: 10ch;
    }
    #span1, #span2 {
      background: gray;
    }
    </style>
    <div id="container">
      000
      <span id="span1">1234 5678</span>
      999
      <span id="span2">12345678</span>
    </div>
  )HTML");

  // "span1" wraps, produces two fragments.
  const LayoutObject* span1 = GetLayoutObjectByElementId("span1");
  ASSERT_NE(span1, nullptr);
  Vector<const NGFragmentItem*> items_for_span1 = ItemsForAsVector(*span1);
  EXPECT_EQ(items_for_span1.size(), 2u);
  EXPECT_EQ(IntRect(0, 0, 80, 20), span1->FragmentsVisualRectBoundingBox());
  EXPECT_TRUE(items_for_span1[0]->IsFirstForNode());
  EXPECT_FALSE(items_for_span1[0]->IsLastForNode());
  EXPECT_FALSE(items_for_span1[1]->IsFirstForNode());
  EXPECT_TRUE(items_for_span1[1]->IsLastForNode());

  // "span2" doesn't wrap, produces only one fragment.
  const LayoutObject* span2 = GetLayoutObjectByElementId("span2");
  ASSERT_NE(span2, nullptr);
  Vector<const NGFragmentItem*> items_for_span2 = ItemsForAsVector(*span2);
  EXPECT_EQ(items_for_span2.size(), 1u);
  EXPECT_EQ(IntRect(0, 20, 80, 10), span2->FragmentsVisualRectBoundingBox());
  EXPECT_TRUE(items_for_span2[0]->IsFirstForNode());
  EXPECT_TRUE(items_for_span2[0]->IsLastForNode());
}

// Same as |BasicInlineBox| but `<span>`s do not have background.
// They will not produce fragment items, but all operations should work the
// same.
TEST_F(NGFragmentItemTest, CulledInlineBox) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0;
      font-family: Ahem;
      font-size: 10px;
      line-height: 1;
    }
    #container {
      width: 10ch;
    }
    </style>
    <div id="container">
      000
      <span id="span1">1234 5678</span>
      999
      <span id="span2">12345678</span>
    </div>
  )HTML");

  // "span1" wraps, produces two fragments.
  const LayoutObject* span1 = GetLayoutObjectByElementId("span1");
  ASSERT_NE(span1, nullptr);
  Vector<const NGFragmentItem*> items_for_span1 = ItemsForAsVector(*span1);
  EXPECT_EQ(items_for_span1.size(), 0u);
  EXPECT_EQ(IntRect(0, 0, 80, 20), span1->AbsoluteBoundingBoxRect());

  // "span2" doesn't wrap, produces only one fragment.
  const LayoutObject* span2 = GetLayoutObjectByElementId("span2");
  ASSERT_NE(span2, nullptr);
  Vector<const NGFragmentItem*> items_for_span2 = ItemsForAsVector(*span2);
  EXPECT_EQ(items_for_span2.size(), 0u);
  EXPECT_EQ(IntRect(0, 20, 80, 10), span2->AbsoluteBoundingBoxRect());
}

TEST_F(NGFragmentItemTest, MarkLineBoxesDirtyByRemoveChildAfterForcedBreak) {
  SetBodyInnerHTML(R"HTML(
    <div id=container>
      line 1<br>
      <b id=target>line 2</b><br>
      line 3<br>
    </div>
  )HTML");
  Element& target = *GetDocument().getElementById("target");
  target.remove();
  // TODO(kojii): This can be more optimized.
  TestFirstDirtyLineIndex("container", 0);
}

TEST_F(NGFragmentItemTest, MarkLineBoxesDirtyByRemoveForcedBreak) {
  SetBodyInnerHTML(R"HTML(
    <div id=container>
      line 1<br>
      line 2<br id=target>
      line 3<br>
    </div>"
  )HTML");
  Element& target = *GetDocument().getElementById("target");
  target.remove();
  // TODO(kojii): This can be more optimized.
  TestFirstDirtyLineIndex("container", 0);
}

TEST_F(NGFragmentItemTest, MarkLineBoxesDirtyByRemoveSpanWithForcedBreak) {
  SetBodyInnerHTML(R"HTML(
    <div id=container>
      line 1<br>
      line 2<span id=target><br>
      </span>line 3<br>
    </div>
  )HTML");
  // |target| is a culled inline box. There is no fragment in fragment tree.
  Element& target = *GetDocument().getElementById("target");
  target.remove();
  // TODO(kojii): This can be more optimized.
  TestFirstDirtyLineIndex("container", 0);
}

TEST_F(NGFragmentItemTest, MarkLineBoxesDirtyByInsertAtStart) {
  SetBodyInnerHTML(R"HTML(
    <div id=container>
      line 1<br>
      <b id=target>line 2</b><br>
      line 3<br>
    </div>
  )HTML");
  Element& target = *GetDocument().getElementById("target");
  target.parentNode()->insertBefore(Text::Create(GetDocument(), "XYZ"),
                                    &target);
  GetDocument().UpdateStyleAndLayoutTree();
  // TODO(kojii): This can be more optimized.
  TestFirstDirtyLineIndex("container", 0);
}

TEST_F(NGFragmentItemTest, MarkLineBoxesDirtyByInsertAtLast) {
  SetBodyInnerHTML(R"HTML(
    <div id=container>
      line 1<br>
      <b id=target>line 2</b><br>
      line 3<br>
    </div>
  )HTML");
  Element& target = *GetDocument().getElementById("target");
  target.parentNode()->appendChild(Text::Create(GetDocument(), "XYZ"));
  GetDocument().UpdateStyleAndLayoutTree();
  TestFirstDirtyLineIndex("container", 1);
}

TEST_F(NGFragmentItemTest, MarkLineBoxesDirtyByInsertAtMiddle) {
  SetBodyInnerHTML(R"HTML(
    <div id=container>
      line 1<br>
      <b id=target>line 2</b><br>
      line 3<br>
    </div>
  )HTML");
  Element& target = *GetDocument().getElementById("target");
  target.parentNode()->insertBefore(Text::Create(GetDocument(), "XYZ"),
                                    target.nextSibling());
  GetDocument().UpdateStyleAndLayoutTree();
  // TODO(kojii): This can be more optimized.
  TestFirstDirtyLineIndex("container", 0);
}

TEST_F(NGFragmentItemTest, MarkLineBoxesDirtyByTextSetData) {
  SetBodyInnerHTML(R"HTML(
    <div id=container>
      line 1<br>
      <b id=target>line 2</b><br>
      line 3<br>
    </div>
  )HTML");
  Element& target = *GetDocument().getElementById("target");
  To<Text>(*target.firstChild()).setData("abc");
  // TODO(kojii): This can be more optimized.
  TestFirstDirtyLineIndex("container", 0);
}

TEST_F(NGFragmentItemTest, MarkLineBoxesDirtyWrappedLine) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #container {
      font-size: 10px;
      width: 10ch;
    }
    </style>
    <div id=container>
      1234567
      123456<span id="target">7</span>
    </div>
  )HTML");
  Element& target = *GetDocument().getElementById("target");
  target.remove();
  // TODO(kojii): This can be more optimized.
  TestFirstDirtyLineIndex("container", 0);
}

TEST_F(NGFragmentItemTest, MarkLineBoxesDirtyInsideInlineBlock) {
  SetBodyInnerHTML(R"HTML(
    <div id=container>
      <div id="inline-block" style="display: inline-block">
        <span id="target">DELETE ME</span>
      </div>
    </div>
  )HTML");
  Element& target = *GetDocument().getElementById("target");
  target.remove();
  TestFirstDirtyLineIndex("container", 0);
}

}  // namespace blink
