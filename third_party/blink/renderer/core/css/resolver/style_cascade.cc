// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/style_cascade.h"

#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/animation/css_interpolation_environment.h"
#include "third_party/blink/renderer/core/animation/css_interpolation_types_map.h"
#include "third_party/blink/renderer/core/animation/invalidatable_interpolation.h"
#include "third_party/blink/renderer/core/animation/property_handle.h"
#include "third_party/blink/renderer/core/animation/transition_interpolation.h"
#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/css_invalid_variable_value.h"
#include "third_party/blink/renderer/core/css/css_pending_substitution_value.h"
#include "third_party/blink/renderer/core/css/css_unset_value.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css/css_variable_reference_value.h"
#include "third_party/blink/renderer/core/css/document_style_environment_variables.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_expansion.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_interpolations.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/css_property_priority.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

AtomicString ConsumeVariableName(CSSParserTokenRange& range) {
  range.ConsumeWhitespace();
  CSSParserToken ident_token = range.ConsumeIncludingWhitespace();
  DCHECK_EQ(ident_token.GetType(), kIdentToken);
  return ident_token.Value().ToAtomicString();
}

bool ConsumeComma(CSSParserTokenRange& range) {
  if (range.Peek().GetType() == kCommaToken) {
    range.Consume();
    return true;
  }
  return false;
}

const CSSValue* Parse(const CSSProperty& property,
                      CSSParserTokenRange range,
                      const CSSParserContext* context) {
  return CSSPropertyParser::ParseSingleValue(property.PropertyID(), range,
                                             context);
}

uint32_t EncodeInterpolationPosition(size_t index,
                                     bool is_presentation_attribute) {
  // Our tests currently expect css properties to win over presentation
  // attributes. Hence, we borrow a bit in the position-uint32_t for this
  // purpose.
  return (static_cast<uint32_t>(!is_presentation_attribute) << 16) |
         static_cast<uint16_t>(index);
}

size_t DecodeInterpolationIndex(uint32_t position) {
  return position & 0xFFFF;
}

bool DecodeIsPresentationAttribute(uint32_t position) {
  return (~position >> 16) & 1;
}

const CSSValue* ValueAt(const MatchResult& result, uint32_t position) {
  size_t matched_properties_index = DecodeMatchedPropertiesIndex(position);
  size_t declaration_index = DecodeDeclarationIndex(position);
  const MatchedPropertiesVector& vector = result.GetMatchedProperties();
  const CSSPropertyValueSet* set = vector[matched_properties_index].properties;
  return &set->PropertyAt(declaration_index).Value();
}

PropertyHandle ToPropertyHandle(const CSSProperty& property,
                                CascadePriority priority) {
  if (IsA<CustomProperty>(property))
    return PropertyHandle(property.GetPropertyNameAtomicString());
  uint32_t position = priority.GetPosition();
  return PropertyHandle(property, DecodeIsPresentationAttribute(position));
}

}  // namespace

MatchResult& StyleCascade::MutableMatchResult() {
  needs_match_result_analyze_ = true;
  return match_result_;
}

void StyleCascade::AddInterpolations(const ActiveInterpolationsMap* map,
                                     CascadeOrigin origin) {
  DCHECK(map);
  needs_interpolations_analyze_ = true;
  interpolations_.Add(map, origin);
}

void StyleCascade::Apply(CascadeFilter filter) {
  AnalyzeIfNeeded();

  CascadeResolver resolver(filter, ++generation_);

  // Affects the computed value of 'color', hence needs to happen before
  // high-priority properties.
  LookupAndApply(GetCSSPropertyColorScheme(), resolver);

  ApplyWebkitBorderImage(resolver);

  // -webkit-mask-image needs to be applied before -webkit-mask-composite,
  // otherwise -webkit-mask-composite has no effect.
  LookupAndApply(GetCSSPropertyWebkitMaskImage(), resolver);

  ApplyHighPriority(resolver);

  ApplyMatchResult(resolver);
  ApplyInterpolations(resolver);

  if (map_.Find(CSSPropertyName(CSSPropertyID::kWebkitAppearance)) &&
      !resolver.filter_.Rejects(GetCSSPropertyWebkitAppearance()) &&
      state_.Style()->HasAppearance()) {
    state_.Style()->SetHasAuthorBackground(HasAuthorBackground());
    state_.Style()->SetHasAuthorBorder(HasAuthorBorder());
  }
}

void StyleCascade::Reset() {
  map_.Reset();
  match_result_.Reset();
  interpolations_.Reset();
  generation_ = 0;
}

const CSSValue* StyleCascade::Resolve(const CSSPropertyName& name,
                                      const CSSValue& value,
                                      CascadeResolver& resolver) {
  CSSPropertyRef ref(name, state_.GetDocument());

  const CSSValue* resolved = Resolve(ref.GetProperty(), value, resolver);

  DCHECK(resolved);

  if (resolved->IsInvalidVariableValue())
    return nullptr;

  return resolved;
}

void StyleCascade::AnalyzeIfNeeded() {
  if (needs_match_result_analyze_) {
    AnalyzeMatchResult();
    needs_match_result_analyze_ = false;
  }
  if (needs_interpolations_analyze_) {
    AnalyzeInterpolations();
    needs_interpolations_analyze_ = true;
  }
}

void StyleCascade::AnalyzeMatchResult() {
  for (auto e : match_result_.Expansions(GetDocument(), CascadeFilter())) {
    for (; !e.AtEnd(); e.Next())
      map_.Add(e.Name(), e.Priority());
  }
}

void StyleCascade::AnalyzeInterpolations() {
  const auto& entries = interpolations_.GetEntries();
  for (size_t i = 0; i < entries.size(); ++i) {
    for (const auto& active_interpolation : *entries[i].map) {
      uint32_t position = EncodeInterpolationPosition(
          i, active_interpolation.key.IsPresentationAttribute());
      CascadePriority priority(entries[i].origin, false, 0, position);

      auto name = active_interpolation.key.GetCSSPropertyName();
      CSSPropertyRef ref(name, GetDocument());
      DCHECK(ref.IsValid());
      const CSSProperty& property = ref.GetProperty();

      map_.Add(name, priority);

      // Since an interpolation for an unvisited property also causes an
      // interpolation of the visited property, add the visited property to
      // the map as well.
      // TODO(crbug.com/1062217): Interpolate visited colors separately
      if (const CSSProperty* visited = property.GetVisitedProperty())
        map_.Add(visited->GetCSSPropertyName(), priority);
    }
  }
}

void StyleCascade::ApplyHighPriority(CascadeResolver& resolver) {
  uint64_t bits = map_.HighPriorityBits();

  if (bits) {
    using HighPriority = CSSPropertyPriorityData<kHighPropertyPriority>;
    int first = static_cast<int>(HighPriority::First());
    int last = static_cast<int>(HighPriority::Last());
    for (int i = first; i <= last; ++i) {
      if (bits & (static_cast<uint64_t>(1) << i))
        LookupAndApply(CSSProperty::Get(convertToCSSPropertyID(i)), resolver);
    }
  }

  state_.GetFontBuilder().CreateFont(
      state_.StyleRef());
  state_.SetConversionFontSizes(CSSToLengthConversionData::FontSizes(
      state_.Style(), state_.RootElementStyle()));
  state_.SetConversionZoom(state_.Style()->EffectiveZoom());

  // Force color-scheme sensitive initial color for the document element,
  // if no value is present in the cascade.
  //
  // TODO(crbug.com/1046753): This should be unnecessary when canvastext is
  // supported.
  uint64_t color_bit = 1ull << static_cast<uint64_t>(CSSPropertyID::kColor);
  if (~bits & color_bit) {
    if (state_.GetElement() == GetDocument().documentElement())
      state_.Style()->SetColor(state_.Style()->InitialColorForColorScheme());
  }
}

void StyleCascade::ApplyWebkitBorderImage(CascadeResolver& resolver) {
  const CascadePriority* priority =
      map_.Find(CSSPropertyName(CSSPropertyID::kWebkitBorderImage));
  if (!priority)
    return;

  // -webkit-border-image is a surrogate for the border-image (shorthand).
  // By applying -webkit-border-image first, we avoid having to "partially"
  // apply -webkit-border-image depending on the border-image-* longhands that
  // have already been applied.
  // See also crbug.com/1056600
  LookupAndApply(GetCSSPropertyWebkitBorderImage(), resolver);

  const auto& shorthand = borderImageShorthand();
  const CSSProperty** longhands = shorthand.properties();
  for (unsigned i = 0; i < shorthand.length(); ++i) {
    const CSSProperty& longhand = *longhands[i];
    if (CascadePriority* p = map_.Find(longhand.GetCSSPropertyName())) {
      // If -webkit-border-image has higher priority than a border-image
      // longhand, we skip applying that longhand.
      if (*p < *priority)
        *p = CascadePriority(*p, resolver.generation_);
    }
  }
}

void StyleCascade::ApplyMatchResult(CascadeResolver& resolver) {
  for (auto e : match_result_.Expansions(GetDocument(), resolver.filter_)) {
    for (; !e.AtEnd(); e.Next()) {
      auto priority = CascadePriority(e.Priority(), resolver.generation_);
      CascadePriority* p = map_.Find(e.Name());
      if (!p || *p >= priority)
        continue;
      *p = priority;
      const CSSProperty& property = e.Property();
      if (property.IsSurrogate()) {
        ApplySurrogate(property, priority, resolver);
        continue;
      }
      const CSSValue* value = Resolve(property, e.Value(), resolver);
      StyleBuilder::ApplyProperty(property, state_, *value);
    }
  }
}

void StyleCascade::ApplyInterpolations(CascadeResolver& resolver) {
  const auto& entries = interpolations_.GetEntries();
  for (size_t i = 0; i < entries.size(); ++i) {
    const auto& entry = entries[i];
    ApplyInterpolationMap(*entry.map, entry.origin, i, resolver);
  }
}

void StyleCascade::ApplyInterpolationMap(const ActiveInterpolationsMap& map,
                                         CascadeOrigin origin,
                                         size_t index,
                                         CascadeResolver& resolver) {
  for (const auto& entry : map) {
    auto name = entry.key.GetCSSPropertyName();
    uint32_t position =
        EncodeInterpolationPosition(index, entry.key.IsPresentationAttribute());
    CascadePriority priority(origin, false, 0, position);
    priority = CascadePriority(priority, resolver.generation_);

    CSSPropertyRef ref(name, GetDocument());
    const CSSProperty& property = ref.GetProperty();
    if (resolver.filter_.Rejects(property))
      continue;

    CascadePriority* p = map_.Find(name);
    if (!p || *p >= priority) {
      if (p->IsImportant())
        state_.SetHasImportantOverrides();
      continue;
    }
    *p = priority;

    if (property.IsSurrogate()) {
      ApplySurrogate(property, priority, resolver);
      continue;
    }

    ApplyInterpolation(property, priority, entry.value, resolver);
  }
}

void StyleCascade::ApplyInterpolation(
    const CSSProperty& property,
    CascadePriority priority,
    const ActiveInterpolations& interpolations,
    CascadeResolver& resolver) {
  const Interpolation& interpolation = *interpolations.front();
  if (IsA<InvalidatableInterpolation>(interpolation)) {
    CSSInterpolationTypesMap map(state_.GetDocument().GetPropertyRegistry(),
                                 state_.GetDocument());
    CSSInterpolationEnvironment environment(map, state_, this, &resolver);
    InvalidatableInterpolation::ApplyStack(interpolations, environment);
  } else {
    To<TransitionInterpolation>(interpolation).Apply(state_);
  }

  if (property.AffectsFont())
    state_.SetHasFontAffectingAnimation();

  // Applying a color property interpolation will also unconditionally apply
  // the -internal-visited- counterpart (see CSSColorInterpolationType::
  // ApplyStandardPropertyValue). To make sure !important rules in :visited
  // selectors win over animations, we re-apply the -internal-visited property
  // if its priority is higher.
  //
  // TODO(crbug.com/1062217): Interpolate visited colors separately
  if (const CSSProperty* visited = property.GetVisitedProperty()) {
    CascadePriority* visited_priority =
        map_.Find(visited->GetCSSPropertyName());
    if (visited_priority && priority < *visited_priority) {
      DCHECK(visited_priority->IsImportant());
      state_.SetHasImportantOverrides();
      // Resetting generation to zero makes it possible to apply the
      // visited property again.
      *visited_priority = CascadePriority(*visited_priority, 0);
      LookupAndApply(*visited, resolver);
    }
  }
}

void StyleCascade::ApplySurrogate(const CSSProperty& surrogate,
                                  CascadePriority surrogate_priority,
                                  CascadeResolver& resolver) {
  DCHECK(surrogate.IsSurrogate());

  const CSSProperty& original = SurrogateFor(surrogate);
  CascadePriority* original_priority = map_.Find(original.GetCSSPropertyName());

  if (original_priority) {
    if (surrogate_priority < *original_priority) {
      // The original has a higher priority, so skip the surrogate property.
      return;
    }

    // The surrogate has a higher priority, so skip the original property.
    // The original might have been applied already, but that doesn't matter,
    // as we're about to overwrite it.
    resolver.MarkApplied(original_priority);
  }

  LookupAndApplyValue(surrogate, surrogate_priority, resolver);
}

void StyleCascade::LookupAndApply(const CSSPropertyName& name,
                                  CascadeResolver& resolver) {
  CSSPropertyRef ref(name, state_.GetDocument());
  DCHECK(ref.IsValid());
  LookupAndApply(ref.GetProperty(), resolver);
}

void StyleCascade::LookupAndApply(const CSSProperty& property,
                                  CascadeResolver& resolver) {
  CSSPropertyName name = property.GetCSSPropertyName();
  DCHECK(!resolver.IsLocked(name));

  CascadePriority* p = map_.Find(name);
  if (!p)
    return;
  CascadePriority priority(*p, resolver.generation_);
  if (*p >= priority)
    return;
  *p = priority;

  if (resolver.filter_.Rejects(property))
    return;
  if (property.IsSurrogate()) {
    ApplySurrogate(property, priority, resolver);
    return;
  }

  LookupAndApplyValue(property, priority, resolver);
}

void StyleCascade::LookupAndApplyValue(const CSSProperty& property,
                                       CascadePriority priority,
                                       CascadeResolver& resolver) {
  if (priority.GetOrigin() < CascadeOrigin::kAnimation)
    LookupAndApplyDeclaration(property, priority, resolver);
  else if (priority.GetOrigin() >= CascadeOrigin::kAnimation)
    LookupAndApplyInterpolation(property, priority, resolver);
}

void StyleCascade::LookupAndApplyDeclaration(const CSSProperty& property,
                                             CascadePriority priority,
                                             CascadeResolver& resolver) {
  DCHECK(priority.GetOrigin() < CascadeOrigin::kAnimation);
  const CSSValue* value = ValueAt(match_result_, priority.GetPosition());
  DCHECK(value);
  value = Resolve(property, *value, resolver);
  DCHECK(!value->IsVariableReferenceValue());
  DCHECK(!value->IsPendingSubstitutionValue());
  StyleBuilder::ApplyProperty(property, state_, *value);
}

void StyleCascade::LookupAndApplyInterpolation(const CSSProperty& property,
                                               CascadePriority priority,
                                               CascadeResolver& resolver) {
  // Interpolations for -internal-visited properties are applied via the
  // interpolation for the main (unvisited) property, so we don't need to
  // apply it twice.
  // TODO(crbug.com/1062217): Interpolate visited colors separately
  if (property.IsVisited())
    return;
  DCHECK(priority.GetOrigin() >= CascadeOrigin::kAnimation);
  size_t index = DecodeInterpolationIndex(priority.GetPosition());
  DCHECK_LE(index, interpolations_.GetEntries().size());
  const ActiveInterpolationsMap& map = *interpolations_.GetEntries()[index].map;
  PropertyHandle handle = ToPropertyHandle(property, priority);
  const auto& entry = map.find(handle);
  DCHECK_NE(entry, map.end());
  ApplyInterpolation(property, priority, entry->value, resolver);
}

bool StyleCascade::IsRootElement() const {
  return &state_.GetElement() == state_.GetDocument().documentElement();
}

StyleCascade::TokenSequence::TokenSequence(const CSSVariableData* data)
    : backing_strings_(data->BackingStrings()),
      is_animation_tainted_(data->IsAnimationTainted()),
      has_font_units_(data->HasFontUnits()),
      has_root_font_units_(data->HasRootFontUnits()),
      base_url_(data->BaseURL()),
      charset_(data->Charset()) {}

void StyleCascade::TokenSequence::Append(const TokenSequence& sequence) {
  tokens_.AppendVector(sequence.tokens_);
  backing_strings_.AppendVector(sequence.backing_strings_);
  is_animation_tainted_ |= sequence.is_animation_tainted_;
  has_font_units_ |= sequence.has_font_units_;
  has_root_font_units_ |= sequence.has_root_font_units_;
}

void StyleCascade::TokenSequence::Append(const CSSVariableData* data) {
  tokens_.AppendVector(data->Tokens());
  backing_strings_.AppendVector(data->BackingStrings());
  is_animation_tainted_ |= data->IsAnimationTainted();
  has_font_units_ |= data->HasFontUnits();
  has_root_font_units_ |= data->HasRootFontUnits();
}

void StyleCascade::TokenSequence::Append(const CSSParserToken& token) {
  tokens_.push_back(token);
}

scoped_refptr<CSSVariableData>
StyleCascade::TokenSequence::BuildVariableData() {
  // TODO(andruud): Why not also std::move tokens?
  const bool absolutized = true;
  return CSSVariableData::CreateResolved(
      tokens_, std::move(backing_strings_), is_animation_tainted_,
      has_font_units_, has_root_font_units_, absolutized, base_url_, charset_);
}

const CSSValue* StyleCascade::Resolve(const CSSProperty& property,
                                      const CSSValue& value,
                                      CascadeResolver& resolver) {
  if (const auto* v = DynamicTo<CSSCustomPropertyDeclaration>(value))
    return ResolveCustomProperty(property, *v, resolver);
  if (const auto* v = DynamicTo<CSSVariableReferenceValue>(value))
    return ResolveVariableReference(property, *v, resolver);
  if (const auto* v = DynamicTo<cssvalue::CSSPendingSubstitutionValue>(value))
    return ResolvePendingSubstitution(property, *v, resolver);
  return &value;
}

const CSSValue* StyleCascade::ResolveCustomProperty(
    const CSSProperty& property,
    const CSSCustomPropertyDeclaration& decl,
    CascadeResolver& resolver) {
  DCHECK(!resolver.IsLocked(property));
  CascadeResolver::AutoLock lock(property, resolver);

  // TODO(andruud): Don't transport css-wide keywords in this value.
  if (!decl.Value())
    return &decl;

  scoped_refptr<CSSVariableData> data = decl.Value();

  if (data->NeedsVariableResolution())
    data = ResolveVariableData(data.get(), resolver);

  if (HasFontSizeDependency(To<CustomProperty>(property), data.get()))
    resolver.DetectCycle(GetCSSPropertyFontSize());

  if (resolver.InCycle())
    return CSSInvalidVariableValue::Create();

  if (!data)
    return cssvalue::CSSUnsetValue::Create();

  if (data == decl.Value())
    return &decl;

  return MakeGarbageCollected<CSSCustomPropertyDeclaration>(decl.GetName(),
                                                            data);
}

const CSSValue* StyleCascade::ResolveVariableReference(
    const CSSProperty& property,
    const CSSVariableReferenceValue& value,
    CascadeResolver& resolver) {
  DCHECK(!resolver.IsLocked(property));
  CascadeResolver::AutoLock lock(property, resolver);

  const CSSVariableData* data = value.VariableDataValue();
  const CSSParserContext* context = GetParserContext(value);

  MarkHasVariableReference(property);

  DCHECK(data);
  DCHECK(context);

  TokenSequence sequence;

  if (ResolveTokensInto(data->Tokens(), resolver, sequence)) {
    if (const auto* parsed = Parse(property, sequence.TokenRange(), context))
      return parsed;
  }

  return cssvalue::CSSUnsetValue::Create();
}

const CSSValue* StyleCascade::ResolvePendingSubstitution(
    const CSSProperty& property,
    const cssvalue::CSSPendingSubstitutionValue& value,
    CascadeResolver& resolver) {
  DCHECK(!resolver.IsLocked(property));
  CascadeResolver::AutoLock lock(property, resolver);

  CascadePriority priority = map_.At(property.GetCSSPropertyName());
  DCHECK_NE(property.PropertyID(), CSSPropertyID::kVariable);
  DCHECK_NE(priority.GetOrigin(), CascadeOrigin::kNone);

  MarkHasVariableReference(property);

  // If the previous call to ResolvePendingSubstitution parsed 'value', then
  // we don't need to do it again.
  bool is_cached = resolver.shorthand_cache_.value == &value;

  if (!is_cached) {
    CSSVariableReferenceValue* shorthand_value = value.ShorthandValue();
    const auto* shorthand_data = shorthand_value->VariableDataValue();
    CSSPropertyID shorthand_property_id = value.ShorthandPropertyId();

    TokenSequence sequence;

    if (!ResolveTokensInto(shorthand_data->Tokens(), resolver, sequence))
      return cssvalue::CSSUnsetValue::Create();

    HeapVector<CSSPropertyValue, 256> parsed_properties;
    const bool important = false;

    if (!CSSPropertyParser::ParseValue(
            shorthand_property_id, important, sequence.TokenRange(),
            shorthand_value->ParserContext(), parsed_properties,
            StyleRule::RuleType::kStyle)) {
      return cssvalue::CSSUnsetValue::Create();
    }

    resolver.shorthand_cache_.value = &value;
    resolver.shorthand_cache_.parsed_properties = std::move(parsed_properties);
  }

  const auto& parsed_properties = resolver.shorthand_cache_.parsed_properties;

  // For -internal-visited-properties with CSSPendingSubstitutionValues,
  // the inner 'shorthand_property_id' will expand to a set of longhands
  // containing the unvisited equivalent. Hence, when parsing the
  // CSSPendingSubstitutionValue, we look for the unvisited property in
  // parsed_properties.
  const CSSProperty* unvisited_property =
      property.IsVisited() ? property.GetUnvisitedProperty() : &property;

  unsigned parsed_properties_count = parsed_properties.size();
  for (unsigned i = 0; i < parsed_properties_count; ++i) {
    const CSSProperty& longhand = CSSProperty::Get(parsed_properties[i].Id());
    const CSSValue* parsed = parsed_properties[i].Value();

    if (unvisited_property == &longhand)
      return parsed;
  }

  NOTREACHED();
  return cssvalue::CSSUnsetValue::Create();
}

scoped_refptr<CSSVariableData> StyleCascade::ResolveVariableData(
    CSSVariableData* data,
    CascadeResolver& resolver) {
  DCHECK(data && data->NeedsVariableResolution());

  TokenSequence sequence(data);

  if (!ResolveTokensInto(data->Tokens(), resolver, sequence))
    return nullptr;

  return sequence.BuildVariableData();
}

bool StyleCascade::ResolveTokensInto(CSSParserTokenRange range,
                                     CascadeResolver& resolver,
                                     TokenSequence& out) {
  bool success = true;
  while (!range.AtEnd()) {
    const CSSParserToken& token = range.Peek();
    if (token.FunctionId() == CSSValueID::kVar)
      success &= ResolveVarInto(range.ConsumeBlock(), resolver, out);
    else if (token.FunctionId() == CSSValueID::kEnv)
      success &= ResolveEnvInto(range.ConsumeBlock(), resolver, out);
    else
      out.Append(range.Consume());
  }
  return success;
}

bool StyleCascade::ResolveVarInto(CSSParserTokenRange range,
                                  CascadeResolver& resolver,
                                  TokenSequence& out) {
  AtomicString variable_name = ConsumeVariableName(range);
  DCHECK(range.AtEnd() || (range.Peek().GetType() == kCommaToken));

  CustomProperty property(variable_name, state_.GetDocument());

  // Any custom property referenced (by anything, even just once) in the
  // document can currently not be animated on the compositor. Hence we mark
  // properties that have been referenced.
  MarkIsReferenced(property);

  if (!resolver.DetectCycle(property)) {
    // We are about to substitute var(property). In order to do that, we must
    // know the computed value of 'property', hence we Apply it.
    //
    // We can however not do this if we're in a cycle. If a cycle is detected
    // here, it means we are already resolving 'property', and have discovered
    // a reference to 'property' during that resolution.
    LookupAndApply(property, resolver);
  }

  // Note that even if we are in a cycle, we must proceed in order to discover
  // secondary cycles via the var() fallback.

  scoped_refptr<CSSVariableData> data = GetVariableData(property);

  // If substitution is not allowed, treat the value as
  // invalid-at-computed-value-time.
  //
  // https://drafts.csswg.org/css-variables/#animation-tainted
  if (!resolver.AllowSubstitution(data.get()))
    data = nullptr;

  // If we have a fallback, we must process it to look for cycles,
  // even if we aren't going to use the fallback.
  //
  // https://drafts.csswg.org/css-variables/#cycles
  if (ConsumeComma(range)) {
    TokenSequence fallback;
    bool success = ResolveTokensInto(range, resolver, fallback);
    // The fallback must match the syntax of the referenced custom property.
    // https://drafts.css-houdini.org/css-properties-values-api-1/#fallbacks-in-var-references
    if (!ValidateFallback(property, fallback.TokenRange()))
      return false;
    if (!data && success)
      data = fallback.BuildVariableData();
  }

  if (!data || resolver.InCycle())
    return false;

  // https://drafts.csswg.org/css-variables/#long-variables
  if (data->Tokens().size() > kMaxSubstitutionTokens)
    return false;

  out.Append(data.get());

  return true;
}

bool StyleCascade::ResolveEnvInto(CSSParserTokenRange range,
                                  CascadeResolver& resolver,
                                  TokenSequence& out) {
  AtomicString variable_name = ConsumeVariableName(range);
  DCHECK(range.AtEnd() || (range.Peek().GetType() == kCommaToken));

  CSSVariableData* data = GetEnvironmentVariable(variable_name);

  if (!data) {
    if (ConsumeComma(range))
      return ResolveTokensInto(range, resolver, out);
    return false;
  }

  out.Append(data);

  return true;
}

CSSVariableData* StyleCascade::GetVariableData(
    const CustomProperty& property) const {
  const AtomicString& name = property.GetPropertyNameAtomicString();
  const bool is_inherited = property.IsInherited();
  return state_.StyleRef().GetVariableData(name, is_inherited);
}

CSSVariableData* StyleCascade::GetEnvironmentVariable(
    const AtomicString& name) const {
  // If we are in a User Agent Shadow DOM then we should not record metrics.
  ContainerNode& scope_root = state_.GetTreeScope().RootNode();
  auto* shadow_root = DynamicTo<ShadowRoot>(&scope_root);
  bool is_ua_scope = shadow_root && shadow_root->IsUserAgent();

  return state_.GetDocument()
      .GetStyleEngine()
      .EnsureEnvironmentVariables()
      .ResolveVariable(name, !is_ua_scope);
}

const CSSParserContext* StyleCascade::GetParserContext(
    const CSSVariableReferenceValue& value) {
  // TODO(crbug.com/985028): CSSVariableReferenceValue should always have a
  // CSSParserContext. (CSSUnparsedValue violates this).
  if (value.ParserContext())
    return value.ParserContext();
  return StrictCSSParserContext(state_.GetDocument().GetSecureContextMode());
}

bool StyleCascade::HasFontSizeDependency(const CustomProperty& property,
                                         CSSVariableData* data) const {
  if (!property.IsRegistered() || !data)
    return false;
  if (data->HasFontUnits())
    return true;
  if (data->HasRootFontUnits() && IsRootElement())
    return true;
  return false;
}

bool StyleCascade::ValidateFallback(const CustomProperty& property,
                                    CSSParserTokenRange range) const {
  if (!property.IsRegistered())
    return true;
  auto context_mode = state_.GetDocument().GetSecureContextMode();
  auto var_mode = CSSParserLocalContext::VariableMode::kTyped;
  auto* context = StrictCSSParserContext(context_mode);
  auto local_context = CSSParserLocalContext().WithVariableMode(var_mode);
  return property.ParseSingleValue(range, *context, local_context);
}

void StyleCascade::MarkIsReferenced(const CustomProperty& property) {
  if (!property.IsRegistered())
    return;
  const AtomicString& name = property.GetPropertyNameAtomicString();
  state_.GetDocument().GetPropertyRegistry()->MarkReferenced(name);
}

void StyleCascade::MarkHasVariableReference(const CSSProperty& property) {
  if (!property.IsInherited())
    state_.Style()->SetHasVariableReferenceFromNonInheritedProperty();
}

const Document& StyleCascade::GetDocument() const {
  return state_.GetDocument();
}

const CSSProperty& StyleCascade::SurrogateFor(
    const CSSProperty& surrogate) const {
  DCHECK(surrogate.IsSurrogate());
  const CSSProperty* original = surrogate.SurrogateFor(
      state_.Style()->Direction(), state_.Style()->GetWritingMode());
  DCHECK(original);
  return *original;
}

bool StyleCascade::HasAuthorDeclaration(const CSSProperty& property) const {
  return map_.At(property.GetCSSPropertyName()).GetOrigin() ==
         CascadeOrigin::kAuthor;
}

bool StyleCascade::HasAuthorBorder() const {
  return HasAuthorDeclaration(GetCSSPropertyBorderBottomColor()) ||
         HasAuthorDeclaration(GetCSSPropertyBorderBottomLeftRadius()) ||
         HasAuthorDeclaration(GetCSSPropertyBorderBottomRightRadius()) ||
         HasAuthorDeclaration(GetCSSPropertyBorderBottomStyle()) ||
         HasAuthorDeclaration(GetCSSPropertyBorderBottomWidth()) ||
         HasAuthorDeclaration(GetCSSPropertyBorderImageOutset()) ||
         HasAuthorDeclaration(GetCSSPropertyBorderImageRepeat()) ||
         HasAuthorDeclaration(GetCSSPropertyBorderImageSlice()) ||
         HasAuthorDeclaration(GetCSSPropertyBorderImageSource()) ||
         HasAuthorDeclaration(GetCSSPropertyBorderImageWidth()) ||
         HasAuthorDeclaration(GetCSSPropertyBorderLeftColor()) ||
         HasAuthorDeclaration(GetCSSPropertyBorderLeftStyle()) ||
         HasAuthorDeclaration(GetCSSPropertyBorderLeftWidth()) ||
         HasAuthorDeclaration(GetCSSPropertyBorderRightColor()) ||
         HasAuthorDeclaration(GetCSSPropertyBorderRightStyle()) ||
         HasAuthorDeclaration(GetCSSPropertyBorderRightWidth()) ||
         HasAuthorDeclaration(GetCSSPropertyBorderTopColor()) ||
         HasAuthorDeclaration(GetCSSPropertyBorderTopLeftRadius()) ||
         HasAuthorDeclaration(GetCSSPropertyBorderTopRightRadius()) ||
         HasAuthorDeclaration(GetCSSPropertyBorderTopStyle()) ||
         HasAuthorDeclaration(GetCSSPropertyBorderTopWidth());
}

bool StyleCascade::HasAuthorBackground() const {
  return HasAuthorDeclaration(GetCSSPropertyBackgroundAttachment()) ||
         HasAuthorDeclaration(GetCSSPropertyBackgroundBlendMode()) ||
         HasAuthorDeclaration(GetCSSPropertyBackgroundClip()) ||
         HasAuthorDeclaration(GetCSSPropertyBackgroundColor()) ||
         HasAuthorDeclaration(GetCSSPropertyBackgroundImage()) ||
         HasAuthorDeclaration(GetCSSPropertyBackgroundOrigin()) ||
         HasAuthorDeclaration(GetCSSPropertyBackgroundPositionX()) ||
         HasAuthorDeclaration(GetCSSPropertyBackgroundPositionY()) ||
         HasAuthorDeclaration(GetCSSPropertyBackgroundSize());
}

}  // namespace blink
