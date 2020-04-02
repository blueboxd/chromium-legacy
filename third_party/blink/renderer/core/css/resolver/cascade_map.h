// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CASCADE_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CASCADE_MAP_H_

#include <bitset>
#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_priority.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// Optimized map from CSSPropertyNames to CascadePriority.
//
// Because using a HashMap for everything is quite expensive in terms of
// performance, this class stores standard (non-custom) properties in a fixed-
// size array, and only custom properties are stored in a HashMap.
class CORE_EXPORT CascadeMap {
  STACK_ALLOCATED();

 public:
  // Get the CascadePriority for the given CSSPropertyName. If there is no
  // entry for the given name, CascadePriority() is returned.
  CascadePriority At(const CSSPropertyName&) const;
  // Find the CascadePriority location for a given name, if present. If there
  // is no entry for the given name, nullptr is returned.
  //
  // Note that the returned pointer may accessed to change the stored value.
  //
  // Note also that calling Add() invalidates the pointer.
  CascadePriority* Find(const CSSPropertyName&);
  // Adds an an entry to the map if the incoming priority is greater than or
  // equal to the current priority for the same name.
  void Add(const CSSPropertyName&, CascadePriority);
  // Added properties with CSSPropertyPriority::kHighPropertyPriority cause the
  // corresponding high_priority_-bit to be set. This provides a fast way to
  // check which high-priority properties have been added (if any).
  uint64_t HighPriorityBits() const { return high_priority_; }
  // Remove all properties (both native and custom) from the CascadeMap.
  void Reset();

 private:
  uint64_t high_priority_ = 0;
  // For performance reasons, a char-array is used to prevent construction of
  // CascadePriority objects. A companion std::bitset keeps track of which
  // properties are initialized.
  std::bitset<numCSSProperties> native_property_bits_;
  alignas(CascadePriority) char native_properties_[numCSSProperties *
                                                   sizeof(CascadePriority)];
  HashMap<CSSPropertyName, CascadePriority> custom_properties_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CASCADE_MAP_H_
