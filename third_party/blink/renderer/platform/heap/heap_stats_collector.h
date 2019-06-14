// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_STATS_COLLECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_STATS_COLLECTOR_H_

#include <stddef.h>

#include "third_party/blink/renderer/platform/heap/blink_gc.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

// Interface for observing changes to heap sizing.
class PLATFORM_EXPORT ThreadHeapStatsObserver {
 public:
  // Called upon allocating/releasing chunks of memory that contain objects.
  //
  // Must not trigger GC or allocate.
  virtual void IncreaseAllocatedSpace(size_t) = 0;
  virtual void DecreaseAllocatedSpace(size_t) = 0;

  // Called once per GC cycle with the accurate number of live |bytes|.
  //
  // Must not trigger GC or allocate.
  virtual void ResetAllocatedObjectSize(size_t bytes) = 0;

  // Called after observing at least
  // |ThreadHeapStatsCollector::kUpdateThreshold| changed bytes through
  // allocation or explicit free. Reports both, negative and positive
  // increments, to allow observer to decide whether absolute values or only the
  // deltas is interesting.
  //
  // May trigger GC but most not allocate.
  virtual void IncreaseAllocatedObjectSize(size_t) = 0;
  virtual void DecreaseAllocatedObjectSize(size_t) = 0;
};

// Manages counters and statistics across garbage collection cycles.
//
// Usage:
//   ThreadHeapStatsCollector stats_collector;
//   stats_collector.NotifyMarkingStarted(<BlinkGC::GCReason>);
//   // Use tracer.
//   stats_collector.NotifySweepingFinished();
//   // Previous event is available using stats_collector.previous().
class PLATFORM_EXPORT ThreadHeapStatsCollector {
  USING_FAST_MALLOC(ThreadHeapStatsCollector);

 public:
  // These ids will form human readable names when used in Scopes.
  enum Id {
    kAtomicPhase,
    kAtomicPhaseCompaction,
    kAtomicPhaseMarking,
    kCompleteSweep,
    kEagerSweep,
    kIncrementalMarkingStartMarking,
    kIncrementalMarkingStep,
    kIncrementalMarkingFinalize,
    kIncrementalMarkingFinalizeMarking,
    kInvokePreFinalizers,
    kLazySweepInIdle,
    kLazySweepOnAllocation,
    kMarkInvokeEphemeronCallbacks,
    kMarkProcessWorklist,
    kMarkNotFullyConstructedObjects,
    kMarkWeakProcessing,
    kUnifiedMarkingAtomicPrologue,
    kUnifiedMarkingStep,
    kVisitCrossThreadPersistents,
    kVisitDOMWrappers,
    kVisitPersistentRoots,
    kVisitPersistents,
    kVisitStackRoots,
    kLastScopeId = kVisitStackRoots,
  };

  static const char* ToString(Id id) {
    switch (id) {
      case Id::kAtomicPhase:
        return "BlinkGC.AtomicPhase";
      case Id::kAtomicPhaseCompaction:
        return "BlinkGC.AtomicPhaseCompaction";
      case Id::kAtomicPhaseMarking:
        return "BlinkGC.AtomicPhaseMarking";
      case Id::kCompleteSweep:
        return "BlinkGC.CompleteSweep";
      case Id::kEagerSweep:
        return "BlinkGC.EagerSweep";
      case Id::kIncrementalMarkingStartMarking:
        return "BlinkGC.IncrementalMarkingStartMarking";
      case Id::kIncrementalMarkingStep:
        return "BlinkGC.IncrementalMarkingStep";
      case Id::kIncrementalMarkingFinalize:
        return "BlinkGC.IncrementalMarkingFinalize";
      case Id::kIncrementalMarkingFinalizeMarking:
        return "BlinkGC.IncrementalMarkingFinalizeMarking";
      case Id::kInvokePreFinalizers:
        return "BlinkGC.InvokePreFinalizers";
      case Id::kLazySweepInIdle:
        return "BlinkGC.LazySweepInIdle";
      case Id::kLazySweepOnAllocation:
        return "BlinkGC.LazySweepOnAllocation";
      case Id::kMarkInvokeEphemeronCallbacks:
        return "BlinkGC.MarkInvokeEphemeronCallbacks";
      case Id::kMarkNotFullyConstructedObjects:
        return "BlinkGC.MarkNotFullyConstructedObjects";
      case Id::kMarkProcessWorklist:
        return "BlinkGC.MarkProcessWorklist";
      case Id::kMarkWeakProcessing:
        return "BlinkGC.MarkWeakProcessing";
      case kUnifiedMarkingAtomicPrologue:
        return "BlinkGC.UnifiedMarkingAtomicPrologue";
      case kUnifiedMarkingStep:
        return "BlinkGC.UnifiedMarkingStep";
      case Id::kVisitCrossThreadPersistents:
        return "BlinkGC.VisitCrossThreadPersistents";
      case Id::kVisitDOMWrappers:
        return "BlinkGC.VisitDOMWrappers";
      case Id::kVisitPersistentRoots:
        return "BlinkGC.VisitPersistentRoots";
      case Id::kVisitPersistents:
        return "BlinkGC.VisitPersistents";
      case Id::kVisitStackRoots:
        return "BlinkGC.VisitStackRoots";
    }
  }

  static constexpr int kNumScopeIds = kLastScopeId + 1;

  enum TraceCategory { kEnabled, kDisabled, kDevTools };

  // Trace a particular scope. Will emit a trace event and record the time in
  // the corresponding ThreadHeapStatsCollector.
  template <TraceCategory trace_category = kDisabled>
  class PLATFORM_EXPORT InternalScope {
    DISALLOW_NEW();
    DISALLOW_COPY_AND_ASSIGN(InternalScope);

   public:
    template <typename... Args>
    inline InternalScope(ThreadHeapStatsCollector* tracer, Id id, Args... args)
        : tracer_(tracer), start_time_(WTF::CurrentTimeTicks()), id_(id) {
      StartTrace(id, args...);
    }

    inline ~InternalScope() {
      StopTrace(id_);
      tracer_->IncreaseScopeTime(id_, WTF::CurrentTimeTicks() - start_time_);
    }

   private:
    constexpr static const char* TraceCategory() {
      switch (trace_category) {
        case kEnabled:
          return "blink_gc";
        case kDisabled:
          return TRACE_DISABLED_BY_DEFAULT("blink_gc");
        case kDevTools:
          return "blink_gc,devtools.timeline";
      }
    }

    void StartTrace(Id id) {
      TRACE_EVENT_BEGIN0(TraceCategory(), ToString(id));
    }

    template <typename Value1>
    void StartTrace(Id id, const char* k1, Value1 v1) {
      TRACE_EVENT_BEGIN1(TraceCategory(), ToString(id), k1, v1);
    }

    template <typename Value1, typename Value2>
    void StartTrace(Id id,
                    const char* k1,
                    Value1 v1,
                    const char* k2,
                    Value2 v2) {
      TRACE_EVENT_BEGIN2(TraceCategory(), ToString(id), k1, v1, k2, v2);
    }

    void StopTrace(Id id) { TRACE_EVENT_END0(TraceCategory(), ToString(id)); }

    ThreadHeapStatsCollector* const tracer_;
    const TimeTicks start_time_;
    const Id id_;
  };

  using Scope = InternalScope<kDisabled>;
  using EnabledScope = InternalScope<kEnabled>;
  using DevToolsScope = InternalScope<kDevTools>;

  class PLATFORM_EXPORT BlinkGCInV8Scope {
    DISALLOW_NEW();
    DISALLOW_COPY_AND_ASSIGN(BlinkGCInV8Scope);

   public:
    template <typename... Args>
    BlinkGCInV8Scope(ThreadHeapStatsCollector* tracer)
        : tracer_(tracer), start_time_(WTF::CurrentTimeTicks()) {}

    ~BlinkGCInV8Scope() {
      if (tracer_)
        tracer_->gc_nested_in_v8_ += WTF::CurrentTimeTicks() - start_time_;
    }

   private:
    ThreadHeapStatsCollector* const tracer_;
    const TimeTicks start_time_;
  };

  // POD to hold interesting data accumulated during a garbage collection cycle.
  // The event is always fully polulated when looking at previous events but
  // is only be partially populated when looking at the current event. See
  // members on when they are available.
  struct PLATFORM_EXPORT Event {
    double marking_time_in_ms() const;
    double marking_time_in_bytes_per_second() const;
    TimeDelta sweeping_time() const;

    // Marked bytes collected during sweeping.
    size_t marked_bytes = 0;
    size_t compaction_freed_bytes = 0;
    size_t compaction_freed_pages = 0;
    TimeDelta scope_data[kNumScopeIds];
    BlinkGC::GCReason reason;
    size_t object_size_in_bytes_before_sweeping = 0;
    size_t allocated_space_in_bytes_before_sweeping = 0;
    size_t partition_alloc_bytes_before_sweeping = 0;
    double live_object_rate = 0;
    size_t wrapper_count_before_sweeping = 0;
    TimeDelta gc_nested_in_v8_;
  };

  // Indicates a new garbage collection cycle.
  void NotifyMarkingStarted(BlinkGC::GCReason);

  // Indicates that marking of the current garbage collection cycle is
  // completed.
  void NotifyMarkingCompleted(size_t marked_bytes);

  // Indicates the end of a garbage collection cycle. This means that sweeping
  // is finished at this point.
  void NotifySweepingCompleted();

  void IncreaseScopeTime(Id id, TimeDelta time) {
    DCHECK(is_started_);
    current_.scope_data[id] += time;
  }

  void UpdateReason(BlinkGC::GCReason);
  void IncreaseCompactionFreedSize(size_t);
  void IncreaseCompactionFreedPages(size_t);
  void IncreaseAllocatedObjectSize(size_t);
  void DecreaseAllocatedObjectSize(size_t);
  void IncreaseAllocatedSpace(size_t);
  void DecreaseAllocatedSpace(size_t);
  void IncreaseWrapperCount(size_t);
  void DecreaseWrapperCount(size_t);
  void IncreaseCollectedWrapperCount(size_t);

  // Called by the GC when it hits a point where allocated memory may be
  // reported and garbage collection is possible. This is necessary, as
  // increments and decrements are reported as close to their actual
  // allocation/reclamation as possible.
  void AllocatedObjectSizeSafepoint();

  // Size of objects on the heap. Based on marked bytes in the previous cycle
  // and newly allocated bytes since the previous cycle.
  size_t object_size_in_bytes() const;

  // Estimated marking time in seconds. Based on marked bytes and mark speed in
  // the previous cycle assuming that the collection rate of the current cycle
  // is similar to the rate of the last GC.
  double estimated_marking_time_in_seconds() const;
  TimeDelta estimated_marking_time() const;

  size_t marked_bytes() const;
  int64_t allocated_bytes_since_prev_gc() const;

  size_t allocated_space_bytes() const;

  size_t wrapper_count() const;
  size_t collected_wrapper_count() const;

  bool is_started() const { return is_started_; }

  // Statistics for the previously running garbage collection.
  const Event& previous() const { return previous_; }

  TimeDelta marking_time_so_far() const {
    return TimeDelta::FromMilliseconds(current_.marking_time_in_ms());
  }

  void RegisterObserver(ThreadHeapStatsObserver* observer);
  void UnregisterObserver(ThreadHeapStatsObserver* observer);

  void IncreaseAllocatedObjectSizeForTesting(size_t);
  void DecreaseAllocatedObjectSizeForTesting(size_t);

 private:
  // Observers are implemented using virtual calls. Avoid notifications below
  // reasonably interesting sizes.
  static constexpr int64_t kUpdateThreshold = 1024;

  // Invokes |callback| for all registered observers.
  template <typename Callback>
  void ForAllObservers(Callback callback);

  void AllocatedObjectSizeSafepointImpl();

  // Statistics for the currently running garbage collection. Note that the
  // Event may not be fully populated yet as some phase may not have been run.
  const Event& current() const { return current_; }

  Event current_;
  Event previous_;

  // Allocated bytes since the last garbage collection. These bytes are reset
  // after marking as they are accounted in marked_bytes then.
  int64_t allocated_bytes_since_prev_gc_ = 0;
  int64_t pos_delta_allocated_bytes_since_prev_gc_ = 0;
  int64_t neg_delta_allocated_bytes_since_prev_gc_ = 0;

  // Allocated space in bytes for all arenas.
  size_t allocated_space_bytes_ = 0;

  size_t wrapper_count_ = 0;
  size_t collected_wrapper_count_ = 0;

  bool is_started_ = false;

  // TimeDelta for RawScope. These don't need to be nested within a garbage
  // collection cycle to make them easier to use.
  TimeDelta gc_nested_in_v8_;

  Vector<ThreadHeapStatsObserver*> observers_;

  FRIEND_TEST_ALL_PREFIXES(ThreadHeapStatsCollectorTest, InitialEmpty);
  FRIEND_TEST_ALL_PREFIXES(ThreadHeapStatsCollectorTest, IncreaseScopeTime);
  FRIEND_TEST_ALL_PREFIXES(ThreadHeapStatsCollectorTest, StopResetsCurrent);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_STATS_COLLECTOR_H_
