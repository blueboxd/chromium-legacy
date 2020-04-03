// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class StyleResolverTest : public PageTestBase {
 public:
  scoped_refptr<ComputedStyle> StyleForId(AtomicString id) {
    Element* element = GetDocument().getElementById(id);
    StyleResolver* resolver = GetStyleEngine().Resolver();
    DCHECK(resolver);
    auto style = resolver->StyleForElement(element);
    DCHECK(style);
    return style;
  }

 protected:
};

TEST_F(StyleResolverTest, StyleForTextInDisplayNone) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <body style="display:none">Text</body>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  GetDocument().body()->EnsureComputedStyle();

  ASSERT_TRUE(GetDocument().body()->GetComputedStyle());
  EXPECT_TRUE(
      GetDocument().body()->GetComputedStyle()->IsEnsuredInDisplayNone());
  EXPECT_FALSE(GetStyleEngine().Resolver()->StyleForText(
      To<Text>(GetDocument().body()->firstChild())));
}

TEST_F(StyleResolverTest, AnimationBaseComputedStyle) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      html { font-size: 10px; }
      body { font-size: 20px; }
    </style>
    <div id="div">Test</div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* div = GetDocument().getElementById("div");
  StyleResolver* resolver = GetStyleEngine().Resolver();
  ASSERT_TRUE(resolver);
  ElementAnimations& animations = div->EnsureElementAnimations();
  animations.SetAnimationStyleChange(true);

  ASSERT_TRUE(resolver->StyleForElement(div));
  EXPECT_EQ(20, resolver->StyleForElement(div)->FontSize());
  ASSERT_TRUE(animations.BaseComputedStyle());
  EXPECT_EQ(20, animations.BaseComputedStyle()->FontSize());

  // Getting style with customized parent style should not affect cached
  // animation base computed style.
  const ComputedStyle* parent_style =
      GetDocument().documentElement()->GetComputedStyle();
  EXPECT_EQ(
      10,
      resolver->StyleForElement(div, parent_style, parent_style)->FontSize());
  ASSERT_TRUE(animations.BaseComputedStyle());
  EXPECT_EQ(20, animations.BaseComputedStyle()->FontSize());
  EXPECT_EQ(20, resolver->StyleForElement(div)->FontSize());
}

TEST_F(StyleResolverTest, ShadowDOMV0Crash) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      span { display: contents; }
    </style>
    <summary><span id="outer"><span id="inner"></b></b></summary>
  )HTML");

  Element* outer = GetDocument().getElementById("outer");
  Element* inner = GetDocument().getElementById("inner");
  ShadowRoot& outer_root = outer->CreateV0ShadowRootForTesting();
  ShadowRoot& inner_root = inner->CreateV0ShadowRootForTesting();
  outer_root.setInnerHTML("<content>");
  inner_root.setInnerHTML("<span>");

  // Test passes if it doesn't crash.
  UpdateAllLifecyclePhasesForTest();
}

TEST_F(StyleResolverTest, HasEmUnits) {
  GetDocument().documentElement()->setInnerHTML("<div id=div>Test</div>");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(StyleForId("div")->HasEmUnits());

  GetDocument().documentElement()->setInnerHTML(
      "<div id=div style='width:1em'>Test</div>");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(StyleForId("div")->HasEmUnits());
}

TEST_F(StyleResolverTest, BasePresentIfFontRelativeUnitsAbsent) {
  GetDocument().documentElement()->setInnerHTML("<div id=div>Test</div>");
  UpdateAllLifecyclePhasesForTest();

  Element* div = GetDocument().getElementById("div");
  StyleResolver* resolver = GetStyleEngine().Resolver();
  ASSERT_TRUE(resolver);
  ElementAnimations& animations = div->EnsureElementAnimations();
  animations.SetAnimationStyleChange(true);
  // We're animating a font affecting property, but we should still be able to
  // use the base computed style optimization, since no font-relative units
  // exist in the base.
  animations.SetHasFontAffectingAnimation();

  EXPECT_TRUE(resolver->StyleForElement(div));
  EXPECT_TRUE(animations.BaseComputedStyle());
}

TEST_F(StyleResolverTest, NoCrashWhenAnimatingWithoutCascade) {
  ScopedCSSCascadeForTest scoped_cascade(false);

  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      @keyframes test {
        from { width: 10px; }
        to { width: 20px; }
      }
      div {
        animation: test 1s;
      }
    </style>
    <div id="div">Test</div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
}

class StyleResolverFontRelativeUnitTest
    : public testing::WithParamInterface<const char*>,
      public StyleResolverTest {};

TEST_P(StyleResolverFontRelativeUnitTest, NoBaseIfFontRelativeUnitPresent) {
  GetDocument().documentElement()->setInnerHTML(
      String::Format("<div id=div style='width:1%s'>Test</div>", GetParam()));
  UpdateAllLifecyclePhasesForTest();

  Element* div = GetDocument().getElementById("div");
  ElementAnimations& animations = div->EnsureElementAnimations();
  animations.SetAnimationStyleChange(true);
  animations.SetHasFontAffectingAnimation();

  EXPECT_TRUE(StyleForId("div")->HasFontRelativeUnits());
  EXPECT_FALSE(animations.BaseComputedStyle());
}

TEST_P(StyleResolverFontRelativeUnitTest,
       BasePresentIfNoFontAffectingAnimation) {
  GetDocument().documentElement()->setInnerHTML(
      String::Format("<div id=div style='width:1%s'>Test</div>", GetParam()));
  UpdateAllLifecyclePhasesForTest();

  Element* div = GetDocument().getElementById("div");
  ElementAnimations& animations = div->EnsureElementAnimations();
  animations.SetAnimationStyleChange(true);

  EXPECT_TRUE(StyleForId("div")->HasFontRelativeUnits());
  EXPECT_TRUE(animations.BaseComputedStyle());
}

INSTANTIATE_TEST_SUITE_P(All,
                         StyleResolverFontRelativeUnitTest,
                         testing::Values("em", "rem", "ex", "ch"));

}  // namespace blink
