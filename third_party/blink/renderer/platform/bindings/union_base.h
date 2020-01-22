// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_UNION_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_UNION_BASE_H_

#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "v8/include/v8.h"

namespace blink {

class Visitor;

namespace bindings {

// This class is the base class for all IDL dictionary implementations.  This
// is designed to collaborate with NativeValueTraits and ToV8 with supporting
// type dispatching (SFINAE, etc.).
class PLATFORM_EXPORT UnionBase {
  DISALLOW_NEW();

 public:
  virtual ~UnionBase() = default;

  virtual v8::Local<v8::Value> CreateV8Object(
      v8::Isolate* isolate,
      v8::Local<v8::Object> creation_context) const = 0;

  void Trace(Visitor*) {}

 protected:
  UnionBase() = default;
  UnionBase(const UnionBase&) = default;
  UnionBase(UnionBase&&) = default;
  UnionBase& operator=(const UnionBase&) = default;
  UnionBase& operator=(UnionBase&&) = default;
};

}  // namespace bindings

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_UNION_BASE_H_
