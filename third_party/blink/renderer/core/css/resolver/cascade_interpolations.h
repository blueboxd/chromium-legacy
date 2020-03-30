// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CASCADE_INTERPOLATIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CASCADE_INTERPOLATIONS_H_

#include "third_party/blink/renderer/core/animation/interpolation.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_origin.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

class CORE_EXPORT CascadeInterpolations {
  STACK_ALLOCATED();

 public:
  static constexpr size_t kMaxEntryIndex = std::numeric_limits<uint16_t>::max();

  struct Entry {
    DISALLOW_NEW();

   public:
    const ActiveInterpolationsMap* map = nullptr;
    CascadeOrigin origin = CascadeOrigin::kNone;
  };

  void Add(const ActiveInterpolationsMap* map, CascadeOrigin origin) {
    DCHECK(map);
    entries_.push_back(Entry{map, origin});
  }

  bool IsEmpty() const { return GetEntries().IsEmpty(); }

  const Vector<Entry, 4>& GetEntries() const {
    using EntryVector = Vector<Entry, 4>;
    DEFINE_STATIC_LOCAL(EntryVector, empty, ());
    if (entries_.size() > kMaxEntryIndex + 1)
      return empty;
    return entries_;
  }

  void Reset() { entries_.clear(); }

 private:
  // We need to add at most four entries (see CSSAnimationUpdate):
  //
  //   1. Standard property transitions
  //   2. Standard property animations
  //   3. Custom property transitions
  //   4. Custom property animations
  //
  // TODO(andruud): Once regular declarations and interpolations are applied
  // using the same StyleCascade object, we can store standard and custom
  // property interpolations together, and use Vector<Entry,2> instead.
  Vector<Entry, 4> entries_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CASCADE_INTERPOLATIONS_H_
