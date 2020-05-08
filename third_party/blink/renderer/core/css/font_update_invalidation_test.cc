// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

// This test suite verifies that after font changes (e.g., font loaded), we do
// not invalidate the full document's style or layout, but for affected elements
// only.
class FontUpdateInvalidationTest
    : private ScopedCSSReducedFontLoadingInvalidationsForTest,
      private ScopedCSSReducedFontLoadingLayoutInvalidationsForTest,
      public SimTest {
 public:
  FontUpdateInvalidationTest()
      : ScopedCSSReducedFontLoadingInvalidationsForTest(true),
        ScopedCSSReducedFontLoadingLayoutInvalidationsForTest(true) {}

 protected:
  static Vector<char> ReadAhemWoff2() {
    return test::ReadFromFile(test::CoreTestDataPath("Ahem.woff2"))
        ->CopyAs<Vector<char>>();
  }
};

TEST_F(FontUpdateInvalidationTest, PartialLayoutInvalidationAfterFontLoading) {
  SimRequest main_resource("https://example.com", "text/html");
  SimRequest font_resource("https://example.com/Ahem.woff2", "font/woff2");

  LoadURL("https://example.com");
  main_resource.Write(R"HTML(
    <!doctype html>
    <style>
      @font-face {
        font-family: custom-font;
        src: url(https://example.com/Ahem.woff2) format("woff2");
      }
      #target {
        font: 25px/1 custom-font, monospace;
      }
      #reference {
        font: 25px/1 monospace;
      }
    </style>
    <div><span id=target>0123456789</span></div>
    <div><span id=reference>0123456789</div>
  )HTML");

  // First rendering the page with fallback
  Compositor().BeginFrame();

  Element* target = GetDocument().getElementById("target");
  Element* reference = GetDocument().getElementById("reference");

  EXPECT_GT(250, target->OffsetWidth());
  EXPECT_GT(250, reference->OffsetWidth());

  // Finish font loading, and trigger invalidations.
  font_resource.Complete(ReadAhemWoff2());
  GetDocument().GetStyleEngine().InvalidateStyleAndLayoutForFontUpdates();

  // No element is marked for style recalc, since no computed style is changed.
  EXPECT_EQ(kNoStyleChange, target->GetStyleChangeType());
  EXPECT_EQ(kNoStyleChange, reference->GetStyleChangeType());

  // Only elements that had pending custom fonts are marked for relayout.
  EXPECT_TRUE(target->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(reference->GetLayoutObject()->NeedsLayout());

  Compositor().BeginFrame();
  EXPECT_EQ(250, target->OffsetWidth());
  EXPECT_GT(250, reference->OffsetWidth());

  main_resource.Finish();
}

TEST_F(FontUpdateInvalidationTest,
       PartialLayoutInvalidationAfterFontFaceDeletion) {
  SimRequest main_resource("https://example.com", "text/html");
  SimRequest font_resource("https://example.com/Ahem.woff2", "font/woff2");

  LoadURL("https://example.com");
  main_resource.Write(R"HTML(
    <!doctype html>
    <script>
    const face = new FontFace('custom-font',
                              'url(https://example.com/Ahem.woff2)');
    face.load();
    document.fonts.add(face);
    </script>
    <style>
      #target {
        font: 25px/1 custom-font, monospace;
      }
      #reference {
        font: 25px/1 monospace;
      }
    </style>
    <div><span id=target>0123456789</span></div>
    <div><span id=reference>0123456789</div>
  )HTML");

  // First render the page with the custom font
  font_resource.Complete(ReadAhemWoff2());
  Compositor().BeginFrame();

  Element* target = GetDocument().getElementById("target");
  Element* reference = GetDocument().getElementById("reference");

  EXPECT_EQ(250, target->OffsetWidth());
  EXPECT_GT(250, reference->OffsetWidth());

  // Then delete the custom font, and trigger invalidations
  main_resource.Write("<script>document.fonts.delete(face);</script>");
  GetDocument().GetStyleEngine().InvalidateStyleAndLayoutForFontUpdates();

  // No element is marked for style recalc, since no computed style is changed.
  EXPECT_EQ(kNoStyleChange, target->GetStyleChangeType());
  EXPECT_EQ(kNoStyleChange, reference->GetStyleChangeType());

  // Only elements using custom fonts are marked for relayout.
  EXPECT_TRUE(target->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(reference->GetLayoutObject()->NeedsLayout());

  Compositor().BeginFrame();
  EXPECT_GT(250, target->OffsetWidth());
  EXPECT_GT(250, reference->OffsetWidth());

  main_resource.Finish();
}

}  // namespace blink
