// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/marking_visitor.h"

#include "third_party/blink/renderer/platform/heap/blink_gc.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"

namespace blink {

namespace {

ALWAYS_INLINE bool IsHashTableDeleteValue(const void* value) {
  return value == reinterpret_cast<void*>(-1);
}

}  // namespace

MarkingVisitorBase::MarkingVisitorBase(ThreadState* state,
                                       MarkingMode marking_mode,
                                       int task_id)
    : Visitor(state),
      marking_worklist_(Heap().GetMarkingWorklist(), task_id),
      not_fully_constructed_worklist_(Heap().GetNotFullyConstructedWorklist(),
                                      task_id),
      weak_callback_worklist_(Heap().GetWeakCallbackWorklist(), task_id),
      movable_reference_worklist_(Heap().GetMovableReferenceWorklist(),
                                  task_id),
      weak_table_worklist_(Heap().GetWeakTableWorklist(), task_id),
      backing_store_callback_worklist_(Heap().GetBackingStoreCallbackWorklist(),
                                       task_id),
      marking_mode_(marking_mode),
      task_id_(task_id) {
  DCHECK(state->InAtomicMarkingPause());
#if DCHECK_IS_ON()
  DCHECK(state->CheckThread());
#endif  // DCHECK_IS_ON
}

void MarkingVisitorBase::FlushCompactionWorklists() {
  movable_reference_worklist_.FlushToGlobal();
  backing_store_callback_worklist_.FlushToGlobal();
}

void MarkingVisitorBase::RegisterWeakCallback(void* object,
                                              WeakCallback callback) {
  weak_callback_worklist_.Push({object, callback});
}

void MarkingVisitorBase::RegisterBackingStoreReference(void** slot) {
  if (marking_mode_ != kGlobalMarkingWithCompaction)
    return;
  MovableReference* movable_reference =
      reinterpret_cast<MovableReference*>(slot);
  if (Heap().ShouldRegisterMovingAddress(
          reinterpret_cast<Address>(movable_reference))) {
    movable_reference_worklist_.Push(movable_reference);
  }
}

void MarkingVisitorBase::RegisterBackingStoreCallback(
    void* backing,
    MovingObjectCallback callback) {
  if (marking_mode_ != kGlobalMarkingWithCompaction)
    return;
  if (Heap().ShouldRegisterMovingAddress(reinterpret_cast<Address>(backing))) {
    backing_store_callback_worklist_.Push({backing, callback});
  }
}

bool MarkingVisitorBase::RegisterWeakTable(
    const void* closure,
    EphemeronCallback iteration_callback) {
  weak_table_worklist_.Push({const_cast<void*>(closure), iteration_callback});
  return true;
}

void MarkingVisitorBase::AdjustMarkedBytes(HeapObjectHeader* header,
                                           size_t old_size) {
  DCHECK(header->IsMarked());
  // Currently, only expansion of an object is supported during marking.
  DCHECK_GE(header->size(), old_size);
  marked_bytes_ += header->size() - old_size;
}

bool MarkingVisitor::WriteBarrierSlow(void* value) {
  if (!value || IsHashTableDeleteValue(value))
    return false;

  ThreadState* const thread_state = ThreadState::Current();
  if (!thread_state->IsIncrementalMarking())
    return false;

  HeapObjectHeader* const header = HeapObjectHeader::FromInnerAddress(
      reinterpret_cast<Address>(const_cast<void*>(value)));
  if (header->IsMarked())
    return false;

  if (header->IsInConstruction()) {
    thread_state->CurrentVisitor()->not_fully_constructed_worklist_.Push(
        header->Payload());
    return true;
  }

  // Mark and push trace callback.
  if (!header->TryMark<HeapObjectHeader::AccessMode::kAtomic>())
    return false;
  MarkingVisitor* visitor = thread_state->CurrentVisitor();
  visitor->AccountMarkedBytes(header);
  visitor->marking_worklist_.Push(
      {header->Payload(),
       GCInfoTable::Get().GCInfoFromIndex(header->GcInfoIndex())->trace});
  return true;
}

void MarkingVisitor::TraceMarkedBackingStoreSlow(void* value) {
  if (!value)
    return;

  ThreadState* const thread_state = ThreadState::Current();
  if (!thread_state->IsIncrementalMarking())
    return;

  // |value| is pointing to the start of a backing store.
  HeapObjectHeader* header = HeapObjectHeader::FromPayload(value);
  CHECK(header->IsMarked());
  DCHECK(thread_state->CurrentVisitor());
  // No weak handling for write barriers. Modifying weakly reachable objects
  // strongifies them for the current cycle.
  GCInfoTable::Get()
      .GCInfoFromIndex(header->GcInfoIndex())
      ->trace(thread_state->CurrentVisitor(), value);
}

MarkingVisitor::MarkingVisitor(ThreadState* state, MarkingMode marking_mode)
    : MarkingVisitorBase(state, marking_mode, WorklistTaskId::MutatorThread) {
  DCHECK(state->InAtomicMarkingPause());
  DCHECK(state->CheckThread());
}

void MarkingVisitor::DynamicallyMarkAddress(Address address) {
  HeapObjectHeader* const header = HeapObjectHeader::FromInnerAddress(address);
  DCHECK(header);
  DCHECK(!header->IsInConstruction());
  const GCInfo* gc_info =
      GCInfoTable::Get().GCInfoFromIndex(header->GcInfoIndex());
  MarkHeader(header, gc_info->trace);
}

void MarkingVisitor::ConservativelyMarkAddress(BasePage* page,
                                               Address address) {
#if DCHECK_IS_ON()
  DCHECK(page->Contains(address));
#endif
  HeapObjectHeader* const header =
      page->IsLargeObjectPage()
          ? static_cast<LargeObjectPage*>(page)->ObjectHeader()
          : static_cast<NormalPage*>(page)->FindHeaderFromAddress(address);
  if (!header || header->IsMarked())
    return;

  // Simple case for fully constructed objects.
  const GCInfo* gc_info =
      GCInfoTable::Get().GCInfoFromIndex(header->GcInfoIndex());
  if (!header->IsInConstruction()) {
    MarkHeader(header, gc_info->trace);
    return;
  }

  // This case is reached for not-fully-constructed objects with vtables.
  // We can differentiate multiple cases:
  // 1. No vtable set up. Example:
  //      class A : public GarbageCollected<A> { virtual void f() = 0; };
  //      class B : public A { B() : A(foo()) {}; };
  //    The vtable for A is not set up if foo() allocates and triggers a GC.
  //
  // 2. Vtables properly set up (non-mixin case).
  // 3. Vtables not properly set up (mixin) if GC is allowed during mixin
  //    construction.
  //
  // We use a simple conservative approach for these cases as they are not
  // performance critical.
  MarkHeaderNoTracing(header);
  Address* payload = reinterpret_cast<Address*>(header->Payload());
  const size_t payload_size = header->PayloadSize();
  for (size_t i = 0; i < (payload_size / sizeof(Address)); ++i) {
    Address maybe_ptr = payload[i];
#if defined(MEMORY_SANITIZER)
    // |payload| may be uninitialized by design or just contain padding bytes.
    // Copy into a local variable that is unpoisoned for conservative marking.
    // Copy into a temporary variable to maintain the original MSAN state.
    __msan_unpoison(&maybe_ptr, sizeof(maybe_ptr));
#endif
    if (maybe_ptr)
      Heap().CheckAndMarkPointer(this, maybe_ptr);
  }
}

void MarkingVisitor::FlushMarkingWorklist() {
  marking_worklist_.FlushToGlobal();
}

ConcurrentMarkingVisitor::ConcurrentMarkingVisitor(ThreadState* state,
                                                   MarkingMode marking_mode,
                                                   int task_id)
    : MarkingVisitorBase(state, marking_mode, task_id) {
  DCHECK(state->InAtomicMarkingPause());
  DCHECK(state->CheckThread());
  DCHECK_NE(WorklistTaskId::MutatorThread, task_id);
}

void ConcurrentMarkingVisitor::FlushWorklists() {
  // Flush marking worklists for further marking on the mutator thread.
  marking_worklist_.FlushToGlobal();
  not_fully_constructed_worklist_.FlushToGlobal();
  weak_callback_worklist_.FlushToGlobal();
  weak_table_worklist_.FlushToGlobal();
  // Flush compaction worklists.
  movable_reference_worklist_.FlushToGlobal();
  backing_store_callback_worklist_.FlushToGlobal();
}

}  // namespace blink
