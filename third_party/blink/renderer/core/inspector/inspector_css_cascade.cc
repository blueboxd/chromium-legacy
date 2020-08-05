// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_css_cascade.h"

#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

InspectorCSSCascade::InspectorCSSCascade(Element* element,
                                         PseudoId element_pseudo_id)
    : element_(element) {
  DCHECK(element_);

  // Update style and layout tree for collecting an up-to-date set of rules
  // and animations.
  element_->GetDocument().UpdateStyleAndLayoutTreeForNode(element_);

  // FIXME: It's really gross for the inspector to reach in and access
  // StyleResolver directly here. We need to provide the Inspector better APIs
  // to get this information without grabbing at internal style classes!
  StyleResolver& style_resolver = element_->GetDocument().GetStyleResolver();

  matched_rules_ = style_resolver.PseudoCSSRulesForElement(
      element_, element_pseudo_id, StyleResolver::kAllCSSRules);

  if (element_pseudo_id)
    return;

  for (PseudoId pseudo_id = kFirstPublicPseudoId;
       pseudo_id < kAfterLastInternalPseudoId;
       pseudo_id = static_cast<PseudoId>(pseudo_id + 1)) {
    if (!PseudoElement::IsWebExposed(pseudo_id, element_))
      continue;
    // If the pseudo-element doesn't exist, exclude UA rules to avoid cluttering
    // all elements.
    unsigned rules_to_include = element_->GetPseudoElement(pseudo_id)
                                    ? StyleResolver::kAllCSSRules
                                    : StyleResolver::kAllButUACSSRules;
    RuleIndexList* matched_rules = style_resolver.PseudoCSSRulesForElement(
        element_, pseudo_id, rules_to_include);
    if (matched_rules && matched_rules->size()) {
      InspectorCSSMatchedRules* match =
          MakeGarbageCollected<InspectorCSSMatchedRules>();
      match->element = element_;
      match->matched_rules = matched_rules;
      match->pseudo_id = pseudo_id;
      pseudo_element_rules_.push_back(match);
    }
  }

  // Parent rules.
  Element* parent_element = FlatTreeTraversal::ParentElement(*element);
  while (parent_element) {
    RuleIndexList* parent_matched_rules = style_resolver.CssRulesForElement(
        parent_element, StyleResolver::kAllCSSRules);
    InspectorCSSMatchedRules* match =
        MakeGarbageCollected<InspectorCSSMatchedRules>();
    match->element = parent_element;
    match->matched_rules = parent_matched_rules;
    match->pseudo_id = kPseudoIdNone;
    parent_rules_.push_back(match);
    parent_element = FlatTreeTraversal::ParentElement(*parent_element);
  }
}

RuleIndexList* InspectorCSSCascade::MatchedRules() const {
  return matched_rules_;
}

HeapVector<Member<InspectorCSSMatchedRules>>
InspectorCSSCascade::PseudoElementRules() {
  return pseudo_element_rules_;
}

HeapVector<Member<InspectorCSSMatchedRules>>
InspectorCSSCascade::ParentRules() {
  return parent_rules_;
}

// This method does not handle different style origins/CSS variables/rule
// priorities.
// TODO (alexrudenko): Use real cascaded value (blocked on a better StyleEngine
// API).
const CSSValue* InspectorCSSCascade::GetCascadedProperty(
    CSSPropertyID property_id) const {
  DCHECK_NE(property_id, CSSPropertyID::kVariable);
  if (const CSSValue* result =
          GetPropertyValueFromStyle(element_->style(), property_id)) {
    return result;
  }

  if (const CSSValue* result =
          GetPropertyValueFromRuleIndexList(MatchedRules(), property_id)) {
    return result;
  }

  return nullptr;
}

const CSSValue* InspectorCSSCascade::GetPropertyValueFromStyle(
    CSSStyleDeclaration* style,
    CSSPropertyID property_id) const {
  if (!style)
    return nullptr;
  return style->GetPropertyCSSValueInternal(property_id);
}

const CSSValue* InspectorCSSCascade::GetPropertyValueFromRuleIndexList(
    RuleIndexList* list,
    CSSPropertyID property_id) const {
  for (auto it = list->rbegin(); it != list->rend(); ++it) {
    CSSRule* rule = it->first;
    CSSStyleRule* style_rule = DynamicTo<CSSStyleRule>(rule);
    if (!style_rule)
      continue;
    if (const CSSValue* result =
            GetPropertyValueFromStyle(style_rule->style(), property_id)) {
      return result;
    }
  }
  return nullptr;
}

}  // namespace blink
