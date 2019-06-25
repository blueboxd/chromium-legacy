// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/unified_heap_marking_visitor.h"

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/heap/unified_heap_controller.h"

namespace blink {

UnifiedHeapMarkingVisitorBase::UnifiedHeapMarkingVisitorBase(
    ThreadState* thread_state,
    MarkingMode mode,
    v8::Isolate* isolate)
    : MarkingVisitor(thread_state, mode),
      isolate_(isolate),
      controller_(thread_state->unified_heap_controller()) {
  DCHECK(controller_);
}

void UnifiedHeapMarkingVisitorBase::Visit(
    const TraceWrapperV8Reference<v8::Value>& v8_reference) {
  if (v8_reference.Get().IsEmpty())
    return;
  DCHECK(isolate_);
  // TODO(mlippautz): Do not call into controller directly but rather use a
  // Worklist or similar as temporary storage.
  controller_->RegisterEmbedderReference(v8_reference.Get());
}

UnifiedHeapMarkingVisitor::UnifiedHeapMarkingVisitor(ThreadState* thread_state,
                                                     MarkingMode mode,
                                                     v8::Isolate* isolate)
    : UnifiedHeapMarkingVisitorBase(thread_state, mode, isolate) {}

void UnifiedHeapMarkingVisitor::WriteBarrier(
    const TraceWrapperV8Reference<v8::Value>& object) {
  if (object.IsEmpty() || !ThreadState::IsAnyIncrementalMarking())
    return;

  ThreadState* thread_state = ThreadState::Current();
  if (!thread_state->IsIncrementalMarking())
    return;

  thread_state->CurrentVisitor()->Trace(object);
}

void UnifiedHeapMarkingVisitor::WriteBarrier(
    v8::Isolate* isolate,
    const WrapperTypeInfo* wrapper_type_info,
    void* object) {
  // |object| here is either ScriptWrappable or CustomWrappable.

  if (!ThreadState::IsAnyIncrementalMarking())
    return;

  ThreadState* thread_state = ThreadState::Current();
  if (!thread_state->IsIncrementalMarking())
    return;

  wrapper_type_info->Trace(thread_state->CurrentVisitor(), object);
}

}  // namespace blink
