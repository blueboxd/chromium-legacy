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

#include "third_party/blink/renderer/platform/heap/thread_state.h"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <memory>

#include "base/atomicops.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/task_runner.h"
#include "base/trace_event/process_memory_dump.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/bindings/active_script_wrappable_base.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/address_cache.h"
#include "third_party/blink/renderer/platform/heap/blink_gc_memory_dump_provider.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_buildflags.h"
#include "third_party/blink/renderer/platform/heap/heap_compact.h"
#include "third_party/blink/renderer/platform/heap/heap_stats_collector.h"
#include "third_party/blink/renderer/platform/heap/marking_visitor.h"
#include "third_party/blink/renderer/platform/heap/page_pool.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/thread_state_scopes.h"
#include "third_party/blink/renderer/platform/heap/unified_heap_controller.h"
#include "third_party/blink/renderer/platform/heap/unified_heap_marking_visitor.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/web_memory_allocator_dump.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/web_process_memory_dump.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/stack_util.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"
#include "third_party/blink/renderer/platform/wtf/time.h"
#include "v8/include/v8-profiler.h"
#include "v8/include/v8.h"

#if defined(OS_WIN)
#include <stddef.h>
#include <windows.h>
#include <winnt.h>
#endif

#if defined(MEMORY_SANITIZER)
#include <sanitizer/msan_interface.h>
#endif

#if defined(OS_FREEBSD)
#include <pthread_np.h>
#endif

namespace blink {

WTF::ThreadSpecific<ThreadState*>* ThreadState::thread_specific_ = nullptr;
uint8_t ThreadState::main_thread_state_storage_[sizeof(ThreadState)];

namespace {

const size_t kDefaultAllocatedObjectSizeThreshold = 100 * 1024;

// Duration of one incremental marking step. Should be short enough that it
// doesn't cause jank even though it is scheduled as a normal task.
constexpr base::TimeDelta kDefaultIncrementalMarkingStepDuration =
    base::TimeDelta::FromMilliseconds(2);

constexpr size_t kMaxTerminationGCLoops = 20;

// Helper function to convert a byte count to a KB count, capping at
// INT_MAX if the number is larger than that.
constexpr base::Histogram::Sample CappedSizeInKB(size_t size_in_bytes) {
  return base::saturated_cast<base::Histogram::Sample>(size_in_bytes / 1024);
}

class WorkerPoolTaskRunner : public base::TaskRunner {
 public:
  bool PostDelayedTask(const base::Location& location,
                       base::OnceClosure task,
                       base::TimeDelta) override {
    worker_pool::PostTask(location, WTF::CrossThreadBindOnce(std::move(task)));
    return true;
  }

  bool RunsTasksInCurrentSequence() const override { return false; }
};

}  // namespace

ThreadState::ThreadState()
    : thread_(CurrentThread()),
      persistent_region_(std::make_unique<PersistentRegion>()),
      weak_persistent_region_(std::make_unique<PersistentRegion>()),
      start_of_stack_(reinterpret_cast<intptr_t*>(WTF::GetStackStart())),
      end_of_stack_(reinterpret_cast<intptr_t*>(WTF::GetStackStart())),
      gc_state_(kNoGCScheduled),
      gc_phase_(GCPhase::kNone),
      reason_for_scheduled_gc_(BlinkGC::GCReason::kForcedGCForTesting),
#if defined(ADDRESS_SANITIZER)
      asan_fake_stack_(__asan_get_current_fake_stack()),
#endif
#if defined(LEAK_SANITIZER)
      disabled_static_persistent_registration_(0),
#endif
      reported_memory_to_v8_(0),
      sweeper_scheduler_(base::MakeRefCounted<WorkerPoolTaskRunner>()) {
  DCHECK(CheckThread());
  DCHECK(!**thread_specific_);
  **thread_specific_ = this;
  heap_ = std::make_unique<ThreadHeap>(this);
}

// Implementation for RAILModeObserver
void ThreadState::OnRAILModeChanged(RAILMode new_mode) {
  should_optimize_for_load_time_ = new_mode == RAILMode::kLoad;
  // When switching RAIL mode to load we try to avoid incremental marking as
  // the write barrier cost is noticeable on throughput and garbage
  // accumulated during loading is likely to be alive during that phase. The
  // same argument holds for unified heap garbage collections with the
  // difference that these collections are triggered by V8 and should thus be
  // avoided on that end.
  if (should_optimize_for_load_time_ && IsIncrementalMarking() &&
      !IsUnifiedGCMarkingInProgress() &&
      GetGCState() == GCState::kIncrementalMarkingStepScheduled)
    ScheduleIncrementalMarkingFinalize();
}

ThreadState::~ThreadState() {
  DCHECK(CheckThread());
  if (IsMainThread())
    DCHECK_EQ(0u, Heap().stats_collector()->allocated_space_bytes());
  CHECK(GetGCState() == ThreadState::kNoGCScheduled);

  **thread_specific_ = nullptr;
}

void ThreadState::AttachMainThread() {
  thread_specific_ = new WTF::ThreadSpecific<ThreadState*>();
  new (main_thread_state_storage_) ThreadState();

  ThreadScheduler::Current()->AddRAILModeObserver(MainThreadState());
}

void ThreadState::AttachCurrentThread() {
  new ThreadState();
}

void ThreadState::DetachCurrentThread() {
  ThreadState* state = Current();
  DCHECK(!state->IsMainThread());
  state->RunTerminationGC();
  delete state;
}

void ThreadState::AttachToIsolate(
    v8::Isolate* isolate,
    V8TraceRootsCallback v8_trace_roots,
    V8BuildEmbedderGraphCallback v8_build_embedder_graph) {
  DCHECK(isolate);
  isolate_ = isolate;
  v8_trace_roots_ = v8_trace_roots;
  v8_build_embedder_graph_ = v8_build_embedder_graph;
  unified_heap_controller_.reset(new UnifiedHeapController(this));
  isolate_->SetEmbedderHeapTracer(unified_heap_controller_.get());
  if (v8::HeapProfiler* profiler = isolate->GetHeapProfiler()) {
    profiler->AddBuildEmbedderGraphCallback(v8_build_embedder_graph, nullptr);
  }
}

void ThreadState::DetachFromIsolate() {
  if (isolate_) {
    isolate_->SetEmbedderHeapTracer(nullptr);
    if (v8::HeapProfiler* profiler = isolate_->GetHeapProfiler()) {
      profiler->RemoveBuildEmbedderGraphCallback(v8_build_embedder_graph_,
                                                 nullptr);
    }
  }
  isolate_ = nullptr;
  v8_trace_roots_ = nullptr;
  v8_build_embedder_graph_ = nullptr;
  unified_heap_controller_.reset();
}

void ThreadState::RunTerminationGC() {
  DCHECK(!IsMainThread());
  DCHECK(CheckThread());

  FinishIncrementalMarkingIfRunning(BlinkGC::kNoHeapPointersOnStack,
                                    BlinkGC::kIncrementalMarking,
                                    BlinkGC::kConcurrentAndLazySweeping,
                                    BlinkGC::GCReason::kThreadTerminationGC);

  // Finish sweeping.
  CompleteSweep();

  ReleaseStaticPersistentNodes();

  // PrepareForThreadStateTermination removes strong references so no need to
  // call it on CrossThreadWeakPersistentRegion.
  ProcessHeap::GetCrossThreadPersistentRegion()
      .PrepareForThreadStateTermination(this);

  // Do thread local GC's as long as the count of thread local Persistents
  // changes and is above zero.
  int old_count = -1;
  int current_count = GetPersistentRegion()->NumberOfPersistents();
  DCHECK_GE(current_count, 0);
  while (current_count != old_count) {
    CollectGarbage(BlinkGC::kNoHeapPointersOnStack, BlinkGC::kAtomicMarking,
                   BlinkGC::kEagerSweeping,
                   BlinkGC::GCReason::kThreadTerminationGC);
    // Release the thread-local static persistents that were
    // instantiated while running the termination GC.
    ReleaseStaticPersistentNodes();
    old_count = current_count;
    current_count = GetPersistentRegion()->NumberOfPersistents();
  }

  // We should not have any persistents left when getting to this point,
  // if we have it is a bug, and we have a reference cycle or a missing
  // RegisterAsStaticReference. Clearing out all the Persistents will avoid
  // stale pointers and gets them reported as nullptr dereferences.
  if (current_count) {
    for (size_t i = 0; i < kMaxTerminationGCLoops &&
                       GetPersistentRegion()->NumberOfPersistents();
         i++) {
      GetPersistentRegion()->PrepareForThreadStateTermination();
      CollectGarbage(BlinkGC::kNoHeapPointersOnStack, BlinkGC::kAtomicMarking,
                     BlinkGC::kEagerSweeping,
                     BlinkGC::GCReason::kThreadTerminationGC);
    }
  }

  CHECK(!GetPersistentRegion()->NumberOfPersistents());

  // All of pre-finalizers should be consumed.
  DCHECK(ordered_pre_finalizers_.empty());
  CHECK_EQ(GetGCState(), kNoGCScheduled);

  Heap().RemoveAllPages();
}

NO_SANITIZE_ADDRESS
void ThreadState::VisitAsanFakeStackForPointer(MarkingVisitor* visitor,
                                               Address ptr) {
#if defined(ADDRESS_SANITIZER)
  Address* start = reinterpret_cast<Address*>(start_of_stack_);
  Address* end = reinterpret_cast<Address*>(end_of_stack_);
  Address* fake_frame_start = nullptr;
  Address* fake_frame_end = nullptr;
  Address* maybe_fake_frame = reinterpret_cast<Address*>(ptr);
  Address* real_frame_for_fake_frame = reinterpret_cast<Address*>(
      __asan_addr_is_in_fake_stack(asan_fake_stack_, maybe_fake_frame,
                                   reinterpret_cast<void**>(&fake_frame_start),
                                   reinterpret_cast<void**>(&fake_frame_end)));
  if (real_frame_for_fake_frame) {
    // This is a fake frame from the asan fake stack.
    if (real_frame_for_fake_frame > end && start > real_frame_for_fake_frame) {
      // The real stack address for the asan fake frame is
      // within the stack range that we need to scan so we need
      // to visit the values in the fake frame.
      for (Address* p = fake_frame_start; p < fake_frame_end; ++p)
        heap_->CheckAndMarkPointer(visitor, *p);
    }
  }
#endif
}

// Stack scanning may overrun the bounds of local objects and/or race with
// other threads that use this stack.
NO_SANITIZE_ADDRESS
NO_SANITIZE_HWADDRESS
NO_SANITIZE_THREAD
void ThreadState::VisitStack(MarkingVisitor* visitor) {
  DCHECK_EQ(current_gc_data_.stack_state, BlinkGC::kHeapPointersOnStack);

  Address* start = reinterpret_cast<Address*>(start_of_stack_);
  Address* end = reinterpret_cast<Address*>(end_of_stack_);

  // Ensure that current is aligned by address size otherwise the loop below
  // will read past start address.
  Address* current = reinterpret_cast<Address*>(
      reinterpret_cast<intptr_t>(end) & ~(sizeof(Address) - 1));

  for (; current < start; ++current) {
    Address ptr = *current;
#if defined(MEMORY_SANITIZER)
    // |ptr| may be uninitialized by design. Mark it as initialized to keep
    // MSan from complaining.
    // Note: it may be tempting to get rid of |ptr| and simply use |current|
    // here, but that would be incorrect. We intentionally use a local
    // variable because we don't want to unpoison the original stack.
    __msan_unpoison(&ptr, sizeof(ptr));
#endif
    heap_->CheckAndMarkPointer(visitor, ptr);
    VisitAsanFakeStackForPointer(visitor, ptr);
  }
}

void ThreadState::VisitDOMWrappers(Visitor* visitor) {
  if (v8_trace_roots_) {
    ThreadHeapStatsCollector::Scope stats_scope(
        Heap().stats_collector(), ThreadHeapStatsCollector::kVisitDOMWrappers);
    v8_trace_roots_(isolate_, visitor);
  }
}

void ThreadState::VisitPersistents(Visitor* visitor) {
  ThreadHeapStatsCollector::Scope stats_scope(
      Heap().stats_collector(),
      ThreadHeapStatsCollector::kVisitPersistentRoots);
  {
    ThreadHeapStatsCollector::Scope inner_stats_scope(
        Heap().stats_collector(),
        ThreadHeapStatsCollector::kVisitCrossThreadPersistents);
    // See ProcessHeap::CrossThreadPersistentMutex().
    MutexLocker persistent_lock(ProcessHeap::CrossThreadPersistentMutex());
    ProcessHeap::GetCrossThreadPersistentRegion().TracePersistentNodes(visitor);
  }
  {
    ThreadHeapStatsCollector::Scope inner_stats_scope(
        Heap().stats_collector(), ThreadHeapStatsCollector::kVisitPersistents);
    persistent_region_->TracePersistentNodes(visitor);
  }
}

void ThreadState::VisitWeakPersistents(Visitor* visitor) {
  ProcessHeap::GetCrossThreadWeakPersistentRegion().TracePersistentNodes(
      visitor);
  weak_persistent_region_->TracePersistentNodes(visitor);
}

ThreadState::GCSnapshotInfo::GCSnapshotInfo(wtf_size_t num_object_types)
    : live_count(Vector<int>(num_object_types)),
      dead_count(Vector<int>(num_object_types)),
      live_size(Vector<size_t>(num_object_types)),
      dead_size(Vector<size_t>(num_object_types)) {}

size_t ThreadState::TotalMemorySize() {
  return heap_->stats_collector()->object_size_in_bytes() +
         WTF::Partitions::TotalSizeOfCommittedPages();
}

size_t ThreadState::EstimatedLiveSize(size_t estimation_base_size,
                                      size_t size_at_last_gc) {
  const ThreadHeapStatsCollector& stats_collector = *heap_->stats_collector();
  const ThreadHeapStatsCollector::Event& prev = stats_collector.previous();

  if (prev.wrapper_count_before_sweeping == 0)
    return estimation_base_size;

  // (estimated size) = (estimation base size) - (heap size at the last GC) /
  //   (# of persistent handles at the last GC) *
  //     (# of persistent handles collected since the last GC)
  size_t size_retained_by_collected_persistents = static_cast<size_t>(
      1.0 * size_at_last_gc / prev.wrapper_count_before_sweeping *
      stats_collector.collected_wrapper_count());
  if (estimation_base_size < size_retained_by_collected_persistents)
    return 0;
  return estimation_base_size - size_retained_by_collected_persistents;
}

double ThreadState::HeapGrowingRate() {
  const size_t current_size = heap_->stats_collector()->object_size_in_bytes();
  // TODO(mlippautz): Clarify those two parameters below.
  const size_t estimated_size =
      EstimatedLiveSize(heap_->stats_collector()->previous().marked_bytes,
                        heap_->stats_collector()->previous().marked_bytes);

  // If the estimatedSize is 0, we set a high growing rate to trigger a GC.
  double growing_rate =
      estimated_size > 0 ? 1.0 * current_size / estimated_size : 100;
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "ThreadState::heapEstimatedSizeKB",
                 CappedSizeInKB(estimated_size));
  TRACE_COUNTER1(
      TRACE_DISABLED_BY_DEFAULT("blink_gc"), "ThreadState::heapGrowingRate",
      base::saturated_cast<base::Histogram::Sample>(100 * growing_rate));
  return growing_rate;
}

double ThreadState::PartitionAllocGrowingRate() {
  size_t current_size = WTF::Partitions::TotalSizeOfCommittedPages();
  size_t estimated_size = EstimatedLiveSize(
      current_size, heap_->stats_collector()
                        ->previous()
                        .partition_alloc_bytes_before_sweeping);

  // If the estimatedSize is 0, we set a high growing rate to trigger a GC.
  double growing_rate =
      estimated_size > 0 ? 1.0 * current_size / estimated_size : 100;
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "ThreadState::partitionAllocEstimatedSizeKB",
                 CappedSizeInKB(estimated_size));
  TRACE_COUNTER1(
      TRACE_DISABLED_BY_DEFAULT("blink_gc"),
      "ThreadState::partitionAllocGrowingRate",
      base::saturated_cast<base::Histogram::Sample>(100 * growing_rate));
  return growing_rate;
}

// TODO(haraken): We should improve the GC heuristics. The heuristics affect
// performance significantly.
bool ThreadState::JudgeGCThreshold(size_t allocated_object_size_threshold,
                                   size_t total_memory_size_threshold,
                                   double heap_growing_rate_threshold) {
  // If the allocated object size or the total memory size is small, don't
  // trigger a GC.
  if (heap_->stats_collector()->allocated_bytes_since_prev_gc() <
          static_cast<int64_t>(allocated_object_size_threshold) ||
      TotalMemorySize() < total_memory_size_threshold)
    return false;

  VLOG(2) << "[state:" << this << "] JudgeGCThreshold:"
          << " heapGrowingRate=" << std::setprecision(1) << HeapGrowingRate()
          << " partitionAllocGrowingRate=" << std::setprecision(1)
          << PartitionAllocGrowingRate();
  // If the growing rate of Oilpan's heap or PartitionAlloc is high enough,
  // trigger a GC.
  return HeapGrowingRate() >= heap_growing_rate_threshold ||
         PartitionAllocGrowingRate() >= heap_growing_rate_threshold;
}

bool ThreadState::ShouldScheduleV8FollowupGC() {
  if (base::FeatureList::IsEnabled(
          blink::features::kBlinkHeapUnifiedGCScheduling))
    return false;

  return JudgeGCThreshold(kDefaultAllocatedObjectSizeThreshold,
                          32 * 1024 * 1024, 1.5);
}

bool ThreadState::ShouldForceConservativeGC() {
  if (base::FeatureList::IsEnabled(
          blink::features::kBlinkHeapUnifiedGCScheduling))
    return false;

  // TODO(haraken): 400% is too large. Lower the heap growing factor.
  return JudgeGCThreshold(kDefaultAllocatedObjectSizeThreshold,
                          32 * 1024 * 1024, 5.0);
}

// If we're consuming too much memory, trigger a conservative GC
// aggressively. This is a safe guard to avoid OOM.
bool ThreadState::ShouldForceMemoryPressureGC() {
  if (base::FeatureList::IsEnabled(
          blink::features::kBlinkHeapUnifiedGCScheduling))
    return false;

  if (TotalMemorySize() < 300 * 1024 * 1024)
    return false;
  return JudgeGCThreshold(0, 0, 1.5);
}

void ThreadState::ScheduleV8FollowupGCIfNeeded(BlinkGC::V8GCType gc_type) {
  VLOG(2) << "[state:" << this << "] ScheduleV8FollowupGCIfNeeded: v8_gc_type="
          << ((gc_type == BlinkGC::kV8MajorGC) ? "MajorGC" : "MinorGC");
  DCHECK(CheckThread());

  if (IsGCForbidden())
    return;

  if (gc_type == BlinkGC::kV8MajorGC) {
    // In case of unified heap garbage collections a V8 major GC also collects
    // the Blink heap.
    return;
  }

  if (ShouldScheduleV8FollowupGC()) {
    // When we want to optimize for load time, we should prioritize throughput
    // over latency and not do incremental marking.
    if (base::FeatureList::IsEnabled(
            blink::features::kBlinkHeapIncrementalMarking) &&
        !should_optimize_for_load_time_) {
      VLOG(2) << "[state:" << this << "] "
              << "ScheduleV8FollowupGCIfNeeded: Scheduled incremental v8 "
                 "followup GC";
      ScheduleIncrementalGC(BlinkGC::GCReason::kIncrementalV8FollowupGC);
    } else {
      VLOG(2) << "[state:" << this << "] "
              << "ScheduleV8FollowupGCIfNeeded: Scheduled precise GC";
      SchedulePreciseGC();
    }
  }
}

void ThreadState::WillStartV8GC(BlinkGC::V8GCType gc_type) {
#if defined(ADDRESS_SANITIZER)
  // In case of running with ASAN we eagerly poison all unmarked objects on
  // Oilpan garbage collections. Those objects may contain v8 handles which may
  // be suspect to being removed. Removing requires clearing out the Oilpan
  // memory. For this reason we need to finish sweeping (which would remove the
  // handles) before V8 is allowed to continue with its garbage collection.
  CompleteSweep();
  return;
#endif  // ADDRESS_SANITIZER

  // Finish Oilpan's complete sweeping before running a V8 major GC.
  // This will let the GC collect more V8 objects.
  if (gc_type == BlinkGC::kV8MajorGC)
    CompleteSweep();
}

void ThreadState::ScheduleForcedGCForTesting() {
  DCHECK(CheckThread());
  CompleteSweep();
  SetGCState(kForcedGCForTestingScheduled);
}

void ThreadState::ScheduleGCIfNeeded() {
  VLOG(2) << "[state:" << this << "] ScheduleGCIfNeeded";
  DCHECK(CheckThread());

  UpdateIncrementalMarkingStepDuration();

  // Allocation is allowed during sweeping, but those allocations should not
  // trigger nested GCs.
  if (IsGCForbidden() || SweepForbidden())
    return;

  // This method should not call out to V8 during unified heap garbage
  // collections. Specifically, reporting memory to V8 may trigger a marking
  // step which is not allowed during construction of an object. The reason is
  // that a parent object's constructor is potentially being invoked which may
  // have already published the object. In that case the object may be colored
  // black in a v8 marking step which invalidates the assumption that write
  // barriers may be avoided when constructing an object as it is white.
  if (IsUnifiedGCMarkingInProgress())
    return;

  ReportMemoryToV8();

  if (ShouldForceMemoryPressureGC()) {
    CompleteSweep();
    if (ShouldForceMemoryPressureGC()) {
      VLOG(2) << "[state:" << this << "] "
              << "ScheduleGCIfNeeded: Scheduled memory pressure GC";
      CollectGarbage(BlinkGC::kHeapPointersOnStack, BlinkGC::kAtomicMarking,
                     BlinkGC::kConcurrentAndLazySweeping,
                     BlinkGC::GCReason::kMemoryPressureGC);
      return;
    }
  }

  if (ShouldForceConservativeGC()) {
    CompleteSweep();
    if (ShouldForceConservativeGC()) {
      VLOG(2) << "[state:" << this << "] "
              << "ScheduleGCIfNeeded: Scheduled conservative GC";
      CollectGarbage(BlinkGC::kHeapPointersOnStack, BlinkGC::kAtomicMarking,
                     BlinkGC::kConcurrentAndLazySweeping,
                     BlinkGC::GCReason::kConservativeGC);
      return;
    }
  }

  if (GetGCState() == kNoGCScheduled &&
      base::FeatureList::IsEnabled(
          blink::features::kBlinkHeapIncrementalMarkingStress)) {
    VLOG(2) << "[state:" << this << "] "
            << "ScheduleGCIfNeeded: Scheduled incremental marking for testing";
    IncrementalMarkingStart(BlinkGC::GCReason::kForcedGCForTesting);
    return;
  }
}

ThreadState* ThreadState::FromObject(const void* object) {
  DCHECK(object);
  BasePage* page = PageFromObject(object);
  DCHECK(page);
  DCHECK(page->Arena());
  return page->Arena()->GetThreadState();
}

void ThreadState::PerformIdleLazySweep(base::TimeTicks deadline) {
  DCHECK(CheckThread());

  // If we are not in a sweeping phase, there is nothing to do here.
  if (!IsSweepingInProgress())
    return;

  // This check is here to prevent performIdleLazySweep() from being called
  // recursively. I'm not sure if it can happen but it would be safer to have
  // the check just in case.
  if (SweepForbidden())
    return;

  RUNTIME_CALL_TIMER_SCOPE_IF_ISOLATE_EXISTS(
      GetIsolate(), RuntimeCallStats::CounterId::kPerformIdleLazySweep);

  bool sweep_completed = false;
  {
    AtomicPauseScope atomic_pause_scope(this);
    SweepForbiddenScope scope(this);
    ThreadHeapStatsCollector::EnabledScope stats_scope(
        Heap().stats_collector(), ThreadHeapStatsCollector::kLazySweepInIdle,
        "idleDeltaInSeconds", (deadline - base::TimeTicks::Now()).InSecondsF());
    sweep_completed = Heap().AdvanceLazySweep(deadline);
    // We couldn't finish the sweeping within the deadline.
    // We request another idle task for the remaining sweeping.
    if (sweep_completed) {
      SynchronizeAndFinishConcurrentSweeping();
    } else {
      ScheduleIdleLazySweep();
    }
  }

  if (sweep_completed) {
    PostSweep();
  }
}

void ThreadState::PerformConcurrentSweep() {
  VLOG(2) << "[state:" << this << "] [threadid:" << CurrentThread() << "] "
          << "ConcurrentSweep";
  // Concurrent sweeper doesn't call finalizers - this guarantees that sweeping
  // is not called recursively.
  ThreadHeapStatsCollector::EnabledConcurrentScope stats_scope(
      Heap().stats_collector(), ThreadHeapStatsCollector::kConcurrentSweep);
  Heap().ConcurrentSweep();
}

void ThreadState::ScheduleIncrementalMarkingStep() {
  CHECK(!IsSweepingInProgress());
  SetGCState(kIncrementalMarkingStepScheduled);
}

void ThreadState::ScheduleIncrementalMarkingFinalize() {
  CHECK(!IsSweepingInProgress());
  SetGCState(kIncrementalMarkingFinalizeScheduled);
}

void ThreadState::ScheduleIdleLazySweep() {
  ThreadScheduler::Current()->PostIdleTask(
      FROM_HERE,
      WTF::Bind(&ThreadState::PerformIdleLazySweep, WTF::Unretained(this)));
}

void ThreadState::ScheduleConcurrentAndLazySweep() {
  ScheduleIdleLazySweep();

  if (!base::FeatureList::IsEnabled(
          blink::features::kBlinkHeapConcurrentSweeping)) {
    return;
  }

  static constexpr size_t kNumberOfSweepingTasks = 1u;

  for (size_t i = 0; i < kNumberOfSweepingTasks; ++i) {
    sweeper_scheduler_.ScheduleTask(
        WTF::CrossThreadBindOnce(&ThreadState::PerformConcurrentSweep,
                                 WTF::CrossThreadUnretained(this)));
  }
}

void ThreadState::SchedulePreciseGC() {
  DCHECK(CheckThread());
  CompleteSweep();
  SetGCState(kPreciseGCScheduled);
}

void ThreadState::ScheduleIncrementalGC(BlinkGC::GCReason reason) {
  DCHECK(CheckThread());
  // Schedule an incremental GC only when no GC is scheduled. Otherwise, already
  // scheduled GCs should be prioritized.
  if (GetGCState() == kNoGCScheduled) {
    CompleteSweep();
    reason_for_scheduled_gc_ = reason;
    SetGCState(kIncrementalGCScheduled);
  }
}

namespace {

#define UNEXPECTED_GCSTATE(s)                                   \
  case ThreadState::s:                                          \
    LOG(FATAL) << "Unexpected transition while in GCState " #s; \
    return

void UnexpectedGCState(ThreadState::GCState gc_state) {
  switch (gc_state) {
    UNEXPECTED_GCSTATE(kNoGCScheduled);
    UNEXPECTED_GCSTATE(kPreciseGCScheduled);
    UNEXPECTED_GCSTATE(kForcedGCForTestingScheduled);
    UNEXPECTED_GCSTATE(kIncrementalMarkingStepPaused);
    UNEXPECTED_GCSTATE(kIncrementalMarkingStepScheduled);
    UNEXPECTED_GCSTATE(kIncrementalMarkingFinalizeScheduled);
    UNEXPECTED_GCSTATE(kIncrementalGCScheduled);
  }
}

#undef UNEXPECTED_GCSTATE

}  // namespace

#define VERIFY_STATE_TRANSITION(condition) \
  if (UNLIKELY(!(condition)))              \
  UnexpectedGCState(gc_state_)

void ThreadState::SetGCState(GCState gc_state) {
  switch (gc_state) {
    case kNoGCScheduled:
      DCHECK(CheckThread());
      VERIFY_STATE_TRANSITION(
          gc_state_ == kNoGCScheduled || gc_state_ == kPreciseGCScheduled ||
          gc_state_ == kForcedGCForTestingScheduled ||
          gc_state_ == kIncrementalMarkingStepPaused ||
          gc_state_ == kIncrementalMarkingStepScheduled ||
          gc_state_ == kIncrementalMarkingFinalizeScheduled ||
          gc_state_ == kIncrementalGCScheduled);
      break;
    case kIncrementalMarkingStepScheduled:
      DCHECK(CheckThread());
      VERIFY_STATE_TRANSITION(gc_state_ == kNoGCScheduled ||
                              gc_state_ == kIncrementalMarkingStepScheduled ||
                              gc_state_ == kIncrementalGCScheduled);
      break;
    case kIncrementalMarkingFinalizeScheduled:
      DCHECK(CheckThread());
      VERIFY_STATE_TRANSITION(gc_state_ == kIncrementalMarkingStepScheduled);
      break;
    case kForcedGCForTestingScheduled:
    case kPreciseGCScheduled:
      DCHECK(CheckThread());
      DCHECK(!IsSweepingInProgress());
      VERIFY_STATE_TRANSITION(gc_state_ == kNoGCScheduled ||
                              gc_state_ == kIncrementalMarkingStepPaused ||
                              gc_state_ == kIncrementalMarkingStepScheduled ||
                              gc_state_ ==
                                  kIncrementalMarkingFinalizeScheduled ||
                              gc_state_ == kPreciseGCScheduled ||
                              gc_state_ == kForcedGCForTestingScheduled ||
                              gc_state_ == kIncrementalGCScheduled);
      break;
    case kIncrementalGCScheduled:
      DCHECK(CheckThread());
      DCHECK(!IsMarkingInProgress());
      DCHECK(!IsSweepingInProgress());
      VERIFY_STATE_TRANSITION(gc_state_ == kNoGCScheduled);
      break;
    case kIncrementalMarkingStepPaused:
      DCHECK(CheckThread());
      DCHECK(IsMarkingInProgress());
      DCHECK(!IsSweepingInProgress());
      VERIFY_STATE_TRANSITION(gc_state_ == kIncrementalMarkingStepScheduled);
      break;
    default:
      NOTREACHED();
  }
  gc_state_ = gc_state;
}

#undef VERIFY_STATE_TRANSITION

void ThreadState::SetGCPhase(GCPhase gc_phase) {
  switch (gc_phase) {
    case GCPhase::kNone:
      DCHECK_EQ(gc_phase_, GCPhase::kSweeping);
      break;
    case GCPhase::kMarking:
      DCHECK_EQ(gc_phase_, GCPhase::kNone);
      break;
    case GCPhase::kSweeping:
      DCHECK_EQ(gc_phase_, GCPhase::kMarking);
      break;
  }
  gc_phase_ = gc_phase;
}

void ThreadState::RunScheduledGC(BlinkGC::StackState stack_state) {
  DCHECK(CheckThread());
  if (stack_state != BlinkGC::kNoHeapPointersOnStack)
    return;

  // If a safe point is entered while initiating a GC, we clearly do
  // not want to do another as part of that -- the safe point is only
  // entered after checking if a scheduled GC ought to run first.
  // Prevent that from happening by marking GCs as forbidden while
  // one is initiated and later running.
  if (IsGCForbidden())
    return;

  switch (GetGCState()) {
    case kForcedGCForTestingScheduled:
      CollectAllGarbageForTesting();
      break;
    case kPreciseGCScheduled:
      CollectGarbage(BlinkGC::kNoHeapPointersOnStack, BlinkGC::kAtomicMarking,
                     BlinkGC::kConcurrentAndLazySweeping,
                     BlinkGC::GCReason::kPreciseGC);
      break;
    case kIncrementalMarkingStepScheduled:
      IncrementalMarkingStep(stack_state);
      break;
    case kIncrementalMarkingFinalizeScheduled:
      IncrementalMarkingFinalize();
      break;
    case kIncrementalGCScheduled:
      IncrementalMarkingStart(reason_for_scheduled_gc_);
      break;
    default:
      break;
  }
}

void ThreadState::FinishSnapshot() {
  // Force setting NoGCScheduled to circumvent checkThread()
  // in setGCState().
  gc_state_ = kNoGCScheduled;
  SetGCPhase(GCPhase::kSweeping);
  SetGCPhase(GCPhase::kNone);
}

void ThreadState::AtomicPauseMarkPrologue(BlinkGC::StackState stack_state,
                                          BlinkGC::MarkingType marking_type,
                                          BlinkGC::GCReason reason) {
  ThreadHeapStatsCollector::EnabledScope mark_prologue_scope(
      Heap().stats_collector(),
      ThreadHeapStatsCollector::kAtomicPauseMarkPrologue, "epoch", gc_age_,
      "forced", IsForcedGC(reason));
  EnterAtomicPause();
  EnterGCForbiddenScope();
  // Compaction needs to be canceled when incremental marking ends with a
  // conservative GC.
  if (stack_state == BlinkGC::kHeapPointersOnStack)
    Heap().Compaction()->Cancel();

  if (IsMarkingInProgress()) {
    // Incremental marking is already in progress. Only update the state
    // that is necessary to update.
    current_gc_data_.reason = reason;
    current_gc_data_.stack_state = stack_state;
    Heap().stats_collector()->UpdateReason(reason);
    SetGCState(kNoGCScheduled);
    DisableIncrementalMarkingBarrier();
  } else {
    MarkPhasePrologue(stack_state, marking_type, reason);
  }

  if (marking_type == BlinkGC::kTakeSnapshot)
    BlinkGCMemoryDumpProvider::Instance()->ClearProcessDumpForCurrentGC();

  if (stack_state == BlinkGC::kNoHeapPointersOnStack) {
    Heap().FlushNotFullyConstructedObjects();
  }

  DCHECK(InAtomicMarkingPause());
  Heap().MakeConsistentForGC();
  Heap().ClearArenaAges();
}

void ThreadState::AtomicPauseEpilogue() {
  if (!IsSweepingInProgress()) {
    // Sweeping was finished during the atomic pause. Update statistics needs to
    // run outside of the top-most stats scope.
    UpdateStatisticsAfterSweeping();
  }
}

void ThreadState::CompleteSweep() {
  DCHECK(CheckThread());
  // If we are not in a sweeping phase, there is nothing to do here.
  if (!IsSweepingInProgress())
    return;

  // completeSweep() can be called recursively if finalizers can allocate
  // memory and the allocation triggers completeSweep(). This check prevents
  // the sweeping from being executed recursively.
  if (SweepForbidden())
    return;

  {
    // CompleteSweep may be called during regular mutator execution, from a
    // task, or from the atomic pause in which the atomic scope has already been
    // opened.
    const bool was_in_atomic_pause = in_atomic_pause();
    if (!was_in_atomic_pause)
      EnterAtomicPause();
    ScriptForbiddenScope script_forbidden;
    SweepForbiddenScope scope(this);
    ThreadHeapStatsCollector::EnabledScope stats_scope(
        Heap().stats_collector(), ThreadHeapStatsCollector::kCompleteSweep,
        "forced", IsForcedGC(current_gc_data_.reason));
    Heap().CompleteSweep();
    SynchronizeAndFinishConcurrentSweeping();

    if (!was_in_atomic_pause)
      LeaveAtomicPause();
  }
  PostSweep();
}

void ThreadState::SynchronizeAndFinishConcurrentSweeping() {
  DCHECK(CheckThread());
  DCHECK(IsSweepingInProgress());
  DCHECK(SweepForbidden());

  // Wait for concurrent sweepers.
  sweeper_scheduler_.CancelAndWait();

  // Concurrent sweepers may perform some work at the last stage (e.g.
  // sweeping the last page and preparing finalizers).
  Heap().InvokeFinalizersOnSweptPages();
}

BlinkGCObserver::BlinkGCObserver(ThreadState* thread_state)
    : thread_state_(thread_state) {
  thread_state_->AddObserver(this);
}

BlinkGCObserver::~BlinkGCObserver() {
  thread_state_->RemoveObserver(this);
}

namespace {

// Update trace counters with statistics from the current and previous garbage
// collection cycle. We allow emitting current values here since these values
// can be useful for inspecting traces.
void UpdateTraceCounters(const ThreadHeapStatsCollector& stats_collector) {
  bool gc_tracing_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                                     &gc_tracing_enabled);
  if (!gc_tracing_enabled)
    return;

  // Previous garbage collection cycle values.
  const ThreadHeapStatsCollector::Event& event = stats_collector.previous();
  const int collection_rate_percent =
      static_cast<int>(100 * (1.0 - event.live_object_rate));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "BlinkGC.CollectionRate", collection_rate_percent);
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "BlinkGC.MarkedObjectSizeAtLastCompleteSweepKB",
                 CappedSizeInKB(event.marked_bytes));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "BlinkGC.ObjectSizeAtLastGCKB",
                 CappedSizeInKB(event.object_size_in_bytes_before_sweeping));
  TRACE_COUNTER1(
      TRACE_DISABLED_BY_DEFAULT("blink_gc"), "BlinkGC.AllocatedSpaceAtLastGCKB",
      CappedSizeInKB(event.allocated_space_in_bytes_before_sweeping));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "BlinkGC.PartitionAllocSizeAtLastGCKB",
                 CappedSizeInKB(event.partition_alloc_bytes_before_sweeping));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "BlinkGC.WrapperCountAtLastGC",
                 event.wrapper_count_before_sweeping);

  // Current values.
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "BlinkGC.AllocatedSpaceKB",
                 CappedSizeInKB(stats_collector.allocated_space_bytes()));
  size_t allocated_bytes_since_prev_gc =
      stats_collector.allocated_bytes_since_prev_gc() > 0
          ? static_cast<size_t>(stats_collector.allocated_bytes_since_prev_gc())
          : 0;
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "BlinkGC.AllocatedObjectSizeSincePreviousGCKB",
                 CappedSizeInKB(allocated_bytes_since_prev_gc));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "PartitionAlloc.TotalSizeOfCommittedPagesKB",
                 CappedSizeInKB(WTF::Partitions::TotalSizeOfCommittedPages()));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"), "BlinkGC.WrapperCount",
                 stats_collector.wrapper_count());
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "BlinkGC.CollectedWrapperCount",
                 stats_collector.collected_wrapper_count());
}

// Update histograms with statistics from the previous garbage collection cycle.
// Anything that is part of a histogram should have a well-defined lifetime wrt.
// to a garbage collection cycle.
void UpdateHistograms(const ThreadHeapStatsCollector::Event& event) {
  UMA_HISTOGRAM_ENUMERATION("BlinkGC.GCReason", event.reason);

  UMA_HISTOGRAM_TIMES("BlinkGC.TimeForAtomicPhase", event.atomic_pause_time());
  UMA_HISTOGRAM_TIMES("BlinkGC.TimeForAtomicPhaseMarking",
                      event.atomic_marking_time());
  UMA_HISTOGRAM_TIMES("BlinkGC.TimeForGCCycle", event.gc_cycle_time());
  UMA_HISTOGRAM_TIMES("BlinkGC.TimeForIncrementalMarking",
                      event.incremental_marking_time());
  UMA_HISTOGRAM_TIMES("BlinkGC.TimeForMarking", event.marking_time());
  UMA_HISTOGRAM_TIMES("BlinkGC.TimeForNestedInV8", event.gc_nested_in_v8_);
  UMA_HISTOGRAM_TIMES("BlinkGC.TimeForSweepingForeground",
                      event.foreground_sweeping_time());
  UMA_HISTOGRAM_TIMES("BlinkGC.TimeForSweepingBackground",
                      event.background_sweeping_time());
  UMA_HISTOGRAM_TIMES("BlinkGC.TimeForSweepingSum", event.sweeping_time());
  UMA_HISTOGRAM_TIMES(
      "BlinkGC.TimeForCompleteSweep",
      event.scope_data[ThreadHeapStatsCollector::kCompleteSweep]);
  UMA_HISTOGRAM_TIMES(
      "BlinkGC.TimeForInvokingPreFinalizers",
      event.scope_data[ThreadHeapStatsCollector::kInvokePreFinalizers]);
  UMA_HISTOGRAM_TIMES(
      "BlinkGC.TimeForHeapCompaction",
      event.scope_data[ThreadHeapStatsCollector::kAtomicPauseCompaction]);
  UMA_HISTOGRAM_TIMES(
      "BlinkGC.TimeForGlobalWeakProcessing",
      event.scope_data[ThreadHeapStatsCollector::kMarkWeakProcessing]);

  base::TimeDelta marking_duration = event.marking_time();
  constexpr size_t kMinObjectSizeForReportingThroughput = 1024 * 1024;
  if (base::TimeTicks::IsHighResolution() &&
      (event.object_size_in_bytes_before_sweeping >
       kMinObjectSizeForReportingThroughput) &&
      !marking_duration.is_zero()) {
    DCHECK_GT(marking_duration.InMillisecondsF(), 0.0);
    const int main_thread_marking_throughput_mb_per_s = static_cast<int>(
        static_cast<double>(event.object_size_in_bytes_before_sweeping) /
        marking_duration.InMillisecondsF() * 1000 / 1024 / 1024);
    UMA_HISTOGRAM_COUNTS_100000("BlinkGC.MainThreadMarkingThroughput",
                                main_thread_marking_throughput_mb_per_s);
  }

  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, object_size_freed_by_heap_compaction,
      ("BlinkGC.ObjectSizeFreedByHeapCompaction", 1, 4 * 1024 * 1024, 50));
  object_size_freed_by_heap_compaction.Count(
      CappedSizeInKB(event.compaction_freed_bytes));

  DEFINE_STATIC_LOCAL(CustomCountHistogram, object_size_before_gc_histogram,
                      ("BlinkGC.ObjectSizeBeforeGC", 1, 4 * 1024 * 1024, 50));
  object_size_before_gc_histogram.Count(
      CappedSizeInKB(event.object_size_in_bytes_before_sweeping));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, object_size_after_gc_histogram,
                      ("BlinkGC.ObjectSizeAfterGC", 1, 4 * 1024 * 1024, 50));
  object_size_after_gc_histogram.Count(CappedSizeInKB(event.marked_bytes));

  const int collection_rate_percent =
      static_cast<int>(100 * (1.0 - event.live_object_rate));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, collection_rate_histogram,
                      ("BlinkGC.CollectionRate", 1, 100, 20));
  collection_rate_histogram.Count(collection_rate_percent);

  // Per GCReason metrics.
  switch (event.reason) {
#define COUNT_BY_GC_REASON(reason)                                        \
  case BlinkGC::GCReason::k##reason: {                                    \
    UMA_HISTOGRAM_TIMES("BlinkGC.AtomicPhaseMarking_" #reason,            \
                        event.atomic_marking_time());                     \
    DEFINE_STATIC_LOCAL(CustomCountHistogram,                             \
                        collection_rate_reason_histogram,                 \
                        ("BlinkGC.CollectionRate_" #reason, 1, 100, 20)); \
    collection_rate_reason_histogram.Count(collection_rate_percent);      \
    break;                                                                \
  }

    // COUNT_BY_GC_REASON(IdleGC)
    COUNT_BY_GC_REASON(PreciseGC)
    COUNT_BY_GC_REASON(ConservativeGC)
    COUNT_BY_GC_REASON(ForcedGCForTesting)
    COUNT_BY_GC_REASON(MemoryPressureGC)
    COUNT_BY_GC_REASON(ThreadTerminationGC)
    COUNT_BY_GC_REASON(IncrementalV8FollowupGC)
    COUNT_BY_GC_REASON(UnifiedHeapGC)
    COUNT_BY_GC_REASON(UnifiedHeapForMemoryReductionGC)

#undef COUNT_BY_GC_REASON
  }

  static constexpr size_t kSupportedMaxSizeInMB = 4 * 1024;
  static size_t max_committed_size_in_mb = 0;

  // +1 for rounding up the size to the next MB.
  size_t size_in_mb =
      event.allocated_space_in_bytes_before_sweeping / 1024 / 1024 + 1;
  if (size_in_mb >= kSupportedMaxSizeInMB)
    size_in_mb = kSupportedMaxSizeInMB - 1;
  if (size_in_mb > max_committed_size_in_mb) {
    // Only update the counter for the maximum value.
    DEFINE_STATIC_LOCAL(EnumerationHistogram, commited_size_histogram,
                        ("BlinkGC.CommittedSize", kSupportedMaxSizeInMB));
    commited_size_histogram.Count(
        base::saturated_cast<base::Histogram::Sample>(size_in_mb));
    max_committed_size_in_mb = size_in_mb;
  }
}

}  // namespace

void ThreadState::UpdateStatisticsAfterSweeping() {
  DCHECK(!IsSweepingInProgress());
  DCHECK(Heap().stats_collector()->is_started());
  Heap().stats_collector()->NotifySweepingCompleted();
  if (IsMainThread())
    UpdateHistograms(Heap().stats_collector()->previous());
  // Emit trace counters for all threads.
  UpdateTraceCounters(*Heap().stats_collector());
}

void ThreadState::PostSweep() {
  DCHECK(CheckThread());

  SetGCPhase(GCPhase::kNone);

  gc_age_++;

  for (auto* const observer : observers_)
    observer->OnCompleteSweepDone();

  if (!in_atomic_pause()) {
    // Immediately update the statistics if running outside of the atomic pause.
    UpdateStatisticsAfterSweeping();
  }
}

void ThreadState::SafePoint(BlinkGC::StackState stack_state) {
  DCHECK(CheckThread());

  RunScheduledGC(stack_state);
}

using PushAllRegistersCallback = void (*)(ThreadState*, intptr_t*);
extern "C" void PushAllRegisters(ThreadState*, PushAllRegistersCallback);

static void DidPushRegisters(ThreadState* state, intptr_t* stack_end) {
  state->RecordStackEnd(stack_end);
}

void ThreadState::PushRegistersAndVisitStack() {
  DCHECK(CheckThread());
  DCHECK(IsGCForbidden());
  DCHECK_EQ(current_gc_data_.stack_state, BlinkGC::kHeapPointersOnStack);
  PushAllRegisters(this, DidPushRegisters);
  VisitStack(static_cast<MarkingVisitor*>(CurrentVisitor()));
}

void ThreadState::AddObserver(BlinkGCObserver* observer) {
  DCHECK(observer);
  DCHECK(!observers_.Contains(observer));
  observers_.insert(observer);
}

void ThreadState::RemoveObserver(BlinkGCObserver* observer) {
  DCHECK(observer);
  DCHECK(observers_.Contains(observer));
  observers_.erase(observer);
}

void ThreadState::ReportMemoryToV8() {
  if (!isolate_ || base::FeatureList::IsEnabled(
                       blink::features::kBlinkHeapUnifiedGCScheduling))
    return;

  const size_t current_heap_size =
      heap_->stats_collector()->object_size_in_bytes();
  int64_t diff = static_cast<int64_t>(current_heap_size) -
                 static_cast<int64_t>(reported_memory_to_v8_);
  isolate_->AdjustAmountOfExternalAllocatedMemory(diff);
  reported_memory_to_v8_ = current_heap_size;
}

void ThreadState::RegisterStaticPersistentNode(
    PersistentNode* node,
    PersistentClearCallback callback) {
#if defined(LEAK_SANITIZER)
  if (disabled_static_persistent_registration_)
    return;
#endif

  DCHECK(!static_persistents_.Contains(node));
  static_persistents_.insert(node, callback);
}

void ThreadState::ReleaseStaticPersistentNodes() {
  HashMap<PersistentNode*, ThreadState::PersistentClearCallback>
      static_persistents;
  static_persistents.swap(static_persistents_);

  PersistentRegion* persistent_region = GetPersistentRegion();
  for (const auto& it : static_persistents)
    persistent_region->ReleasePersistentNode(it.key, it.value);
}

void ThreadState::FreePersistentNode(PersistentRegion* persistent_region,
                                     PersistentNode* persistent_node) {
  persistent_region->FreePersistentNode(persistent_node);
  // Do not allow static persistents to be freed before
  // they're all released in releaseStaticPersistentNodes().
  //
  // There's no fundamental reason why this couldn't be supported,
  // but no known use for it.
  if (persistent_region == GetPersistentRegion())
    DCHECK(!static_persistents_.Contains(persistent_node));
}

#if defined(LEAK_SANITIZER)
void ThreadState::enterStaticReferenceRegistrationDisabledScope() {
  disabled_static_persistent_registration_++;
}

void ThreadState::leaveStaticReferenceRegistrationDisabledScope() {
  DCHECK(disabled_static_persistent_registration_);
  disabled_static_persistent_registration_--;
}
#endif

void ThreadState::InvokePreFinalizers() {
  DCHECK(CheckThread());
  DCHECK(!SweepForbidden());

  ThreadHeapStatsCollector::Scope stats_scope(
      Heap().stats_collector(), ThreadHeapStatsCollector::kInvokePreFinalizers);
  SweepForbiddenScope sweep_forbidden(this);
  // Pre finalizers may access unmarked objects but are forbidden from
  // ressurecting them.
  ObjectResurrectionForbiddenScope object_resurrection_forbidden(this);

  // Call the prefinalizers in the opposite order to their registration.
  //
  // LinkedHashSet does not support modification during iteration, so
  // copy items first.
  //
  // The prefinalizer callback wrapper returns |true| when its associated
  // object is unreachable garbage and the prefinalizer callback has run.
  // The registered prefinalizer entry must then be removed and deleted.
  Deque<PreFinalizer> remaining_ordered_pre_finalizers;
  for (auto rit = ordered_pre_finalizers_.rbegin();
       rit != ordered_pre_finalizers_.rend(); ++rit) {
    const PreFinalizer& pre_finalizer = *rit;
    if (!(pre_finalizer.second)(pre_finalizer.first))
      remaining_ordered_pre_finalizers.push_front(pre_finalizer);
  }

  ordered_pre_finalizers_ = std::move(remaining_ordered_pre_finalizers);
}

// static
AtomicEntryFlag ThreadState::incremental_marking_flag_;

void ThreadState::EnableIncrementalMarkingBarrier() {
  CHECK(!IsIncrementalMarking());
  incremental_marking_flag_.Enter();
  SetIncrementalMarking(true);
}

void ThreadState::DisableIncrementalMarkingBarrier() {
  CHECK(IsIncrementalMarking());
  incremental_marking_flag_.Exit();
  SetIncrementalMarking(false);
}

void ThreadState::IncrementalMarkingStart(BlinkGC::GCReason reason) {
  VLOG(2) << "[state:" << this << "] "
          << "IncrementalMarking: Start";
  DCHECK(!IsMarkingInProgress());
  CompleteSweep();
  DCHECK(!IsSweepingInProgress());
  Heap().stats_collector()->NotifyMarkingStarted(reason);
  {
    ThreadHeapStatsCollector::EnabledScope stats_scope(
        Heap().stats_collector(),
        ThreadHeapStatsCollector::kIncrementalMarkingStartMarking, "reason",
        BlinkGC::ToString(reason));
    AtomicPauseScope atomic_pause_scope(this);
    next_incremental_marking_step_duration_ =
        kDefaultIncrementalMarkingStepDuration;
    previous_incremental_marking_time_left_ = base::TimeDelta::Max();
    MarkPhasePrologue(BlinkGC::kNoHeapPointersOnStack,
                      BlinkGC::kIncrementalMarking, reason);
    MarkPhaseVisitRoots();
    EnableIncrementalMarkingBarrier();
    ScheduleIncrementalMarkingStep();
    DCHECK(IsMarkingInProgress());
  }
}

void ThreadState::IncrementalMarkingStep(BlinkGC::StackState stack_state) {
  DCHECK(IsMarkingInProgress());

  ThreadHeapStatsCollector::EnabledScope stats_scope(
      Heap().stats_collector(),
      ThreadHeapStatsCollector::kIncrementalMarkingStep);
  VLOG(2) << "[state:" << this << "] "
          << "IncrementalMarking: Step "
          << "Reason: " << BlinkGC::ToString(current_gc_data_.reason);
  AtomicPauseScope atomic_pause_scope(this);
  if (stack_state == BlinkGC::kNoHeapPointersOnStack) {
    Heap().FlushNotFullyConstructedObjects();
  }
  const bool complete = MarkPhaseAdvanceMarking(
      base::TimeTicks::Now() + next_incremental_marking_step_duration_);
  if (complete) {
    if (IsUnifiedGCMarkingInProgress()) {
      // If there are no more objects to mark for unified garbage collections
      // just bail out of helping incrementally using tasks. V8 will drive
      // further marking if new objects are discovered. Otherwise, just process
      // the rest in the atomic pause.
      DCHECK(IsUnifiedGCMarkingInProgress());
      SetGCState(kIncrementalMarkingStepPaused);
    } else {
      ScheduleIncrementalMarkingFinalize();
    }
  } else {
    ScheduleIncrementalMarkingStep();
  }
  DCHECK(IsMarkingInProgress());
}

void ThreadState::IncrementalMarkingFinalize() {
  DCHECK(IsMarkingInProgress());
  DCHECK(!IsUnifiedGCMarkingInProgress());

  ThreadHeapStatsCollector::EnabledScope stats_scope(
      Heap().stats_collector(),
      ThreadHeapStatsCollector::kIncrementalMarkingFinalize);
  VLOG(2) << "[state:" << this << "] "
          << "IncrementalMarking: Finalize "
          << "Reason: " << BlinkGC::ToString(current_gc_data_.reason);
  // Call into the regular bottleneck instead of the internal version to get
  // UMA accounting and allow follow up GCs if necessary.
  CollectGarbage(BlinkGC::kNoHeapPointersOnStack, BlinkGC::kIncrementalMarking,
                 BlinkGC::kConcurrentAndLazySweeping, current_gc_data_.reason);
}

bool ThreadState::FinishIncrementalMarkingIfRunning(
    BlinkGC::StackState stack_state,
    BlinkGC::MarkingType marking_type,
    BlinkGC::SweepingType sweeping_type,
    BlinkGC::GCReason reason) {
  if (IsMarkingInProgress()) {
    // TODO(mlippautz): Consider improving this mechanism as it will pull in
    // finalization of V8 upon Oilpan GCs during a unified GC. Alternative
    // include either breaking up the GCs or avoiding the call in first place.
    if (IsUnifiedGCMarkingInProgress()) {
      unified_heap_controller()->FinalizeTracing();
    } else {
      RunAtomicPause(stack_state, marking_type, sweeping_type, reason);
    }
    return true;
  }
  return false;
}

void ThreadState::CollectGarbage(BlinkGC::StackState stack_state,
                                 BlinkGC::MarkingType marking_type,
                                 BlinkGC::SweepingType sweeping_type,
                                 BlinkGC::GCReason reason) {
  // Nested garbage collection invocations are not supported.
  CHECK(!IsGCForbidden());
  // Garbage collection during sweeping is not supported. This can happen when
  // finalizers trigger garbage collections.
  if (SweepForbidden())
    return;

  base::TimeTicks start_total_collect_garbage_time = base::TimeTicks::Now();
  RUNTIME_CALL_TIMER_SCOPE_IF_ISOLATE_EXISTS(
      GetIsolate(), RuntimeCallStats::CounterId::kCollectGarbage);

  const bool was_incremental_marking = FinishIncrementalMarkingIfRunning(
      stack_state, marking_type, sweeping_type, reason);

  // We don't want floating garbage for the specific garbage collection types
  // mentioned below. In this case we will follow up with a regular full
  // garbage collection.
  const bool should_do_full_gc =
      !was_incremental_marking ||
      reason == BlinkGC::GCReason::kForcedGCForTesting ||
      reason == BlinkGC::GCReason::kMemoryPressureGC ||
      reason == BlinkGC::GCReason::kThreadTerminationGC;
  if (should_do_full_gc) {
    CompleteSweep();
    SetGCState(kNoGCScheduled);
    Heap().stats_collector()->NotifyMarkingStarted(reason);
    RunAtomicPause(stack_state, marking_type, sweeping_type, reason);
  }

  const base::TimeDelta total_collect_garbage_time =
      base::TimeTicks::Now() - start_total_collect_garbage_time;
  UMA_HISTOGRAM_TIMES("BlinkGC.TimeForTotalCollectGarbage",
                      total_collect_garbage_time);

#define COUNT_BY_GC_REASON(reason)                                     \
  case BlinkGC::GCReason::k##reason: {                                 \
    UMA_HISTOGRAM_TIMES("BlinkGC.TimeForTotalCollectGarbage_" #reason, \
                        total_collect_garbage_time);                   \
    break;                                                             \
  }

  switch (reason) {
    COUNT_BY_GC_REASON(PreciseGC)
    COUNT_BY_GC_REASON(ConservativeGC)
    COUNT_BY_GC_REASON(ForcedGCForTesting)
    COUNT_BY_GC_REASON(MemoryPressureGC)
    COUNT_BY_GC_REASON(ThreadTerminationGC)
    COUNT_BY_GC_REASON(IncrementalV8FollowupGC)
    COUNT_BY_GC_REASON(UnifiedHeapGC)
    COUNT_BY_GC_REASON(UnifiedHeapForMemoryReductionGC)
  }
#undef COUNT_BY_GC_REASON

  VLOG(1) << "[state:" << this << "]"
          << " CollectGarbage: time: " << std::setprecision(2)
          << total_collect_garbage_time.InMillisecondsF() << "ms"
          << " stack: " << BlinkGC::ToString(stack_state)
          << " marking: " << BlinkGC::ToString(marking_type)
          << " sweeping: " << BlinkGC::ToString(sweeping_type)
          << " reason: " << BlinkGC::ToString(reason);
}

void ThreadState::AtomicPauseMarkRoots(BlinkGC::StackState stack_state,
                                       BlinkGC::MarkingType marking_type,
                                       BlinkGC::GCReason reason) {
  ThreadHeapStatsCollector::EnabledScope advance_tracing_scope(
      Heap().stats_collector(), ThreadHeapStatsCollector::kAtomicPauseMarkRoots,
      "epoch", gc_age_, "forced", IsForcedGC(current_gc_data_.reason));
  MarkPhaseVisitRoots();
  MarkPhaseVisitNotFullyConstructedObjects();
}

void ThreadState::AtomicPauseMarkTransitiveClosure() {
  ThreadHeapStatsCollector::EnabledScope advance_tracing_scope(
      Heap().stats_collector(),
      ThreadHeapStatsCollector::kAtomicPauseMarkTransitiveClosure, "epoch",
      gc_age_, "forced", IsForcedGC(current_gc_data_.reason));
  CHECK(MarkPhaseAdvanceMarking(base::TimeTicks::Max()));
}

void ThreadState::AtomicPauseMarkEpilogue(BlinkGC::MarkingType marking_type) {
  ThreadHeapStatsCollector::EnabledScope stats_scope(
      Heap().stats_collector(),
      ThreadHeapStatsCollector::kAtomicPauseMarkEpilogue, "epoch", gc_age_,
      "forced", IsForcedGC(current_gc_data_.reason));
  MarkPhaseEpilogue(marking_type);
  LeaveAtomicPause();
  LeaveGCForbiddenScope();
}

void ThreadState::AtomicPauseSweepAndCompact(
    BlinkGC::MarkingType marking_type,
    BlinkGC::SweepingType sweeping_type) {
  ThreadHeapStatsCollector::EnabledScope stats(
      Heap().stats_collector(),
      ThreadHeapStatsCollector::kAtomicPauseSweepAndCompact, "epoch", gc_age_,
      "forced", IsForcedGC(current_gc_data_.reason));
  AtomicPauseScope atomic_pause_scope(this);

  DCHECK(InAtomicMarkingPause());
  DCHECK(CheckThread());
  Heap().PrepareForSweep();

  if (marking_type == BlinkGC::kTakeSnapshot) {
    // Doing lazy sweeping for kTakeSnapshot doesn't make any sense so the
    // sweeping type should always be kEagerSweeping.
    DCHECK_EQ(sweeping_type, BlinkGC::kEagerSweeping);
    Heap().TakeSnapshot(ThreadHeap::SnapshotType::kHeapSnapshot);

    // This unmarks all marked objects and marks all unmarked objects dead.
    Heap().MakeConsistentForMutator();

    Heap().TakeSnapshot(ThreadHeap::SnapshotType::kFreelistSnapshot);

    FinishSnapshot();
    CHECK(!IsSweepingInProgress());
    CHECK_EQ(GetGCState(), kNoGCScheduled);
    return;
  }

  // We have to set the GCPhase to Sweeping before calling pre-finalizers
  // to disallow a GC during the pre-finalizers.
  SetGCPhase(GCPhase::kSweeping);

  // Allocation is allowed during the pre-finalizers and destructors.
  // However, they must not mutate an object graph in a way in which
  // a dead object gets resurrected.
  InvokePreFinalizers();

  // Slots filtering requires liveness information which is only present before
  // sweeping any arena.
  {
    ThreadHeapStatsCollector::Scope stats_scope(
        Heap().stats_collector(),
        ThreadHeapStatsCollector::kAtomicPauseCompaction);
    Heap().Compaction()->FilterNonLiveSlots();
  }

  // Last point where all mark bits are present.
  VerifyMarking(marking_type);

  // Any sweep compaction must happen after pre-finalizers, as it will
  // finalize dead objects in compactable arenas (e.g., backing stores
  // for container objects.)
  //
  // As per-contract for prefinalizers, those finalizable objects must
  // still be accessible when the prefinalizer runs, hence we cannot
  // schedule compaction until those have run.
  {
    SweepForbiddenScope scope(this);
    NoAllocationScope no_allocation_scope(this);
    Heap().Compact();
    Heap().DestroyCompactionWorklists();
  }

#if defined(ADDRESS_SANITIZER)
  Heap().PoisonAllHeaps();
#endif
  DCHECK(IsSweepingInProgress());
  if (sweeping_type == BlinkGC::kEagerSweeping) {
    // Eager sweeping should happen only in testing.
    CompleteSweep();
  } else {
    DCHECK(sweeping_type == BlinkGC::kConcurrentAndLazySweeping);
    // The default behavior is concurrent and lazy sweeping.
    ScheduleConcurrentAndLazySweep();
  }
}

void ThreadState::RunAtomicPause(BlinkGC::StackState stack_state,
                                 BlinkGC::MarkingType marking_type,
                                 BlinkGC::SweepingType sweeping_type,
                                 BlinkGC::GCReason reason) {
  // Legacy scope that is used to add stand-alone Oilpan GCs to DevTools
  // timeline.
  TRACE_EVENT1("blink_gc,devtools.timeline", "BlinkGC.AtomicPhase", "forced",
               IsForcedGC(reason));

  AtomicPauseMarkPrologue(stack_state, marking_type, reason);
  AtomicPauseMarkRoots(stack_state, marking_type, reason);
  AtomicPauseMarkTransitiveClosure();
  AtomicPauseMarkEpilogue(marking_type);
  AtomicPauseSweepAndCompact(marking_type, sweeping_type);
  AtomicPauseEpilogue();
}

namespace {

MarkingVisitor::MarkingMode GetMarkingMode(bool should_compact,
                                           bool create_snapshot) {
  CHECK(!should_compact || !create_snapshot);
  return (create_snapshot)
             ? MarkingVisitor::kSnapshotMarking
             : (should_compact) ? MarkingVisitor::kGlobalMarkingWithCompaction
                                : MarkingVisitor::kGlobalMarking;
}

}  // namespace

void ThreadState::MarkPhasePrologue(BlinkGC::StackState stack_state,
                                    BlinkGC::MarkingType marking_type,
                                    BlinkGC::GCReason reason) {
  SetGCPhase(GCPhase::kMarking);
  Heap().SetupWorklists();

  const bool take_snapshot = marking_type == BlinkGC::kTakeSnapshot;

  // Enable compaction if supported and feasible.
  const bool compaction_enabled =
      !take_snapshot &&
      Heap().Compaction()->ShouldCompact(stack_state, marking_type, reason);
  if (compaction_enabled) {
    Heap().Compaction()->Initialize(this);
  }

  current_gc_data_.reason = reason;
  current_gc_data_.visitor =
      IsUnifiedGCMarkingInProgress()
          ? std::make_unique<UnifiedHeapMarkingVisitor>(
                this, GetMarkingMode(compaction_enabled, take_snapshot),
                GetIsolate())
          : std::make_unique<MarkingVisitor>(
                this, GetMarkingMode(compaction_enabled, take_snapshot));
  current_gc_data_.stack_state = stack_state;
  current_gc_data_.marking_type = marking_type;
}

void ThreadState::MarkPhaseVisitRoots() {
  Visitor* visitor = current_gc_data_.visitor.get();

  VisitPersistents(visitor);

  // DOM wrapper references from V8 are considered as roots. Exceptions are:
  // - Unified garbage collections: The cross-component references between
  //   V8<->Blink are found using collaborative tracing where both GCs report
  //   live references to each other.
  // - Termination GCs that do not care about V8 any longer.
  if (!IsUnifiedGCMarkingInProgress() &&
      current_gc_data_.reason != BlinkGC::GCReason::kThreadTerminationGC) {
    VisitDOMWrappers(visitor);
  }

  // For unified garbage collections any active ScriptWrappable objects are
  // considered as roots.
  if (IsUnifiedGCMarkingInProgress()) {
    ActiveScriptWrappableBase::TraceActiveScriptWrappables(isolate_, visitor);
  }

  if (current_gc_data_.stack_state == BlinkGC::kHeapPointersOnStack) {
    ThreadHeapStatsCollector::Scope stats_scope(
        Heap().stats_collector(), ThreadHeapStatsCollector::kVisitStackRoots);
    AddressCache::EnabledScope address_cache_scope(Heap().address_cache());
    PushRegistersAndVisitStack();
  }
}

bool ThreadState::MarkPhaseAdvanceMarking(base::TimeTicks deadline) {
  return Heap().AdvanceMarking(
      reinterpret_cast<MarkingVisitor*>(current_gc_data_.visitor.get()),
      deadline);
}

bool ThreadState::VerifyMarkingEnabled() const {
  bool should_verify_marking = base::FeatureList::IsEnabled(
      blink::features::kBlinkHeapIncrementalMarkingStress);
#if BUILDFLAG(BLINK_HEAP_VERIFICATION)
  should_verify_marking = true;
#endif  // BLINK_HEAP_VERIFICATION
  return should_verify_marking;
}

void ThreadState::MarkPhaseVisitNotFullyConstructedObjects() {
  Heap().MarkNotFullyConstructedObjects(
      reinterpret_cast<MarkingVisitor*>(current_gc_data_.visitor.get()));
}

void ThreadState::MarkPhaseEpilogue(BlinkGC::MarkingType marking_type) {
  Visitor* visitor = current_gc_data_.visitor.get();
  {
    // See ProcessHeap::CrossThreadPersistentMutex().
    MutexLocker persistent_lock(ProcessHeap::CrossThreadPersistentMutex());
    VisitWeakPersistents(visitor);
    Heap().WeakProcessing(visitor);
  }
  Heap().DestroyMarkingWorklists(current_gc_data_.stack_state);

  // TODO(omerkatz): When migrating to concurrent marking, the following 3
  // lines will need to be wrapped with a loop iterating over all visitors.
  current_gc_data_.visitor->FlushCompactionWorklists();
  const size_t marked_bytes = current_gc_data_.visitor->marked_bytes();
  current_gc_data_.visitor.reset();

  Heap().stats_collector()->NotifyMarkingCompleted(marked_bytes);

  WTF::Partitions::ReportMemoryUsageHistogram();

  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      CustomCountHistogram, total_object_space_histogram,
      ("BlinkGC.TotalObjectSpace", 0, 4 * 1024 * 1024, 50));
  total_object_space_histogram.Count(
      CappedSizeInKB(ProcessHeap::TotalAllocatedObjectSize()));
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      CustomCountHistogram, total_allocated_space_histogram,
      ("BlinkGC.TotalAllocatedSpace", 0, 4 * 1024 * 1024, 50));
  total_allocated_space_histogram.Count(
      CappedSizeInKB(ProcessHeap::TotalAllocatedSpace()));
}

void ThreadState::VerifyMarking(BlinkGC::MarkingType marking_type) {
  DCHECK_NE(BlinkGC::kTakeSnapshot, marking_type);

  if (VerifyMarkingEnabled())
    Heap().VerifyMarking();
}

void ThreadState::CollectAllGarbageForTesting(BlinkGC::StackState stack_state) {
  // We need to run multiple GCs to collect a chain of persistent handles.
  size_t previous_live_objects = 0;
  for (int i = 0; i < 5; ++i) {
    CollectGarbage(stack_state, BlinkGC::kAtomicMarking,
                   BlinkGC::kEagerSweeping,
                   BlinkGC::GCReason::kForcedGCForTesting);
    const size_t live_objects =
        Heap().stats_collector()->previous().marked_bytes;
    if (live_objects == previous_live_objects)
      break;
    previous_live_objects = live_objects;
  }
}

void ThreadState::EnableCompactionForNextGCForTesting() {
  Heap().Compaction()->EnableCompactionForNextGCForTesting();
}

void ThreadState::UpdateIncrementalMarkingStepDuration() {
  if (!IsIncrementalMarking())
    return;
  base::TimeDelta time_left =
      Heap().stats_collector()->estimated_marking_time() -
      Heap().stats_collector()->marking_time_so_far();
  // Increase step size if estimated time left is increasing.
  if (previous_incremental_marking_time_left_ < time_left) {
    constexpr double ratio = 2.0;
    next_incremental_marking_step_duration_ *= ratio;
  }
  previous_incremental_marking_time_left_ = time_left;
}

}  // namespace blink
