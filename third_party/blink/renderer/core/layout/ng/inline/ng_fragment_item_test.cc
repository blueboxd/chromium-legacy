// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items_builder.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"

namespace blink {

class NGFragmentItemTest : public NGLayoutTest,
                           ScopedLayoutNGFragmentItemForTest {
 public:
  NGFragmentItemTest() : ScopedLayoutNGFragmentItemForTest(true) {}
};

TEST_F(NGFragmentItemTest, Simple) {
  SetBodyInnerHTML(R"HTML(
    <div id="container">
      text
    </div>
  )HTML");

  LayoutBlockFlow* container =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));
  const NGPhysicalBoxFragment* box = container->CurrentFragment();
  EXPECT_NE(box, nullptr);
  const NGFragmentItems* items = box->Items();
  EXPECT_NE(items, nullptr);
}

TEST_F(NGFragmentItemTest, ForLayoutObject) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #container {
      font-family: monospace;
      width: 5ch;
    }
    #span {
      background: gray;
    }
    </style>
    <div id="container">
      0123
      <span id="span">1234 5678</span>
      6789
    </div>
  )HTML");

  const LayoutObject* span = GetLayoutObjectByElementId("span");
  ASSERT_NE(span, nullptr);
  const auto items = NGFragmentItem::ItemsFor(*span);
  EXPECT_FALSE(items.IsEmpty());
  unsigned count = 0;
  for (const NGFragmentItem& item : items) {
    EXPECT_EQ(item.GetLayoutObject(), span);
    ++count;
  }
  EXPECT_EQ(count, 2u);
}

}  // namespace blink
