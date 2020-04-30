// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/cascade_resolver.h"

#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_priority.h"

namespace blink {

bool CascadeResolver::IsLocked(const CSSProperty& property) const {
  return IsLocked(property.GetCSSPropertyName());
}

bool CascadeResolver::IsLocked(const CSSPropertyName& name) const {
  return stack_.Contains(name);
}

bool CascadeResolver::AllowSubstitution(CSSVariableData* data) const {
  if (data && data->IsAnimationTainted() && stack_.size()) {
    const CSSPropertyName& name = stack_.back();
    if (name.IsCustomProperty())
      return true;
    const CSSProperty& property = CSSProperty::Get(name.Id());
    return !CSSAnimations::IsAnimationAffectingProperty(property);
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
  wtf_size_t index = stack_.Find(property.GetCSSPropertyName());
  if (index == kNotFound)
    return false;
  cycle_depth_ = std::min(cycle_depth_, index);
  return true;
}

bool CascadeResolver::InCycle() const {
  return cycle_depth_ != kNotFound;
}

CascadeResolver::AutoLock::AutoLock(const CSSProperty& property,
                                    CascadeResolver& resolver)
    : AutoLock(property.GetCSSPropertyName(), resolver) {}

CascadeResolver::AutoLock::AutoLock(const CSSPropertyName& name,
                                    CascadeResolver& resolver)
    : resolver_(resolver) {
  DCHECK(!resolver.IsLocked(name));
  resolver_.stack_.push_back(name);
}

CascadeResolver::AutoLock::~AutoLock() {
  resolver_.stack_.pop_back();
  if (resolver_.stack_.size() <= resolver_.cycle_depth_)
    resolver_.cycle_depth_ = kNotFound;
}

}  // namespace blink
