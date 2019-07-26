/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_THREAD_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_THREAD_STATE_H_

#include <memory>

#include "base/macros.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/heap/atomic_entry_flag.h"
#include "third_party/blink/renderer/platform/heap/blink_gc.h"
#include "third_party/blink/renderer/platform/heap/cancelable_task_scheduler.h"
#include "third_party/blink/renderer/platform/heap/threading_traits.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/public/rail_mode_observer.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/linked_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/sanitizers.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace v8 {
class EmbedderGraph;
class Isolate;
}

namespace blink {

namespace incremental_marking_test {
class IncrementalMarkingScope;
class IncrementalMarkingTestDriver;
}  // namespace incremental_marking_test

class MarkingVisitor;
class PersistentNode;
class PersistentRegion;
class ThreadHeap;
class ThreadState;
class UnifiedHeapController;
class Visitor;

template <ThreadAffinity affinity>
class ThreadStateFor;

// Declare that a class has a pre-finalizer. The pre-finalizer is called
// before any object gets swept, so it is safe to touch on-heap objects
// that may be collected in the same GC cycle. If you cannot avoid touching
// on-heap objects in a destructor (which is not allowed), you can consider
// using the pre-finalizer. The only restriction is that the pre-finalizer
// must not resurrect dead objects (e.g., store unmarked objects into
// Members etc). The pre-finalizer is called on the thread that registered
// the pre-finalizer.
//
// Since a pre-finalizer adds pressure on GC performance, you should use it
// only if necessary.
//
// A pre-finalizer is similar to the
// HeapHashMap<WeakMember<Foo>, std::unique_ptr<Disposer>> idiom.  The
// difference between this and the idiom is that pre-finalizer function is
// called whenever an object is destructed with this feature.  The
// HeapHashMap<WeakMember<Foo>, std::unique_ptr<Disposer>> idiom requires an
// assumption that the HeapHashMap outlives objects pointed by WeakMembers.
// FIXME: Replace all of the
// HeapHashMap<WeakMember<Foo>, std::unique_ptr<Disposer>> idiom usages with the
// pre-finalizer if the replacement won't cause performance regressions.
//
// Usage:
//
// class Foo : GarbageCollected<Foo> {
//     USING_PRE_FINALIZER(Foo, dispose);
// private:
//     void dispose()
//     {
//         bar_->...; // It is safe to touch other on-heap objects.
//     }
//     Member<Bar> bar_;
// };
#define USING_PRE_FINALIZER(Class, preFinalizer)                           \
 public:                                                                   \
  static bool InvokePreFinalizer(void* object) {                           \
    Class* self = reinterpret_cast<Class*>(object);                        \
    if (ThreadHeap::IsHeapObjectAlive(self))                               \
      return false;                                                        \
    self->Class::preFinalizer();                                           \
    return true;                                                           \
  }                                                                        \
                                                                           \
 private:                                                                  \
  ThreadState::PrefinalizerRegistration<Class> prefinalizer_dummy_ = this; \
  using UsingPreFinalizerMacroNeedsTrailingSemiColon = char

class PLATFORM_EXPORT BlinkGCObserver {
  USING_FAST_MALLOC(BlinkGCObserver);

 public:
  // The constructor automatically register this object to ThreadState's
  // observer lists. The argument must not be null.
  explicit BlinkGCObserver(ThreadState*);

  // The destructor automatically unregister this object from ThreadState's
  // observer lists.
  virtual ~BlinkGCObserver();

  virtual void OnCompleteSweepDone() = 0;

 private:
  // As a ThreadState must live when a BlinkGCObserver lives, holding a raw
  // pointer is safe.
  ThreadState* thread_state_;
};

class PLATFORM_EXPORT ThreadState final : private RAILModeObserver {
  USING_FAST_MALLOC(ThreadState);

 public:
  // See setGCState() for possible state transitions.
  enum GCState {
    kNoGCScheduled,
    kIncrementalMarkingStepPaused,
    kIncrementalMarkingStepScheduled,
    kIncrementalMarkingFinalizeScheduled,
    kPreciseGCScheduled,
    kForcedGCForTestingScheduled,
    kIncrementalGCScheduled,
  };

  // The phase that the GC is in. The GCPhase will not return kNone for mutators
  // running during incremental marking and lazy sweeping. See SetGCPhase() for
  // possible state transitions.
  enum class GCPhase {
    // GC is doing nothing.
    kNone,
    // GC is in marking phase.
    kMarking,
    // GC is in sweeping phase.
    kSweeping,
  };

  class AtomicPauseScope;
  class GCForbiddenScope;
  class MainThreadGCForbiddenScope;
  class NoAllocationScope;
  class ObjectResurrectionForbiddenScope;
  class SweepForbiddenScope;

  using V8TraceRootsCallback = void (*)(v8::Isolate*, Visitor*);
  using V8BuildEmbedderGraphCallback = void (*)(v8::Isolate*,
                                                v8::EmbedderGraph*,
                                                void*);

  // Returns true if some thread (possibly the current thread) may be doing
  // incremental marking. If false is returned, the *current* thread is
  // definitely not doing incremental marking. See atomic_entry_flag.h for
  // details.
  //
  // For an exact check, use ThreadState::IsIncrementalMarking.
  ALWAYS_INLINE static bool IsAnyIncrementalMarking() {
    return incremental_marking_flag_.MightBeEntered();
  }

  static void AttachMainThread();

  // Associate ThreadState object with the current thread. After this
  // call thread can start using the garbage collected heap infrastructure.
  // It also has to periodically check for safepoints.
  static void AttachCurrentThread();

  // Disassociate attached ThreadState from the current thread. The thread
  // can no longer use the garbage collected heap after this call.
  static void DetachCurrentThread();

  static ThreadState* Current() { return **thread_specific_; }

  static ThreadState* MainThreadState() {
    return reinterpret_cast<ThreadState*>(main_thread_state_storage_);
  }

  static ThreadState* FromObject(const void*);

  bool IsMainThread() const { return this == MainThreadState(); }
  bool CheckThread() const { return thread_ == CurrentThread(); }

  ThreadHeap& Heap() const { return *heap_; }
  base::PlatformThreadId ThreadId() const { return thread_; }

  // Associates |ThreadState| with a given |v8::Isolate|, essentially tying
  // there garbage collectors together.
  void AttachToIsolate(v8::Isolate*,
                       V8TraceRootsCallback,
                       V8BuildEmbedderGraphCallback);

  // Removes the association from a potentially attached |v8::Isolate|.
  void DetachFromIsolate();

  // Returns an |UnifiedHeapController| if ThreadState is attached to a V8
  // isolate (see |AttachToIsolate|), nullptr otherwise.
  UnifiedHeapController* unified_heap_controller() const {
    DCHECK(isolate_);
    return unified_heap_controller_.get();
  }

  // When ThreadState is detaching from non-main thread its
  // heap is expected to be empty (because it is going away).
  // Perform registered cleanup tasks and garbage collection
  // to sweep away any objects that are left on this heap.
  // We assert that nothing must remain after this cleanup.
  // If assertion does not hold we crash as we are potentially
  // in the dangling pointer situation.
  void RunTerminationGC();

  void PerformIdleLazySweep(base::TimeTicks deadline);
  void PerformConcurrentSweep();

  void ScheduleIdleLazySweep();
  void ScheduleConcurrentAndLazySweep();
  void SchedulePreciseGC();
  void ScheduleIncrementalGC(BlinkGC::GCReason);
  void ScheduleV8FollowupGCIfNeeded(BlinkGC::V8GCType);
  void ScheduleForcedGCForTesting();
  void ScheduleGCIfNeeded();
  void PostIdleGCTask();
  void WillStartV8GC(BlinkGC::V8GCType);
  void SetGCState(GCState);
  GCState GetGCState() const { return gc_state_; }
  void SetGCPhase(GCPhase);
  bool IsMarkingInProgress() const { return gc_phase_ == GCPhase::kMarking; }
  bool IsSweepingInProgress() const { return gc_phase_ == GCPhase::kSweeping; }
  bool IsUnifiedGCMarkingInProgress() const {
    return IsMarkingInProgress() &&
           (current_gc_data_.reason == BlinkGC::GCReason::kUnifiedHeapGC ||
            current_gc_data_.reason ==
                BlinkGC::GCReason::kUnifiedHeapForMemoryReductionGC);
  }

  void EnableCompactionForNextGCForTesting();

  // Incremental GC.
  void ScheduleIncrementalMarkingStep();
  void ScheduleIncrementalMarkingFinalize();

  void IncrementalMarkingStart(BlinkGC::GCReason);
  void IncrementalMarkingStep(BlinkGC::StackState);
  void IncrementalMarkingFinalize();
  bool FinishIncrementalMarkingIfRunning(BlinkGC::StackState,
                                         BlinkGC::MarkingType,
                                         BlinkGC::SweepingType,
                                         BlinkGC::GCReason);

  void EnableIncrementalMarkingBarrier();
  void DisableIncrementalMarkingBarrier();

  void CompleteSweep();
  void FinishSnapshot();
  void PostSweep();

  // Returns whether it is currently allowed to allocate an object. Mainly used
  // for sanity checks asserts.
  bool IsAllocationAllowed() const {
    // Allocation is not allowed during atomic marking pause, but it is allowed
    // during atomic sweeping pause.
    return !InAtomicMarkingPause() && !no_allocation_count_;
  }

  // Returns whether it is currently forbidden to trigger a GC.
  bool IsGCForbidden() const { return gc_forbidden_count_; }

  // Returns whether it is currently forbidden to sweep objects.
  bool SweepForbidden() const { return sweep_forbidden_; }

  // Returns whether is is currently forbidden to resurrect objects.
  bool IsObjectResurrectionForbidden() const {
    return object_resurrection_forbidden_;
  }

  bool in_atomic_pause() const { return in_atomic_pause_; }

  bool InAtomicMarkingPause() const {
    return in_atomic_pause() && IsMarkingInProgress();
  }
  bool InAtomicSweepingPause() const {
    return in_atomic_pause() && IsSweepingInProgress();
  }

  bool IsIncrementalMarking() const { return incremental_marking_; }
  void SetIncrementalMarking(bool value) { incremental_marking_ = value; }

  void FlushHeapDoesNotContainCacheIfNeeded();

  void SafePoint(BlinkGC::StackState);

  void RecordStackEnd(intptr_t* end_of_stack) { end_of_stack_ = end_of_stack; }

  void PushRegistersAndVisitStack();

  // A region of non-weak PersistentNodes allocated on the given thread.
  PersistentRegion* GetPersistentRegion() const {
    return persistent_region_.get();
  }

  // A region of PersistentNodes for WeakPersistents allocated on the given
  // thread.
  PersistentRegion* GetWeakPersistentRegion() const {
    return weak_persistent_region_.get();
  }

  // Visit local thread stack and trace all pointers conservatively.
  void VisitStack(MarkingVisitor*);

  // Visit the asan fake stack frame corresponding to a slot on the
  // real machine stack if there is one.
  void VisitAsanFakeStackForPointer(MarkingVisitor*, Address);

  // Visit all non-weak persistents allocated on this thread.
  void VisitPersistents(Visitor*);

  // Visit all weak persistents allocated on this thread.
  void VisitWeakPersistents(Visitor*);

  // Visit all DOM wrappers allocatd on this thread.
  void VisitDOMWrappers(Visitor*);

  struct GCSnapshotInfo {
    STACK_ALLOCATED();

   public:
    GCSnapshotInfo(wtf_size_t num_object_types);

    // Map from gcInfoIndex (vector-index) to count/size.
    Vector<int> live_count;
    Vector<int> dead_count;
    Vector<size_t> live_size;
    Vector<size_t> dead_size;
  };

  void FreePersistentNode(PersistentRegion*, PersistentNode*);

  using PersistentClearCallback = void (*)(void*);

  void RegisterStaticPersistentNode(PersistentNode*, PersistentClearCallback);
  void ReleaseStaticPersistentNodes();

#if defined(LEAK_SANITIZER)
  void enterStaticReferenceRegistrationDisabledScope();
  void leaveStaticReferenceRegistrationDisabledScope();
#endif

  v8::Isolate* GetIsolate() const { return isolate_; }

  // Use CollectAllGarbageForTesting below for testing!
  void CollectGarbage(BlinkGC::StackState,
                      BlinkGC::MarkingType,
                      BlinkGC::SweepingType,
                      BlinkGC::GCReason);

  // Forced garbage collection for testing.
  void CollectAllGarbageForTesting(
      BlinkGC::StackState stack_state =
          BlinkGC::StackState::kNoHeapPointersOnStack);

  // Register the pre-finalizer for the |self| object. The class T must have
  // USING_PRE_FINALIZER().
  template <typename T>
  class PrefinalizerRegistration final {
    DISALLOW_NEW();

   public:
    PrefinalizerRegistration(T* self) {
      static_assert(sizeof(&T::InvokePreFinalizer) > 0,
                    "USING_PRE_FINALIZER(T) must be defined.");
      ThreadState* state =
          ThreadStateFor<ThreadingTrait<T>::kAffinity>::GetState();
#if DCHECK_IS_ON()
      DCHECK(state->CheckThread());
#endif
      DCHECK(!state->SweepForbidden());
      DCHECK(std::find(state->ordered_pre_finalizers_.begin(),
                       state->ordered_pre_finalizers_.end(),
                       PreFinalizer(self, T::InvokePreFinalizer)) ==
             state->ordered_pre_finalizers_.end());
      state->ordered_pre_finalizers_.emplace_back(self, T::InvokePreFinalizer);
    }
  };

  // Returns |true| if |object| resides on this thread's heap.
  // It is well-defined to call this method on any heap allocated
  // reference, provided its associated heap hasn't been detached
  // and shut down. Its behavior is undefined for any other pointer
  // value.
  bool IsOnThreadHeap(const void* object) const {
    return &FromObject(object)->Heap() == &Heap();
  }

  int GcAge() const { return gc_age_; }

  MarkingVisitor* CurrentVisitor() { return current_gc_data_.visitor.get(); }

  // Implementation for RAILModeObserver
  void OnRAILModeChanged(RAILMode new_mode) override;

  bool VerifyMarkingEnabled() const;

  // Returns true if the current GC is a memory reducing GC.
  bool IsMemoryReducingGC() {
    return current_gc_data_.reason ==
           BlinkGC::GCReason::kUnifiedHeapForMemoryReductionGC;
  }

 private:
  // Stores whether some ThreadState is currently in incremental marking.
  static AtomicEntryFlag incremental_marking_flag_;

  static WTF::ThreadSpecific<ThreadState*>* thread_specific_;

  // We can't create a static member of type ThreadState here because it will
  // introduce global constructor and destructor. We would like to manage
  // lifetime of the ThreadState attached to the main thread explicitly instead
  // and still use normal constructor and destructor for the ThreadState class.
  // For this we reserve static storage for the main ThreadState and lazily
  // construct ThreadState in it using placement new.
  static uint8_t main_thread_state_storage_[];

  ThreadState();
  ~ThreadState() override;

  void EnterNoAllocationScope() { no_allocation_count_++; }
  void LeaveNoAllocationScope() { no_allocation_count_--; }

  void EnterObjectResurrectionForbiddenScope() {
    DCHECK(!object_resurrection_forbidden_);
    object_resurrection_forbidden_ = true;
  }
  void LeaveObjectResurrectionForbiddenScope() {
    DCHECK(object_resurrection_forbidden_);
    object_resurrection_forbidden_ = false;
  }

  void EnterAtomicPause() {
    DCHECK(!in_atomic_pause_);
    in_atomic_pause_ = true;
  }
  void LeaveAtomicPause() {
    DCHECK(in_atomic_pause_);
    in_atomic_pause_ = false;
  }

  void EnterGCForbiddenScope() { gc_forbidden_count_++; }
  void LeaveGCForbiddenScope() {
    DCHECK_GT(gc_forbidden_count_, 0u);
    gc_forbidden_count_--;
  }

  // The following methods are used to compose RunAtomicPause. Public users
  // should use the CollectGarbage entrypoint. Internal users should use these
  // methods to compose a full garbage collection.
  void AtomicPauseMarkPrologue(BlinkGC::StackState,
                               BlinkGC::MarkingType,
                               BlinkGC::GCReason);
  void AtomicPauseMarkRoots(BlinkGC::StackState,
                            BlinkGC::MarkingType,
                            BlinkGC::GCReason);
  void AtomicPauseMarkTransitiveClosure();
  void AtomicPauseMarkEpilogue(BlinkGC::MarkingType);
  void AtomicPauseSweepAndCompact(BlinkGC::MarkingType marking_type,
                                  BlinkGC::SweepingType sweeping_type);
  void AtomicPauseEpilogue();

  // RunAtomicPause composes the final atomic pause that finishes a mark-compact
  // phase of a garbage collection. Depending on SweepingType it may also finish
  // sweeping or schedule lazy/concurrent sweeping.
  void RunAtomicPause(BlinkGC::StackState,
                      BlinkGC::MarkingType,
                      BlinkGC::SweepingType,
                      BlinkGC::GCReason);

  void UpdateStatisticsAfterSweeping();

  // The version is needed to be able to start incremental marking.
  void MarkPhasePrologue(BlinkGC::StackState,
                         BlinkGC::MarkingType,
                         BlinkGC::GCReason);
  void MarkPhaseEpilogue(BlinkGC::MarkingType);
  void MarkPhaseVisitRoots();
  void MarkPhaseVisitNotFullyConstructedObjects();
  bool MarkPhaseAdvanceMarking(base::TimeTicks deadline);
  void VerifyMarking(BlinkGC::MarkingType);

  // ShouldForceConservativeGC
  // implements the heuristics that are used to determine when to collect
  // garbage.
  // If shouldForceConservativeGC returns true, we force the garbage
  // collection immediately. Otherwise, if should*GC returns true, we
  // record that we should garbage collect the next time we return
  // to the event loop. If both return false, we don't need to
  // collect garbage at this point.
  bool ShouldForceConservativeGC();
  bool ShouldScheduleIncrementalMarking();
  // V8 minor or major GC is likely to drop a lot of references to objects
  // on Oilpan's heap. We give a chance to schedule a GC.
  bool ShouldScheduleV8FollowupGC();

  // Internal helpers to handle memory pressure conditions.

  // Returns true if memory use is in a near-OOM state
  // (aka being under "memory pressure".)
  bool ShouldForceMemoryPressureGC();

  size_t EstimatedLiveSize(size_t current_size, size_t size_at_last_gc);
  size_t TotalMemorySize();
  double HeapGrowingRate();
  double PartitionAllocGrowingRate();
  bool JudgeGCThreshold(size_t allocated_object_size_threshold,
                        size_t total_memory_size_threshold,
                        double heap_growing_rate_threshold);

  void RunScheduledGC(BlinkGC::StackState);

  void UpdateIncrementalMarkingStepDuration();

  void SynchronizeAndFinishConcurrentSweeping();

  void InvokePreFinalizers();

  void ReportMemoryToV8();

  // Adds the given observer to the ThreadState's observer list. This doesn't
  // take ownership of the argument. The argument must not be null. The argument
  // must not be registered before calling this.
  void AddObserver(BlinkGCObserver*);

  // Removes the given observer from the ThreadState's observer list. This
  // doesn't take ownership of the argument. The argument must not be null.
  // The argument must be registered before calling this.
  void RemoveObserver(BlinkGCObserver*);

  bool IsForcedGC(BlinkGC::GCReason reason) {
    return reason == BlinkGC::GCReason::kThreadTerminationGC ||
           reason == BlinkGC::GCReason::kForcedGCForTesting;
  }

  std::unique_ptr<ThreadHeap> heap_;
  base::PlatformThreadId thread_;
  std::unique_ptr<PersistentRegion> persistent_region_;
  std::unique_ptr<PersistentRegion> weak_persistent_region_;
  intptr_t* start_of_stack_;
  intptr_t* end_of_stack_;

  bool object_resurrection_forbidden_ = false;
  bool in_atomic_pause_ = false;
  bool sweep_forbidden_ = false;
  bool incremental_marking_ = false;
  bool should_optimize_for_load_time_ = false;
  size_t no_allocation_count_ = 0;
  size_t gc_forbidden_count_ = 0;

  base::TimeDelta next_incremental_marking_step_duration_;
  base::TimeDelta previous_incremental_marking_time_left_;

  GCState gc_state_;
  GCPhase gc_phase_;
  BlinkGC::GCReason reason_for_scheduled_gc_;

  using PreFinalizerCallback = bool (*)(void*);
  using PreFinalizer = std::pair<void*, PreFinalizerCallback>;

  // Pre-finalizers are called in the reverse order in which they are
  // registered by the constructors (including constructors of Mixin objects)
  // for an object, by processing the ordered_pre_finalizers_ back-to-front.
  Deque<PreFinalizer> ordered_pre_finalizers_;

  v8::Isolate* isolate_ = nullptr;
  V8TraceRootsCallback v8_trace_roots_ = nullptr;
  V8BuildEmbedderGraphCallback v8_build_embedder_graph_ = nullptr;
  std::unique_ptr<UnifiedHeapController> unified_heap_controller_;

#if defined(ADDRESS_SANITIZER)
  void* asan_fake_stack_;
#endif

  HashSet<BlinkGCObserver*> observers_;

  // PersistentNodes that are stored in static references;
  // references that either have to be cleared upon the thread
  // detaching from Oilpan and shutting down or references we
  // have to clear before initiating LSan's leak detection.
  HashMap<PersistentNode*, PersistentClearCallback> static_persistents_;

#if defined(LEAK_SANITIZER)
  // Count that controls scoped disabling of persistent registration.
  size_t disabled_static_persistent_registration_;
#endif

  size_t reported_memory_to_v8_;

  int gc_age_ = 0;

  struct GCData {
    BlinkGC::StackState stack_state;
    BlinkGC::MarkingType marking_type;
    BlinkGC::GCReason reason;
    std::unique_ptr<MarkingVisitor> visitor;
  };
  GCData current_gc_data_;

  CancelableTaskScheduler sweeper_scheduler_;

  friend class BlinkGCObserver;
  friend class incremental_marking_test::IncrementalMarkingScope;
  friend class incremental_marking_test::IncrementalMarkingTestDriver;
  friend class HeapAllocator;
  template <typename T>
  friend class PrefinalizerRegistration;
  friend class TestGCScope;
  friend class ThreadStateSchedulingTest;
  friend class UnifiedHeapController;

  DISALLOW_COPY_AND_ASSIGN(ThreadState);
};

template <>
class ThreadStateFor<kMainThreadOnly> {
  STATIC_ONLY(ThreadStateFor);

 public:
  static ThreadState* GetState() {
    // This specialization must only be used from the main thread.
    DCHECK(ThreadState::Current()->IsMainThread());
    return ThreadState::MainThreadState();
  }
};

template <>
class ThreadStateFor<kAnyThread> {
  STATIC_ONLY(ThreadStateFor);

 public:
  static ThreadState* GetState() { return ThreadState::Current(); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_THREAD_STATE_H_
