// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node_data.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"

namespace blink {

using ::testing::ElementsAre;

class NGInlineCursorTest : public NGLayoutTest,
                           private ScopedLayoutNGFragmentItemForTest,
                           public testing::WithParamInterface<bool> {
 public:
  NGInlineCursorTest() : ScopedLayoutNGFragmentItemForTest(GetParam()) {}

 protected:
  NGInlineCursor SetupCursor(const String& html) {
    SetBodyInnerHTML(html);
    const LayoutBlockFlow& block_flow =
        *To<LayoutBlockFlow>(GetLayoutObjectByElementId("root"));
    return NGInlineCursor(block_flow);
  }

  Vector<String> ToDebugStringList(const NGInlineCursor& start) {
    Vector<String> list;
    for (NGInlineCursor cursor(start); cursor; cursor.MoveToNext())
      list.push_back(ToDebugString(cursor));
    return list;
  }

  String ToDebugString(const NGInlineCursor& cursor) {
    if (cursor.IsLineBox())
      return "#linebox";

    const String text_content =
        cursor.GetLayoutBlockFlow()->GetNGInlineNodeData()->text_content;
    if (const LayoutObject* layout_object = cursor.CurrentLayoutObject()) {
      if (layout_object->IsText()) {
        return text_content
            .Substring(
                cursor.CurrentTextStartOffset(),
                cursor.CurrentTextEndOffset() - cursor.CurrentTextStartOffset())
            .StripWhiteSpace();
      }
      if (const Element* element =
              DynamicTo<Element>(layout_object->GetNode())) {
        if (const AtomicString& id = element->GetIdAttribute())
          return "#" + id;
      }

      return layout_object->DebugName();
    }

    return "#null";
  }
};

INSTANTIATE_TEST_SUITE_P(NGInlineCursorTest,
                         NGInlineCursorTest,
                         testing::Bool());

TEST_P(NGInlineCursorTest, ContainingLine) {
  // TDOO(yosin): Remove <style> once NGFragmentItem don't do culled inline.
  InsertStyleElement("a, b { background: gray; }");
  NGInlineCursor cursor =
      SetupCursor("<div id=root>abc<a id=target>def</a>ghi<br>xyz</div>");
  const LayoutBlockFlow& block_flow = *cursor.GetLayoutBlockFlow();
  NGInlineCursor line1(cursor);
  ASSERT_TRUE(line1.IsLineBox());

  NGInlineCursor line2(line1);
  line2.MoveToNextSibling();
  ASSERT_TRUE(line2.IsLineBox());

  cursor.MoveTo(*block_flow.FirstChild());
  cursor.MoveToContainingLine();
  EXPECT_EQ(line1, cursor);

  const LayoutInline& target =
      ToLayoutInline(*GetLayoutObjectByElementId("target"));
  cursor.MoveTo(target);
  cursor.MoveToContainingLine();
  EXPECT_EQ(line1, cursor);

  cursor.MoveTo(*target.FirstChild());
  cursor.MoveToContainingLine();
  EXPECT_EQ(line1, cursor);

  cursor.MoveTo(*block_flow.LastChild());
  cursor.MoveToContainingLine();
  EXPECT_EQ(line2, cursor);
}

TEST_P(NGInlineCursorTest, CulledInline) {
  NGInlineCursor cursor =
      SetupCursor("<div id=root><a><b>abc</b><br><i>xyz</i></a></div>");
  const LayoutInline& layout_inline =
      ToLayoutInline(*cursor.GetLayoutBlockFlow()->FirstChild());
  cursor.MoveTo(layout_inline);
  Vector<String> list;
  while (cursor) {
    list.push_back(ToDebugString(cursor));
    cursor.MoveToNextForSameLayoutObject();
  }
  EXPECT_THAT(list, ElementsAre("abc", "", "xyz"));
}

TEST_P(NGInlineCursorTest, FirstChild) {
  // TDOO(yosin): Remove <style> once NGFragmentItem don't do culled inline.
  InsertStyleElement("a, b { background: gray; }");
  NGInlineCursor cursor =
      SetupCursor("<div id=root>abc<a>DEF<b>GHI</b></a>xyz</div>");
  cursor.MoveToFirstChild();
  EXPECT_EQ("abc", ToDebugString(cursor));
  EXPECT_FALSE(cursor.TryToMoveToFirstChild());
}

TEST_P(NGInlineCursorTest, FirstChild2) {
  // TDOO(yosin): Remove <style> once NGFragmentItem don't do culled inline.
  InsertStyleElement("a, b { background: gray; }");
  NGInlineCursor cursor = SetupCursor(
      "<div id=root><b id=first>abc</b><a>DEF<b>GHI</b></a><a "
      "id=last>xyz</a></div>");
  cursor.MoveToFirstChild();
  EXPECT_EQ("#first", ToDebugString(cursor));
  cursor.MoveToFirstChild();
  EXPECT_EQ("abc", ToDebugString(cursor));
  EXPECT_FALSE(cursor.TryToMoveToFirstChild());
}

TEST_P(NGInlineCursorTest, FirstLastLogicalLeafInSimpleText) {
  // TDOO(yosin): Remove <style> once NGFragmentItem don't do culled inline.
  InsertStyleElement("b { background: gray; }");
  NGInlineCursor cursor =
      SetupCursor("<div id=root><b>first</b><b>middle</b><b>last</b></div>");

  NGInlineCursor first_logical_leaf(cursor);
  first_logical_leaf.MoveToFirstLogicalLeaf();
  EXPECT_EQ("first", ToDebugString(first_logical_leaf));

  NGInlineCursor last_logical_leaf(cursor);
  last_logical_leaf.MoveToLastLogicalLeaf();
  EXPECT_EQ("last", ToDebugString(last_logical_leaf));
}

TEST_P(NGInlineCursorTest, FirstLastLogicalLeafInRtlText) {
  // TDOO(yosin): Remove <style> once NGFragmentItem don't do culled inline.
  InsertStyleElement("b { background: gray; }");
  NGInlineCursor cursor = SetupCursor(
      "<bdo id=root dir=rtl style=display:block>"
      "<b>first</b><b>middle</b><b>last</b>"
      "</bdo>");

  NGInlineCursor first_logical_leaf(cursor);
  first_logical_leaf.MoveToFirstLogicalLeaf();
  EXPECT_EQ("first", ToDebugString(first_logical_leaf));

  NGInlineCursor last_logical_leaf(cursor);
  last_logical_leaf.MoveToLastLogicalLeaf();
  EXPECT_EQ("last", ToDebugString(last_logical_leaf));
}

TEST_P(NGInlineCursorTest, FirstLastLogicalLeafInTextAsDeepDescendants) {
  // TDOO(yosin): Remove <style> once NGFragmentItem don't do culled inline.
  InsertStyleElement("b { background: gray; }");
  NGInlineCursor cursor = SetupCursor(
      "<div id=root>"
      "<b><b>first</b>ABC</b>"
      "<b>middle</b>"
      "<b>DEF<b>last</b></b>"
      "</div>");

  NGInlineCursor first_logical_leaf(cursor);
  first_logical_leaf.MoveToFirstLogicalLeaf();
  EXPECT_EQ("first", ToDebugString(first_logical_leaf));

  NGInlineCursor last_logical_leaf(cursor);
  last_logical_leaf.MoveToLastLogicalLeaf();
  EXPECT_EQ("last", ToDebugString(last_logical_leaf));
}

TEST_P(NGInlineCursorTest, FirstLastLogicalLeafWithInlineBlock) {
  InsertStyleElement("b { display: inline-block; }");
  NGInlineCursor cursor = SetupCursor(
      "<div id=root>"
      "<b id=first>first</b>middle<b id=last>last</b>"
      "</div>");

  NGInlineCursor first_logical_leaf(cursor);
  first_logical_leaf.MoveToFirstLogicalLeaf();
  EXPECT_EQ("#first", ToDebugString(first_logical_leaf))
      << "stop at inline-block";

  NGInlineCursor last_logical_leaf(cursor);
  last_logical_leaf.MoveToLastLogicalLeaf();
  EXPECT_EQ("#last", ToDebugString(last_logical_leaf))
      << "stop at inline-block";
}

TEST_P(NGInlineCursorTest, FirstLastLogicalLeafWithImages) {
  NGInlineCursor cursor =
      SetupCursor("<div id=root><img id=first>middle<img id=last></div>");

  NGInlineCursor first_logical_leaf(cursor);
  first_logical_leaf.MoveToFirstLogicalLeaf();
  EXPECT_EQ("#first", ToDebugString(first_logical_leaf));

  NGInlineCursor last_logical_leaf(cursor);
  last_logical_leaf.MoveToLastLogicalLeaf();
  EXPECT_EQ("#last", ToDebugString(last_logical_leaf));
}

TEST_P(NGInlineCursorTest, LastChild) {
  // TDOO(yosin): Remove <style> once NGFragmentItem don't do culled inline.
  InsertStyleElement("a, b { background: gray; }");
  NGInlineCursor cursor =
      SetupCursor("<div id=root>abc<a>DEF<b>GHI</b></a>xyz</div>");
  cursor.MoveToLastChild();
  EXPECT_EQ("xyz", ToDebugString(cursor));
  EXPECT_FALSE(cursor.TryToMoveToLastChild());
}

TEST_P(NGInlineCursorTest, LastChild2) {
  // TDOO(yosin): Remove <style> once NGFragmentItem don't do culled inline.
  InsertStyleElement("a, b { background: gray; }");
  NGInlineCursor cursor = SetupCursor(
      "<div id=root><b id=first>abc</b><a>DEF<b>GHI</b></a>"
      "<a id=last>xyz</a></div>");
  cursor.MoveToLastChild();
  EXPECT_EQ("#last", ToDebugString(cursor));
  cursor.MoveToLastChild();
  EXPECT_EQ("xyz", ToDebugString(cursor));
  EXPECT_FALSE(cursor.TryToMoveToLastChild());
}

TEST_P(NGInlineCursorTest, Next) {
  SetBodyInnerHTML(R"HTML(
    <style>
    span { background: gray; }
    </style>
    <div id=root>
      text1
      <span id="span1">
        text2
        <span id="span2">
          text3
        </span>
        text4
      </span>
      text5
    </div>
  )HTML");

  LayoutBlockFlow* block_flow =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("root"));
  NGInlineCursor cursor(*block_flow);
  Vector<String> list = ToDebugStringList(cursor);
  EXPECT_THAT(list, ElementsAre("#linebox", "text1", "#span1", "text2",
                                "#span2", "text3", "text4", "text5"));
}

TEST_P(NGInlineCursorTest, NextWithImage) {
  NGInlineCursor cursor = SetupCursor("<div id=root>abc<img id=img>xyz</div>");
  Vector<String> list = ToDebugStringList(cursor);
  EXPECT_THAT(list, ElementsAre("#linebox", "abc", "#img", "xyz"));
}

TEST_P(NGInlineCursorTest, NextWithInlineBox) {
  InsertStyleElement("b { display: inline-block; }");
  NGInlineCursor cursor =
      SetupCursor("<div id=root>abc<b id=ib>def</b>xyz</div>");
  Vector<String> list = ToDebugStringList(cursor);
  EXPECT_THAT(list, ElementsAre("#linebox", "abc", "#ib", "xyz"));
}

TEST_P(NGInlineCursorTest, NextForSameLayoutObject) {
  NGInlineCursor cursor = SetupCursor("<pre id=root>abc\ndef\nghi</pre>");
  cursor.MoveTo(*GetLayoutObjectByElementId("root")->SlowFirstChild());
  Vector<String> list;
  while (cursor) {
    list.push_back(ToDebugString(cursor));
    cursor.MoveToNextForSameLayoutObject();
  }
  EXPECT_THAT(list, ElementsAre("abc", "", "def", "", "ghi"));
}

TEST_P(NGInlineCursorTest, NextSibling) {
  // TDOO(yosin): Remove <style> once NGFragmentItem don't do culled inline.
  InsertStyleElement("a, b { background: gray; }");
  NGInlineCursor cursor =
      SetupCursor("<div id=root>abc<a>DEF<b>GHI</b></a>xyz</div>");
  cursor.MoveToFirstChild();  // go to "abc"
  Vector<String> list;
  while (cursor) {
    list.push_back(ToDebugString(cursor));
    cursor.MoveToNextSibling();
  }
  EXPECT_THAT(list, ElementsAre("abc", "LayoutInline A", "xyz"));
}

TEST_P(NGInlineCursorTest, NextSibling2) {
  // TDOO(yosin): Remove <style> once NGFragmentItem don't do culled inline.
  InsertStyleElement("a, b { background: gray; }");
  NGInlineCursor cursor =
      SetupCursor("<div id=root><a>abc<b>def</b>xyz</a></div>");
  cursor.MoveToFirstChild();  // go to <a>abc</a>
  cursor.MoveToFirstChild();  // go to "abc"
  Vector<String> list;
  while (cursor) {
    list.push_back(ToDebugString(cursor));
    cursor.MoveToNextSibling();
  }
  EXPECT_THAT(list, ElementsAre("abc", "LayoutInline B", "xyz"));
}

TEST_P(NGInlineCursorTest, NextSkippingChildren) {
  // TDOO(yosin): Remove <style> once NGFragmentItem don't do culled inline.
  InsertStyleElement("span { background: gray; }");
  SetBodyInnerHTML(R"HTML(
    <div id=root>
      text1
      <span id="span1">
        text2
        <span id="span2">
          text3
        </span>
        text4
      </span>
      text5
    </div>
  )HTML");

  LayoutBlockFlow* block_flow =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("root"));
  NGInlineCursor cursor(*block_flow);
  for (unsigned i = 0; i < 3; ++i)
    cursor.MoveToNext();
  EXPECT_EQ("text2", ToDebugString(cursor));
  Vector<String> list;
  while (true) {
    cursor.MoveToNextSkippingChildren();
    if (!cursor)
      break;
    list.push_back(ToDebugString(cursor));
  }
  EXPECT_THAT(list, ElementsAre("#span2", "text4", "text5"));
}

TEST_P(NGInlineCursorTest, EmptyOutOfFlow) {
  SetBodyInnerHTML(R"HTML(
    <div id=root>
      <span style="position: absolute"></span>
    </div>
  )HTML");

  LayoutBlockFlow* block_flow =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("root"));
  NGInlineCursor cursor(*block_flow);
  Vector<String> list = ToDebugStringList(cursor);
  EXPECT_THAT(list, ElementsAre());
}

TEST_P(NGInlineCursorTest, PreviousLine) {
  NGInlineCursor cursor = SetupCursor("<div id=root>abc<br>xyz</div>");
  NGInlineCursor line1(cursor);
  while (line1 && !line1.IsLineBox())
    line1.MoveToNext();
  ASSERT_TRUE(line1.IsNotNull());
  NGInlineCursor line2(line1);
  line2.MoveToNext();
  while (line2 && !line2.IsLineBox())
    line2.MoveToNext();
  ASSERT_NE(line1, line2);

  NGInlineCursor should_be_null(line1);
  should_be_null.MoveToPreviousLine();
  EXPECT_TRUE(should_be_null.IsNull());

  NGInlineCursor should_be_line1(line2);
  should_be_line1.MoveToPreviousLine();
  EXPECT_EQ(line1, should_be_line1);
}

}  // namespace blink
