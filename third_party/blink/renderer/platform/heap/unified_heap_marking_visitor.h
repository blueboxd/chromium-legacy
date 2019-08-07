// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_UNIFIED_HEAP_MARKING_VISITOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_UNIFIED_HEAP_MARKING_VISITOR_H_

#include "base/macros.h"
#include "third_party/blink/renderer/platform/heap/marking_visitor.h"

namespace v8 {
class EmbedderHeapTracer;
}

namespace blink {

struct WrapperTypeInfo;

// Marking visitor for unified heap garbage collections. Extends the regular
// Oilpan marking visitor by also providing write barriers and visitation
// methods that allow for announcing reachable objects to V8. Visitor can be
// used from any thread.
class PLATFORM_EXPORT UnifiedHeapMarkingVisitorBase : public MarkingVisitor {
 public:
  ~UnifiedHeapMarkingVisitorBase() override = default;

  // Visitation methods that announce reachable wrappers to V8.
  void Visit(const TraceWrapperV8Reference<v8::Value>&) final;

  bool is_concurrent_marking_enabled() const {
    return is_concurrent_marking_enabled_;
  }

  // Flush all V8 references in the private segments to the global pool.
  void FlushV8References();

 protected:
  UnifiedHeapMarkingVisitorBase(ThreadState*, MarkingMode, v8::Isolate*);

  v8::Isolate* const isolate_;
  v8::EmbedderHeapTracer* const controller_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedHeapMarkingVisitorBase);

 private:
  bool is_concurrent_marking_enabled_;
  V8ReferencesWorklist::View v8_references_worklist_;
};

// Same as the base visitor with the difference that it is bound to main thread.
// Also implements various sorts of write barriers that should only be called
// from the main thread.
class PLATFORM_EXPORT UnifiedHeapMarkingVisitor
    : public UnifiedHeapMarkingVisitorBase {
 public:
  UnifiedHeapMarkingVisitor(ThreadState*, MarkingMode, v8::Isolate*);

  // Write barriers for annotating a write during incremental marking.
  static void WriteBarrier(const TraceWrapperV8Reference<v8::Value>&);
  static void WriteBarrier(v8::Isolate*, const WrapperTypeInfo*, void*);

 private:
  DISALLOW_COPY_AND_ASSIGN(UnifiedHeapMarkingVisitor);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_UNIFIED_HEAP_MARKING_VISITOR_H_
