// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CASCADE_RESOLVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CASCADE_RESOLVER_H_

#include "base/auto_reset.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_filter.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_origin.h"
#include "third_party/blink/renderer/core/css/rule_set.h"

namespace blink {

class CascadePriority;
class CSSProperty;
class CSSVariableData;
class CSSProperty;

namespace cssvalue {

class CSSPendingSubstitutionValue;

}  // namespace cssvalue

// CascadeResolver is an object passed on the stack during Apply. Its most
// important job is to detect cycles during Apply (in general, keep track of
// which properties we're currently applying).
class CORE_EXPORT CascadeResolver {
  STACK_ALLOCATED();

 public:
  // TODO(crbug.com/985047): Probably use a HashMap for this.
  using NameStack = Vector<CSSPropertyName, 8>;

  // A 'locked' property is a property we are in the process of applying.
  // In other words, once a property is locked, locking it again would form
  // a cycle, and is therefore an error.
  bool IsLocked(const CSSProperty&) const;
  bool IsLocked(const CSSPropertyName&) const;

  // We do not allow substitution of animation-tainted values into
  // an animation-affecting property.
  //
  // https://drafts.csswg.org/css-variables/#animation-tainted
  bool AllowSubstitution(CSSVariableData*) const;

  // Sets the generation of the priority to zero, which has the effect of
  // marking it as unapplied. (I.e. this can be used to force re-application of
  // a declaration).
  void MarkUnapplied(CascadePriority*) const;

  // Sets the generation of the priority to the current generation,
  // which has the effect of marking it as already applied. (I.e. this can be
  // used to skip application of a declaration).
  void MarkApplied(CascadePriority*) const;

  // If the incoming origin is kAuthor, collect flags from 'property'.
  // AuthorFlags() can then later be used to see which flags have been observed.
  void CollectAuthorFlags(const CSSProperty& property, CascadeOrigin origin) {
    author_flags_ |=
        (origin == CascadeOrigin::kAuthor ? property.GetFlags() : 0);
  }
  CSSProperty::Flags AuthorFlags() const { return author_flags_; }

  // Automatically locks and unlocks the given property. (See
  // CascadeResolver::IsLocked).
  class CORE_EXPORT AutoLock {
    STACK_ALLOCATED();

   public:
    AutoLock(const CSSProperty&, CascadeResolver&);
    AutoLock(const CSSPropertyName&, CascadeResolver&);
    ~AutoLock();

   private:
    CascadeResolver& resolver_;
  };

 private:
  friend class AutoLock;
  friend class StyleCascade;
  friend class TestCascadeResolver;

  CascadeResolver(CascadeFilter filter, uint8_t generation)
      : filter_(filter), generation_(generation) {}

  // If the given property is already being applied, returns true.
  // The return value is the same value you would get from InCycle(), and
  // is just returned for convenience.
  //
  // When a cycle has been detected, the CascadeResolver will *persist the cycle
  // state* (i.e. InCycle() will continue to return true) until we reach
  // the start of the cycle.
  //
  // The cycle state is cleared by ~AutoLock, once we have moved far enough
  // up the stack.
  bool DetectCycle(const CSSProperty&);
  // Returns true whenever the CascadeResolver is in a cycle state.
  // This DOES NOT detect cycles; the caller must call DetectCycle first.
  bool InCycle() const;

  NameStack stack_;
  wtf_size_t cycle_depth_ = kNotFound;
  CascadeFilter filter_;
  const uint8_t generation_ = 0;
  CSSProperty::Flags author_flags_ = 0;

  // A very simple cache for CSSPendingSubstitutionValues. We cache only the
  // most recently parsed CSSPendingSubstitutionValue, such that consecutive
  // calls to ResolvePendingSubstitution with the same value don't need to
  // do the same parsing job all over again.
  struct {
    STACK_ALLOCATED();

   public:
    const cssvalue::CSSPendingSubstitutionValue* value = nullptr;
    HeapVector<CSSPropertyValue, 256> parsed_properties;
  } shorthand_cache_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CASCADE_RESOLVER_H_
