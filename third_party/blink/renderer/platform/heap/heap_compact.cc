// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/heap_compact.h"

#include <memory>

#include "base/debug/alias.h"
#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_stats_collector.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

// The real worker behind heap compaction, recording references to movable
// objects ("slots".) When the objects end up being compacted and moved,
// relocate() will adjust the slots to point to the new location of the
// object along with handling fixups for interior pointers.
//
// The "fixups" object is created and maintained for the lifetime of one
// heap compaction-enhanced GC.
class HeapCompact::MovableObjectFixups final {
  USING_FAST_MALLOC(HeapCompact::MovableObjectFixups);

 public:
  explicit MovableObjectFixups(ThreadHeap* heap) : heap_(heap) {}
  ~MovableObjectFixups() = default;

  // For the arenas being compacted, record all pages belonging to them.
  // This is needed to handle 'interior slots', pointers that themselves
  // can move (independently from the reference the slot points to.)
  void AddCompactingPage(BasePage* page) {
    DCHECK(!page->IsLargeObjectPage());
    relocatable_pages_.insert(page);
  }

  void AddOrFilter(MovableReference* slot, const char* name) {
    MovableReference value = *slot;
    CHECK(value);

    // All slots and values are part of Oilpan's heap.
    // - Slots may be contained within dead objects if e.g. the write barrier
    //   registered the slot while the out backing itself has not been marked
    //   live in time. Slots in dead objects are filtered below.
    // - Values may only be contained in or point to live objects.

    // Slots handling.
    BasePage* const slot_page =
        heap_->LookupPageForAddress(reinterpret_cast<Address>(slot));
    CHECK(slot_page);
    HeapObjectHeader* const header =
        slot_page->IsLargeObjectPage()
            ? static_cast<LargeObjectPage*>(slot_page)->ObjectHeader()
            : static_cast<NormalPage*>(slot_page)->FindHeaderFromAddress(
                  reinterpret_cast<Address>(slot));
    CHECK(header);
    // Filter the slot since the object that contains the slot is dead.
    if (!header->IsMarked())
      return;

    // Value handling.
    BasePage* const value_page =
        heap_->LookupPageForAddress(reinterpret_cast<Address>(value));
    CHECK(value_page);

    // The following cases are not compacted and do not require recording:
    // - Backings in large pages.
    // - Inline backings that are part of a non-backing arena.
    if (value_page->IsLargeObjectPage() ||
        !HeapCompact::IsCompactableArena(value_page->Arena()->ArenaIndex()))
      return;

    // Slots must reside in and values must point to live objects at this
    // point, with the exception of slots in eagerly swept arenas where objects
    // have already been processed. |value| usually points to a separate
    // backing store but can also point to inlined storage which is why the
    // dynamic header lookup is required.
    HeapObjectHeader* const value_header =
        static_cast<NormalPage*>(value_page)
            ->FindHeaderFromAddress(reinterpret_cast<Address>(value));
    CHECK(value_header);
    CHECK(value_header->IsMarked());

    // Slots may have been recorded already but must point to the same
    // value. Example: Ephemeron iterations may register slots multiple
    // times.
    auto fixup_it = fixups_.find(value);
    if (UNLIKELY(fixup_it != fixups_.end())) {
      CHECK_EQ(slot, fixup_it->second);
      return;
    }

    // Add regular fixup.
    fixups_.insert({value, slot});
    fixup_names_.insert({slot, name});

    // Check whether the slot itself resides on a page that is compacted.
    if (LIKELY(!relocatable_pages_.Contains(slot_page)))
      return;

    auto interior_it = interior_fixups_.find(slot);
    CHECK(interior_fixups_.end() == interior_it);
    interior_fixups_.insert({slot, nullptr});
    LOG_HEAP_COMPACTION() << "Interior slot: " << slot;
  }

  void AddFixupCallback(MovableReference* slot,
                        MovingObjectCallback callback,
                        void* callback_data) {
    DCHECK(!fixup_callbacks_.Contains(slot));
    fixup_callbacks_.insert(
        slot, std::pair<void*, MovingObjectCallback>(callback_data, callback));
  }

  void RelocateInteriorFixups(Address from, Address to, size_t size) {
    // |from| is a valid address for a slot.
    auto interior_it =
        interior_fixups_.lower_bound(reinterpret_cast<MovableReference*>(from));
    if (interior_it == interior_fixups_.end())
      return;

    CHECK_GE(reinterpret_cast<Address>(interior_it->first), from);
    size_t offset = reinterpret_cast<Address>(interior_it->first) - from;
    while (offset < size) {
      if (!interior_it->second) {
        // Update the interior fixup value, so that when the object the slot is
        // pointing to is moved, it can re-use this value.
        Address fixup = to + offset;
        interior_it->second = fixup;

        // If the |slot|'s content is pointing into the region [from, from +
        // size) we are dealing with an interior pointer that does not point to
        // a valid HeapObjectHeader. Such references need to be fixed up
        // immediately.
        Address fixup_contents = *reinterpret_cast<Address*>(fixup);
        if (fixup_contents > from && fixup_contents < (from + size)) {
          *reinterpret_cast<Address*>(fixup) = fixup_contents - from + to;
        }
      }

      interior_it++;
      if (interior_it == interior_fixups_.end())
        return;
      offset = reinterpret_cast<Address>(interior_it->first) - from;
    }
  }

  void Relocate(Address from, Address to) {
    auto it = fixups_.find(from);
    // This means that there is no corresponding slot for a live backing store.
    // This may happen because a mutator may change the slot to point to a
    // different backing store because e.g.:
    // - Incremental marking marked a backing store as live that was later on
    //   replaced.
    // - Backings were changed when being processed in
    //   EagerSweep/PreFinalizer/WeakProcessing.
    if (it == fixups_.end())
      return;

#if DCHECK_IS_ON()
    BasePage* from_page = PageFromObject(from);
    DCHECK(relocatable_pages_.Contains(from_page));
#endif

    // TODO(keishi): Code to determine if crash is related to interior fixups.
    // Remove when finished. crbug.com/918064
    enum DebugSlotType {
      kNormalSlot,
      kInteriorSlotPreMove,
      kInteriorSlotPostMove,
    };
    DebugSlotType slot_type = kNormalSlot;
    base::debug::Alias(&slot_type);

    // TODO(918064): Remove this after getting the type of the object that
    // crashes compaction.
    constexpr size_t kMaxNameLen = 256;
    char slot_container_name[kMaxNameLen];
    base::debug::Alias(slot_container_name);

    // If the object is referenced by a slot that is contained on a compacted
    // area itself, check whether it can be updated already.
    MovableReference* slot = reinterpret_cast<MovableReference*>(it->second);
    auto interior_it = interior_fixups_.find(slot);
    if (interior_it != interior_fixups_.end()) {
      MovableReference* slot_location =
          reinterpret_cast<MovableReference*>(interior_it->second);
      if (!slot_location) {
        interior_it->second = to;
        slot_type = kInteriorSlotPreMove;
        const char* name = fixup_names_.find(slot)->second;
        size_t len = strlen(name);
        if (len > kMaxNameLen)
          len = kMaxNameLen;
        strncpy(slot_container_name, name, len);
        slot_container_name[len - 1] = 0;
      } else {
        LOG_HEAP_COMPACTION()
            << "Redirected slot: " << slot << " => " << slot_location;
        slot = slot_location;
        slot_type = kInteriorSlotPostMove;
      }
    }

    // If the slot has subsequently been updated, a prefinalizer or
    // a destructor having mutated and expanded/shrunk the collection,
    // do not update and relocate the slot -- |from| is no longer valid
    // and referenced.
    //
    // The slot's contents may also have been cleared during weak processing;
    // no work to be done in that case either.
    if (UNLIKELY(*slot != from)) {
      LOG_HEAP_COMPACTION()
          << "No relocation: slot = " << slot << ", *slot = " << *slot
          << ", from = " << from << ", to = " << to;
      VerifyUpdatedSlot(slot);
      return;
    }

    // Update the slots new value.
    *slot = to;

    size_t size = 0;

    // Execute potential fixup callbacks.
    MovableReference* callback_slot =
        reinterpret_cast<MovableReference*>(it->second);
    auto callback = fixup_callbacks_.find(callback_slot);
    if (UNLIKELY(callback != fixup_callbacks_.end())) {
      size = HeapObjectHeader::FromPayload(to)->PayloadSize();
      callback->value.second(callback->value.first, from, to, size);
    }

    if (interior_fixups_.empty())
      return;

    if (!size)
      size = HeapObjectHeader::FromPayload(to)->PayloadSize();
    RelocateInteriorFixups(from, to, size);
  }

#if DEBUG_HEAP_COMPACTION
  void dumpDebugStats() {
    LOG_HEAP_COMPACTION() << "Fixups: pages=" << relocatable_pages_.size()
                          << " objects=" << fixups_.size()
                          << " callbacks=" << fixup_callbacks_.size()
                          << " interior-size="
                          << interior_fixups_.size());
  }
#endif

 private:
  void VerifyUpdatedSlot(MovableReference* slot);

  ThreadHeap* const heap_;

  // Tracking movable and updatable references. For now, we keep a
  // map which for each movable object, recording the slot that
  // points to it. Upon moving the object, that slot needs to be
  // updated.
  //
  // (TODO: consider in-place updating schemes.)
  std::unordered_map<MovableReference, MovableReference*> fixups_;
  std::unordered_map<MovableReference*, const char*> fixup_names_;

  // Map from movable reference to callbacks that need to be invoked
  // when the object moves.
  HashMap<MovableReference*, std::pair<void*, MovingObjectCallback>>
      fixup_callbacks_;

  // Map of interior slots to their final location. Needs to be an ordered map
  // as it is used to walk through slots starting at a given memory address.
  // Requires log(n) lookup to make the early bailout reasonably fast. Currently
  // only std::map fullfills those requirements.
  //
  // - The initial value for a given key is nullptr.
  // - Upon moving a an object this value is adjusted accordingly.
  std::map<MovableReference*, Address> interior_fixups_;

  // All pages that are being compacted. The set keeps references to
  // BasePage instances. The void* type was selected to allow to check
  // arbitrary addresses.
  HashSet<void*> relocatable_pages_;
};

void HeapCompact::MovableObjectFixups::VerifyUpdatedSlot(
    MovableReference* slot) {
// Verify that the already updated slot is valid, meaning:
//  - has been cleared.
//  - has been updated & expanded with a large object backing store.
//  - has been updated with a larger, freshly allocated backing store.
//    (on a fresh page in a compactable arena that is not being
//    compacted.)
#if DCHECK_IS_ON()
  if (!*slot)
    return;
  BasePage* slot_page =
      heap_->LookupPageForAddress(reinterpret_cast<Address>(*slot));
  // ref_page is null if *slot is pointing to an off-heap region. This may
  // happy if *slot is pointing to an inline buffer of HeapVector with
  // inline capacity.
  if (!slot_page)
    return;
  DCHECK(slot_page->IsLargeObjectPage() ||
         (HeapCompact::IsCompactableArena(slot_page->Arena()->ArenaIndex()) &&
          !relocatable_pages_.Contains(slot_page)));
#endif  // DCHECK_IS_ON()
}

HeapCompact::HeapCompact(ThreadHeap* heap) : heap_(heap) {
  // The heap compaction implementation assumes the contiguous range,
  //
  //   [Vector1ArenaIndex, HashTableArenaIndex]
  //
  // in a few places. Use static asserts here to not have that assumption
  // be silently invalidated by ArenaIndices changes.
  static_assert(BlinkGC::kVector1ArenaIndex + 3 == BlinkGC::kVector4ArenaIndex,
                "unexpected ArenaIndices ordering");
  static_assert(
      BlinkGC::kVector4ArenaIndex + 1 == BlinkGC::kInlineVectorArenaIndex,
      "unexpected ArenaIndices ordering");
  static_assert(
      BlinkGC::kInlineVectorArenaIndex + 1 == BlinkGC::kHashTableArenaIndex,
      "unexpected ArenaIndices ordering");
}

HeapCompact::~HeapCompact() = default;

HeapCompact::MovableObjectFixups& HeapCompact::Fixups() {
  if (!fixups_)
    fixups_ = std::make_unique<MovableObjectFixups>(heap_);
  return *fixups_;
}

bool HeapCompact::ShouldCompact(BlinkGC::StackState stack_state,
                                BlinkGC::MarkingType marking_type,
                                BlinkGC::GCReason reason) {
  if (!RuntimeEnabledFeatures::HeapCompactionEnabled()) {
    return false;
  }

  DCHECK_NE(BlinkGC::MarkingType::kTakeSnapshot, marking_type);
  if (marking_type == BlinkGC::MarkingType::kAtomicMarking &&
      stack_state == BlinkGC::StackState::kHeapPointersOnStack) {
    // The following check ensures that tests that want to test compaction are
    // not interrupted by garbage collections that cannot use compaction.
    CHECK(!force_for_next_gc_);
    return false;
  }

  UpdateHeapResidency();

  if (force_for_next_gc_) {
    return true;
  }

  // Only enable compaction when in a memory reduction garbage collection as it
  // may significantly increase the final garbage collection pause.
  if (reason == BlinkGC::GCReason::kUnifiedHeapForMemoryReductionGC) {
    return free_list_size_ > kFreeListSizeThreshold;
  }

  return false;
}

void HeapCompact::Initialize(ThreadState* state) {
  DCHECK(RuntimeEnabledFeatures::HeapCompactionEnabled());
  LOG_HEAP_COMPACTION() << "Compacting: free=" << free_list_size_;
  do_compact_ = true;
  fixups_.reset();
  gc_count_since_last_compaction_ = 0;
  force_for_next_gc_ = false;
}

void HeapCompact::RegisterMovingObjectReference(const char* name,
                                                MovableReference* slot) {
  CHECK(heap_->LookupPageForAddress(reinterpret_cast<Address>(slot)));

  if (!do_compact_)
    return;

  traced_slots_.insert(slot);
  traced_slots_names_.insert(slot, name);
}

void HeapCompact::RegisterMovingObjectCallback(MovableReference* slot,
                                               MovingObjectCallback callback,
                                               void* callback_data) {
  DCHECK(heap_->LookupPageForAddress(reinterpret_cast<Address>(slot)));

  if (!do_compact_)
    return;

  Fixups().AddFixupCallback(slot, callback, callback_data);
}

void HeapCompact::UpdateHeapResidency() {
  size_t total_arena_size = 0;
  size_t total_free_list_size = 0;

  compactable_arenas_ = 0;
#if DEBUG_HEAP_FREELIST
  std::stringstream stream;
#endif
  for (int i = BlinkGC::kVector1ArenaIndex; i <= BlinkGC::kHashTableArenaIndex;
       ++i) {
    NormalPageArena* arena = static_cast<NormalPageArena*>(heap_->Arena(i));
    size_t arena_size = arena->ArenaSize();
    size_t free_list_size = arena->FreeListSize();
    total_arena_size += arena_size;
    total_free_list_size += free_list_size;
#if DEBUG_HEAP_FREELIST
    stream << i << ": [" << arena_size << ", " << free_list_size << "], ";
#endif
    // TODO: be more discriminating and consider arena
    // load factor, effectiveness of past compactions etc.
    if (!arena_size)
      continue;
    // Mark the arena as compactable.
    compactable_arenas_ |= 0x1u << i;
  }
#if DEBUG_HEAP_FREELIST
  LOG_HEAP_FREELIST() << "Arena residencies: {" << stream.str() << "}";
  LOG_HEAP_FREELIST() << "Total = " << total_arena_size
                      << ", Free = " << total_free_list_size;
#endif

  // TODO(sof): consider smoothing the reported sizes.
  free_list_size_ = total_free_list_size;
}

void HeapCompact::FinishedArenaCompaction(NormalPageArena* arena,
                                          size_t freed_pages,
                                          size_t freed_size) {
  if (!do_compact_)
    return;

  heap_->stats_collector()->IncreaseCompactionFreedPages(freed_pages);
  heap_->stats_collector()->IncreaseCompactionFreedSize(freed_size);
}

void HeapCompact::Relocate(Address from, Address to) {
  Fixups().Relocate(from, to);
}

void HeapCompact::FilterNonLiveSlots() {
  if (!do_compact_)
    return;

  last_fixup_count_for_testing_ = 0;
  for (auto** slot : traced_slots_) {
    if (*slot) {
      Fixups().AddOrFilter(slot, traced_slots_names_.find(slot)->value);
      last_fixup_count_for_testing_++;
    }
  }
  traced_slots_.clear();
  traced_slots_names_.clear();
}

void HeapCompact::Finish() {
  if (!do_compact_)
    return;

#if DEBUG_HEAP_COMPACTION
  if (fixups_)
    fixups_->dumpDebugStats();
#endif
  fixups_.reset();
  do_compact_ = false;
}

void HeapCompact::Cancel() {
  if (!do_compact_)
    return;

  last_fixup_count_for_testing_ = 0;
  traced_slots_.clear();
  traced_slots_names_.clear();
  fixups_.reset();
  do_compact_ = false;
}

void HeapCompact::AddCompactingPage(BasePage* page) {
  DCHECK(do_compact_);
  DCHECK(IsCompactingArena(page->Arena()->ArenaIndex()));
  Fixups().AddCompactingPage(page);
}

}  // namespace blink
