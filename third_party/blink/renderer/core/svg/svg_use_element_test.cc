// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_element.h"

#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/svg/svg_use_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

using LifecycleUpdateReason = DocumentLifecycle::LifecycleUpdateReason;

class SVGUseElementTest : public PageTestBase {};

TEST_F(SVGUseElementTest, InstanceInvalidatedWhenNonAttachedTargetRemoved) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <style></style>
    <svg>
        <unknown>
          <g id="parent">
            <a id="target">
          </g>
          <use id="use" href="#parent">
        </unknown>
    </svg>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases(LifecycleUpdateReason::kTest);

  // Remove #target.
  ASSERT_TRUE(GetDocument().getElementById("target"));
  GetDocument().getElementById("target")->remove();

  // This should cause a rebuild of the <use> shadow tree.
  GetDocument().View()->UpdateAllLifecyclePhases(LifecycleUpdateReason::kTest);

  // There should be no instance for #target anymore, since that element was
  // removed.
  auto* use = ToSVGUseElement(GetDocument().getElementById("use"));
  ASSERT_TRUE(use);
  ASSERT_TRUE(use->GetShadowRoot());
  ASSERT_FALSE(use->GetShadowRoot()->getElementById("target"));
}

TEST_F(SVGUseElementTest,
       InstanceInvalidatedWhenNonAttachedTargetMovedInDocument) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <svg>
      <use id="use" href="#path"/>
      <textPath id="path">
        <textPath>
          <a id="target" systemLanguage="th"></a>
        </textPath>
      </textPath>
    </svg>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases(LifecycleUpdateReason::kTest);

  // Move #target in the document (leaving it still "connected").
  Element* target = GetDocument().getElementById("target");
  ASSERT_TRUE(target);
  GetDocument().body()->appendChild(target);

  // This should cause a rebuild of the <use> shadow tree.
  GetDocument().View()->UpdateAllLifecyclePhases(LifecycleUpdateReason::kTest);

  // There should be no instance for #target anymore, since that element was
  // removed.
  auto* use = ToSVGUseElement(GetDocument().getElementById("use"));
  ASSERT_TRUE(use);
  ASSERT_TRUE(use->GetShadowRoot());
  ASSERT_FALSE(use->GetShadowRoot()->getElementById("target"));
}

}  // namespace blink
