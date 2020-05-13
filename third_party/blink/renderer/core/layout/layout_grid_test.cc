// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class LayoutGridTest : public RenderingTest {};

TEST_F(LayoutGridTest, RowGapUseCounter) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: grid; gap: 20%;">
      <div></div>
      <div></div>
    </div>
  )HTML");
  RunDocumentLifecycle();

  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kGridRowGapPercent));
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kGridRowGapPercentIndefinite));
}

TEST_F(LayoutGridTest, RowGapUseCounterOneTrack) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: grid; gap: 20%;">
      <div></div>
    </div>
  )HTML");
  RunDocumentLifecycle();

  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kGridRowGapPercent));
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kGridRowGapPercentIndefinite));
}

}  // namespace blink
