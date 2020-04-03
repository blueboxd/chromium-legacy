// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/style_cascade.h"

#include <vector>

#include "third_party/blink/renderer/bindings/core/v8/v8_css_style_sheet_init.h"
#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/css/active_style_sheets.h"
#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_pending_substitution_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/css_variable_reference_value.h"
#include "third_party/blink/renderer/core/css/document_style_environment_variables.h"
#include "third_party/blink/renderer/core/css/media_query_evaluator.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_property_instances.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/properties/longhands/custom_property.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_filter.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_interpolations.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_map.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_priority.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/scoped_style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

using css_test_helpers::ParseDeclarationBlock;
using css_test_helpers::RegisterProperty;
using Origin = CascadeOrigin;
using Priority = CascadePriority;
using UnitType = CSSPrimitiveValue::UnitType;

class TestCascade {
  STACK_ALLOCATED();

 public:
  TestCascade(Document& document, Element* target = nullptr)
      : state_(document, target ? *target : *document.body()),
        cascade_(InitState(state_)) {}

  scoped_refptr<ComputedStyle> TakeStyle() { return state_.TakeStyle(); }

  StyleResolverState& State() { return state_; }
  StyleCascade& InnerCascade() { return cascade_; }

  void InheritFrom(scoped_refptr<ComputedStyle> parent) {
    state_.SetParentStyle(parent);
    state_.StyleRef().InheritFrom(*parent);
  }

  //  Note that because of how MatchResult works, declarations must be added
  //  in "origin order", i.e. UserAgent first, then User, then Author.

  void Add(String block,
           CascadeOrigin origin = CascadeOrigin::kAuthor,
           unsigned link_match_type = CSSSelector::kMatchAll) {
    CSSParserMode mode =
        origin == CascadeOrigin::kUserAgent ? kUASheetMode : kHTMLStandardMode;
    Add(ParseDeclarationBlock(block, mode), origin, link_match_type);
  }

  void Add(String name, String value, CascadeOrigin origin = Origin::kAuthor) {
    Add(name + ":" + value, origin);
  }

  void Add(const CSSPropertyValueSet* set,
           CascadeOrigin origin = CascadeOrigin::kAuthor,
           unsigned link_match_type = CSSSelector::kMatchAll) {
    DCHECK_LE(origin, CascadeOrigin::kAuthor) << "Animations not supported";
    DCHECK_LE(current_origin_, origin) << "Please add declarations in order";
    EnsureAtLeast(origin);
    cascade_.MutableMatchResult().AddMatchedProperties(set, link_match_type);
  }

  void Apply(CascadeFilter filter = CascadeFilter()) {
    EnsureAtLeast(CascadeOrigin::kAuthor);
    cascade_.Apply(filter);
  }

  String ComputedValue(String name) const {
    CSSPropertyRef ref(name, GetDocument());
    DCHECK(ref.IsValid());
    const LayoutObject* layout_object = nullptr;
    bool allow_visited_style = false;
    const CSSValue* value = ref.GetProperty().CSSValueFromComputedStyle(
        *state_.Style(), layout_object, allow_visited_style);
    return value ? value->CssText() : g_null_atom;
  }

  CascadePriority GetPriority(String name) {
    return GetPriority(
        *CSSPropertyName::From(GetDocument().GetExecutionContext(), name));
  }

  CascadePriority GetPriority(CSSPropertyName name) {
    CascadePriority* c = cascade_.map_.Find(name);
    return c ? *c : CascadePriority();
  }

  CascadeOrigin GetOrigin(String name) { return GetPriority(name).GetOrigin(); }

  void CalculateTransitionUpdate() {
    CSSAnimations::CalculateTransitionUpdate(
        state_.AnimationUpdate(), CSSAnimations::PropertyPass::kCustom,
        &state_.GetElement(), *state_.Style());
    CSSAnimations::CalculateTransitionUpdate(
        state_.AnimationUpdate(), CSSAnimations::PropertyPass::kStandard,
        &state_.GetElement(), *state_.Style());
    AddTransitions();
  }

  void CalculateAnimationUpdate() {
    CSSAnimations::CalculateAnimationUpdate(
        state_.AnimationUpdate(), &state_.GetElement(), state_.GetElement(),
        *state_.Style(), state_.ParentStyle(),
        &GetDocument().EnsureStyleResolver());
    AddAnimations();
  }

  void Reset() { cascade_.Reset(); }

 private:
  Document& GetDocument() const { return state_.GetDocument(); }
  Element* Body() const { return GetDocument().body(); }

  static StyleResolverState& InitState(StyleResolverState& state) {
    state.SetStyle(InitialStyle(state.GetDocument()));
    state.SetParentStyle(InitialStyle(state.GetDocument()));
    return state;
  }

  static scoped_refptr<ComputedStyle> InitialStyle(Document& document) {
    return StyleResolver::InitialStyleForElement(document);
  }

  void FinishOrigin() {
    switch (current_origin_) {
      case CascadeOrigin::kUserAgent:
        cascade_.MutableMatchResult().FinishAddingUARules();
        current_origin_ = CascadeOrigin::kUser;
        break;
      case CascadeOrigin::kUser:
        cascade_.MutableMatchResult().FinishAddingUserRules();
        current_origin_ = CascadeOrigin::kAuthor;
        break;
      case CascadeOrigin::kAuthor:
      default:
        NOTREACHED();
        break;
    }
  }

  void EnsureAtLeast(CascadeOrigin origin) {
    while (current_origin_ < origin)
      FinishOrigin();
  }

  void AddAnimations() {
    const auto& update = state_.AnimationUpdate();
    if (update.IsEmpty())
      return;
    cascade_.AddInterpolations(
        &update.ActiveInterpolationsForCustomAnimations(),
        CascadeOrigin::kAnimation);
    cascade_.AddInterpolations(
        &update.ActiveInterpolationsForStandardAnimations(),
        CascadeOrigin::kAnimation);
  }

  void AddTransitions() {
    const auto& update = state_.AnimationUpdate();
    if (update.IsEmpty())
      return;
    cascade_.AddInterpolations(
        &update.ActiveInterpolationsForCustomTransitions(),
        CascadeOrigin::kTransition);
    cascade_.AddInterpolations(
        &update.ActiveInterpolationsForStandardTransitions(),
        CascadeOrigin::kTransition);
  }

  CascadeOrigin current_origin_ = CascadeOrigin::kUserAgent;
  StyleResolverState state_;
  StyleCascade cascade_;
};

class TestCascadeResolver {
  STACK_ALLOCATED();

 public:
  explicit TestCascadeResolver(Document& document, uint8_t generation = 0)
      : document_(document), resolver_(CascadeFilter(), generation) {}
  bool InCycle() const { return resolver_.InCycle(); }
  bool DetectCycle(String name) {
    CSSPropertyRef ref(name, document_);
    DCHECK(ref.IsValid());
    const CSSProperty& property = ref.GetProperty();
    return resolver_.DetectCycle(property);
  }
  wtf_size_t CycleDepth() const { return resolver_.cycle_depth_; }
  void MarkApplied(CascadePriority* priority) {
    resolver_.MarkApplied(priority);
  }
  void MarkUnapplied(CascadePriority* priority) {
    resolver_.MarkUnapplied(priority);
  }
  uint8_t GetGeneration() { return resolver_.generation_; }

 private:
  friend class TestCascadeAutoLock;

  Document& document_;
  CascadeResolver resolver_;
};

class TestCascadeAutoLock {
  STACK_ALLOCATED();

 public:
  TestCascadeAutoLock(const CSSPropertyName& name,
                      TestCascadeResolver& resolver)
      : lock_(name, resolver.resolver_) {}

 private:
  CascadeResolver::AutoLock lock_;
};

class StyleCascadeTest : public PageTestBase, private ScopedCSSCascadeForTest {
 public:
  StyleCascadeTest() : ScopedCSSCascadeForTest(true) {}

  CSSStyleSheet* CreateSheet(const String& css_text) {
    auto* init = MakeGarbageCollected<CSSStyleSheetInit>();
    DummyExceptionStateForTesting exception_state;
    CSSStyleSheet* sheet =
        CSSStyleSheet::Create(GetDocument(), init, exception_state);
    sheet->replaceSync(css_text, exception_state);
    sheet->Contents()->EnsureRuleSet(MediaQueryEvaluator(),
                                     kRuleHasNoSpecialState);
    return sheet;
  }

  void AppendSheet(const String& css_text) {
    CSSStyleSheet* sheet = CreateSheet(css_text);
    ASSERT_TRUE(sheet);

    Element* body = GetDocument().body();
    ASSERT_TRUE(body->IsInTreeScope());
    TreeScope& tree_scope = body->GetTreeScope();
    ScopedStyleResolver& scoped_resolver =
        tree_scope.EnsureScopedStyleResolver();
    ActiveStyleSheetVector active_sheets;
    active_sheets.push_back(
        std::make_pair(sheet, &sheet->Contents()->GetRuleSet()));
    scoped_resolver.AppendActiveStyleSheets(0, active_sheets);
  }

  Element* DocumentElement() const { return GetDocument().documentElement(); }

  void SetRootFont(String value) {
    DocumentElement()->SetInlineStyleProperty(CSSPropertyID::kFontSize, value);
    UpdateAllLifecyclePhasesForTest();
  }

  const MutableCSSPropertyValueSet* AnimationTaintedSet(AtomicString name,
                                                        String value) {
    CSSParserMode mode = kHTMLStandardMode;
    auto* set = MakeGarbageCollected<MutableCSSPropertyValueSet>(mode);
    set->SetProperty(name, value, /* important */ false,
                     SecureContextMode::kSecureContext,
                     /* context_style_sheet */ nullptr,
                     /* is_animation_tainted */ true);
    return set;
  }

  // Temporarily create a CSS Environment Variable.
  // https://drafts.csswg.org/css-env-1/
  class AutoEnv {
    STACK_ALLOCATED();

   public:
    AutoEnv(PageTestBase& test, AtomicString name, String value)
        : document_(&test.GetDocument()), name_(name) {
      EnsureEnvironmentVariables().SetVariable(name, value);
    }
    ~AutoEnv() { EnsureEnvironmentVariables().RemoveVariable(name_); }

   private:
    DocumentStyleEnvironmentVariables& EnsureEnvironmentVariables() {
      return document_->GetStyleEngine().EnsureEnvironmentVariables();
    }

    Document* document_;
    AtomicString name_;
  };
};

TEST_F(StyleCascadeTest, ApplySingle) {
  TestCascade cascade(GetDocument());
  cascade.Add("width", "1px", CascadeOrigin::kUserAgent);
  cascade.Add("width", "2px", CascadeOrigin::kAuthor);
  cascade.Apply();

  EXPECT_EQ("2px", cascade.ComputedValue("width"));
}

TEST_F(StyleCascadeTest, ApplyImportance) {
  TestCascade cascade(GetDocument());
  cascade.Add("width:1px !important", CascadeOrigin::kUserAgent);
  cascade.Add("width:2px", CascadeOrigin::kAuthor);
  cascade.Apply();

  EXPECT_EQ("1px", cascade.ComputedValue("width"));
}

TEST_F(StyleCascadeTest, ApplyAll) {
  TestCascade cascade(GetDocument());
  cascade.Add("width:1px", CascadeOrigin::kUserAgent);
  cascade.Add("height:1px", CascadeOrigin::kUserAgent);
  cascade.Add("all:initial", CascadeOrigin::kAuthor);
  cascade.Apply();

  EXPECT_EQ("auto", cascade.ComputedValue("width"));
  EXPECT_EQ("auto", cascade.ComputedValue("height"));
}

TEST_F(StyleCascadeTest, ApplyAllImportance) {
  TestCascade cascade(GetDocument());
  cascade.Add("opacity:0.5", CascadeOrigin::kUserAgent);
  cascade.Add("display:block !important", CascadeOrigin::kUserAgent);
  cascade.Add("all:initial", CascadeOrigin::kAuthor);
  cascade.Apply();

  EXPECT_EQ("1", cascade.ComputedValue("opacity"));
  EXPECT_EQ("block", cascade.ComputedValue("display"));
}

TEST_F(StyleCascadeTest, ApplyAllWithPhysicalLonghands) {
  TestCascade cascade(GetDocument());
  cascade.Add("width:1px", CascadeOrigin::kUserAgent);
  cascade.Add("height:1px !important", CascadeOrigin::kUserAgent);
  cascade.Add("all:initial", CascadeOrigin::kAuthor);
  cascade.Apply();
  EXPECT_EQ("auto", cascade.ComputedValue("width"));
  EXPECT_EQ("1px", cascade.ComputedValue("height"));
}

TEST_F(StyleCascadeTest, ApplyCustomProperty) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x", " 10px ");
  cascade.Add("--y", "nope");
  cascade.Apply();

  EXPECT_EQ(" 10px ", cascade.ComputedValue("--x"));
  EXPECT_EQ("nope", cascade.ComputedValue("--y"));
}

TEST_F(StyleCascadeTest, ApplyGenerations) {
  TestCascade cascade(GetDocument());

  cascade.Add("--x:10px");
  cascade.Add("width:20px");
  cascade.Apply();
  EXPECT_EQ("10px", cascade.ComputedValue("--x"));
  EXPECT_EQ("20px", cascade.ComputedValue("width"));

  cascade.State().StyleRef().SetWidth(Length::Auto());
  cascade.State().StyleRef().SetVariableData("--x", nullptr, true);
  EXPECT_EQ(g_null_atom, cascade.ComputedValue("--x"));
  EXPECT_EQ("auto", cascade.ComputedValue("width"));

  // Apply again
  cascade.Apply();
  EXPECT_EQ("10px", cascade.ComputedValue("--x"));
  EXPECT_EQ("20px", cascade.ComputedValue("width"));
}

TEST_F(StyleCascadeTest, ApplyCustomPropertyVar) {
  // Apply --x first.
  {
    TestCascade cascade(GetDocument());
    cascade.Add("--x", "yes and var(--y)");
    cascade.Add("--y", "no");
    cascade.Apply();

    EXPECT_EQ("yes and no", cascade.ComputedValue("--x"));
    EXPECT_EQ("no", cascade.ComputedValue("--y"));
  }

  // Apply --y first.
  {
    TestCascade cascade(GetDocument());
    cascade.Add("--y", "no");
    cascade.Add("--x", "yes and var(--y)");
    cascade.Apply();

    EXPECT_EQ("yes and no", cascade.ComputedValue("--x"));
    EXPECT_EQ("no", cascade.ComputedValue("--y"));
  }
}

TEST_F(StyleCascadeTest, InvalidVarReferenceCauseInvalidVariable) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x", "nope var(--y)");
  cascade.Apply();

  EXPECT_EQ(g_null_atom, cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, ApplyCustomPropertyFallback) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x", "yes and var(--y,no)");
  cascade.Apply();

  EXPECT_EQ("yes and no", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, RegisteredPropertyFallback) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("--x", "var(--y,10px)");
  cascade.Apply();

  EXPECT_EQ("10px", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, RegisteredPropertyFallbackValidation) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("--x", "10px");
  cascade.Add("--y", "var(--x,red)");  // Fallback must be valid <length>.
  cascade.Add("--z", "var(--y,pass)");
  cascade.Apply();

  EXPECT_EQ("pass", cascade.ComputedValue("--z"));
}

TEST_F(StyleCascadeTest, VarInFallback) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x", "one var(--z,two var(--y))");
  cascade.Add("--y", "three");
  cascade.Apply();

  EXPECT_EQ("one two three", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, VarReferenceInNormalProperty) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x", "10px");
  cascade.Add("width", "var(--x)");
  cascade.Apply();

  EXPECT_EQ("10px", cascade.ComputedValue("width"));
}

TEST_F(StyleCascadeTest, MultipleVarRefs) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x", "var(--y) bar var(--y)");
  cascade.Add("--y", "foo");
  cascade.Apply();

  EXPECT_EQ("foo bar foo", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, RegisteredPropertyComputedValue) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("--x", "1in");
  cascade.Apply();

  EXPECT_EQ("96px", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, RegisteredPropertySyntaxErrorCausesInitial) {
  RegisterProperty(GetDocument(), "--x", "<length>", "10px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("--x", "#fefefe");
  cascade.Add("--y", "var(--x)");
  cascade.Apply();

  EXPECT_EQ("10px", cascade.ComputedValue("--x"));
  EXPECT_EQ("10px", cascade.ComputedValue("--y"));
}

TEST_F(StyleCascadeTest, RegisteredPropertySubstitution) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("--x", "1in");
  cascade.Add("--y", "var(--x)");
  cascade.Apply();

  EXPECT_EQ("96px", cascade.ComputedValue("--y"));
}

TEST_F(StyleCascadeTest, RegisteredPropertyChain) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);
  RegisterProperty(GetDocument(), "--z", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("--x", "1in");
  cascade.Add("--y", "var(--x)");
  cascade.Add("--z", "calc(var(--y) + 1in)");
  cascade.Apply();

  EXPECT_EQ("96px", cascade.ComputedValue("--x"));
  EXPECT_EQ("96px", cascade.ComputedValue("--y"));
  EXPECT_EQ("192px", cascade.ComputedValue("--z"));
}

TEST_F(StyleCascadeTest, BasicShorthand) {
  TestCascade cascade(GetDocument());
  cascade.Add("margin", "1px 2px 3px 4px");
  cascade.Apply();

  EXPECT_EQ("1px", cascade.ComputedValue("margin-top"));
  EXPECT_EQ("2px", cascade.ComputedValue("margin-right"));
  EXPECT_EQ("3px", cascade.ComputedValue("margin-bottom"));
  EXPECT_EQ("4px", cascade.ComputedValue("margin-left"));
}

TEST_F(StyleCascadeTest, BasicVarShorthand) {
  TestCascade cascade(GetDocument());
  cascade.Add("margin", "1px var(--x) 3px 4px");
  cascade.Add("--x", "2px");
  cascade.Apply();

  EXPECT_EQ("1px", cascade.ComputedValue("margin-top"));
  EXPECT_EQ("2px", cascade.ComputedValue("margin-right"));
  EXPECT_EQ("3px", cascade.ComputedValue("margin-bottom"));
  EXPECT_EQ("4px", cascade.ComputedValue("margin-left"));
}

TEST_F(StyleCascadeTest, ApplyingPendingSubstitutionFirst) {
  TestCascade cascade(GetDocument());
  cascade.Add("margin", "1px var(--x) 3px 4px");
  cascade.Add("--x", "2px");
  cascade.Add("margin-right", "5px");
  cascade.Apply();

  EXPECT_EQ("1px", cascade.ComputedValue("margin-top"));
  EXPECT_EQ("5px", cascade.ComputedValue("margin-right"));
  EXPECT_EQ("3px", cascade.ComputedValue("margin-bottom"));
  EXPECT_EQ("4px", cascade.ComputedValue("margin-left"));
}

TEST_F(StyleCascadeTest, ApplyingPendingSubstitutionLast) {
  TestCascade cascade(GetDocument());
  cascade.Add("margin-right", "5px");
  cascade.Add("margin", "1px var(--x) 3px 4px");
  cascade.Add("--x", "2px");
  cascade.Apply();

  EXPECT_EQ("1px", cascade.ComputedValue("margin-top"));
  EXPECT_EQ("2px", cascade.ComputedValue("margin-right"));
  EXPECT_EQ("3px", cascade.ComputedValue("margin-bottom"));
  EXPECT_EQ("4px", cascade.ComputedValue("margin-left"));
}

TEST_F(StyleCascadeTest, ResolverDetectCycle) {
  TestCascade cascade(GetDocument());
  TestCascadeResolver resolver(GetDocument());

  {
    TestCascadeAutoLock lock(CSSPropertyName("--a"), resolver);
    EXPECT_FALSE(resolver.InCycle());
    {
      TestCascadeAutoLock lock(CSSPropertyName("--b"), resolver);
      EXPECT_FALSE(resolver.InCycle());
      {
        TestCascadeAutoLock lock(CSSPropertyName("--c"), resolver);
        EXPECT_FALSE(resolver.InCycle());

        EXPECT_TRUE(resolver.DetectCycle("--a"));
        EXPECT_TRUE(resolver.InCycle());
      }
      EXPECT_TRUE(resolver.InCycle());
    }
    EXPECT_TRUE(resolver.InCycle());
  }
  EXPECT_FALSE(resolver.InCycle());
}

TEST_F(StyleCascadeTest, ResolverDetectNoCycle) {
  TestCascade cascade(GetDocument());
  TestCascadeResolver resolver(GetDocument());

  {
    TestCascadeAutoLock lock(CSSPropertyName("--a"), resolver);
    EXPECT_FALSE(resolver.InCycle());
    {
      TestCascadeAutoLock lock(CSSPropertyName("--b"), resolver);
      EXPECT_FALSE(resolver.InCycle());
      {
        TestCascadeAutoLock lock(CSSPropertyName("--c"), resolver);
        EXPECT_FALSE(resolver.InCycle());

        EXPECT_FALSE(resolver.DetectCycle("--x"));
        EXPECT_FALSE(resolver.InCycle());
      }
      EXPECT_FALSE(resolver.InCycle());
    }
    EXPECT_FALSE(resolver.InCycle());
  }
  EXPECT_FALSE(resolver.InCycle());
}

TEST_F(StyleCascadeTest, ResolverDetectCycleSelf) {
  TestCascade cascade(GetDocument());
  TestCascadeResolver resolver(GetDocument());

  {
    TestCascadeAutoLock lock(CSSPropertyName("--a"), resolver);
    EXPECT_FALSE(resolver.InCycle());

    EXPECT_TRUE(resolver.DetectCycle("--a"));
    EXPECT_TRUE(resolver.InCycle());
  }
  EXPECT_FALSE(resolver.InCycle());
}

TEST_F(StyleCascadeTest, ResolverDetectMultiCycle) {
  using AutoLock = TestCascadeAutoLock;

  TestCascade cascade(GetDocument());
  TestCascadeResolver resolver(GetDocument());

  {
    AutoLock lock(CSSPropertyName("--a"), resolver);
    EXPECT_FALSE(resolver.InCycle());
    {
      AutoLock lock(CSSPropertyName("--b"), resolver);
      EXPECT_FALSE(resolver.InCycle());
      {
        AutoLock lock(CSSPropertyName("--c"), resolver);
        EXPECT_FALSE(resolver.InCycle());
        {
          AutoLock lock(CSSPropertyName("--d"), resolver);
          EXPECT_FALSE(resolver.InCycle());

          // Cycle 1 (big cycle):
          EXPECT_TRUE(resolver.DetectCycle("--b"));
          EXPECT_TRUE(resolver.InCycle());
          EXPECT_EQ(1u, resolver.CycleDepth());

          // Cycle 2 (small cycle):
          EXPECT_TRUE(resolver.DetectCycle("--c"));
          EXPECT_TRUE(resolver.InCycle());
          EXPECT_EQ(1u, resolver.CycleDepth());
        }
      }
      EXPECT_TRUE(resolver.InCycle());
    }
    EXPECT_FALSE(resolver.InCycle());
  }
  EXPECT_FALSE(resolver.InCycle());
}

TEST_F(StyleCascadeTest, ResolverDetectMultiCycleReverse) {
  using AutoLock = TestCascadeAutoLock;

  TestCascade cascade(GetDocument());
  TestCascadeResolver resolver(GetDocument());

  {
    AutoLock lock(CSSPropertyName("--a"), resolver);
    EXPECT_FALSE(resolver.InCycle());
    {
      AutoLock lock(CSSPropertyName("--b"), resolver);
      EXPECT_FALSE(resolver.InCycle());
      {
        AutoLock lock(CSSPropertyName("--c"), resolver);
        EXPECT_FALSE(resolver.InCycle());
        {
          AutoLock lock(CSSPropertyName("--d"), resolver);
          EXPECT_FALSE(resolver.InCycle());

          // Cycle 1 (small cycle):
          EXPECT_TRUE(resolver.DetectCycle("--c"));
          EXPECT_TRUE(resolver.InCycle());
          EXPECT_EQ(2u, resolver.CycleDepth());

          // Cycle 2 (big cycle):
          EXPECT_TRUE(resolver.DetectCycle("--b"));
          EXPECT_TRUE(resolver.InCycle());
          EXPECT_EQ(1u, resolver.CycleDepth());
        }
      }
      EXPECT_TRUE(resolver.InCycle());
    }
    EXPECT_FALSE(resolver.InCycle());
  }
  EXPECT_FALSE(resolver.InCycle());
}

TEST_F(StyleCascadeTest, ResolverMarkApplied) {
  TestCascadeResolver resolver(GetDocument(), 2);

  CascadePriority priority(CascadeOrigin::kAuthor);
  EXPECT_EQ(0, priority.GetGeneration());

  resolver.MarkApplied(&priority);
  EXPECT_EQ(2, priority.GetGeneration());

  // Mark a second time to verify observation of the same generation.
  resolver.MarkApplied(&priority);
  EXPECT_EQ(2, priority.GetGeneration());
}

TEST_F(StyleCascadeTest, ResolverMarkUnapplied) {
  TestCascadeResolver resolver(GetDocument(), 7);

  CascadePriority priority(CascadeOrigin::kAuthor);
  EXPECT_EQ(0, priority.GetGeneration());

  resolver.MarkApplied(&priority);
  EXPECT_EQ(7, priority.GetGeneration());

  resolver.MarkUnapplied(&priority);
  EXPECT_EQ(0, priority.GetGeneration());

  // Mark a second time to verify observation of the same generation.
  resolver.MarkUnapplied(&priority);
  EXPECT_EQ(0, priority.GetGeneration());
}

TEST_F(StyleCascadeTest, BasicCycle) {
  TestCascade cascade(GetDocument());
  cascade.Add("--a", "foo");
  cascade.Add("--b", "bar");
  cascade.Apply();

  EXPECT_EQ("foo", cascade.ComputedValue("--a"));
  EXPECT_EQ("bar", cascade.ComputedValue("--b"));

  cascade.Add("--a", "var(--b)");
  cascade.Add("--b", "var(--a)");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--a"));
  EXPECT_FALSE(cascade.ComputedValue("--b"));
}

TEST_F(StyleCascadeTest, SelfCycle) {
  TestCascade cascade(GetDocument());
  cascade.Add("--a", "foo");
  cascade.Apply();

  EXPECT_EQ("foo", cascade.ComputedValue("--a"));

  cascade.Add("--a", "var(--a)");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--a"));
}

TEST_F(StyleCascadeTest, SelfCycleInFallback) {
  TestCascade cascade(GetDocument());
  cascade.Add("--a", "var(--x, var(--a))");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--a"));
}

TEST_F(StyleCascadeTest, SelfCycleInUnusedFallback) {
  TestCascade cascade(GetDocument());
  cascade.Add("--a", "var(--b, var(--a))");
  cascade.Add("--b", "10px");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--a"));
  EXPECT_EQ("10px", cascade.ComputedValue("--b"));
}

TEST_F(StyleCascadeTest, LongCycle) {
  TestCascade cascade(GetDocument());
  cascade.Add("--a", "var(--b)");
  cascade.Add("--b", "var(--c)");
  cascade.Add("--c", "var(--d)");
  cascade.Add("--d", "var(--e)");
  cascade.Add("--e", "var(--a)");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--a"));
  EXPECT_FALSE(cascade.ComputedValue("--b"));
  EXPECT_FALSE(cascade.ComputedValue("--c"));
  EXPECT_FALSE(cascade.ComputedValue("--d"));
  EXPECT_FALSE(cascade.ComputedValue("--e"));
}

TEST_F(StyleCascadeTest, PartialCycle) {
  TestCascade cascade(GetDocument());
  cascade.Add("--a", "var(--b)");
  cascade.Add("--b", "var(--a)");
  cascade.Add("--c", "bar var(--d) var(--a)");
  cascade.Add("--d", "foo");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--a"));
  EXPECT_FALSE(cascade.ComputedValue("--b"));
  EXPECT_FALSE(cascade.ComputedValue("--c"));
  EXPECT_EQ("foo", cascade.ComputedValue("--d"));
}

TEST_F(StyleCascadeTest, VarCycleViaFallback) {
  TestCascade cascade(GetDocument());
  cascade.Add("--a", "var(--b)");
  cascade.Add("--b", "var(--x, var(--a))");
  cascade.Add("--c", "var(--a)");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--a"));
  EXPECT_FALSE(cascade.ComputedValue("--b"));
  EXPECT_FALSE(cascade.ComputedValue("--c"));
}

TEST_F(StyleCascadeTest, FallbackTriggeredByCycle) {
  TestCascade cascade(GetDocument());
  cascade.Add("--a", "var(--b)");
  cascade.Add("--b", "var(--a)");
  cascade.Add("--c", "var(--a,foo)");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--a"));
  EXPECT_FALSE(cascade.ComputedValue("--b"));
  EXPECT_EQ("foo", cascade.ComputedValue("--c"));
}

TEST_F(StyleCascadeTest, RegisteredCycle) {
  RegisterProperty(GetDocument(), "--a", "<length>", "0px", false);
  RegisterProperty(GetDocument(), "--b", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("--a", "var(--b)");
  cascade.Add("--b", "var(--a)");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--a"));
  EXPECT_FALSE(cascade.ComputedValue("--b"));
}

TEST_F(StyleCascadeTest, PartiallyRegisteredCycle) {
  RegisterProperty(GetDocument(), "--a", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("--a", "var(--b)");
  cascade.Add("--b", "var(--a)");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--a"));
  EXPECT_FALSE(cascade.ComputedValue("--b"));
}

TEST_F(StyleCascadeTest, FallbackTriggeredByRegisteredCycle) {
  RegisterProperty(GetDocument(), "--a", "<length>", "0px", false);
  RegisterProperty(GetDocument(), "--b", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  // Cycle:
  cascade.Add("--a", "var(--b)");
  cascade.Add("--b", "var(--a)");
  // References to cycle:
  cascade.Add("--c", "var(--a,1px)");
  cascade.Add("--d", "var(--b,2px)");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--a"));
  EXPECT_FALSE(cascade.ComputedValue("--b"));
  EXPECT_EQ("1px", cascade.ComputedValue("--c"));
  EXPECT_EQ("2px", cascade.ComputedValue("--d"));
}

TEST_F(StyleCascadeTest, CycleStillInvalidWithFallback) {
  TestCascade cascade(GetDocument());
  // Cycle:
  cascade.Add("--a", "var(--b,red)");
  cascade.Add("--b", "var(--a,red)");
  // References to cycle:
  cascade.Add("--c", "var(--a,green)");
  cascade.Add("--d", "var(--b,green)");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--a"));
  EXPECT_FALSE(cascade.ComputedValue("--b"));
  EXPECT_EQ("green", cascade.ComputedValue("--c"));
  EXPECT_EQ("green", cascade.ComputedValue("--d"));
}

TEST_F(StyleCascadeTest, CycleInFallbackStillInvalid) {
  TestCascade cascade(GetDocument());
  // Cycle:
  cascade.Add("--a", "var(--b,red)");
  cascade.Add("--b", "var(--x,var(--a))");
  // References to cycle:
  cascade.Add("--c", "var(--a,green)");
  cascade.Add("--d", "var(--b,green)");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--a"));
  EXPECT_FALSE(cascade.ComputedValue("--b"));
  EXPECT_EQ("green", cascade.ComputedValue("--c"));
  EXPECT_EQ("green", cascade.ComputedValue("--d"));
}

TEST_F(StyleCascadeTest, CycleMultiple) {
  TestCascade cascade(GetDocument());
  // Cycle:
  cascade.Add("--a", "var(--c, red)");
  cascade.Add("--b", "var(--c, red)");
  cascade.Add("--c", "var(--a, blue) var(--b, blue)");
  // References to cycle:
  cascade.Add("--d", "var(--a,green)");
  cascade.Add("--e", "var(--b,green)");
  cascade.Add("--f", "var(--c,green)");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--a"));
  EXPECT_FALSE(cascade.ComputedValue("--b"));
  EXPECT_FALSE(cascade.ComputedValue("--c"));
  EXPECT_EQ("green", cascade.ComputedValue("--d"));
  EXPECT_EQ("green", cascade.ComputedValue("--e"));
  EXPECT_EQ("green", cascade.ComputedValue("--f"));
}

TEST_F(StyleCascadeTest, CycleMultipleFallback) {
  TestCascade cascade(GetDocument());
  // Cycle:
  cascade.Add("--a", "var(--b, red)");
  cascade.Add("--b", "var(--a, var(--c, red))");
  cascade.Add("--c", "var(--b, red)");
  // References to cycle:
  cascade.Add("--d", "var(--a,green)");
  cascade.Add("--e", "var(--b,green)");
  cascade.Add("--f", "var(--c,green)");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--a"));
  EXPECT_FALSE(cascade.ComputedValue("--b"));
  EXPECT_FALSE(cascade.ComputedValue("--c"));
  EXPECT_EQ("green", cascade.ComputedValue("--d"));
  EXPECT_EQ("green", cascade.ComputedValue("--e"));
  EXPECT_EQ("green", cascade.ComputedValue("--f"));
}

TEST_F(StyleCascadeTest, CycleMultipleUnusedFallback) {
  TestCascade cascade(GetDocument());
  cascade.Add("--a", "red");
  // Cycle:
  cascade.Add("--b", "var(--c, red)");
  cascade.Add("--c", "var(--a, var(--b, red) var(--d, red))");
  cascade.Add("--d", "var(--c, red)");
  // References to cycle:
  cascade.Add("--e", "var(--b,green)");
  cascade.Add("--f", "var(--c,green)");
  cascade.Add("--g", "var(--d,green)");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--b"));
  EXPECT_FALSE(cascade.ComputedValue("--c"));
  EXPECT_FALSE(cascade.ComputedValue("--d"));
  EXPECT_EQ("green", cascade.ComputedValue("--e"));
  EXPECT_EQ("green", cascade.ComputedValue("--f"));
  EXPECT_EQ("green", cascade.ComputedValue("--g"));
}

TEST_F(StyleCascadeTest, CycleReferencedFromStandardProperty) {
  TestCascade cascade(GetDocument());
  cascade.Add("--a", "var(--b)");
  cascade.Add("--b", "var(--a)");
  cascade.Add("color:var(--a,green)");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--a"));
  EXPECT_FALSE(cascade.ComputedValue("--b"));
  EXPECT_EQ("rgb(0, 128, 0)", cascade.ComputedValue("color"));
}

TEST_F(StyleCascadeTest, CycleReferencedFromShorthand) {
  TestCascade cascade(GetDocument());
  cascade.Add("--a", "var(--b)");
  cascade.Add("--b", "var(--a)");
  cascade.Add("background", "var(--a,green)");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--a"));
  EXPECT_FALSE(cascade.ComputedValue("--b"));
  EXPECT_EQ("rgb(0, 128, 0)", cascade.ComputedValue("background-color"));
}

TEST_F(StyleCascadeTest, EmUnit) {
  TestCascade cascade(GetDocument());
  cascade.Add("font-size", "10px");
  cascade.Add("width", "10em");
  cascade.Apply();

  EXPECT_EQ("100px", cascade.ComputedValue("width"));
}

TEST_F(StyleCascadeTest, EmUnitCustomProperty) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("font-size", "10px");
  cascade.Add("--x", "10em");
  cascade.Apply();

  EXPECT_EQ("100px", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, EmUnitNonCycle) {
  TestCascade parent(GetDocument());
  parent.Add("font-size", "10px");
  parent.Apply();

  TestCascade cascade(GetDocument());
  cascade.InheritFrom(parent.TakeStyle());
  cascade.Add("font-size", "var(--x)");
  cascade.Add("--x", "10em");
  cascade.Apply();

  // Note: Only registered properties can have cycles with font-size.
  EXPECT_EQ("100px", cascade.ComputedValue("font-size"));
}

TEST_F(StyleCascadeTest, EmUnitCycle) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("font-size", "var(--x)");
  cascade.Add("--x", "10em");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, SubstitutingEmCycles) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("font-size", "var(--x)");
  cascade.Add("--x", "10em");
  cascade.Add("--y", "var(--x)");
  cascade.Add("--z", "var(--x,1px)");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--y"));
  EXPECT_EQ("1px", cascade.ComputedValue("--z"));
}

TEST_F(StyleCascadeTest, RemUnit) {
  SetRootFont("10px");
  UpdateAllLifecyclePhasesForTest();

  TestCascade cascade(GetDocument());
  cascade.Add("width", "10rem");
  cascade.Apply();

  EXPECT_EQ("100px", cascade.ComputedValue("width"));
}

TEST_F(StyleCascadeTest, RemUnitCustomProperty) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  SetRootFont("10px");
  UpdateAllLifecyclePhasesForTest();

  TestCascade cascade(GetDocument());
  cascade.Add("--x", "10rem");
  cascade.Apply();

  EXPECT_EQ("100px", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, RemUnitInFontSize) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  SetRootFont("10px");
  UpdateAllLifecyclePhasesForTest();

  TestCascade cascade(GetDocument());
  cascade.Add("font-size", "1rem");
  cascade.Add("--x", "10rem");
  cascade.Apply();

  EXPECT_EQ("100px", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, RemUnitInRootFontSizeCycle) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  TestCascade cascade(GetDocument(), DocumentElement());
  cascade.Add("font-size", "var(--x)");
  cascade.Add("--x", "1rem");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, RemUnitInRootFontSizeNonCycle) {
  TestCascade cascade(GetDocument(), DocumentElement());
  cascade.Add("font-size", "initial");
  cascade.Apply();

  String expected = cascade.ComputedValue("font-size");

  cascade.Add("font-size", "var(--x)");
  cascade.Add("--x", "1rem");
  cascade.Apply();

  // Note: Only registered properties can have cycles with font-size.
  EXPECT_EQ("1rem", cascade.ComputedValue("--x"));
  EXPECT_EQ(expected, cascade.ComputedValue("font-size"));
}

TEST_F(StyleCascadeTest, Initial) {
  TestCascade parent(GetDocument());
  parent.Add("--x", "foo");
  parent.Apply();

  TestCascade cascade(GetDocument());
  cascade.InheritFrom(parent.TakeStyle());
  cascade.Add("--y", "foo");
  cascade.Apply();

  EXPECT_EQ("foo", cascade.ComputedValue("--x"));
  EXPECT_EQ("foo", cascade.ComputedValue("--y"));

  cascade.Add("--x", "initial");
  cascade.Add("--y", "initial");
  cascade.Apply();

  EXPECT_FALSE(cascade.ComputedValue("--x"));
  EXPECT_FALSE(cascade.ComputedValue("--y"));
}

TEST_F(StyleCascadeTest, Inherit) {
  TestCascade parent(GetDocument());
  parent.Add("--x", "foo");
  parent.Apply();

  TestCascade cascade(GetDocument());
  cascade.InheritFrom(parent.TakeStyle());

  EXPECT_EQ("foo", cascade.ComputedValue("--x"));

  cascade.Add("--x", "bar");
  cascade.Apply();
  EXPECT_EQ("bar", cascade.ComputedValue("--x"));

  cascade.Add("--x", "inherit");
  cascade.Apply();
  EXPECT_EQ("foo", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, Unset) {
  TestCascade parent(GetDocument());
  parent.Add("--x", "foo");
  parent.Apply();

  TestCascade cascade(GetDocument());
  cascade.InheritFrom(parent.TakeStyle());
  EXPECT_EQ("foo", cascade.ComputedValue("--x"));

  cascade.Add("--x", "bar");
  cascade.Apply();
  EXPECT_EQ("bar", cascade.ComputedValue("--x"));

  cascade.Add("--x", "unset");
  cascade.Apply();
  EXPECT_EQ("foo", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, RegisteredInitial) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  cascade.Apply();
  EXPECT_EQ("0px", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, SubstituteRegisteredImplicitInitialValue) {
  RegisterProperty(GetDocument(), "--x", "<length>", "13px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("--y", " var(--x) ");
  cascade.Apply();
  EXPECT_EQ("13px", cascade.ComputedValue("--x"));
  EXPECT_EQ(" 13px ", cascade.ComputedValue("--y"));
}

TEST_F(StyleCascadeTest, SubstituteRegisteredUniversal) {
  RegisterProperty(GetDocument(), "--x", "*", "foo", false);

  TestCascade cascade(GetDocument());
  cascade.Add("--x", "bar");
  cascade.Add("--y", "var(--x)");
  cascade.Apply();
  EXPECT_EQ("bar", cascade.ComputedValue("--x"));
  EXPECT_EQ("bar", cascade.ComputedValue("--y"));
}

TEST_F(StyleCascadeTest, SubstituteRegisteredUniversalInvalid) {
  RegisterProperty(GetDocument(), "--x", "*", g_null_atom, false);

  TestCascade cascade(GetDocument());
  cascade.Add("--y", " var(--x) ");
  cascade.Apply();
  EXPECT_FALSE(cascade.ComputedValue("--x"));
  EXPECT_FALSE(cascade.ComputedValue("--y"));
}

TEST_F(StyleCascadeTest, SubstituteRegisteredUniversalInitial) {
  RegisterProperty(GetDocument(), "--x", "*", "foo", false);

  TestCascade cascade(GetDocument());
  cascade.Add("--y", " var(--x) ");
  cascade.Apply();
  EXPECT_EQ("foo", cascade.ComputedValue("--x"));
  EXPECT_EQ(" foo ", cascade.ComputedValue("--y"));
}

TEST_F(StyleCascadeTest, RegisteredExplicitInitial) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("--x", "10px");
  cascade.Apply();
  EXPECT_EQ("10px", cascade.ComputedValue("--x"));

  cascade.Add("--x", "initial");
  cascade.Add("--y", "var(--x)");
  cascade.Apply();
  EXPECT_EQ("0px", cascade.ComputedValue("--x"));
  EXPECT_EQ("0px", cascade.ComputedValue("--y"));
}

TEST_F(StyleCascadeTest, RegisteredExplicitInherit) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  TestCascade parent(GetDocument());
  parent.Add("--x", "15px");
  parent.Apply();
  EXPECT_EQ("15px", parent.ComputedValue("--x"));

  TestCascade cascade(GetDocument());
  cascade.InheritFrom(parent.TakeStyle());
  cascade.Apply();
  EXPECT_EQ("0px", cascade.ComputedValue("--x"));  // Note: inherit==false

  cascade.Add("--x", "inherit");
  cascade.Add("--y", "var(--x)");
  cascade.Apply();
  EXPECT_EQ("15px", cascade.ComputedValue("--x"));
  EXPECT_EQ("15px", cascade.ComputedValue("--y"));
}

TEST_F(StyleCascadeTest, RegisteredExplicitUnset) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);
  RegisterProperty(GetDocument(), "--y", "<length>", "0px", true);

  TestCascade parent(GetDocument());
  parent.Add("--x", "15px");
  parent.Add("--y", "15px");
  parent.Apply();
  EXPECT_EQ("15px", parent.ComputedValue("--x"));
  EXPECT_EQ("15px", parent.ComputedValue("--y"));

  TestCascade cascade(GetDocument());
  cascade.InheritFrom(parent.TakeStyle());
  cascade.Add("--x", "2px");
  cascade.Add("--y", "2px");
  cascade.Apply();
  EXPECT_EQ("2px", cascade.ComputedValue("--x"));
  EXPECT_EQ("2px", cascade.ComputedValue("--y"));

  cascade.Add("--x", "unset");
  cascade.Add("--y", "unset");
  cascade.Add("--z", "var(--x) var(--y)");
  cascade.Apply();
  EXPECT_EQ("0px", cascade.ComputedValue("--x"));
  EXPECT_EQ("15px", cascade.ComputedValue("--y"));
  EXPECT_EQ("0px 15px", cascade.ComputedValue("--z"));
}

TEST_F(StyleCascadeTest, SubstituteAnimationTaintedInCustomProperty) {
  TestCascade cascade(GetDocument());
  cascade.Add(AnimationTaintedSet("--x", "15px"));
  cascade.Add("--y", "var(--x)");
  cascade.Apply();
  EXPECT_EQ("15px", cascade.ComputedValue("--x"));
  EXPECT_EQ("15px", cascade.ComputedValue("--y"));
}

TEST_F(StyleCascadeTest, SubstituteAnimationTaintedInStandardProperty) {
  TestCascade cascade(GetDocument());
  cascade.Add(AnimationTaintedSet("--x", "15px"));
  cascade.Add("width", "var(--x)");
  cascade.Apply();
  EXPECT_EQ("15px", cascade.ComputedValue("--x"));
  EXPECT_EQ("15px", cascade.ComputedValue("width"));
}

TEST_F(StyleCascadeTest, SubstituteAnimationTaintedInAnimationProperty) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x", "20s");
  cascade.Add("animation-duration", "var(--x)");
  cascade.Apply();

  EXPECT_EQ("20s", cascade.ComputedValue("--x"));
  EXPECT_EQ("20s", cascade.ComputedValue("animation-duration"));

  cascade.Add(AnimationTaintedSet("--y", "20s"));
  cascade.Add("animation-duration", "var(--y)");
  cascade.Apply();

  EXPECT_EQ("20s", cascade.ComputedValue("--y"));
  EXPECT_EQ("0s", cascade.ComputedValue("animation-duration"));
}

TEST_F(StyleCascadeTest, IndirectlyAnimationTainted) {
  TestCascade cascade(GetDocument());
  cascade.Add(AnimationTaintedSet("--x", "20s"));
  cascade.Add("--y", "var(--x)");
  cascade.Add("animation-duration", "var(--y)");
  cascade.Apply();

  EXPECT_EQ("20s", cascade.ComputedValue("--x"));
  EXPECT_EQ("20s", cascade.ComputedValue("--y"));
  EXPECT_EQ("0s", cascade.ComputedValue("animation-duration"));
}

TEST_F(StyleCascadeTest, AnimationTaintedFallback) {
  TestCascade cascade(GetDocument());
  cascade.Add(AnimationTaintedSet("--x", "20s"));
  cascade.Add("animation-duration", "var(--x,1s)");
  cascade.Apply();

  EXPECT_EQ("20s", cascade.ComputedValue("--x"));
  EXPECT_EQ("1s", cascade.ComputedValue("animation-duration"));
}

TEST_F(StyleCascadeTest, EnvMissingNestedVar) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x", "rgb(0, 0, 0)");
  cascade.Add("background-color", "env(missing, var(--x))");
  cascade.Apply();

  EXPECT_EQ("rgb(0, 0, 0)", cascade.ComputedValue("--x"));
  EXPECT_EQ("rgb(0, 0, 0)", cascade.ComputedValue("background-color"));
}

TEST_F(StyleCascadeTest, EnvMissingNestedVarFallback) {
  TestCascade cascade(GetDocument());
  cascade.Add("background-color", "env(missing, var(--missing, blue))");
  cascade.Apply();

  EXPECT_EQ("rgb(0, 0, 255)", cascade.ComputedValue("background-color"));
}

TEST_F(StyleCascadeTest, EnvMissingFallback) {
  TestCascade cascade(GetDocument());
  cascade.Add("background-color", "env(missing, blue)");
  cascade.Apply();

  EXPECT_EQ("rgb(0, 0, 255)", cascade.ComputedValue("background-color"));
}

TEST_F(StyleCascadeTest, ValidEnv) {
  AutoEnv env(*this, "test", "red");

  TestCascade cascade(GetDocument());
  cascade.Add("background-color", "env(test, blue)");
  cascade.Apply();

  EXPECT_EQ("rgb(255, 0, 0)", cascade.ComputedValue("background-color"));
}

TEST_F(StyleCascadeTest, ValidEnvFallback) {
  AutoEnv env(*this, "test", "red");

  TestCascade cascade(GetDocument());
  cascade.Add("background-color", "env(test, blue)");
  cascade.Apply();

  EXPECT_EQ("rgb(255, 0, 0)", cascade.ComputedValue("background-color"));
}

TEST_F(StyleCascadeTest, ValidEnvInUnusedFallback) {
  AutoEnv env(*this, "test", "red");

  TestCascade cascade(GetDocument());
  cascade.Add("--x", "rgb(0, 0, 0)");
  cascade.Add("background-color", "var(--x, env(test))");
  cascade.Apply();

  EXPECT_EQ("rgb(0, 0, 0)", cascade.ComputedValue("--x"));
  EXPECT_EQ("rgb(0, 0, 0)", cascade.ComputedValue("background-color"));
}

TEST_F(StyleCascadeTest, ValidEnvInUsedFallback) {
  AutoEnv env(*this, "test", "red");

  TestCascade cascade(GetDocument());
  cascade.Add("background-color", "var(--missing, env(test))");
  cascade.Apply();

  EXPECT_EQ("rgb(255, 0, 0)", cascade.ComputedValue("background-color"));
}

TEST_F(StyleCascadeTest, AnimationApplyFilter) {
  AppendSheet(R"HTML(
     @keyframes test {
        from { color: white; background-color: white; }
        to { color: gray; background-color: gray; }
     }
    )HTML");

  TestCascade cascade(GetDocument());

  cascade.Add("animation: test linear 10s -5s");
  cascade.Add("color:green");
  cascade.Apply();

  cascade.CalculateAnimationUpdate();
  cascade.Apply(CascadeFilter(CSSProperty::kInherited, true));

  EXPECT_EQ("rgb(0, 128, 0)", cascade.ComputedValue("color"));
  EXPECT_EQ("rgb(192, 192, 192)", cascade.ComputedValue("background-color"));
}

TEST_F(StyleCascadeTest, TransitionApplyFilter) {
  TestCascade cascade1(GetDocument());
  cascade1.Add("background-color: white");
  cascade1.Add("color: white");
  cascade1.Add("transition: all steps(2, start) 100s");
  cascade1.Apply();

  // Set the old style on the element, so that the transition
  // update detects it.
  GetDocument().body()->SetComputedStyle(cascade1.TakeStyle());

  // Now simulate a new style, with new color values.
  TestCascade cascade2(GetDocument());
  cascade2.Add("background-color: gray");
  cascade2.Add("color: gray");
  cascade2.Add("transition: all steps(2, start) 100s");
  cascade2.Apply();

  cascade2.CalculateTransitionUpdate();
  cascade2.Apply(CascadeFilter(CSSProperty::kInherited, true));

  EXPECT_EQ("rgb(128, 128, 128)", cascade2.ComputedValue("color"));
  EXPECT_EQ("rgb(192, 192, 192)", cascade2.ComputedValue("background-color"));
}

TEST_F(StyleCascadeTest, PendingKeyframeAnimation) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  AppendSheet(R"HTML(
     @keyframes test {
        from { --x: 10px; }
        to { --x: 20px; }
     }
    )HTML");

  TestCascade cascade(GetDocument());

  cascade.Add("animation-name", "test");
  cascade.Add("animation-duration", "1s");
  cascade.Apply();

  cascade.CalculateAnimationUpdate();
  cascade.Apply();

  EXPECT_EQ(CascadeOrigin::kAnimation, cascade.GetPriority("--x").GetOrigin());
}

TEST_F(StyleCascadeTest, PendingKeyframeAnimationApply) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  AppendSheet(R"HTML(
     @keyframes test {
        from { --x: 10px; }
        to { --x: 20px; }
     }
    )HTML");

  TestCascade cascade(GetDocument());

  cascade.Add("animation-name", "test");
  cascade.Add("animation-duration", "10s");
  cascade.Add("animation-timing-function", "linear");
  cascade.Add("animation-delay", "-5s");
  cascade.Apply();

  cascade.CalculateAnimationUpdate();
  cascade.Apply();

  EXPECT_EQ(CascadeOrigin::kAnimation, cascade.GetPriority("--x").GetOrigin());
  EXPECT_EQ("15px", cascade.ComputedValue("--x"));
}

TEST_F(StyleCascadeTest, TransitionCausesInterpolationValue) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  // First, simulate an "old style".
  TestCascade cascade1(GetDocument());
  cascade1.Add("--x", "10px");
  cascade1.Add("transition", "--x 1s");
  cascade1.Apply();

  // Set the old style on the element, so that the animation
  // update detects it.
  GetDocument().body()->SetComputedStyle(cascade1.TakeStyle());

  // Now simulate a new style, with a new value for --x.
  TestCascade cascade2(GetDocument());
  cascade2.Add("--x", "20px");
  cascade2.Add("transition", "--x 1s");
  cascade2.Apply();

  cascade2.CalculateTransitionUpdate();
  cascade2.Apply();

  EXPECT_EQ(CascadeOrigin::kTransition,
            cascade2.GetPriority("--x").GetOrigin());
}

TEST_F(StyleCascadeTest, TransitionDetectedForChangedFontSize) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  TestCascade cascade1(GetDocument());
  cascade1.Add("font-size", "10px");
  cascade1.Add("--x", "10em");
  cascade1.Add("width", "10em");
  cascade1.Add("height", "10px");
  cascade1.Add("transition", "--x 1s, width 1s");
  cascade1.Apply();

  GetDocument().body()->SetComputedStyle(cascade1.TakeStyle());

  TestCascade cascade2(GetDocument());
  cascade2.Add("font-size", "20px");
  cascade2.Add("--x", "10em");
  cascade2.Add("width", "10em");
  cascade2.Add("height", "10px");
  cascade2.Add("transition", "--x 1s, width 1s");
  cascade2.Apply();

  cascade2.CalculateTransitionUpdate();
  cascade2.Apply();

  EXPECT_EQ(CascadeOrigin::kTransition, cascade2.GetOrigin("--x"));
  EXPECT_EQ(CascadeOrigin::kTransition, cascade2.GetOrigin("width"));
  EXPECT_EQ("10px", cascade2.ComputedValue("height"));
}

TEST_F(StyleCascadeTest, AnimatingVarReferences) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  AppendSheet(R"HTML(
     @keyframes test {
        from { --x: var(--from); }
        to { --x: var(--to); }
     }
    )HTML");

  TestCascade cascade(GetDocument());

  cascade.Add("animation-name", "test");
  cascade.Add("animation-duration", "10s");
  cascade.Add("animation-timing-function", "linear");
  cascade.Add("animation-delay", "-5s");
  cascade.Apply();

  cascade.CalculateAnimationUpdate();
  cascade.Add("--from", "10px");
  cascade.Add("--to", "20px");
  cascade.Add("--y", "var(--x)");
  cascade.Apply();

  EXPECT_EQ("15px", cascade.ComputedValue("--x"));
  EXPECT_EQ("15px", cascade.ComputedValue("--y"));
}

TEST_F(StyleCascadeTest, AnimateStandardProperty) {
  AppendSheet(R"HTML(
     @keyframes test {
        from { width: 10px; }
        to { width: 20px; }
     }
    )HTML");

  TestCascade cascade(GetDocument());

  cascade.Add("animation-name", "test");
  cascade.Add("animation-duration", "10s");
  cascade.Add("animation-timing-function", "linear");
  cascade.Add("animation-delay", "-5s");
  cascade.Apply();

  cascade.CalculateAnimationUpdate();
  cascade.Apply();

  EXPECT_EQ(CascadeOrigin::kAnimation, cascade.GetOrigin("width"));
  EXPECT_EQ("15px", cascade.ComputedValue("width"));
}

TEST_F(StyleCascadeTest, AuthorImportantWinOverAnimations) {
  AppendSheet(R"HTML(
     @keyframes test {
        from { width: 10px; height: 10px; }
        to { width: 20px; height: 20px; }
     }
    )HTML");

  TestCascade cascade(GetDocument());

  cascade.Add("animation-name", "test");
  cascade.Add("animation-duration", "10s");
  cascade.Add("animation-timing-function", "linear");
  cascade.Add("animation-delay", "-5s");
  cascade.Add("width:40px");
  cascade.Add("height:40px !important");
  cascade.Apply();

  cascade.CalculateAnimationUpdate();
  cascade.Apply();

  EXPECT_EQ(CascadeOrigin::kAnimation, cascade.GetOrigin("width"));
  EXPECT_EQ(CascadeOrigin::kAuthor, cascade.GetOrigin("height"));

  EXPECT_EQ("15px", cascade.ComputedValue("width"));
  EXPECT_EQ("40px", cascade.ComputedValue("height"));
}

TEST_F(StyleCascadeTest, TransitionsWinOverAuthorImportant) {
  // First, simulate an "old style".
  TestCascade cascade1(GetDocument());
  cascade1.Add("width:10px !important");
  cascade1.Add("height:10px !important");
  cascade1.Add("transition:all 1s");
  cascade1.Apply();

  // Set the old style on the element, so that the animation
  // update detects it.
  GetDocument().body()->SetComputedStyle(cascade1.TakeStyle());

  // Now simulate a new style, with a new value for width/height.
  TestCascade cascade2(GetDocument());
  cascade2.Add("width:20px !important");
  cascade2.Add("height:20px !important");
  cascade2.Add("transition:all 1s");
  cascade2.Apply();

  cascade2.CalculateTransitionUpdate();
  cascade2.Apply();

  EXPECT_EQ(CascadeOrigin::kTransition,
            cascade2.GetPriority("width").GetOrigin());
  EXPECT_EQ(CascadeOrigin::kTransition,
            cascade2.GetPriority("height").GetOrigin());
}

TEST_F(StyleCascadeTest, EmRespondsToAnimatedFontSize) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  AppendSheet(R"HTML(
     @keyframes test {
        from { font-size: 10px; }
        to { font-size: 20px; }
     }
    )HTML");

  TestCascade cascade(GetDocument());

  cascade.Add("animation-name", "test");
  cascade.Add("animation-duration", "10s");
  cascade.Add("animation-timing-function", "linear");
  cascade.Add("animation-delay", "-5s");
  cascade.Apply();

  cascade.CalculateAnimationUpdate();
  cascade.Add("--x", "2em");
  cascade.Add("width", "10em");

  cascade.Apply();
  EXPECT_EQ("30px", cascade.ComputedValue("--x"));
  EXPECT_EQ("150px", cascade.ComputedValue("width"));
}

TEST_F(StyleCascadeTest, AnimateStandardPropertyWithVar) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  AppendSheet(R"HTML(
     @keyframes test {
        from { width: var(--from); }
        to { width: var(--to); }
     }
    )HTML");

  TestCascade cascade(GetDocument());

  cascade.Add("animation-name", "test");
  cascade.Add("animation-duration", "10s");
  cascade.Add("animation-timing-function", "linear");
  cascade.Add("animation-delay", "-5s");
  cascade.Apply();

  cascade.CalculateAnimationUpdate();
  cascade.Add("--from", "10px");
  cascade.Add("--to", "20px");

  cascade.Apply();
  EXPECT_EQ("15px", cascade.ComputedValue("width"));
}

TEST_F(StyleCascadeTest, AnimateStandardShorthand) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  AppendSheet(R"HTML(
     @keyframes test {
        from { margin: 10px; }
        to { margin: 20px; }
     }
    )HTML");

  TestCascade cascade(GetDocument());

  cascade.Add("animation-name", "test");
  cascade.Add("animation-duration", "10s");
  cascade.Add("animation-timing-function", "linear");
  cascade.Add("animation-delay", "-5s");
  cascade.Apply();

  cascade.CalculateAnimationUpdate();
  cascade.Apply();

  EXPECT_EQ(CascadeOrigin::kAnimation, cascade.GetOrigin("margin-top"));
  EXPECT_EQ(CascadeOrigin::kAnimation, cascade.GetOrigin("margin-right"));
  EXPECT_EQ(CascadeOrigin::kAnimation, cascade.GetOrigin("margin-bottom"));
  EXPECT_EQ(CascadeOrigin::kAnimation, cascade.GetOrigin("margin-left"));

  EXPECT_EQ("15px", cascade.ComputedValue("margin-top"));
  EXPECT_EQ("15px", cascade.ComputedValue("margin-right"));
  EXPECT_EQ("15px", cascade.ComputedValue("margin-bottom"));
  EXPECT_EQ("15px", cascade.ComputedValue("margin-left"));
}

TEST_F(StyleCascadeTest, AnimatedVisitedImportantOverride) {
  AppendSheet(R"HTML(
     @keyframes test {
        from { background-color: rgb(100, 100, 100); }
        to { background-color: rgb(200, 200, 200); }
     }
    )HTML");

  TestCascade cascade(GetDocument());
  cascade.State().Style()->SetInsideLink(EInsideLink::kInsideVisitedLink);

  cascade.Add(ParseDeclarationBlock("background-color:red !important"),
              CascadeOrigin::kAuthor, CSSSelector::kMatchVisited);
  cascade.Add("animation-name:test");
  cascade.Add("animation-duration:10s");
  cascade.Add("animation-timing-function:linear");
  cascade.Add("animation-delay:-5s");
  cascade.Apply();

  cascade.CalculateAnimationUpdate();
  cascade.Apply();
  EXPECT_EQ("rgb(150, 150, 150)", cascade.ComputedValue("background-color"));

  auto style = cascade.TakeStyle();

  style->SetInsideLink(EInsideLink::kInsideVisitedLink);
  EXPECT_EQ(Color(255, 0, 0),
            style->VisitedDependentColor(GetCSSPropertyBackgroundColor()));

  style->SetInsideLink(EInsideLink::kNotInsideLink);
  EXPECT_EQ(Color(150, 150, 150),
            style->VisitedDependentColor(GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleCascadeTest, AnimatedVisitedHighPrio) {
  AppendSheet(R"HTML(
     @keyframes test {
        from { color: rgb(100, 100, 100); }
        to { color: rgb(200, 200, 200); }
     }
    )HTML");

  TestCascade cascade(GetDocument());
  cascade.Add("color:red");
  cascade.Add("animation:test 10s -5s linear");
  cascade.Apply();

  cascade.CalculateAnimationUpdate();
  cascade.Apply();
  EXPECT_EQ("rgb(150, 150, 150)", cascade.ComputedValue("color"));

  auto style = cascade.TakeStyle();

  style->SetInsideLink(EInsideLink::kInsideVisitedLink);
  EXPECT_EQ(Color(150, 150, 150),
            style->VisitedDependentColor(GetCSSPropertyColor()));

  style->SetInsideLink(EInsideLink::kNotInsideLink);
  EXPECT_EQ(Color(150, 150, 150),
            style->VisitedDependentColor(GetCSSPropertyColor()));
}

TEST_F(StyleCascadeTest, AnimatedImportantOverrideFlag) {
  AppendSheet(R"HTML(
     @keyframes test {
        from { background-color: white; }
        to { background-color: gray; }
     }
    )HTML");

  TestCascade cascade(GetDocument());
  cascade.Add("animation:test 10s -5s linear");
  cascade.Add("background-color: green !important");
  cascade.Apply();
  EXPECT_FALSE(cascade.State().HasImportantOverrides());

  cascade.CalculateAnimationUpdate();
  cascade.Apply();
  EXPECT_TRUE(cascade.State().HasImportantOverrides());
}

TEST_F(StyleCascadeTest, AnimatedImportantOverrideNoFlag) {
  AppendSheet(R"HTML(
     @keyframes test {
        from { background-color: white; }
        to { background-color: gray; }
     }
    )HTML");

  TestCascade cascade(GetDocument());
  cascade.Add("animation:test 10s -5s linear");
  cascade.Add("color:green !important");
  cascade.Apply();
  EXPECT_FALSE(cascade.State().HasImportantOverrides());

  cascade.CalculateAnimationUpdate();
  cascade.Apply();
  EXPECT_FALSE(cascade.State().HasImportantOverrides());
}

TEST_F(StyleCascadeTest, AnimatedImportantOverrideFlagHighPriority) {
  AppendSheet(R"HTML(
     @keyframes test {
        from { color: white; }
        to { color: gray; }
     }
    )HTML");

  // 'color' is a high priority property, and therefore applied by lookup.
  TestCascade cascade(GetDocument());
  cascade.Add("animation:test 10s -5s linear");
  cascade.Add("color:green !important");
  cascade.Apply();
  EXPECT_FALSE(cascade.State().HasImportantOverrides());

  cascade.CalculateAnimationUpdate();
  cascade.Apply();
  EXPECT_TRUE(cascade.State().HasImportantOverrides());
}

TEST_F(StyleCascadeTest, AnimatedImportantOverrideFlagVisited) {
  AppendSheet(R"HTML(
     @keyframes test {
        from { background-color: white; }
        to { background-color: gray; }
     }
    )HTML");

  TestCascade cascade(GetDocument());
  cascade.State().Style()->SetInsideLink(EInsideLink::kInsideVisitedLink);

  cascade.Add(ParseDeclarationBlock("background-color:red !important"),
              CascadeOrigin::kAuthor, CSSSelector::kMatchVisited);
  cascade.Add("animation:test 10s -5s linear");
  cascade.Apply();
  EXPECT_FALSE(cascade.State().HasImportantOverrides());

  cascade.CalculateAnimationUpdate();
  cascade.Apply();
  EXPECT_TRUE(cascade.State().HasImportantOverrides());
}

TEST_F(StyleCascadeTest, AnimatePendingSubstitutionValue) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);

  AppendSheet(R"HTML(
     @keyframes test {
        from { margin: var(--from); }
        to { margin: var(--to); }
     }
    )HTML");

  TestCascade cascade(GetDocument());

  cascade.Add("animation-name", "test");
  cascade.Add("animation-duration", "10s");
  cascade.Add("animation-timing-function", "linear");
  cascade.Add("animation-delay", "-5s");
  cascade.Apply();

  cascade.CalculateAnimationUpdate();
  cascade.Add("--from", "10px");
  cascade.Add("--to", "20px");
  cascade.Apply();

  EXPECT_EQ(CascadeOrigin::kAnimation, cascade.GetOrigin("margin-top"));
  EXPECT_EQ(CascadeOrigin::kAnimation, cascade.GetOrigin("margin-right"));
  EXPECT_EQ(CascadeOrigin::kAnimation, cascade.GetOrigin("margin-bottom"));
  EXPECT_EQ(CascadeOrigin::kAnimation, cascade.GetOrigin("margin-left"));

  EXPECT_EQ("15px", cascade.ComputedValue("margin-top"));
  EXPECT_EQ("15px", cascade.ComputedValue("margin-right"));
  EXPECT_EQ("15px", cascade.ComputedValue("margin-bottom"));
  EXPECT_EQ("15px", cascade.ComputedValue("margin-left"));
}

TEST_F(StyleCascadeTest, ForeignObjectZoomVsEffectiveZoom) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <svg>
      <foreignObject id='foreign'></foreignObject>
    </svg>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* foreign_object = GetDocument().getElementById("foreign");
  ASSERT_TRUE(foreign_object);

  TestCascade cascade(GetDocument(), foreign_object);
  cascade.Add("-internal-effective-zoom:initial !important",
              CascadeOrigin::kUserAgent);
  cascade.Add("zoom:200%");
  cascade.Apply();

  EXPECT_EQ(1.0f, cascade.TakeStyle()->EffectiveZoom());
}

TEST_F(StyleCascadeTest, ZoomCascadeOrder) {
  TestCascade cascade(GetDocument());
  cascade.Add("zoom:200%", CascadeOrigin::kUserAgent);
  cascade.Add("-internal-effective-zoom:initial", CascadeOrigin::kUserAgent);
  cascade.Apply();

  EXPECT_EQ(1.0f, cascade.TakeStyle()->EffectiveZoom());
}

TEST_F(StyleCascadeTest, ZoomVsAll) {
  TestCascade cascade(GetDocument());
  cascade.Add("zoom:200%", CascadeOrigin::kUserAgent);
  cascade.Add("all:initial");
  cascade.Apply();

  EXPECT_EQ(1.0f, cascade.TakeStyle()->EffectiveZoom());
}

TEST_F(StyleCascadeTest, InternalEffectiveZoomVsAll) {
  TestCascade cascade(GetDocument());
  cascade.Add("-internal-effective-zoom:200%", CascadeOrigin::kUserAgent);
  cascade.Add("all:initial");
  cascade.Apply();

  EXPECT_EQ(1.0f, cascade.TakeStyle()->EffectiveZoom());
}

TEST_F(StyleCascadeTest, ZoomReversedCascadeOrder) {
  TestCascade cascade(GetDocument());
  cascade.Add("-internal-effective-zoom:initial", CascadeOrigin::kUserAgent);
  cascade.Add("zoom:200%", CascadeOrigin::kUserAgent);
  cascade.Apply();

  EXPECT_EQ(2.0f, cascade.TakeStyle()->EffectiveZoom());
}

TEST_F(StyleCascadeTest, ZoomImportant) {
  TestCascade cascade(GetDocument());
  cascade.Add("zoom:200% !important", CascadeOrigin::kUserAgent);
  cascade.Add("-internal-effective-zoom:initial", CascadeOrigin::kAuthor);
  cascade.Apply();

  EXPECT_EQ(2.0f, cascade.TakeStyle()->EffectiveZoom());
}

TEST_F(StyleCascadeTest, WritingModeCascadeOrder) {
  TestCascade cascade(GetDocument());
  cascade.Add("writing-mode", "vertical-lr");
  cascade.Add("-webkit-writing-mode", "vertical-rl");
  cascade.Apply();

  EXPECT_EQ("vertical-rl", cascade.ComputedValue("writing-mode"));
  EXPECT_EQ("vertical-rl", cascade.ComputedValue("-webkit-writing-mode"));
}

TEST_F(StyleCascadeTest, WritingModeReversedCascadeOrder) {
  TestCascade cascade(GetDocument());
  cascade.Add("-webkit-writing-mode", "vertical-rl");
  cascade.Add("writing-mode", "vertical-lr");
  cascade.Apply();

  EXPECT_EQ("vertical-lr", cascade.ComputedValue("writing-mode"));
  EXPECT_EQ("vertical-lr", cascade.ComputedValue("-webkit-writing-mode"));
}

TEST_F(StyleCascadeTest, WritingModePriority) {
  TestCascade cascade(GetDocument());
  cascade.Add("writing-mode:vertical-lr !important", Origin::kAuthor);
  cascade.Add("-webkit-writing-mode:vertical-rl", Origin::kAuthor);
  cascade.Apply();

  EXPECT_EQ("vertical-lr", cascade.ComputedValue("writing-mode"));
  EXPECT_EQ("vertical-lr", cascade.ComputedValue("-webkit-writing-mode"));
}

TEST_F(StyleCascadeTest, WebkitBorderImageCascadeOrder) {
  String gradient1("linear-gradient(rgb(0, 0, 0), rgb(0, 128, 0))");
  String gradient2("linear-gradient(rgb(0, 0, 0), rgb(0, 200, 0))");

  TestCascade cascade(GetDocument());
  cascade.Add("-webkit-border-image", gradient1 + " round 40 / 10px / 20px",
              Origin::kAuthor);
  cascade.Add("border-image-source", gradient2, Origin::kAuthor);
  cascade.Add("border-image-slice", "20", Origin::kAuthor);
  cascade.Add("border-image-width", "6px", Origin::kAuthor);
  cascade.Add("border-image-outset", "4px", Origin::kAuthor);
  cascade.Add("border-image-repeat", "space", Origin::kAuthor);
  cascade.Apply();

  EXPECT_EQ(gradient2, cascade.ComputedValue("border-image-source"));
  EXPECT_EQ("20", cascade.ComputedValue("border-image-slice"));
  EXPECT_EQ("6px", cascade.ComputedValue("border-image-width"));
  EXPECT_EQ("4px", cascade.ComputedValue("border-image-outset"));
  EXPECT_EQ("space", cascade.ComputedValue("border-image-repeat"));
}

TEST_F(StyleCascadeTest, WebkitBorderImageReverseCascadeOrder) {
  String gradient1("linear-gradient(rgb(0, 0, 0), rgb(0, 128, 0))");
  String gradient2("linear-gradient(rgb(0, 0, 0), rgb(0, 200, 0))");

  TestCascade cascade(GetDocument());
  cascade.Add("border-image-source", gradient2, Origin::kAuthor);
  cascade.Add("border-image-slice", "20", Origin::kAuthor);
  cascade.Add("border-image-width", "6px", Origin::kAuthor);
  cascade.Add("border-image-outset", "4px", Origin::kAuthor);
  cascade.Add("border-image-repeat", "space", Origin::kAuthor);
  cascade.Add("-webkit-border-image", gradient1 + " round 40 / 10px / 20px",
              Origin::kAuthor);
  cascade.Apply();

  EXPECT_EQ(gradient1, cascade.ComputedValue("border-image-source"));
  EXPECT_EQ("40 fill", cascade.ComputedValue("border-image-slice"));
  EXPECT_EQ("10px", cascade.ComputedValue("border-image-width"));
  EXPECT_EQ("20px", cascade.ComputedValue("border-image-outset"));
  EXPECT_EQ("round", cascade.ComputedValue("border-image-repeat"));
}

TEST_F(StyleCascadeTest, WebkitBorderImageMixedOrder) {
  String gradient1("linear-gradient(rgb(0, 0, 0), rgb(0, 128, 0))");
  String gradient2("linear-gradient(rgb(0, 0, 0), rgb(0, 200, 0))");

  TestCascade cascade(GetDocument());
  cascade.Add("border-image-source", gradient2, Origin::kAuthor);
  cascade.Add("border-image-width", "6px", Origin::kAuthor);
  cascade.Add("-webkit-border-image", gradient1 + " round 40 / 10px / 20px",
              Origin::kAuthor);
  cascade.Add("border-image-slice", "20", Origin::kAuthor);
  cascade.Add("border-image-outset", "4px", Origin::kAuthor);
  cascade.Add("border-image-repeat", "space", Origin::kAuthor);
  cascade.Apply();

  EXPECT_EQ(gradient1, cascade.ComputedValue("border-image-source"));
  EXPECT_EQ("20", cascade.ComputedValue("border-image-slice"));
  EXPECT_EQ("10px", cascade.ComputedValue("border-image-width"));
  EXPECT_EQ("4px", cascade.ComputedValue("border-image-outset"));
  EXPECT_EQ("space", cascade.ComputedValue("border-image-repeat"));
}

TEST_F(StyleCascadeTest, MarkReferenced) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);
  RegisterProperty(GetDocument(), "--y", "<length>", "0px", false);

  TestCascade cascade(GetDocument());
  cascade.Add("width", "var(--x)");
  cascade.Apply();

  const auto* registry = GetDocument().GetPropertyRegistry();
  ASSERT_TRUE(registry);

  EXPECT_TRUE(registry->WasReferenced("--x"));
  EXPECT_FALSE(registry->WasReferenced("--y"));
}

TEST_F(StyleCascadeTest, MarkHasVariableReferenceLonghand) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x", "1px");
  cascade.Add("width", "var(--x)");
  cascade.Apply();
  auto style = cascade.TakeStyle();
  EXPECT_TRUE(style->HasVariableReferenceFromNonInheritedProperty());
}

TEST_F(StyleCascadeTest, MarkHasVariableReferenceShorthand) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x", "1px");
  cascade.Add("margin", "var(--x)");
  cascade.Apply();
  auto style = cascade.TakeStyle();
  EXPECT_TRUE(style->HasVariableReferenceFromNonInheritedProperty());
}

TEST_F(StyleCascadeTest, MarkHasVariableReferenceLonghandMissingVar) {
  TestCascade cascade(GetDocument());
  cascade.Add("width", "var(--x)");
  cascade.Apply();
  auto style = cascade.TakeStyle();
  EXPECT_TRUE(style->HasVariableReferenceFromNonInheritedProperty());
}

TEST_F(StyleCascadeTest, MarkHasVariableReferenceShorthandMissingVar) {
  TestCascade cascade(GetDocument());
  cascade.Add("margin", "var(--x)");
  cascade.Apply();
  auto style = cascade.TakeStyle();
  EXPECT_TRUE(style->HasVariableReferenceFromNonInheritedProperty());
}

TEST_F(StyleCascadeTest, NoMarkHasVariableReferenceInherited) {
  TestCascade cascade(GetDocument());
  cascade.Add("color", "var(--x)");
  cascade.Apply();
  auto style = cascade.TakeStyle();
  EXPECT_FALSE(style->HasVariableReferenceFromNonInheritedProperty());
}

TEST_F(StyleCascadeTest, NoMarkHasVariableReferenceWithoutVar) {
  TestCascade cascade(GetDocument());
  cascade.Add("width", "1px");
  cascade.Apply();
  auto style = cascade.TakeStyle();
  EXPECT_FALSE(style->HasVariableReferenceFromNonInheritedProperty());
}

TEST_F(StyleCascadeTest, InternalVisitedColorLonghand) {
  TestCascade cascade(GetDocument());
  cascade.Add("color:green", CascadeOrigin::kAuthor);
  cascade.Add("color:red", CascadeOrigin::kAuthor, CSSSelector::kMatchVisited);

  cascade.State().Style()->SetInsideLink(EInsideLink::kInsideVisitedLink);
  cascade.Apply();

  EXPECT_EQ("rgb(0, 128, 0)", cascade.ComputedValue("color"));

  Color red(255, 0, 0);
  const CSSProperty& color = GetCSSPropertyColor();
  EXPECT_EQ(red, cascade.TakeStyle()->VisitedDependentColor(color));
}

TEST_F(StyleCascadeTest, VarInInternalVisitedColorShorthand) {
  TestCascade cascade(GetDocument());
  cascade.Add("--x:red", CascadeOrigin::kAuthor);
  cascade.Add("outline:medium solid var(--x)", CascadeOrigin::kAuthor,
              CSSSelector::kMatchVisited);
  cascade.Add("outline-color:green", CascadeOrigin::kAuthor,
              CSSSelector::kMatchLink);

  cascade.State().Style()->SetInsideLink(EInsideLink::kInsideVisitedLink);
  cascade.Apply();

  EXPECT_EQ("rgb(0, 128, 0)", cascade.ComputedValue("outline-color"));

  Color red(255, 0, 0);
  const CSSProperty& outline_color = GetCSSPropertyOutlineColor();
  EXPECT_EQ(red, cascade.TakeStyle()->VisitedDependentColor(outline_color));
}

TEST_F(StyleCascadeTest, ApplyWithFilter) {
  TestCascade cascade(GetDocument());
  cascade.Add("color", "blue", Origin::kAuthor);
  cascade.Add("background-color", "green", Origin::kAuthor);
  cascade.Add("display", "inline", Origin::kAuthor);
  cascade.Apply();
  cascade.Add("color", "green", Origin::kAuthor);
  cascade.Add("background-color", "red", Origin::kAuthor);
  cascade.Add("display", "block", Origin::kAuthor);
  cascade.Apply(CascadeFilter(CSSProperty::kInherited, false));
  EXPECT_EQ("rgb(0, 128, 0)", cascade.ComputedValue("color"));
  EXPECT_EQ("rgb(0, 128, 0)", cascade.ComputedValue("background-color"));
  EXPECT_EQ("inline", cascade.ComputedValue("display"));
}

TEST_F(StyleCascadeTest, HasAuthorBackground) {
  Vector<String> properties = {"background-attachment"/*, "background-blend-mode",
                               "background-clip",       "background-image",
                               "background-origin",     "background-position-x",
                               "background-position-y", "background-size"*/};

  for (String property : properties) {
    TestCascade cascade(GetDocument());
    cascade.Add("-webkit-appearance", "button", Origin::kUserAgent);
    cascade.Add(property, "unset", Origin::kAuthor);
    cascade.Apply();
    EXPECT_TRUE(cascade.TakeStyle()->HasAuthorBackground());
  }
}

TEST_F(StyleCascadeTest, HasAuthorBorder) {
  Vector<String> properties = {
      "border-top-color",          "border-right-color",
      "border-bottom-color",       "border-left-color",
      "border-top-style",          "border-right-style",
      "border-bottom-style",       "border-left-style",
      "border-top-width",          "border-right-width",
      "border-bottom-width",       "border-left-width",
      "border-top-left-radius",    "border-top-right-radius",
      "border-bottom-left-radius", "border-bottom-right-radius",
      "border-image-source",       "border-image-slice",
      "border-image-width",        "border-image-outset",
      "border-image-repeat"};

  for (String property : properties) {
    TestCascade cascade(GetDocument());
    cascade.Add("-webkit-appearance", "button", Origin::kUserAgent);
    cascade.Add(property, "unset", Origin::kAuthor);
    cascade.Apply();
    EXPECT_TRUE(cascade.TakeStyle()->HasAuthorBorder());
  }
}

TEST_F(StyleCascadeTest, NoAuthorBackgroundOrBorder) {
  TestCascade cascade(GetDocument());
  cascade.Add("-webkit-appearance", "button", Origin::kUserAgent);
  cascade.Add("background-color", "red", Origin::kUserAgent);
  cascade.Add("border-left-color", "green", Origin::kUserAgent);
  cascade.Add("background-clip", "padding-box", Origin::kUser);
  cascade.Add("border-right-color", "green", Origin::kUser);
  cascade.Apply();
  auto style = cascade.TakeStyle();
  EXPECT_FALSE(style->HasAuthorBackground());
  EXPECT_FALSE(style->HasAuthorBorder());
}

TEST_F(StyleCascadeTest, AnalyzeMatchResult) {
  auto ua = CascadeOrigin::kUserAgent;
  auto author = CascadeOrigin::kAuthor;

  TestCascade cascade(GetDocument());
  cascade.Add("display:none;left:5px", ua);
  cascade.Add("font-size:1px !important", ua);
  cascade.Add("display:block;color:red", author);
  cascade.Add("font-size:3px", author);
  cascade.Apply();

  EXPECT_EQ(cascade.GetPriority("display").GetOrigin(), author);
  EXPECT_EQ(cascade.GetPriority("left").GetOrigin(), ua);
  EXPECT_EQ(cascade.GetPriority("color").GetOrigin(), author);
  EXPECT_EQ(cascade.GetPriority("font-size").GetOrigin(), ua);
}

TEST_F(StyleCascadeTest, AnalyzeMatchResultAll) {
  auto ua = CascadeOrigin::kUserAgent;
  auto author = CascadeOrigin::kAuthor;

  TestCascade cascade(GetDocument());
  cascade.Add("display:block", ua);
  cascade.Add("font-size:1px !important", ua);
  cascade.Add("all:unset", author);
  cascade.Apply();

  EXPECT_EQ(cascade.GetPriority("display").GetOrigin(), author);
  EXPECT_EQ(cascade.GetPriority("font-size").GetOrigin(), ua);

  // Random sample from another property affected by 'all'.
  EXPECT_EQ(cascade.GetPriority("color").GetOrigin(), author);
  EXPECT_EQ(cascade.GetPriority("color"), cascade.GetPriority("display"));
}

TEST_F(StyleCascadeTest, ApplyMatchResultFilter) {
  TestCascade cascade(GetDocument());
  cascade.Add("display:block");
  cascade.Add("color:green");
  cascade.Add("font-size:3px");
  cascade.Apply();

  cascade.Add("display:inline");
  cascade.Add("color:red");
  cascade.Apply(CascadeFilter(CSSProperty::kInherited, true));

  EXPECT_EQ("inline", cascade.ComputedValue("display"));
  EXPECT_EQ("rgb(0, 128, 0)", cascade.ComputedValue("color"));
  EXPECT_EQ("3px", cascade.ComputedValue("font-size"));
}

TEST_F(StyleCascadeTest, ApplyMatchResultAllFilter) {
  TestCascade cascade(GetDocument());
  cascade.Add("color:green");
  cascade.Add("display:block");
  cascade.Apply();

  cascade.Add("all:unset");
  cascade.Apply(CascadeFilter(CSSProperty::kInherited, true));

  EXPECT_EQ("rgb(0, 128, 0)", cascade.ComputedValue("color"));
  EXPECT_EQ("inline", cascade.ComputedValue("display"));
}

TEST_F(StyleCascadeTest, MarkHasReferenceLonghand) {
  TestCascade cascade(GetDocument());

  cascade.Add("--x:red");
  cascade.Add("background-color:var(--x)");
  cascade.Apply();

  EXPECT_TRUE(cascade.State()
                  .StyleRef()
                  .HasVariableReferenceFromNonInheritedProperty());
}

TEST_F(StyleCascadeTest, MarkHasReferenceShorthand) {
  TestCascade cascade(GetDocument());

  cascade.Add("--x:red");
  cascade.Add("background:var(--x)");
  cascade.Apply();

  EXPECT_TRUE(cascade.State()
                  .StyleRef()
                  .HasVariableReferenceFromNonInheritedProperty());
}

TEST_F(StyleCascadeTest, NoMarkHasReferenceForInherited) {
  TestCascade cascade(GetDocument());

  cascade.Add("--x:red");
  cascade.Add("--y:caption");
  cascade.Add("color:var(--x)");
  cascade.Add("font:var(--y)");
  cascade.Apply();

  EXPECT_FALSE(cascade.State()
                   .StyleRef()
                   .HasVariableReferenceFromNonInheritedProperty());
}

TEST_F(StyleCascadeTest, Reset) {
  TestCascade cascade(GetDocument());

  EXPECT_EQ(CascadePriority(), cascade.GetPriority("color"));
  EXPECT_EQ(CascadePriority(), cascade.GetPriority("--x"));

  cascade.Add("color:red");
  cascade.Add("--x:red");
  cascade.Apply();  // generation=1
  cascade.Apply();  // generation=2

  EXPECT_EQ(2u, cascade.GetPriority("color").GetGeneration());
  EXPECT_EQ(2u, cascade.GetPriority("--x").GetGeneration());

  cascade.Reset();

  EXPECT_EQ(CascadePriority(), cascade.GetPriority("color"));
  EXPECT_EQ(CascadePriority(), cascade.GetPriority("--x"));
}

}  // namespace blink
