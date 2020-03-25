// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_HASH_TABLE_BACKING_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_HASH_TABLE_BACKING_H_

#include "third_party/blink/renderer/platform/heap/heap_page.h"
#include "third_party/blink/renderer/platform/heap/threading_traits.h"
#include "third_party/blink/renderer/platform/heap/trace_traits.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

template <typename Table>
class HeapHashTableBacking {
  DISALLOW_NEW();
  IS_GARBAGE_COLLECTED_TYPE();

 public:
  template <typename Backing>
  static void* AllocateObject(size_t size);

  static void Finalize(void* pointer);
  void FinalizeGarbageCollectedObject() { Finalize(this); }
};

template <typename Table>
struct ThreadingTrait<HeapHashTableBacking<Table>> {
  STATIC_ONLY(ThreadingTrait);
  using Key = typename Table::KeyType;
  using Value = typename Table::ValueType;
  static const ThreadAffinity kAffinity =
      (ThreadingTrait<Key>::kAffinity == kMainThreadOnly) &&
              (ThreadingTrait<Value>::kAffinity == kMainThreadOnly)
          ? kMainThreadOnly
          : kAnyThread;
};

// static
template <typename Table>
template <typename Backing>
void* HeapHashTableBacking<Table>::AllocateObject(size_t size) {
  ThreadState* state =
      ThreadStateFor<ThreadingTrait<Backing>::kAffinity>::GetState();
  DCHECK(state->IsAllocationAllowed());
  const char* type_name = WTF_HEAP_PROFILER_TYPE_NAME(Backing);
  return state->Heap().AllocateOnArenaIndex(
      state, size, BlinkGC::kHashTableArenaIndex, GCInfoTrait<Backing>::Index(),
      type_name);
}

template <typename Table>
void HeapHashTableBacking<Table>::Finalize(void* pointer) {
  using Value = typename Table::ValueType;
  static_assert(
      !std::is_trivially_destructible<Value>::value,
      "Finalization of trivially destructible classes should not happen.");
  HeapObjectHeader* header = HeapObjectHeader::FromPayload(pointer);
  // Use the payload size as recorded by the heap to determine how many
  // elements to finalize.
  size_t length = header->PayloadSize() / sizeof(Value);
  Value* table = reinterpret_cast<Value*>(pointer);
  for (unsigned i = 0; i < length; ++i) {
    if (!Table::IsEmptyOrDeletedBucket(table[i]))
      table[i].~Value();
  }
}

template <typename Table>
struct MakeGarbageCollectedTrait<HeapHashTableBacking<Table>> {
  static HeapHashTableBacking<Table>* Call(size_t num_elements) {
    CHECK_GT(num_elements, 0u);
    void* memory = HeapHashTableBacking<Table>::template AllocateObject<
        HeapHashTableBacking<Table>>(num_elements *
                                     sizeof(typename Table::ValueType));
    HeapObjectHeader* header = HeapObjectHeader::FromPayload(memory);
    // Placement new as regular operator new() is deleted.
    HeapHashTableBacking<Table>* object =
        ::new (memory) HeapHashTableBacking<Table>();
    header->MarkFullyConstructed<HeapObjectHeader::AccessMode::kAtomic>();
    return object;
  }
};

template <typename Table>
struct FinalizerTrait<HeapHashTableBacking<Table>> {
  STATIC_ONLY(FinalizerTrait);
  static const bool kNonTrivialFinalizer =
      !std::is_trivially_destructible<typename Table::ValueType>::value;
  static void Finalize(void* obj) {
    internal::FinalizerTraitImpl<HeapHashTableBacking<Table>,
                                 kNonTrivialFinalizer>::Finalize(obj);
  }
};

// The trace trait for the heap hashtable backing is used when we find a
// direct pointer to the backing from the conservative stack scanner. This
// normally indicates that there is an ongoing iteration over the table, and so
// we disable weak processing of table entries. When the backing is found
// through the owning hash table we mark differently, in order to do weak
// processing.
template <typename Table>
struct TraceTrait<HeapHashTableBacking<Table>> {
  STATIC_ONLY(TraceTrait);
  using Backing = HeapHashTableBacking<Table>;
  using ValueType = typename Table::ValueTraits::TraitType;
  using Traits = typename Table::ValueTraits;

 public:
  static TraceDescriptor GetTraceDescriptor(const void* self) {
    return {self, Trace<WTF::kNoWeakHandling>};
  }

  static TraceDescriptor GetWeakTraceDescriptor(const void* self) {
    return GetWeakTraceDescriptorImpl<ValueType>::GetWeakTraceDescriptor(self);
  }

  template <WTF::WeakHandlingFlag WeakHandling = WTF::kNoWeakHandling>
  static void Trace(Visitor* visitor, const void* self) {
    if (visitor->ConcurrentTracingBailOut({self, &Trace}))
      return;

    static_assert(WTF::IsTraceableInCollectionTrait<Traits>::value ||
                      WTF::IsWeak<ValueType>::value,
                  "T should not be traced");
    WTF::TraceInCollectionTrait<WeakHandling, Backing, void>::Trace(visitor,
                                                                    self);
  }

 private:
  template <typename ValueType>
  struct GetWeakTraceDescriptorImpl {
    static TraceDescriptor GetWeakTraceDescriptor(const void* backing) {
      return {backing, nullptr};
    }
  };

  template <typename K, typename V>
  struct GetWeakTraceDescriptorImpl<WTF::KeyValuePair<K, V>> {
    static TraceDescriptor GetWeakTraceDescriptor(const void* backing) {
      return GetWeakTraceDescriptorKVPImpl<K, V>::GetWeakTraceDescriptor(
          backing);
    }

    template <typename KeyType,
              typename ValueType,
              bool ephemeron_semantics = (WTF::IsWeak<KeyType>::value &&
                                          !WTF::IsWeak<ValueType>::value &&
                                          WTF::IsTraceable<ValueType>::value) ||
                                         (WTF::IsWeak<ValueType>::value &&
                                          !WTF::IsWeak<KeyType>::value &&
                                          WTF::IsTraceable<KeyType>::value)>
    struct GetWeakTraceDescriptorKVPImpl {
      static TraceDescriptor GetWeakTraceDescriptor(const void* backing) {
        return {backing, nullptr};
      }
    };

    template <typename KeyType, typename ValueType>
    struct GetWeakTraceDescriptorKVPImpl<KeyType, ValueType, true> {
      static TraceDescriptor GetWeakTraceDescriptor(const void* backing) {
        return {backing, Trace<WTF::kWeakHandling>};
      }
    };
  };
};

}  // namespace blink

namespace WTF {

// This trace method is for tracing a HashTableBacking either through regular
// tracing (via the relevant TraceTraits) or when finding a HashTableBacking
// through conservative stack scanning (which will treat all references in the
// backing strongly).
template <WTF::WeakHandlingFlag WeakHandling, typename Table>
struct TraceHashTableBackingInCollectionTrait {
  using Value = typename Table::ValueType;
  using Traits = typename Table::ValueTraits;

  static bool Trace(blink::Visitor* visitor, const void* self) {
    static_assert(IsTraceableInCollectionTrait<Traits>::value ||
                      WTF::IsWeak<Value>::value,
                  "Table should not be traced");
    const Value* array = reinterpret_cast<const Value*>(self);
    blink::HeapObjectHeader* header =
        blink::HeapObjectHeader::FromPayload(self);
    // Use the payload size as recorded by the heap to determine how many
    // elements to trace.
    size_t length = header->PayloadSize() / sizeof(Value);
    const bool is_concurrent = visitor->IsConcurrent();
    for (size_t i = 0; i < length; ++i) {
      // If tracing concurrently, use a concurrent-safe version of
      // IsEmptyOrDeletedBucket (check performed on a local copy instead
      // of directly on the bucket).
      if (is_concurrent) {
        if (!HashTableHelper<Value, typename Table::ExtractorType,
                             typename Table::KeyTraitsType>::
                IsEmptyOrDeletedBucketSafe(array[i])) {
          blink::TraceCollectionIfEnabled<WeakHandling, Value, Traits>::Trace(
              visitor, &array[i]);
        }
      } else {
        if (!HashTableHelper<Value, typename Table::ExtractorType,
                             typename Table::KeyTraitsType>::
                IsEmptyOrDeletedBucket(array[i])) {
          blink::TraceCollectionIfEnabled<WeakHandling, Value, Traits>::Trace(
              visitor, &array[i]);
        }
      }
    }
    return false;
  }
};

template <typename Table>
struct TraceInCollectionTrait<kNoWeakHandling,
                              blink::HeapHashTableBacking<Table>,
                              void> {
  static bool Trace(blink::Visitor* visitor, const void* self) {
    return TraceHashTableBackingInCollectionTrait<kNoWeakHandling,
                                                  Table>::Trace(visitor, self);
  }
};

template <typename Table>
struct TraceInCollectionTrait<kWeakHandling,
                              blink::HeapHashTableBacking<Table>,
                              void> {
  static bool Trace(blink::Visitor* visitor, const void* self) {
    return TraceHashTableBackingInCollectionTrait<kWeakHandling, Table>::Trace(
        visitor, self);
  }
};

// Key value pairs, as used in HashMap.  To disambiguate template choice we have
// to have two versions, first the one with no special weak handling, then the
// one with weak handling.
template <typename Key, typename Value, typename Traits>
struct TraceInCollectionTrait<kNoWeakHandling,
                              KeyValuePair<Key, Value>,
                              Traits> {
  using EphemeronHelper =
      blink::EphemeronKeyValuePair<Key,
                                   Value,
                                   typename Traits::KeyTraits,
                                   typename Traits::ValueTraits>;

  static bool Trace(blink::Visitor* visitor,
                    const KeyValuePair<Key, Value>& self) {
    if (WTF::IsWeak<Key>::value != WTF::IsWeak<Value>::value) {
      // Strongification of Weak/Strong and Strong/Weak.
      EphemeronHelper helper(&self.key, &self.value);
      visitor->VisitEphemeronKeyValuePair(
          helper.key, helper.value,
          blink::TraceCollectionIfEnabled<
              kNoWeakHandling, typename EphemeronHelper::KeyType,
              typename EphemeronHelper::KeyTraits>::Trace,
          blink::TraceCollectionIfEnabled<
              kNoWeakHandling, typename EphemeronHelper::ValueType,
              typename EphemeronHelper::ValueTraits>::Trace);
    } else {
      // Strongification of Strong/Strong or Weak/Weak. Order does not matter
      // here.
      blink::TraceCollectionIfEnabled<
          kNoWeakHandling, Key, typename Traits::KeyTraits>::Trace(visitor,
                                                                   &self.key);
      blink::TraceCollectionIfEnabled<
          kNoWeakHandling, Value,
          typename Traits::ValueTraits>::Trace(visitor, &self.value);
    }
    return false;
  }
};

template <typename Key, typename Value, typename Traits>
struct TraceInCollectionTrait<kWeakHandling, KeyValuePair<Key, Value>, Traits> {
  using EphemeronHelper =
      blink::EphemeronKeyValuePair<Key,
                                   Value,
                                   typename Traits::KeyTraits,
                                   typename Traits::ValueTraits>;

  static bool IsAlive(const KeyValuePair<Key, Value>& self) {
    // Needed for Weak/Weak, Strong/Weak (reverse ephemeron), and Weak/Strong
    // (ephemeron). Order of invocation does not matter as tracing weak key or
    // value does not have any side effects.
    return blink::TraceCollectionIfEnabled<
               WeakHandlingTrait<Key>::value, Key,
               typename Traits::KeyTraits>::IsAlive(self.key) &&
           blink::TraceCollectionIfEnabled<
               WeakHandlingTrait<Value>::value, Value,
               typename Traits::ValueTraits>::IsAlive(self.value);
  }

  static bool Trace(blink::Visitor* visitor,
                    const KeyValuePair<Key, Value>& self) {
    EphemeronHelper helper(&self.key, &self.value);
    return visitor->VisitEphemeronKeyValuePair(
        helper.key, helper.value,
        blink::TraceCollectionIfEnabled<
            WeakHandlingTrait<typename EphemeronHelper::KeyType>::value,
            typename EphemeronHelper::KeyType,
            typename EphemeronHelper::KeyTraits>::Trace,
        blink::TraceCollectionIfEnabled<
            WeakHandlingTrait<typename EphemeronHelper::ValueType>::value,
            typename EphemeronHelper::ValueType,
            typename EphemeronHelper::ValueTraits>::Trace);
  }
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_HASH_TABLE_BACKING_H_
