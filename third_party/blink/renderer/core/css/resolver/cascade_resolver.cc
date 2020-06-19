// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/cascade_resolver.h"

#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/properties/longhands/custom_property.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_priority.h"

namespace blink {

bool CascadeResolver::IsLocked(const CSSProperty& property) const {
  return Find(property) != kNotFound;
}

bool CascadeResolver::AllowSubstitution(CSSVariableData* data) const {
  if (data && data->IsAnimationTainted() && stack_.size()) {
    const CSSProperty* property = CurrentProperty();
    if (IsA<CustomProperty>(*property))
      return true;
    return !CSSAnimations::IsAnimationAffectingProperty(*property);
  }
  return true;
}

void CascadeResolver::MarkUnapplied(CascadePriority* priority) const {
  DCHECK(priority);
  *priority = CascadePriority(*priority, 0);
}

void CascadeResolver::MarkApplied(CascadePriority* priority) const {
  DCHECK(priority);
  *priority = CascadePriority(*priority, generation_);
}

bool CascadeResolver::DetectCycle(const CSSProperty& property) {
  wtf_size_t index = Find(property);
  if (index == kNotFound)
    return false;
  cycle_depth_ = std::min(cycle_depth_, index);
  return true;
}

bool CascadeResolver::InCycle() const {
  return cycle_depth_ != kNotFound;
}

wtf_size_t CascadeResolver::Find(const CSSProperty& property) const {
  wtf_size_t index = 0;
  for (const CSSProperty* p : stack_) {
    if (p->GetCSSPropertyName() == property.GetCSSPropertyName())
      return index;
    ++index;
  }
  return kNotFound;
}

CascadeResolver::AutoLock::AutoLock(const CSSProperty& property,
                                    CascadeResolver& resolver)
    : resolver_(resolver) {
  DCHECK(!resolver.IsLocked(property));
  resolver_.stack_.push_back(&property);
}

CascadeResolver::AutoLock::~AutoLock() {
  resolver_.stack_.pop_back();
  if (resolver_.stack_.size() <= resolver_.cycle_depth_)
    resolver_.cycle_depth_ = kNotFound;
}

}  // namespace blink
