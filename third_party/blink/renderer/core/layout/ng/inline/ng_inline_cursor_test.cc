// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"

namespace blink {

using ::testing::ElementsAre;

class NGInlineCursorTest : public NGLayoutTest,
                           private ScopedLayoutNGFragmentItemForTest,
                           public testing::WithParamInterface<bool> {
 public:
  NGInlineCursorTest() : ScopedLayoutNGFragmentItemForTest(GetParam()) {}

 protected:
  Vector<String> ToDebugStringList(NGInlineCursor* cursor) {
    Vector<String> list;
    while (cursor->MoveToNext())
      list.push_back(ToDebugString(*cursor));
    return list;
  }

  String ToDebugString(const NGInlineCursor& cursor) {
    if (const LayoutObject* layout_object = cursor.CurrentLayoutObject()) {
      if (const LayoutText* text = ToLayoutTextOrNull(layout_object))
        return text->GetText().StripWhiteSpace();

      if (const Element* element =
              DynamicTo<Element>(layout_object->GetNode())) {
        if (const AtomicString& id = element->GetIdAttribute())
          return "#" + id;
      }

      return layout_object->DebugName();
    }

    if (cursor.IsLineBox())
      return "#linebox";
    return "#null";
  }
};

INSTANTIATE_TEST_SUITE_P(NGInlineCursorTest,
                         NGInlineCursorTest,
                         testing::Bool());

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
  NGInlineCursor cursor(block_flow);
  Vector<String> list = ToDebugStringList(&cursor);
  EXPECT_THAT(list, ElementsAre("#linebox", "text1", "#span1", "text2",
                                "#span2", "text3", "text4", "text5"));
}

TEST_P(NGInlineCursorTest, NextSkippingChildren) {
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
  NGInlineCursor cursor(block_flow);
  for (unsigned i = 0; i < 4; ++i)
    cursor.MoveToNext();
  EXPECT_EQ("text2", ToDebugString(cursor));
  Vector<String> list;
  while (cursor.MoveToNextSkippingChildren())
    list.push_back(ToDebugString(cursor));
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
  NGInlineCursor cursor(block_flow);
  Vector<String> list = ToDebugStringList(&cursor);
  EXPECT_THAT(list, ElementsAre());
}

}  // namespace blink
