// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_TRACE_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_TRACE_TRAITS_H_

#include "base/optional.h"
#include "third_party/blink/renderer/platform/heap/gc_info.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/hash_counted_set.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/hash_table.h"
#include "third_party/blink/renderer/platform/wtf/linked_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/list_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"

namespace blink {

template <typename Table>
class HeapHashTableBacking;
template <typename ValueArg, wtf_size_t inlineCapacity>
class HeapListHashSetAllocator;
template <typename T>
struct TraceTrait;
template <typename T>
class WeakMember;

template <typename T, bool = NeedsAdjustPointer<T>::value>
struct AdjustPointerTrait;

template <typename T>
struct AdjustPointerTrait<T, false> {
  STATIC_ONLY(AdjustPointerTrait);

  static TraceDescriptor GetTraceDescriptor(const void* self) {
    return {self, TraceTrait<T>::Trace};
  }

  static HeapObjectHeader* GetHeapObjectHeader(const void* self) {
    return HeapObjectHeader::FromPayload(self);
  }
};

template <typename T>
struct AdjustPointerTrait<T, true> {
  STATIC_ONLY(AdjustPointerTrait);

  static TraceDescriptor GetTraceDescriptor(const void* self) {
    return static_cast<const T*>(self)->GetTraceDescriptor();
  }

  static HeapObjectHeader* GetHeapObjectHeader(const void* self) {
    return static_cast<const T*>(self)->GetHeapObjectHeader();
  }
};

template <typename T, bool = WTF::IsTraceable<T>::value>
struct TraceIfNeeded;

template <typename T>
struct TraceIfNeeded<T, false> {
  STATIC_ONLY(TraceIfNeeded);
  static void Trace(Visitor*, const T&) {}
};

template <typename T>
struct TraceIfNeeded<T, true> {
  STATIC_ONLY(TraceIfNeeded);
  static void Trace(Visitor* visitor, const T& t) { visitor->Trace(t); }
};

template <WTF::WeakHandlingFlag weakness,
          typename T,
          typename Traits,
          bool = WTF::IsTraceableInCollectionTrait<Traits>::value,
          WTF::WeakHandlingFlag = WTF::WeakHandlingTrait<T>::value>
struct TraceCollectionIfEnabled;

template <WTF::WeakHandlingFlag weakness, typename T, typename Traits>
struct TraceCollectionIfEnabled<weakness,
                                T,
                                Traits,
                                false,
                                WTF::kNoWeakHandling> {
  STATIC_ONLY(TraceCollectionIfEnabled);

  static bool IsAlive(const T&) { return true; }

  static bool Trace(Visitor*, const void*) {
    static_assert(!WTF::IsTraceableInCollectionTrait<Traits>::value,
                  "T should not be traced");
    return false;
  }
};

template <typename T, typename Traits>
struct TraceCollectionIfEnabled<WTF::kNoWeakHandling,
                                T,
                                Traits,
                                false,
                                WTF::kWeakHandling> {
  STATIC_ONLY(TraceCollectionIfEnabled);

  static bool Trace(Visitor* visitor, const void* t) {
    return WTF::TraceInCollectionTrait<WTF::kNoWeakHandling, T, Traits>::Trace(
        visitor, *reinterpret_cast<const T*>(t));
  }
};

template <WTF::WeakHandlingFlag weakness,
          typename T,
          typename Traits,
          bool,
          WTF::WeakHandlingFlag>
struct TraceCollectionIfEnabled {
  STATIC_ONLY(TraceCollectionIfEnabled);

  static bool IsAlive(const T& traceable) {
    return WTF::TraceInCollectionTrait<weakness, T, Traits>::IsAlive(traceable);
  }

  static bool Trace(Visitor* visitor, const void* t) {
    static_assert(WTF::IsTraceableInCollectionTrait<Traits>::value ||
                      weakness == WTF::kWeakHandling,
                  "Traits should be traced");
    return WTF::TraceInCollectionTrait<weakness, T, Traits>::Trace(
        visitor, *reinterpret_cast<const T*>(t));
  }
};

// The TraceTrait is used to specify how to trace and object for Oilpan and
// wrapper tracing.
//
//
// By default, the 'Trace' method implemented on an object itself is
// used to trace the pointers to other heap objects inside the object.
//
// However, the TraceTrait can be specialized to use a different
// implementation. A common case where a TraceTrait specialization is
// needed is when multiple inheritance leads to pointers that are not
// to the start of the object in the Blink garbage-collected heap. In
// that case the pointer has to be adjusted before marking.
template <typename T>
struct TraceTrait {
  STATIC_ONLY(TraceTrait);

 public:
  static TraceDescriptor GetTraceDescriptor(const void* self) {
    return AdjustPointerTrait<T>::GetTraceDescriptor(
        static_cast<const T*>(self));
  }

  static TraceDescriptor GetWeakTraceDescriptor(const void* self) {
    return {self, nullptr};
  }

  static HeapObjectHeader* GetHeapObjectHeader(const void* self) {
    return AdjustPointerTrait<T>::GetHeapObjectHeader(
        static_cast<const T*>(self));
  }

  static void Trace(Visitor*, const void* self);
};

template <typename T>
struct TraceTrait<const T> : public TraceTrait<T> {};

template <typename T>
void TraceTrait<T>::Trace(Visitor* visitor, const void* self) {
  static_assert(WTF::IsTraceable<T>::value, "T should not be traced");
  static_cast<T*>(const_cast<void*>(self))->Trace(visitor);
}

// This trace trait for std::pair will null weak members if their referent is
// collected. If you have a collection that contain weakness it does not remove
// entries from the collection that contain nulled weak members.
template <typename T, typename U>
struct TraceTrait<std::pair<T, U>> {
  STATIC_ONLY(TraceTrait);

 public:
  static void Trace(Visitor* visitor, const std::pair<T, U>* pair) {
    TraceIfNeeded<T>::Trace(visitor, pair->first);
    TraceIfNeeded<U>::Trace(visitor, pair->second);
  }
};

// While using base::Optional<T> with garbage-collected types is generally
// disallowed by the OptionalGarbageCollected check in blink_gc_plugin,
// garbage-collected containers such as HeapVector are allowed and need to be
// traced.
template <typename T>
struct TraceTrait<base::Optional<T>> {
  STATIC_ONLY(TraceTrait);

 public:
  static void Trace(Visitor* visitor, const base::Optional<T>* optional) {
    if (*optional != base::nullopt) {
      TraceIfNeeded<T>::Trace(visitor, optional->value());
    }
  }
};

// Reorders parameters for use in blink::Visitor::VisitEphemeronKeyValuePair.
template <typename _KeyType,
          typename _ValueType,
          typename _KeyTraits,
          typename _ValueTraits,
          bool = WTF::IsWeak<_ValueType>::value>
struct EphemeronKeyValuePair {
  using KeyType = _KeyType;
  using ValueType = _ValueType;
  using KeyTraits = _KeyTraits;
  using ValueTraits = _ValueTraits;

  EphemeronKeyValuePair(const KeyType* k, const ValueType* v)
      : key(k), value(v) {}
  const KeyType* key;
  const ValueType* value;
};

template <typename _KeyType,
          typename _ValueType,
          typename _KeyTraits,
          typename _ValueTraits>
struct EphemeronKeyValuePair<_KeyType,
                             _ValueType,
                             _KeyTraits,
                             _ValueTraits,
                             true> : EphemeronKeyValuePair<_ValueType,
                                                           _KeyType,
                                                           _ValueTraits,
                                                           _KeyTraits,
                                                           false> {
  EphemeronKeyValuePair(const _KeyType* k, const _ValueType* v)
      : EphemeronKeyValuePair<_ValueType,
                              _KeyType,
                              _ValueTraits,
                              _KeyTraits,
                              false>(v, k) {}
};

}  // namespace blink

namespace WTF {

// Catch-all for types that have a way to trace that don't have special
// handling for weakness in collections.  This means that if this type
// contains WeakMember fields, they will simply be zeroed, but the entry
// will not be removed from the collection.  This always happens for
// things in vectors, which don't currently support special handling of
// weak elements.
template <typename T, typename Traits>
struct TraceInCollectionTrait<kNoWeakHandling, T, Traits> {
  static bool IsAlive(const T& t) { return true; }

  static bool Trace(blink::Visitor* visitor, const T& t) {
    static_assert(IsTraceableInCollectionTrait<Traits>::value,
                  "T should not be traced");
    visitor->Trace(t);
    return false;
  }
};

template <typename T, typename Traits>
struct TraceInCollectionTrait<kNoWeakHandling, blink::Member<T>, Traits> {
  static bool IsAlive(const blink::Member<T>& t) { return true; }
  static bool Trace(blink::Visitor* visitor, const blink::Member<T>& t) {
    visitor->TraceMaybeDeleted(t);
    return false;
  }
};

template <typename T, typename Traits>
struct TraceInCollectionTrait<kWeakHandling, blink::Member<T>, Traits> {
  static bool IsAlive(const blink::Member<T>& t) { return true; }
  static bool Trace(blink::Visitor* visitor, const blink::Member<T>& t) {
    visitor->TraceMaybeDeleted(t);
    return false;
  }
};

template <typename T, typename Traits>
struct TraceInCollectionTrait<kNoWeakHandling, blink::WeakMember<T>, Traits> {
  static bool Trace(blink::Visitor* visitor, const blink::WeakMember<T>& t) {
    visitor->TraceMaybeDeleted(t);
    return false;
  }
};

// Catch-all for types that have HashTrait support for tracing with weakness.
// Empty to enforce specialization.
template <typename T, typename Traits>
struct TraceInCollectionTrait<kWeakHandling, T, Traits> {};

template <typename T, typename Traits>
struct TraceInCollectionTrait<kWeakHandling, blink::WeakMember<T>, Traits> {
  static bool IsAlive(const blink::WeakMember<T>& value) {
    return blink::ThreadHeap::IsHeapObjectAlive(value);
  }

  static bool Trace(blink::Visitor* visitor,
                    const blink::WeakMember<T>& value) {
    return !blink::ThreadHeap::IsHeapObjectAlive(value);
  }
};

// This specialization of TraceInCollectionTrait is for the backing of
// HeapListHashSet.  This is for the case that we find a reference to the
// backing from the stack.  That probably means we have a GC while we are in a
// ListHashSet method since normal API use does not put pointers to the backing
// on the stack.
template <typename NodeContents,
          size_t inlineCapacity,
          typename T,
          typename U,
          typename V,
          typename W,
          typename X,
          typename Y>
struct TraceInCollectionTrait<
    kNoWeakHandling,
    blink::HeapHashTableBacking<HashTable<
        ListHashSetNode<NodeContents,
                        blink::HeapListHashSetAllocator<T, inlineCapacity>>*,
        U,
        V,
        W,
        X,
        Y,
        blink::HeapAllocator>>,
    void> {
  using Node =
      ListHashSetNode<NodeContents,
                      blink::HeapListHashSetAllocator<T, inlineCapacity>>;
  using Table = HashTable<Node*, U, V, W, X, Y, blink::HeapAllocator>;

  static bool Trace(blink::Visitor* visitor, const void* self) {
    const Node* const* array = reinterpret_cast<const Node* const*>(self);
    blink::HeapObjectHeader* header =
        blink::HeapObjectHeader::FromPayload(self);
    size_t length = header->PayloadSize() / sizeof(Node*);
    const bool is_concurrent = visitor->IsConcurrent();
    for (size_t i = 0; i < length; ++i) {
      const Node* node;
      if (is_concurrent) {
        // If tracing concurrently, IsEmptyOrDeletedBucket can cause data
        // races. Loading array[i] atomically prevents possible data races.
        // array[i] is of type Node* so can directly loaded atomically.
        node = AsAtomicPtr(&array[i])->load(std::memory_order_relaxed);
      } else {
        node = array[i];
      }
      if (!HashTableHelper<
              const Node*, typename Table::ExtractorType,
              typename Table::KeyTraitsType>::IsEmptyOrDeletedBucket(node)) {
        visitor->Trace(node);
      }
    }
    return false;
  }
};

// Nodes used by LinkedHashSet.  Again we need two versions to disambiguate the
// template.
template <typename Value, typename Traits>
struct TraceInCollectionTrait<kNoWeakHandling,
                              LinkedHashSetNode<Value>,
                              Traits> {
  static bool IsAlive(const LinkedHashSetNode<Value>& self) {
    return TraceInCollectionTrait<
        kNoWeakHandling, Value,
        typename Traits::ValueTraits>::IsAlive(self.value_);
  }

  static bool Trace(blink::Visitor* visitor,
                    const LinkedHashSetNode<Value>& self) {
    static_assert(
        IsTraceableInCollectionTrait<Traits>::value || IsWeak<Value>::value,
        "T should not be traced");
    return TraceInCollectionTrait<
        kNoWeakHandling, Value,
        typename Traits::ValueTraits>::Trace(visitor, self.value_);
  }
};

template <typename Value, typename Traits>
struct TraceInCollectionTrait<kWeakHandling, LinkedHashSetNode<Value>, Traits> {
  static bool IsAlive(const LinkedHashSetNode<Value>& self) {
    return TraceInCollectionTrait<
        kWeakHandling, Value,
        typename Traits::ValueTraits>::IsAlive(self.value_);
  }

  static bool Trace(blink::Visitor* visitor,
                    const LinkedHashSetNode<Value>& self) {
    return TraceInCollectionTrait<
        kWeakHandling, Value, typename Traits::ValueTraits>::Trace(visitor,
                                                                   self.value_);
  }
};

// ListHashSetNode pointers (a ListHashSet is implemented as a hash table of
// these pointers).
template <typename Value, size_t inlineCapacity, typename Traits>
struct TraceInCollectionTrait<
    kNoWeakHandling,
    ListHashSetNode<Value,
                    blink::HeapListHashSetAllocator<Value, inlineCapacity>>*,
    Traits> {
  using Node =
      ListHashSetNode<Value,
                      blink::HeapListHashSetAllocator<Value, inlineCapacity>>;

  static bool Trace(blink::Visitor* visitor, const Node* node) {
    static_assert(!IsWeak<Node>::value,
                  "ListHashSet does not support weakness");
    static_assert(IsTraceableInCollectionTrait<Traits>::value,
                  "T should not be traced");
    visitor->Trace(node);
    return false;
  }
};

}  // namespace WTF

#endif
