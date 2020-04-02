// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/cascade_map.h"
#include "third_party/blink/renderer/core/css/resolver/css_property_priority.h"

namespace blink {

CascadePriority CascadeMap::At(const CSSPropertyName& name) const {
  if (name.IsCustomProperty())
    return custom_properties_.at(name);
  size_t index = static_cast<size_t>(name.Id());
  DCHECK_LT(index, static_cast<size_t>(numCSSProperties));
  return native_property_bits_.test(index)
             ? reinterpret_cast<const CascadePriority*>(
                   native_properties_)[index]
             : CascadePriority();
}

CascadePriority* CascadeMap::Find(const CSSPropertyName& name) {
  if (name.IsCustomProperty()) {
    auto iter = custom_properties_.find(name);
    if (iter != custom_properties_.end())
      return &iter->value;
    return nullptr;
  }
  size_t index = static_cast<size_t>(name.Id());
  DCHECK_LT(index, static_cast<size_t>(numCSSProperties));
  if (!native_property_bits_.test(index))
    return nullptr;
  return reinterpret_cast<CascadePriority*>(native_properties_) + index;
}

void CascadeMap::Add(const CSSPropertyName& name, CascadePriority priority) {
  if (name.IsCustomProperty()) {
    DCHECK_NE(CascadeOrigin::kUserAgent, priority.GetOrigin());
    auto result = custom_properties_.insert(name, priority);
    if (result.is_new_entry || result.stored_value->value < priority)
      result.stored_value->value = priority;
    return;
  }
  CSSPropertyID id = name.Id();
  size_t index = static_cast<size_t>(id);
  DCHECK_LT(index, static_cast<size_t>(numCSSProperties));

  // Set bit in high_priority_, if appropriate.
  using HighPriority = CSSPropertyPriorityData<kHighPropertyPriority>;
  static_assert(static_cast<int>(HighPriority::Last()) < 64,
                "CascadeMap supports at most 63 high-priority properties");
  if (HighPriority::PropertyHasPriority(id))
    high_priority_ |= (1ull << index);
  CascadePriority* p =
      reinterpret_cast<CascadePriority*>(native_properties_) + index;
  if (!native_property_bits_.test(index) || *p < priority) {
    native_property_bits_.set(index);
    static_assert(
        std::is_trivially_destructible<CascadePriority>::value,
        "~CascadePriority is never called on these CascadePriority objects");
    new (p) CascadePriority(priority);
  }
}

void CascadeMap::Reset() {
  high_priority_ = 0;
  native_property_bits_.reset();
  custom_properties_.clear();
}

}  // namespace blink
