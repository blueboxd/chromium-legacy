// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_INTERNING_INDEX_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_INTERNING_INDEX_H_

#include <array>
#include <cstdint>
#include <tuple>

#include "base/component_export.h"

namespace tracing {

// Value 0 is an invalid ID.
using InterningID = uint32_t;

struct COMPONENT_EXPORT(TRACING_CPP) InterningIndexEntry {
  InterningID id = 0;

  // Whether the entry was emitted since the last reset of emitted state. If
  // |false|, the sink should (re)emit the entry in the current TracePacket.
  //
  // We don't remove entries on reset of emitted state, so that we can continue
  // to use their original IDs and avoid unnecessarily incrementing the ID
  // counter.
  bool was_emitted = false;
};

// Interning index that associates interned values with interning IDs. It can
// track entries of different types within the same ID space, e.g. so that both
// copied strings and pointers to static strings can co-exist in the same index.
//
// The index will cache up to |N| values per ValueType after which it will start
// replacing them in a FIFO basis. N must be a power of 2 for performance
// reasons.
template <size_t N, typename... ValueTypes>
class COMPONENT_EXPORT(TRACING_CPP) InterningIndex {
 public:
  // IndexCache is just a pair of std::arrays which arg kept in sync with each
  // other. This has an advantage over a std::array<pairs> since we can load a
  // bunch of keys to search in one cache line without loading values.
  template <typename ValueType>
  class IndexCache {
   public:
    IndexCache() {
      // Assert that N is a power of two. This allows the "% N" in Insert() to
      // compile to a bunch of bit shifts which improves performance over an
      // arbitrary modulo division.
      static_assert(
          N && ((N & (N - 1)) == 0),
          "InterningIndex requires that the cache size be a power of 2.");
    }

    void Clear() {
      for (auto& val : keys_) {
        val = ValueType{};
      }
    }

    typename std::array<InterningIndexEntry, N>::iterator Find(
        const ValueType& value) {
      auto it = std::find(keys_.begin(), keys_.end(), value);
      // If the find above returns it == keys_.end(), then (it - keys_.begin())
      // will equal keys_.size() which is equal to values_.size(). So
      // values_.begin() + values_size() will be values_.end() and we've
      // returned the correct value. This saves us checking a conditional in
      // this function.
      return values_.begin() + (it - keys_.begin());
    }

    typename std::array<InterningIndexEntry, N>::iterator Insert(
        const ValueType& key,
        InterningIndexEntry&& value) {
      size_t new_position = current_index_++ % N;
      keys_[new_position] = key;
      values_[new_position] = std::move(value);
      return values_.begin() + new_position;
    }

    typename std::array<InterningIndexEntry, N>::iterator begin() {
      return values_.begin();
    }
    typename std::array<InterningIndexEntry, N>::iterator end() {
      return values_.end();
    }

   private:
    size_t current_index_ = 0;
    std::array<ValueType, N> keys_{{}};
    std::array<InterningIndexEntry, N> values_{{}};
  };

  // Construct a new index with caches for each of the ValueTypes. Every cache
  // is |N| elements.
  //
  // For example, to construct an index containing at most 1024 char* pointers
  // and 1024 std::string objects:
  //     InterningIndex<1024, char*, std::string> index;
  InterningIndex() = default;

  // Returns the entry for the given interned |value|, adding it to the index if
  // it didn't exist previously or was evicted from the index. Entries may be
  // evicted if they are accessed infrequently and the index for the respective
  // ValueType is at full capacity.
  //
  // If the returned entry's |was_emitted| flag is false, the caller should
  // (re)emit the entry in the current TracePacket's InternedData message.
  template <typename ValueType>
  InterningIndexEntry LookupOrAdd(const ValueType& value) {
    IndexCache<ValueType>& cache =
        std::get<IndexCache<ValueType>>(entry_caches_);
    auto it = cache.Find(value);
    if (it == cache.end()) {
      it = cache.Insert(value, InterningIndexEntry{next_id_++, false});
    }
    bool was_emitted = it->was_emitted;
    // The caller will (re)emit the entry, so mark it as emitted.
    it->was_emitted = true;
    return InterningIndexEntry{it->id, was_emitted};
  }

  // Marks all entries as "not emitted", so that they will be reemitted when
  // next accessed.
  void ResetEmittedState() { ResetStateHelper<ValueTypes...>(/*clear=*/false); }

  // Removes all entries from the index and restarts from the first valid ID.
  void Clear() {
    ResetStateHelper<ValueTypes...>(/*clear=*/true);
    next_id_ = 1u;
  }

 private:
  // Recursive helper template methods to reset the emitted state for all
  // entries or remove all entries from each of the caches.
  template <typename ValueType>
  void ResetStateHelper(bool clear) {
    return ResetStateForValueType<ValueType>(clear);
  }

  template <typename ValueType1,
            typename ValueType2,
            typename... RemainingValueTypes>
  void ResetStateHelper(bool clear) {
    ResetStateForValueType<ValueType1>(clear);
    ResetStateHelper<ValueType2, RemainingValueTypes...>(clear);
  }

  template <typename ValueType>
  void ResetStateForValueType(bool clear) {
    auto& cache = std::get<IndexCache<ValueType>>(entry_caches_);
    if (clear) {
      cache.Clear();
    } else {
      for (auto& entry : cache) {
        entry.was_emitted = false;
      }
    }
  }

  std::tuple<IndexCache<ValueTypes>...> entry_caches_;
  InterningID next_id_ = 1u;  // ID 0 indicates an unset field value.
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_INTERNING_INDEX_H_
