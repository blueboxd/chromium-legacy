// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/animation/animation_test_helper.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
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

  String ComputedValue(String name, const ComputedStyle& style) {
    CSSPropertyRef ref(name, GetDocument());
    DCHECK(ref.IsValid());
    return ref.GetProperty()
        .CSSValueFromComputedStyle(style, nullptr, false)
        ->CssText();
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

  auto* effect = CreateSimpleKeyframeEffectForTest(
      div, CSSPropertyID::kFontSize, "50px", "100px");
  GetDocument().Timeline().Play(effect);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ("50px", ComputedValue("font-size", *StyleForId("div")));

  div->SetNeedsAnimationStyleRecalc();
  StyleForId("div");

  ASSERT_TRUE(div->GetElementAnimations());
  EXPECT_TRUE(div->GetElementAnimations()->BaseComputedStyle());
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

TEST_F(StyleResolverTest, AnimationNotMaskedByImportant) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      div {
        width: 10px;
        height: 10px !important;
      }
    </style>
    <div id=div></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  Element* div = GetDocument().getElementById("div");

  auto* effect = CreateSimpleKeyframeEffectForTest(div, CSSPropertyID::kWidth,
                                                   "50px", "100px");
  GetDocument().Timeline().Play(effect);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ("50px", ComputedValue("width", *StyleForId("div")));
  EXPECT_EQ("10px", ComputedValue("height", *StyleForId("div")));

  div->SetNeedsAnimationStyleRecalc();
  StyleForId("div");

  ASSERT_TRUE(div->GetElementAnimations());
  const CSSBitset* bitset = div->GetElementAnimations()->BaseImportantSet();
  EXPECT_FALSE(CSSAnimations::IsAnimatingStandardProperties(
      div->GetElementAnimations(), bitset, KeyframeEffect::kDefaultPriority));
  EXPECT_TRUE(div->GetElementAnimations()->BaseComputedStyle());
  EXPECT_FALSE(bitset && bitset->Has(CSSPropertyID::kWidth));
  EXPECT_TRUE(bitset && bitset->Has(CSSPropertyID::kHeight));
}

TEST_F(StyleResolverTest, AnimationNotMaskedWithoutElementAnimations) {
  EXPECT_FALSE(CSSAnimations::IsAnimatingStandardProperties(
      /* ElementAnimations */ nullptr, std::make_unique<CSSBitset>().get(),
      KeyframeEffect::kDefaultPriority));
}

TEST_F(StyleResolverTest, AnimationNotMaskedWithoutBitset) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      div {
        width: 10px;
        height: 10px !important;
      }
    </style>
    <div id=div></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  Element* div = GetDocument().getElementById("div");

  auto* effect = CreateSimpleKeyframeEffectForTest(div, CSSPropertyID::kWidth,
                                                   "50px", "100px");
  GetDocument().Timeline().Play(effect);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ("50px", ComputedValue("width", *StyleForId("div")));
  EXPECT_EQ("10px", ComputedValue("height", *StyleForId("div")));

  div->SetNeedsAnimationStyleRecalc();
  StyleForId("div");

  ASSERT_TRUE(div->GetElementAnimations());
  EXPECT_FALSE(CSSAnimations::IsAnimatingStandardProperties(
      div->GetElementAnimations(), /* CSSBitset */ nullptr,
      KeyframeEffect::kDefaultPriority));
}

TEST_F(StyleResolverTest, AnimationMaskedByImportant) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      div {
        width: 10px;
        height: 10px !important;
      }
    </style>
    <div id=div></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  Element* div = GetDocument().getElementById("div");

  auto* effect = CreateSimpleKeyframeEffectForTest(div, CSSPropertyID::kHeight,
                                                   "50px", "100px");
  GetDocument().Timeline().Play(effect);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ("10px", ComputedValue("width", *StyleForId("div")));
  EXPECT_EQ("10px", ComputedValue("height", *StyleForId("div")));

  div->SetNeedsAnimationStyleRecalc();
  StyleForId("div");

  ASSERT_TRUE(div->GetElementAnimations());
  EXPECT_FALSE(div->GetElementAnimations()->BaseComputedStyle());
  EXPECT_FALSE(div->GetElementAnimations()->BaseImportantSet());
}

class StyleResolverFontRelativeUnitTest
    : public testing::WithParamInterface<const char*>,
      public StyleResolverTest {};

TEST_P(StyleResolverFontRelativeUnitTest, NoBaseIfFontRelativeUnitPresent) {
  GetDocument().documentElement()->setInnerHTML(
      String::Format("<div id=div style='width:1%s'>Test</div>", GetParam()));
  UpdateAllLifecyclePhasesForTest();

  Element* div = GetDocument().getElementById("div");
  auto* effect = CreateSimpleKeyframeEffectForTest(
      div, CSSPropertyID::kFontSize, "50px", "100px");
  GetDocument().Timeline().Play(effect);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ("50px", ComputedValue("font-size", *StyleForId("div")));

  div->SetNeedsAnimationStyleRecalc();
  auto computed_style = StyleForId("div");

  EXPECT_TRUE(computed_style->HasFontRelativeUnits());
  ASSERT_TRUE(div->GetElementAnimations());
  EXPECT_FALSE(div->GetElementAnimations()->BaseComputedStyle());
}

TEST_P(StyleResolverFontRelativeUnitTest,
       BasePresentIfNoFontAffectingAnimation) {
  GetDocument().documentElement()->setInnerHTML(
      String::Format("<div id=div style='width:1%s'>Test</div>", GetParam()));
  UpdateAllLifecyclePhasesForTest();

  Element* div = GetDocument().getElementById("div");
  auto* effect = CreateSimpleKeyframeEffectForTest(div, CSSPropertyID::kHeight,
                                                   "50px", "100px");
  GetDocument().Timeline().Play(effect);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ("50px", ComputedValue("height", *StyleForId("div")));

  div->SetNeedsAnimationStyleRecalc();
  auto computed_style = StyleForId("div");

  EXPECT_TRUE(computed_style->HasFontRelativeUnits());
  ASSERT_TRUE(div->GetElementAnimations());
  EXPECT_TRUE(div->GetElementAnimations()->BaseComputedStyle());
}

INSTANTIATE_TEST_SUITE_P(All,
                         StyleResolverFontRelativeUnitTest,
                         testing::Values("em", "rem", "ex", "ch"));

}  // namespace blink
