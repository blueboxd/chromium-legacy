// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_VECTOR_BACKING_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_VECTOR_BACKING_H_

#include "base/logging.h"
#include "third_party/blink/renderer/platform/heap/finalizer_traits.h"
#include "third_party/blink/renderer/platform/heap/gc_info.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/heap/threading_traits.h"
#include "third_party/blink/renderer/platform/heap/trace_traits.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

template <typename T, typename Traits = WTF::VectorTraits<T>>
class HeapVectorBacking final {
  DISALLOW_NEW();
  IS_GARBAGE_COLLECTED_TYPE();

 public:
  template <typename Backing>
  static void* AllocateObject(size_t size) {
    ThreadState* state =
        ThreadStateFor<ThreadingTrait<T>::kAffinity>::GetState();
    DCHECK(state->IsAllocationAllowed());
    const char* type_name = WTF_HEAP_PROFILER_TYPE_NAME(Backing);
    return state->Heap().AllocateOnArenaIndex(
        state, size, BlinkGC::kVectorArenaIndex, GCInfoTrait<Backing>::Index(),
        type_name);
  }

  static void Finalize(void* pointer);
  void FinalizeGarbageCollectedObject() { Finalize(this); }
};

template <typename T, typename Traits>
void HeapVectorBacking<T, Traits>::Finalize(void* pointer) {
  static_assert(Traits::kNeedsDestruction,
                "Only vector buffers with items requiring destruction should "
                "be finalized");
  static_assert(
      Traits::kCanClearUnusedSlotsWithMemset || std::is_polymorphic<T>::value,
      "HeapVectorBacking doesn't support objects that cannot be cleared as "
      "unused with memset or don't have a vtable");

  static_assert(
      !std::is_trivially_destructible<T>::value,
      "Finalization of trivially destructible classes should not happen.");
  HeapObjectHeader* header = HeapObjectHeader::FromPayload(pointer);
  // Use the payload size as recorded by the heap to determine how many
  // elements to finalize.
  size_t length = header->PayloadSize() / sizeof(T);
  char* payload = static_cast<char*>(pointer);
#ifdef ANNOTATE_CONTIGUOUS_CONTAINER
  ANNOTATE_CHANGE_SIZE(payload, length * sizeof(T), 0, length * sizeof(T));
#endif
  // As commented above, HeapVectorBacking calls finalizers for unused slots
  // (which are already zeroed out).
  if (std::is_polymorphic<T>::value) {
    for (unsigned i = 0; i < length; ++i) {
      char* element = payload + i * sizeof(T);
      if (blink::VTableInitialized(element))
        reinterpret_cast<T*>(element)->~T();
    }
  } else {
    T* buffer = reinterpret_cast<T*>(payload);
    for (unsigned i = 0; i < length; ++i)
      buffer[i].~T();
  }
}

template <typename T>
struct MakeGarbageCollectedTrait<HeapVectorBacking<T>> {
  static HeapVectorBacking<T>* Call(size_t num_elements) {
    CHECK_GT(num_elements, 0u);
    void* memory =
        HeapVectorBacking<T>::template AllocateObject<HeapVectorBacking<T>>(
            num_elements * sizeof(T));
    HeapObjectHeader* header = HeapObjectHeader::FromPayload(memory);
    // Placement new as regular operator new() is deleted.
    HeapVectorBacking<T>* object = ::new (memory) HeapVectorBacking<T>();
    header->MarkFullyConstructed<HeapObjectHeader::AccessMode::kAtomic>();
    return object;
  }
};

template <typename T, typename Traits>
struct FinalizerTrait<HeapVectorBacking<T, Traits>> {
  STATIC_ONLY(FinalizerTrait);
  static const bool kNonTrivialFinalizer = Traits::kNeedsDestruction;
  static void Finalize(void* obj) {
    internal::FinalizerTraitImpl<HeapVectorBacking<T, Traits>,
                                 kNonTrivialFinalizer>::Finalize(obj);
  }
};

template <typename T, typename Traits>
struct ThreadingTrait<HeapVectorBacking<T, Traits>> {
  STATIC_ONLY(ThreadingTrait);
  static const ThreadAffinity kAffinity = ThreadingTrait<T>::Affinity;
};

template <typename T, typename Traits>
struct TraceTrait<HeapVectorBacking<T, Traits>> {
  STATIC_ONLY(TraceTrait);
  using Backing = HeapVectorBacking<T, Traits>;

 public:
  static TraceDescriptor GetTraceDescriptor(const void* self) {
    return {self, TraceTrait<Backing>::Trace};
  }

  static void Trace(Visitor* visitor, const void* self) {
    if (visitor->ConcurrentTracingBailOut({self, &Trace}))
      return;

    static_assert(!WTF::IsWeak<T>::value,
                  "Weakness is not supported in HeapVector and HeapDeque");
    if (WTF::IsTraceableInCollectionTrait<Traits>::value) {
      WTF::TraceInCollectionTrait<WTF::kNoWeakHandling,
                                  HeapVectorBacking<T, Traits>,
                                  void>::Trace(visitor, self);
    }
  }
};

}  // namespace blink

namespace WTF {

// This trace method is used only for on-stack HeapVectors found in
// conservative scanning. On-heap HeapVectors are traced by Vector::trace.
template <typename T, typename Traits>
struct TraceInCollectionTrait<kNoWeakHandling,
                              blink::HeapVectorBacking<T, Traits>,
                              void> {
  static bool Trace(blink::Visitor* visitor, const void* self) {
    // HeapVectorBacking does not know the exact size of the vector
    // and just knows the capacity of the vector. Due to the constraint,
    // HeapVectorBacking can support only the following objects:
    //
    // - An object that has a vtable. In this case, HeapVectorBacking
    //   traces only slots that are not zeroed out. This is because if
    //   the object has a vtable, the zeroed slot means that it is
    //   an unused slot (Remember that the unused slots are guaranteed
    //   to be zeroed out by VectorUnusedSlotClearer).
    //
    // - An object that can be initialized with memset. In this case,
    //   HeapVectorBacking traces all slots including unused slots.
    //   This is fine because the fact that the object can be initialized
    //   with memset indicates that it is safe to treat the zerod slot
    //   as a valid object.
    static_assert(!IsTraceableInCollectionTrait<Traits>::value ||
                      Traits::kCanClearUnusedSlotsWithMemset ||
                      std::is_polymorphic<T>::value,
                  "HeapVectorBacking doesn't support objects that cannot be "
                  "cleared as unused with memset.");

    // This trace method is instantiated for vectors where
    // IsTraceableInCollectionTrait<Traits>::value is false, but the trace
    // method should not be called. Thus we cannot static-assert
    // IsTraceableInCollectionTrait<Traits>::value but should runtime-assert it.
    DCHECK(IsTraceableInCollectionTrait<Traits>::value);

    const T* array = reinterpret_cast<const T*>(self);
    blink::HeapObjectHeader* header =
        blink::HeapObjectHeader::FromPayload(self);
    // Use the payload size as recorded by the heap to determine how many
    // elements to trace.
    size_t length = header->PayloadSize() / sizeof(T);
#ifdef ANNOTATE_CONTIGUOUS_CONTAINER
    // As commented above, HeapVectorBacking can trace unused slots
    // (which are already zeroed out).
    ANNOTATE_CHANGE_SIZE(array, length, 0, length);
#endif
    if (std::is_polymorphic<T>::value) {
      const char* pointer = reinterpret_cast<const char*>(array);
      for (unsigned i = 0; i < length; ++i) {
        const char* element = pointer + i * sizeof(T);
        if (blink::VTableInitialized(element)) {
          blink::TraceIfNeeded<
              T, IsTraceableInCollectionTrait<Traits>::value>::Trace(visitor,
                                                                     array[i]);
        }
      }
    } else {
      for (size_t i = 0; i < length; ++i) {
        blink::TraceIfNeeded<
            T, IsTraceableInCollectionTrait<Traits>::value>::Trace(visitor,
                                                                   array[i]);
      }
    }
    return false;
  }
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_VECTOR_BACKING_H_
