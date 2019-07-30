// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_FRAME_INTERFERENCE_RECORDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_FRAME_INTERFERENCE_RECORDER_H_

#include <atomic>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/task/sequence_manager/enqueue_order.h"
#include "base/task/sequence_manager/lazy_now.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {
namespace scheduler {

class MainThreadTaskQueue;

// Records the RendererScheduler.TimeRunningOtherFramesWhileTaskReady histogram,
// which tracks how much time is spent running tasks from other frames between
// when a frame task becomes ready to run and when it starts running.
//
// Implementation details
// ----------------------
//
// The time running tasks from other frames while a task is ready is defined as:
//
//   ([Time running tasks for all frames between thread start and task start] -
//    [Time running tasks for all frames between thread start and task ready]) -
//   ([Time running tasks for same frame between thread start and task start] -
//    [Time running tasks for same frame between thread start and task ready]) -
//
// To get these values, we maintain the following state:
//
// A) For the task currently running at each nesting level:
//     A1) Start time
//     A2) Associated frame
// B) For each frame, an accumulator of time running tasks for that frame since
//    thread start.
// C) An accumulator of time running tasks for any frame since thread start.
// D) For each queued task for which we want to record the histogram:
//     D1) Time running tasks for all frames between thread start and task
//         queued.
//     D2) Time running tasks for same frame between thread start and task
//         queued.
//
// At any given time, we can compute:
//
// E) The time running tasks for a specific FRAME since thread start:
//      Read the FRAME's accumulator in B. Add [Current Time] - A1 if the
//      task at the highest nesting level is associated with FRAME.
// F) The time running tasks for any frame since thread start:
//      Read C. Add [Current Time] - A1 if the task at highest nesting level is
//      associated with any frame.
//
// When a task becomes ready, we decide whether we want to record the histogram
// for it. If so, we use E and F to add an entry to D.
//
// When a task starts running, we check whether it is in D. If so, we use D1,
// D2, E and F to compute the value to record to the histogram. Then, we update
// the entry at the top of A. When a task finishes running, we update B and C
// and we clear the entry at the top of A. Note that even though we sample the
// tasks for which we record the histogram, we need to accumulate the time all
// tasks in B and C since it can contribute to the value recorded for a sampled
// task.
//
// Entering a nested loop is equivalent to finishing the current task. Exiting a
// nested loop is equivalent to resuming the task that was finished when the
// nested loop was entered (no histogram is recorded when resuming an existing
// task).
class PLATFORM_EXPORT FrameInterferenceRecorder {
 public:
  // The histogram is recorded for 1 out of |sampling_rate| tasks.
  explicit FrameInterferenceRecorder(int sampling_rate = 1000);
  ~FrameInterferenceRecorder();

  // Invoked when a task becomes ready. For a non-delayed task, this is at post
  // time. For a delayed task, this is when the task's delay expires.
  //
  // |frame| is the FrameScheduler associated with the task (passed as void*
  // because it's only used a key and FrameScheduler isn't thread-safe - void
  // prevents undesired access).
  //
  // This is the only public method of this class that can be called from
  // another thread than the main thread.
  void OnTaskReady(const void* frame_scheduler,
                   base::sequence_manager::EnqueueOrder enqueue_order,
                   base::sequence_manager::LazyNow* lazy_now);

  // Invoked from the main thread when a task starts/finishes running, or when a
  // nested loop is exited/entered. When a nested loop is exited,
  // |enqueue_order| is EnqueueOrder::none().
  void OnTaskStarted(MainThreadTaskQueue* queue,
                     base::sequence_manager::EnqueueOrder enqueue_order,
                     base::TimeTicks start_time);
  void OnTaskCompleted(MainThreadTaskQueue* queue, base::TimeTicks end_time);

  // Invoked at the end of the destructor of a FrameScheduler, on the main
  // thread. Cleans up state associated with that FrameScheduler. Does not
  // dereference |frame_scheduler|.
  void OnFrameSchedulerDestroyed(const FrameScheduler* frame_scheduler);

 private:
  // Information about a ready task for which the histogram will be recorded.
  struct ReadyTask {
    // The time read from |time_for_all_frames_| when the task became ready.
    base::TimeDelta time_for_all_frames_when_ready;

    // The time read from |time_for_frame_[task's frame]| when the task became
    // ready.
    base::TimeDelta time_for_this_frame_when_ready;

#if DCHECK_IS_ON()
    // The FrameScheduler associated with the task. Stored in a void* because
    // it's only used a key and FrameScheduler isn't thread-safe (so void
    // prevents undesired access).
    const void* frame_scheduler = nullptr;
#endif
  };

  // Updates |time_for_all_frames_| and |time_for_frame_[current frame]| so that
  // they reflect current running time.
  void AccumulateCurrentTaskRunningTime(
      base::sequence_manager::LazyNow* lazy_now)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Returns the running time of the current frame task. Cannot be called if the
  // task running at the highest nesting level isn't a frame task.
  base::TimeDelta GetCurrentFrameTaskRunningTime(
      base::sequence_manager::LazyNow* lazy_now) const
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Records the histogram for |ready_task|.
  void RecordHistogramForReadyTask(
      const ReadyTask& ready_task,
      const MainThreadTaskQueue* queue,
      const FrameScheduler* frame_scheduler,
      base::sequence_manager::EnqueueOrder enqueue_order)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Returns true if the next ready task should be sampled. Thread-safe.
  uint32_t ShouldSampleNextReadyTask();

  // Virtual for testing.
  virtual void RecordHistogram(const MainThreadTaskQueue* queue,
                               base::TimeDelta sample);
  virtual const FrameScheduler* GetFrameSchedulerForQueue(
      const MainThreadTaskQueue* queue);

  // Sampling rate. The histogram is recorded for 1/|sampling_rate_| tasks.
  const int sampling_rate_;

  // Current random value. Used to determine which tasks are sampled.
  std::atomic<uint32_t> random_value_;

  // Protects all members below. Low contention because most accesses are from
  // the main thread. Is only occasionally acquired from other threads when
  // OnTaskReady() decides to record the histogram for a task.
  base::Lock lock_;

  // Start time of the currently running task, or is_null() if no task is
  // running.
  base::TimeTicks current_task_start_time_ GUARDED_BY(lock_);

  // FrameScheduler of the currently running task, or nullptr if no task is
  // running. Stored as a void* because it's only used a key and FrameScheduler
  // isn't thread-safe (so void prevents undesired access).
  const void* current_task_frame_scheduler_ GUARDED_BY(lock_) = nullptr;

  // Time spent running tasks for all frames since thread start.
  base::TimeDelta time_for_all_frames_ GUARDED_BY(lock_);

  // Time running tasks for each frame since thread start.
  WTF::HashMap<const void*, base::TimeDelta> time_for_frame_ GUARDED_BY(lock_);

  // Information about ready tasks for which the histogram will be recorded. Key
  // is the task's sequence number.
  base::flat_map<base::sequence_manager::EnqueueOrder, ReadyTask> ready_tasks_
      GUARDED_BY(lock_);

  DISALLOW_COPY_AND_ASSIGN(FrameInterferenceRecorder);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_FRAME_INTERFERENCE_RECORDER_H_
