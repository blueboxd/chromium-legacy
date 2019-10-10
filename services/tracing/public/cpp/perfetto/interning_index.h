// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_INTERNING_INDEX_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_INTERNING_INDEX_H_

#include <cstdint>
#include <tuple>

#include "base/component_export.h"
#include "base/containers/mru_cache.h"

namespace tracing {

// Value 0 is an invalid ID.
using InterningID = uint32_t;

struct COMPONENT_EXPORT(TRACING_CPP) InterningIndexEntry {
  InterningID id;

  // Whether the entry was emitted since the last reset of emitted state. If
  // |false|, the sink should (re)emit the entry in the current TracePacket.
  //
  // We don't remove entries on reset of emitted state, so that we can continue
  // to use their original IDs and avoid unnecessarily incrementing the ID
  // counter.
  bool was_emitted;
};

// Interning index that associates interned values with interning IDs. It can
// track entries of different types within the same ID space, e.g. so that both
// copied strings and pointers to static strings can co-exist in the same index.
// Uses base::MRUCaches to track the ID associations while enforcing an upper
// bound on the index size.
template <typename... ValueTypes>
class COMPONENT_EXPORT(TRACING_CPP) InterningIndex {
 public:
  template <typename ValueType>
  using IndexCache = base::MRUCache<ValueType, InterningIndexEntry>;

  // Construct a new index with caches for each of the ValueTypes. The cache
  // size for the n-th ValueType will be limited to max_entry_counts[n] entries.
  //
  // For example, to construct an index containing at most 1000 char* pointers
  // and 100 std::string objects:
  //     InterningIndex<char*, std::string> index(1000, 100);
  template <typename... SizeType>
  InterningIndex(SizeType... max_entry_counts)
      : entry_caches_(max_entry_counts...) {}

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
    auto it = cache.Get(value);
    if (it == cache.end()) {
      it = cache.Put(value, InterningIndexEntry{next_id_++, false});
    }
    bool was_emitted = it->second.was_emitted;
    // The caller will (re)emit the entry, so mark it as emitted.
    it->second.was_emitted = true;
    return InterningIndexEntry{it->second.id, was_emitted};
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
        entry.second.was_emitted = false;
      }
    }
  }

  std::tuple<IndexCache<ValueTypes>...> entry_caches_;
  InterningID next_id_ = 1u;  // ID 0 indicates an unset field value.
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_INTERNING_INDEX_H_
