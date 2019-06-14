// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_PERSISTENT_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_PERSISTENT_NODE_H_

#include <atomic>
#include <memory>
#include "third_party/blink/renderer/platform/heap/process_heap.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

namespace blink {

class CrossThreadPersistentRegion;
class PersistentRegion;

enum WeaknessPersistentConfiguration {
  kNonWeakPersistentConfiguration,
  kWeakPersistentConfiguration
};

class PersistentNode final {
  DISALLOW_NEW();

 public:
  PersistentNode() : self_(nullptr), trace_(nullptr) { DCHECK(IsUnused()); }

#if DCHECK_IS_ON()
  ~PersistentNode() {
    // If you hit this assert, it means that the thread finished
    // without clearing persistent handles that the thread created.
    // We don't enable the assert for the main thread because the
    // main thread finishes without clearing all persistent handles.
    DCHECK(IsMainThread() || IsUnused());
  }
#endif

  // It is dangerous to copy the PersistentNode because it breaks the
  // free list.
  PersistentNode& operator=(const PersistentNode& otherref) = delete;

  // Ideally the trace method should be virtual and automatically dispatch
  // to the most specific implementation. However having a virtual method
  // on PersistentNode leads to too eager template instantiation with MSVC
  // which leads to include cycles.
  // Instead we call the constructor with a TraceCallback which knows the
  // type of the most specific child and calls trace directly. See
  // TraceMethodDelegate in Visitor.h for how this is done.
  void TracePersistentNode(Visitor* visitor) {
    DCHECK(!IsUnused());
    DCHECK(trace_);
    trace_(visitor, self_);
  }

  void Initialize(void* self, TraceCallback trace) {
    DCHECK(IsUnused());
    self_ = self;
    trace_ = trace;
  }

  void SetFreeListNext(PersistentNode* node) {
    DCHECK(!node || node->IsUnused());
    self_ = node;
    trace_ = nullptr;
    DCHECK(IsUnused());
  }

  PersistentNode* FreeListNext() {
    DCHECK(IsUnused());
    PersistentNode* node = reinterpret_cast<PersistentNode*>(self_);
    DCHECK(!node || node->IsUnused());
    return node;
  }

  bool IsUnused() const { return !trace_; }

  void* Self() const { return self_; }

 private:
  // If this PersistentNode is in use:
  //   - m_self points to the corresponding Persistent handle.
  //   - m_trace points to the trace method.
  // If this PersistentNode is freed:
  //   - m_self points to the next freed PersistentNode.
  //   - m_trace is nullptr.
  void* self_;
  TraceCallback trace_;
};

struct PersistentNodeSlots final {
  USING_FAST_MALLOC(PersistentNodeSlots);

 private:
  static const int kSlotCount = 256;
  PersistentNodeSlots* next_;
  PersistentNode slot_[kSlotCount];
  friend class PersistentRegion;
  friend class CrossThreadPersistentRegion;
};

// Used by PersistentBase to manage a pointer to a thread heap persistent node.
// This class mostly passes accesses through, but provides an interface
// compatible with CrossThreadPersistentNodePtr.
template <ThreadAffinity affinity,
          WeaknessPersistentConfiguration weakness_configuration>
class PersistentNodePtr {
  STACK_ALLOCATED();

 public:
  PersistentNode* Get() const { return ptr_; }
  bool IsInitialized() const { return ptr_; }

  void Initialize(void* owner, TraceCallback);
  void Uninitialize();

 private:
  PersistentNode* ptr_ = nullptr;
#if DCHECK_IS_ON()
  ThreadState* state_ = nullptr;
#endif
};

// Used by PersistentBase to manage a pointer to a cross-thread persistent node.
// It uses ProcessHeap::CrossThreadPersistentMutex() to protect most accesses,
// but can be polled to see whether it is initialized without the mutex.
template <WeaknessPersistentConfiguration weakness_configuration>
class CrossThreadPersistentNodePtr {
  STACK_ALLOCATED();

 public:
  PersistentNode* Get() const {
#if DCHECK_IS_ON()
    ProcessHeap::CrossThreadPersistentMutex().AssertAcquired();
#endif
    return ptr_.load(std::memory_order_relaxed);
  }
  bool IsInitialized() const { return ptr_.load(std::memory_order_acquire); }

  void Initialize(void* owner, TraceCallback);
  void Uninitialize();

  void ClearWithLockHeld();

 private:
  // Access must either be protected by the cross-thread persistent mutex or
  // handle the fact that this may be changed concurrently (with a
  // release-store).
  std::atomic<PersistentNode*> ptr_{nullptr};
};

// PersistentRegion provides a region of PersistentNodes. PersistentRegion
// holds a linked list of PersistentNodeSlots, each of which stores
// a predefined number of PersistentNodes. You can call allocatePersistentNode/
// freePersistentNode to allocate/free a PersistentNode on the region.
class PLATFORM_EXPORT PersistentRegion final {
  USING_FAST_MALLOC(PersistentRegion);

 public:
  PersistentRegion()
      : free_list_head_(nullptr),
        slots_(nullptr)
#if DCHECK_IS_ON()
        ,
        persistent_count_(0)
#endif
  {
  }
  ~PersistentRegion();

  PersistentNode* AllocatePersistentNode(void* self, TraceCallback trace) {
#if DCHECK_IS_ON()
    ++persistent_count_;
#endif
    if (UNLIKELY(!free_list_head_))
      EnsurePersistentNodeSlots(self, trace);
    DCHECK(free_list_head_);
    PersistentNode* node = free_list_head_;
    free_list_head_ = free_list_head_->FreeListNext();
    node->Initialize(self, trace);
    DCHECK(!node->IsUnused());
    return node;
  }

  void FreePersistentNode(PersistentNode* persistent_node) {
#if DCHECK_IS_ON()
    DCHECK_GT(persistent_count_, 0);
#endif
    persistent_node->SetFreeListNext(free_list_head_);
    free_list_head_ = persistent_node;
#if DCHECK_IS_ON()
    --persistent_count_;
#endif
  }

  static bool ShouldTracePersistentNode(Visitor*, PersistentNode*) {
    return true;
  }

  void ReleasePersistentNode(PersistentNode*,
                             ThreadState::PersistentClearCallback);
  using ShouldTraceCallback = bool (*)(Visitor*, PersistentNode*);
  void TracePersistentNodes(
      Visitor*,
      ShouldTraceCallback = PersistentRegion::ShouldTracePersistentNode);
  int NumberOfPersistents();
  void PrepareForThreadStateTermination();

 private:
  friend CrossThreadPersistentRegion;

  void EnsurePersistentNodeSlots(void*, TraceCallback);

  PersistentNode* free_list_head_;
  PersistentNodeSlots* slots_;
#if DCHECK_IS_ON()
  int persistent_count_;
#endif
};

// Protected by ProcessHeap::CrossThreadPersistentMutex.
class PLATFORM_EXPORT CrossThreadPersistentRegion final {
  USING_FAST_MALLOC(CrossThreadPersistentRegion);

 public:
  PersistentNode* AllocatePersistentNode(void* self, TraceCallback trace) {
#if DCHECK_IS_ON()
    ProcessHeap::CrossThreadPersistentMutex().AssertAcquired();
#endif
    return persistent_region_.AllocatePersistentNode(self, trace);
  }

  void FreePersistentNode(PersistentNode* node) {
#if DCHECK_IS_ON()
    ProcessHeap::CrossThreadPersistentMutex().AssertAcquired();
#endif
    // When the thread that holds the heap object that the cross-thread
    // persistent shuts down, prepareForThreadStateTermination() will clear out
    // the associated CrossThreadPersistent<> and PersistentNode so as to avoid
    // unsafe access. This can overlap with a holder of the
    // CrossThreadPersistent<> also clearing the persistent and freeing the
    // PersistentNode.
    //
    // The lock ensures the updating is ordered, but by the time lock has been
    // acquired the PersistentNode reference may have been cleared out already;
    // check for this.
    if (!node)
      return;
    persistent_region_.FreePersistentNode(node);
  }

  void TracePersistentNodes(Visitor* visitor) {
// If this assert triggers, you're tracing without being in a LockScope.
#if DCHECK_IS_ON()
    ProcessHeap::CrossThreadPersistentMutex().AssertAcquired();
#endif
    persistent_region_.TracePersistentNodes(
        visitor, CrossThreadPersistentRegion::ShouldTracePersistentNode);
  }

  void PrepareForThreadStateTermination(ThreadState*);

  NO_SANITIZE_ADDRESS
  static bool ShouldTracePersistentNode(Visitor*, PersistentNode*);

#if defined(ADDRESS_SANITIZER)
  void UnpoisonCrossThreadPersistents();
#endif

 private:

  // We don't make CrossThreadPersistentRegion inherit from PersistentRegion
  // because we don't want to virtualize performance-sensitive methods
  // such as PersistentRegion::allocate/freePersistentNode.
  PersistentRegion persistent_region_;
};

template <ThreadAffinity affinity,
          WeaknessPersistentConfiguration weakness_configuration>
void PersistentNodePtr<affinity, weakness_configuration>::Initialize(
    void* owner,
    TraceCallback trace_callback) {
  ThreadState* state = ThreadStateFor<affinity>::GetState();
  DCHECK(state->CheckThread());
  PersistentRegion* region =
      weakness_configuration == kWeakPersistentConfiguration
          ? state->GetWeakPersistentRegion()
          : state->GetPersistentRegion();
  ptr_ = region->AllocatePersistentNode(owner, trace_callback);
#if DCHECK_IS_ON()
  state_ = state;
#endif
}

template <ThreadAffinity affinity,
          WeaknessPersistentConfiguration weakness_configuration>
void PersistentNodePtr<affinity, weakness_configuration>::Uninitialize() {
  if (!ptr_)
    return;
  ThreadState* state = ThreadStateFor<affinity>::GetState();
  DCHECK(state->CheckThread());
#if DCHECK_IS_ON()
  DCHECK_EQ(state_, state)
      << "must be initialized and uninitialized on the same thread";
  state_ = nullptr;
#endif
  PersistentRegion* region =
      weakness_configuration == kWeakPersistentConfiguration
          ? state->GetWeakPersistentRegion()
          : state->GetPersistentRegion();
  state->FreePersistentNode(region, ptr_);
  ptr_ = nullptr;
}

template <WeaknessPersistentConfiguration weakness_configuration>
void CrossThreadPersistentNodePtr<weakness_configuration>::Initialize(
    void* owner,
    TraceCallback trace_callback) {
  CrossThreadPersistentRegion& region =
      weakness_configuration == kWeakPersistentConfiguration
          ? ProcessHeap::GetCrossThreadWeakPersistentRegion()
          : ProcessHeap::GetCrossThreadPersistentRegion();
  MutexLocker lock(ProcessHeap::CrossThreadPersistentMutex());
  PersistentNode* node = region.AllocatePersistentNode(owner, trace_callback);
  ptr_.store(node, std::memory_order_release);
}

template <WeaknessPersistentConfiguration weakness_configuration>
void CrossThreadPersistentNodePtr<weakness_configuration>::Uninitialize() {
  // As an optimization, skip the mutex acquisition.
  //
  // Persistent handles are often assigned or destroyed while being
  // uninitialized.
  //
  // Calling code is still expected to synchronize mutations to persistent
  // handles, so if this thread can see the node pointer as having been
  // cleared and the program does not have a data race, then this pointer would
  // still have been blank after waiting for the cross-thread persistent mutex.
  if (!ptr_.load(std::memory_order_acquire))
    return;

  CrossThreadPersistentRegion& region =
      weakness_configuration == kWeakPersistentConfiguration
          ? ProcessHeap::GetCrossThreadWeakPersistentRegion()
          : ProcessHeap::GetCrossThreadPersistentRegion();
  MutexLocker lock(ProcessHeap::CrossThreadPersistentMutex());
  region.FreePersistentNode(ptr_.load(std::memory_order_relaxed));
  ptr_.store(nullptr, std::memory_order_release);
}

template <WeaknessPersistentConfiguration weakness_configuration>
void CrossThreadPersistentNodePtr<weakness_configuration>::ClearWithLockHeld() {
#if DCHECK_IS_ON()
  ProcessHeap::CrossThreadPersistentMutex().AssertAcquired();
#endif
  CrossThreadPersistentRegion& region =
      weakness_configuration == kWeakPersistentConfiguration
          ? ProcessHeap::GetCrossThreadWeakPersistentRegion()
          : ProcessHeap::GetCrossThreadPersistentRegion();
  region.FreePersistentNode(ptr_.load(std::memory_order_relaxed));
  ptr_.store(nullptr, std::memory_order_release);
}

}  // namespace blink

#endif
