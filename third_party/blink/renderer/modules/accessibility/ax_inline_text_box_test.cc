// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_inline_text_box.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/testing/accessibility_test.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace test {

TEST_P(ParameterizedAccessibilityTest, GetWordBoundaries) {
  // &#9728; is the sun emoji symbol.
  // &#2460; is circled digit one.
  SetBodyInnerHTML(R"HTML(
      <p id="paragraph">
        &quot;This, &#9728; &#2460; is ... a---+++test.&quot;
      </p>)HTML");

  AXObject* ax_paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, ax_paragraph);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_paragraph->RoleValue());
  ax_paragraph->LoadInlineTextBoxes();

  const AXObject* ax_inline_text_box =
      ax_paragraph->DeepestFirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  ASSERT_EQ(ax::mojom::Role::kInlineTextBox, ax_inline_text_box->RoleValue());

  VectorOf<int> expected_word_starts{0, 1, 5, 9, 11, 14, 18, 19, 25, 29};
  VectorOf<int> expected_word_ends{1, 5, 6, 10, 13, 17, 19, 22, 29, 31};
  VectorOf<int> word_starts, word_ends;
  ax_inline_text_box->GetWordBoundaries(word_starts, word_ends);
  EXPECT_EQ(expected_word_starts, word_starts);
  EXPECT_EQ(expected_word_ends, word_ends);
}

}  // namespace test
}  // namespace blink
