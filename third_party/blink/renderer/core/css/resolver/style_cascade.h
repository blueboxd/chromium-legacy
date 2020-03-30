// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_STYLE_CASCADE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_STYLE_CASCADE_H_

#include "third_party/blink/renderer/core/animation/interpolation.h"
#include "third_party/blink/renderer/core/css/css_property_id_templates.h"
#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/css_property_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_filter.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_map.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_origin.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_priority.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CascadeInterpolations;
class CascadeResolver;
class CSSCustomPropertyDeclaration;
class CSSParserContext;
class CSSProperty;
class CSSValue;
class CSSVariableData;
class CSSVariableReferenceValue;
class CustomProperty;
class MatchResult;
class StyleResolverState;

namespace cssvalue {

class CSSPendingSubstitutionValue;

}  // namespace cssvalue

// StyleCascade can analyze a MatchResult/CascadeInterpolations object to figure
// out which declarations should be skipped (e.g. due to a subsequent
// declaration with a higher priority), and which should be applied.
//
// Usage:
//
//   MatchResult result;
//   AddRulesSomehow(result);
//
//   StyleCascade cascade(state);
//   CascadeFilter allow_all;
//   cascade.Analyze(result, allow_all);
//   cascade.Apply(result, allow_all);
//
// [1] https://drafts.csswg.org/css-cascade/#cascade
class CORE_EXPORT StyleCascade {
  STACK_ALLOCATED();

  using CSSPendingSubstitutionValue = cssvalue::CSSPendingSubstitutionValue;

 public:
  StyleCascade(StyleResolverState& state) : state_(state) {}

  // The Analyze pass goes through the MatchResult (or CascadeInterpolations),
  // and produces a CascadePriority for each declaration. Each declaration
  // is compared against the currently stored priority for the associated
  // property, and either added the CascadeMap, or discarded, depending on which
  // priority is greater.
  //
  // Note that the MatchResult/CascadeInterpolations (and their values) are
  // not retained by StyleCascade. The caller must provide the same object
  // (or a compatible object) when calling Apply.
  void Analyze(const MatchResult&, CascadeFilter);
  void Analyze(const CascadeInterpolations&, CascadeFilter);

  // The Apply pass goes through the MatchResult (or CascadeInterpolations),
  // and produces a CascadePriority for each declaration. If the priority of
  // the declaration is equal to the priority stored for the associated
  // property, then we Apply that declaration to the ComputedStyle. Otherwise,
  // the declaration is skipped.
  void Apply(const MatchResult& result, CascadeFilter filter) {
    Apply(&result, nullptr, filter);
  }
  void Apply(const CascadeInterpolations& i, CascadeFilter filter) {
    Apply(nullptr, &i, filter);
  }
  // Applying a MatchResult and CascadeInterpolations at the same time means
  // that dependency resolution can take place across the two "declaration
  // sources".
  //
  // For example, if there is an interpolation currently taking place on
  // 'font-size', static declarations from the MatchResult object that contain
  // 'em' units would be responsive to to that interpolation. This would not be
  // the case if two are applied separately.
  void Apply(const MatchResult*, const CascadeInterpolations*, CascadeFilter);

  // Resets the cascade to its initial state. Note that this does not undo
  // any changes already applied to the StyleResolverState/ComputedStyle.
  void Reset();

  // Applying interpolations may involve resolving values, since we may be
  // applying a keyframe from e.g. "color: var(--x)" to "color: var(--y)".
  // Hence that code needs an entry point to the resolving process.
  //
  // TODO(crbug.com/985023): This function has an associated const
  // violation, which isn't great. (This vilation was not introduced with
  // StyleCascade, however).
  //
  // See documentation the other Resolve* functions for what resolve means.
  const CSSValue* Resolve(const CSSPropertyName&,
                          const CSSValue&,
                          CascadeResolver&);

 private:
  friend class TestCascade;

  // The maximum number of tokens that may be produced by a var()
  // reference.
  //
  // https://drafts.csswg.org/css-variables/#long-variables
  static const size_t kMaxSubstitutionTokens = 16384;

  // Applies kHighPropertyPriority properties.
  //
  // In theory, it would be possible for each property/value that contains
  // em/ch/etc to dynamically apply font-size (and related properties), but
  // in practice, it is very inconvenient to detect these dependencies. Hence,
  // we apply font-affecting properties (among others) before all the others.
  void ApplyHighPriority(CascadeResolver&);

  // Applies -webkit-appearance, and excludes -internal-ua-* properties if
  // we don't have an appearance.
  void ApplyAppearance(CascadeResolver&);

  // Applies -webkit-border-image (if present), and skips any border-image
  // longhands found with lower priority than -webkit-border-image.
  //
  // The -webkit-border-image property is unique (in a bad way), since it's
  // a surrogate of a shorthand. Therefore it needs special treatment to
  // behave correctly.
  void ApplyWebkitBorderImage(CascadeResolver&);

  void ApplyMatchResult(const MatchResult&, CascadeResolver&);
  void ApplyInterpolations(const CascadeInterpolations&, CascadeResolver&);
  void ApplyInterpolationMap(const ActiveInterpolationsMap&,
                             CascadeOrigin,
                             size_t index,
                             CascadeResolver&);
  void ApplyInterpolation(const CSSProperty&,
                          CascadePriority,
                          const ActiveInterpolations&,
                          CascadeResolver&);

  // Looks up a value with random access, and applies it.
  void LookupAndApply(const CSSPropertyName&, CascadeResolver&);
  void LookupAndApply(const CSSProperty&, CascadeResolver&);
  void LookupAndApplyDeclaration(const CSSProperty&,
                                 CascadePriority,
                                 const MatchResult&,
                                 CascadeResolver&);
  void LookupAndApplyInterpolation(const CSSProperty&,
                                   CascadePriority,
                                   const CascadeInterpolations&,
                                   CascadeResolver&);

  // Whether or not we are calculating the style for the root element.
  // We need to know this to detect cycles with 'rem' units.
  // https://drafts.css-houdini.org/css-properties-values-api-1/#dependency-cycles
  bool IsRootElement() const;

  // The TokenSequence class acts as a builder for CSSVariableData.
  //
  // However, actually building a CSSVariableData is optional; you can also
  // get a CSSParserTokenRange directly, which is useful when resolving a
  // CSSVariableData which won't ultimately end up in a CSSVariableData
  // (i.e. CSSVariableReferenceValue or CSSPendingSubstitutionValue).
  class TokenSequence {
    STACK_ALLOCATED();

   public:
    TokenSequence() = default;
    // Initialize a TokenSequence from a CSSVariableData, preparing the
    // TokenSequence for var() resolution.
    //
    // This copies everything except the tokens.
    explicit TokenSequence(const CSSVariableData*);

    bool IsAnimationTainted() const { return is_animation_tainted_; }
    CSSParserTokenRange TokenRange() const { return tokens_; }

    void Append(const TokenSequence&);
    void Append(const CSSVariableData*);
    void Append(const CSSParserToken&);

    scoped_refptr<CSSVariableData> BuildVariableData();

   private:
    Vector<CSSParserToken> tokens_;
    Vector<String> backing_strings_;
    // https://drafts.csswg.org/css-variables/#animation-tainted
    bool is_animation_tainted_ = false;
    // https://drafts.css-houdini.org/css-properties-values-api-1/#dependency-cycles
    bool has_font_units_ = false;
    bool has_root_font_units_ = false;

    // The base URL and charset are currently needed to calculate the computed
    // value of <url>-registered custom properties correctly.
    //
    // TODO(crbug.com/985013): Store CSSParserContext on
    // CSSCustomPropertyDeclaration and avoid this.
    //
    // https://drafts.css-houdini.org/css-properties-values-api-1/#relative-urls
    String base_url_;
    WTF::TextEncoding charset_;
  };

  enum class Surrogate { kSkip, kApply, kNoSurrogate };

  // A property can be skipped apply-time if it's a surrogate property.
  //
  // A surrogate property is an alias-like property that shares a storage
  // location on ComputedStyle with the property it's a surrogate for. Unlike
  // aliases, they are not resolved parse-time, but exist alongside the original
  // property in the parsed rule.
  //
  // For example, consider a rule:
  //
  // div {
  //   inline-size: 20px;
  //   width: 10px;
  // }
  //
  // The inline-size property is a surrogate of width in this case. When we
  // encounter inline-size during Apply, we check the CascadePriority of the
  // the original property (width). Since width has a higher priority (it
  // appears later), we skip applying inline-size.
  //
  // Had the order been reversed:
  //
  //  div {
  //    width: 10px;
  //    inline-size: 20px;
  //  }
  //
  // we would first apply width (it's unaware that it has surrogates pointing to
  // it), and then later also apply inline-size (we check the CascadePriority
  // of width and find that it's lower than inline-size's).
  //
  // Surrogate properties should be skipped (i.e. not applied after all) if
  // the corresponding original property has a higher priority.
  //
  Surrogate ResolveSurrogate(const CSSProperty&,
                             CascadePriority,
                             CascadeResolver&);

  inline Surrogate ResolveIfSurrogate(const CSSProperty& property,
                                      CascadePriority priority,
                                      CascadeResolver& resolver) {
    if (!property.IsSurrogate())
      return Surrogate::kNoSurrogate;
    return ResolveSurrogate(property, priority, resolver);
  }

  // Resolving Values
  //
  // *Resolving* a value, means looking at the dependencies for a given
  // CSSValue, and ensuring that these dependencies are satisfied. The result
  // of a Resolve call is either the same CSSValue (e.g. if there were no
  // dependencies), or a new CSSValue with the dependencies resolved.
  //
  // For example, consider the following properties:
  //
  //  --x: 10px;
  //  --y: var(--x);
  //  width: var(--y);
  //
  // Here, to resolve 'width', the computed value of --y must be known. In
  // other words, we must first Apply '--y'. Hence, resolving 'width' will
  // Apply '--y' as a side-effect. (This process would then continue to '--x').

  const CSSValue* Resolve(const CSSProperty&,
                          const CSSValue&,
                          CascadeResolver&);
  const CSSValue* ResolveCustomProperty(const CSSProperty&,
                                        const CSSCustomPropertyDeclaration&,
                                        CascadeResolver&);
  const CSSValue* ResolveVariableReference(const CSSProperty&,
                                           const CSSVariableReferenceValue&,
                                           CascadeResolver&);
  const CSSValue* ResolvePendingSubstitution(const CSSProperty&,
                                             const CSSPendingSubstitutionValue&,
                                             CascadeResolver&);

  scoped_refptr<CSSVariableData> ResolveVariableData(CSSVariableData*,
                                                     CascadeResolver&);

  // The Resolve*Into functions either resolve dependencies, append to the
  // TokenSequence accordingly, and return true; or it returns false when
  // the TokenSequence is "invalid at computed-value time" [1]. This happens
  // when there was a reference to an invalid/missing custom property, or when a
  // cycle was detected.
  //
  // [1] https://drafts.csswg.org/css-variables/#invalid-at-computed-value-time

  bool ResolveTokensInto(CSSParserTokenRange, CascadeResolver&, TokenSequence&);
  bool ResolveVarInto(CSSParserTokenRange, CascadeResolver&, TokenSequence&);
  bool ResolveEnvInto(CSSParserTokenRange, CascadeResolver&, TokenSequence&);

  CSSVariableData* GetVariableData(const CustomProperty&) const;
  CSSVariableData* GetEnvironmentVariable(const AtomicString&) const;
  const CSSParserContext* GetParserContext(const CSSVariableReferenceValue&);

  // Detects if the given property/data depends on the font-size property
  // of the Element we're calculating the style for.
  //
  // https://drafts.css-houdini.org/css-properties-values-api-1/#dependency-cycles
  bool HasFontSizeDependency(const CustomProperty&, CSSVariableData*) const;
  // The fallback must match the syntax of the custom property, otherwise the
  // the declaration is "invalid at computed-value time".'
  //
  // https://drafts.css-houdini.org/css-properties-values-api-1/#fallbacks-in-var-references
  bool ValidateFallback(const CustomProperty&, CSSParserTokenRange) const;
  // Marks the CustomProperty as referenced by something. Needed to avoid
  // animating these custom properties on the compositor.
  void MarkIsReferenced(const CustomProperty&);
  // Marks a CSSProperty as having a reference to a custom property. Needed to
  // disable the matched property cache in some cases.
  void MarkHasVariableReference(const CSSProperty&);

  const Document& GetDocument() const;

  bool HasAuthorDeclaration(const CSSProperty&) const;
  bool HasAuthorBorder() const;
  bool HasAuthorBackground() const;

  StyleResolverState& state_;
  CascadeMap map_;
  // Generational Apply
  //
  // Generation is a number that's incremented by one for each call to Apply
  // (the first call to Apply has generation 1). When a declaration is applied
  // to ComputedStyle, the current Apply-generation is stored in the CascadeMap.
  // In other words, the CascadeMap knows which declarations have already been
  // applied to ComputedStyle, which makes it possible to avoid applying the
  // same declaration twice during a single call to Apply:
  //
  // For example:
  //
  //   --x: red;
  //   background-color: var(--x);
  //
  // During Apply (generation=1), we linearly traverse the declarations above,
  // and first apply '--x' to the ComputedStyle. Then, we proceed to
  // 'background-color', which must first have its dependencies resolved before
  // we can apply it. This is where we check the current generation stored for
  // '--x'. If it's equal to the generation associated with the Apply call, we
  // know that we already applied it. Either something else referenced it before
  // we did, or it appeared before us in the MatchResult. Either way, we don't
  // have to apply '--x' again.
  //
  // Had the order been reversed, such that the '--x' declaration appeared after
  // the 'background-color' declaration, we would discover (during resolution of
  // var(--x), that the current generation of '--x' is _less_ than the
  // generation associated with the Apply call, hence we need to LookupAndApply
  // '--x' before applying 'background-color'.
  //
  // A secondary benefit to the generational apply mechanic, is that it's
  // possible to efficiently apply the StyleCascade more than once (perhaps with
  // a different CascadeFilter for each call), without rebuilding it. By
  // incrementing generation_, the existing record of what has been applied is
  // immediately invalidated, and everything will be applied again.
  //
  // Note: The maximum generation number is currently 15. This is more than
  //       enough for our needs.
  uint8_t generation_ = 0;

  DISALLOW_COPY_AND_ASSIGN(StyleCascade);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_STYLE_CASCADE_H_
