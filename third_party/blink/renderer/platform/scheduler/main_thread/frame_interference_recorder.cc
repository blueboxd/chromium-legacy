// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_interference_recorder.h"

#include <algorithm>

#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"

namespace blink {
namespace scheduler {

namespace {

// Park–Miller random number generator. Not secure, used only for sampling.
// https://en.wikipedia.org/wiki/Lehmer_random_number_generator
uint32_t GetNextRandomNumber(uint32_t previous_value) {
  constexpr uint32_t kModulo = (1U << 31) - 1;
  return 48271 * static_cast<int64_t>(previous_value) % kModulo;
}

}  // namespace

FrameInterferenceRecorder::FrameInterferenceRecorder(int sampling_rate)
    : sampling_rate_(sampling_rate),
      random_value_(static_cast<uint32_t>(base::RandUint64())) {}

FrameInterferenceRecorder::~FrameInterferenceRecorder() = default;

void FrameInterferenceRecorder::OnTaskReady(
    const void* frame_scheduler,
    base::sequence_manager::EnqueueOrder enqueue_order,
    base::sequence_manager::LazyNow* lazy_now) {
  if (!ShouldSampleNextReadyTask())
    return;

  if (!frame_scheduler)
    return;

  base::AutoLock auto_lock(lock_);

  DCHECK(!base::Contains(ready_tasks_, enqueue_order));
  ReadyTask& ready_task = ready_tasks_[enqueue_order];
  ready_task.time_for_all_frames_when_ready = time_for_all_frames_;
  auto time_for_frame_it = time_for_frame_.find(frame_scheduler);
  if (time_for_frame_it != time_for_frame_.end())
    ready_task.time_for_this_frame_when_ready = time_for_frame_it->value;
#if DCHECK_IS_ON()
  ready_task.frame_scheduler = frame_scheduler;
#endif

  // If the currently running time is associated with a frame, adjust the
  // clock readings in |ready_task| to include its execution time.
  if (!current_task_frame_scheduler_)
    return;

  base::TimeDelta running_time = GetCurrentFrameTaskRunningTime(lazy_now);
  // Clamp the value above zero to handle the case where the time in |lazy_now|
  // was captured after the current main thread task started running.
  running_time = std::max(base::TimeDelta(), running_time);

  ready_task.time_for_all_frames_when_ready += running_time;
  if (current_task_frame_scheduler_ == frame_scheduler)
    ready_task.time_for_this_frame_when_ready += running_time;
}

void FrameInterferenceRecorder::OnTaskStarted(
    MainThreadTaskQueue* queue,
    base::sequence_manager::EnqueueOrder enqueue_order,
    base::TimeTicks start_time) {
  base::AutoLock auto_lock(lock_);
  DCHECK(current_task_start_time_.is_null());
  DCHECK(!current_task_frame_scheduler_);

  const FrameScheduler* frame_scheduler =
      queue ? GetFrameSchedulerForQueue(queue) : nullptr;

  auto ready_task_it = ready_tasks_.find(enqueue_order);
  if (ready_task_it != ready_tasks_.end()) {
    RecordHistogramForReadyTask(ready_task_it->second, queue, frame_scheduler,
                                enqueue_order);
    ready_tasks_.erase(ready_task_it);
  }

  current_task_start_time_ = start_time;
  current_task_frame_scheduler_ = frame_scheduler;
}

void FrameInterferenceRecorder::OnTaskCompleted(MainThreadTaskQueue* queue,
                                                base::TimeTicks end_time) {
  base::AutoLock auto_lock(lock_);
  DCHECK(!end_time.is_null());
  if (queue)
    DCHECK_EQ(GetFrameSchedulerForQueue(queue), current_task_frame_scheduler_);

  base::sequence_manager::LazyNow lazy_now(end_time);
  AccumulateCurrentTaskRunningTime(&lazy_now);
  current_task_start_time_ = base::TimeTicks();
  current_task_frame_scheduler_ = nullptr;
}

void FrameInterferenceRecorder::OnFrameSchedulerDestroyed(
    const FrameScheduler* frame_scheduler) {
  base::AutoLock auto_lock(lock_);
  time_for_frame_.erase(frame_scheduler);
  if (current_task_frame_scheduler_ == frame_scheduler)
    current_task_frame_scheduler_ = nullptr;
}

void FrameInterferenceRecorder::AccumulateCurrentTaskRunningTime(
    base::sequence_manager::LazyNow* lazy_now) {
  DCHECK(!current_task_start_time_.is_null());

  if (!current_task_frame_scheduler_)
    return;

  // Update clocks.
  const base::TimeDelta running_time = GetCurrentFrameTaskRunningTime(lazy_now);
  DCHECK_GE(running_time, base::TimeDelta());

  time_for_all_frames_ += running_time;

  auto time_for_frame_it = time_for_frame_.find(current_task_frame_scheduler_);
  if (time_for_frame_it == time_for_frame_.end())
    time_for_frame_.insert(current_task_frame_scheduler_, running_time);
  else
    time_for_frame_it->value += running_time;

  DCHECK_GE(time_for_all_frames_,
            time_for_frame_.find(current_task_frame_scheduler_)->value);
}

base::TimeDelta FrameInterferenceRecorder::GetCurrentFrameTaskRunningTime(
    base::sequence_manager::LazyNow* lazy_now) const {
  DCHECK(current_task_frame_scheduler_);
  DCHECK(!current_task_start_time_.is_null());
  return lazy_now->Now() - current_task_start_time_;
}

uint32_t FrameInterferenceRecorder::ShouldSampleNextReadyTask() {
  uint32_t previous_random_value =
      random_value_.load(std::memory_order_relaxed);
  uint32_t next_random_value;
  do {
    next_random_value = GetNextRandomNumber(previous_random_value);
  } while (!random_value_.compare_exchange_weak(
      previous_random_value, next_random_value, std::memory_order_relaxed));
  return next_random_value % sampling_rate_ == 0;
}

void FrameInterferenceRecorder::RecordHistogramForReadyTask(
    const ReadyTask& ready_task,
    const MainThreadTaskQueue* queue,
    const FrameScheduler* frame_scheduler,
    base::sequence_manager::EnqueueOrder enqueue_order) {
  // Record the histogram if the task is associated with a frame and wasn't
  // blocked by a fence or by the TaskQueue being disabled.
  if (!frame_scheduler || enqueue_order < queue->GetLastUnblockEnqueueOrder())
    return;

#if DCHECK_IS_ON()
  DCHECK_EQ(frame_scheduler, ready_task.frame_scheduler);
#endif

  // |time_for_all_frames_since_ready| and |time_for_this_frame_since_ready| are
  // clamped above zero to mitigate this problem:
  //
  // X = initial |time_for_all_frames_|.
  //
  // Thread 1: Captures time T1.
  //           Invokes OnTaskStarted with T1.
  //           Captures time T2.
  // Thread 2: Captures time T3.
  //           Invokes OnTaskReady with T3. [*]
  //             |time_for_all_frames_when_ready| = X + T3 - T1
  //             (Ready task is from the same frame as the running task)
  // Thread 1: Invokes OnTaskCompleted with T2.
  //             |time_for_all_frames_| += T2 - T1
  //           Invokes OnTaskStarted for the next task.
  //             |time_for_all_frames_since_ready|
  //                 = |time_for_all_frames_| - |time_for_all_frames_when_ready|
  //                 = (X + T2 - T1) - (X + T3 - T1)
  //                 = T2 - T3
  //             Which is a negative value.
  //
  // |time_for_other_frames_since_ready| is clamped above zero for the case
  // where the ready and running tasks are not from the same frame at [*]. In
  // that case, |ready_task.time_for_all_frames_when_ready| was incremented with
  // the current task's running time, but not
  // |ready_task.time_for_this_frames_when_ready|, which can cause
  // |time_for_this_frame_since_ready| to be greater than
  // |time_for_all_frames_since_ready|.
  //
  // Note: These problem exists because OnTask*() methods use times captured
  // outside the scope of |lock_|.
  DCHECK_GE(ready_task.time_for_all_frames_when_ready,
            ready_task.time_for_this_frame_when_ready);
  const base::TimeDelta time_for_all_frames_since_ready = std::max(
      base::TimeDelta(),
      time_for_all_frames_ - ready_task.time_for_all_frames_when_ready);

  auto time_for_frame_it = time_for_frame_.find(frame_scheduler);
  const base::TimeDelta time_for_frame =
      time_for_frame_it == time_for_frame_.end() ? base::TimeDelta()
                                                 : time_for_frame_it->value;
  const base::TimeDelta time_for_this_frame_since_ready =
      std::max(base::TimeDelta(),
               time_for_frame - ready_task.time_for_this_frame_when_ready);

  const base::TimeDelta time_for_other_frames_since_ready =
      std::max(base::TimeDelta(), time_for_all_frames_since_ready -
                                      time_for_this_frame_since_ready);
  RecordHistogram(queue, time_for_other_frames_since_ready);
}

void FrameInterferenceRecorder::RecordHistogram(
    const MainThreadTaskQueue* queue,
    base::TimeDelta sample) {
  // Histogram should only be recorded for queue types that can be associated
  // with frames.
  DCHECK(GetFrameSchedulerForQueue(queue));
  const MainThreadTaskQueue::QueueType queue_type = queue->queue_type();
  // TODO(altimin): Remove kDefault once there is no per-frame kDefault queue.
  DCHECK(queue_type == MainThreadTaskQueue::QueueType::kDefault ||
         queue_type == MainThreadTaskQueue::QueueType::kFrameLoading ||
         queue_type == MainThreadTaskQueue::QueueType::kFrameLoadingControl ||
         queue_type == MainThreadTaskQueue::QueueType::kFrameThrottleable ||
         queue_type == MainThreadTaskQueue::QueueType::kFrameDeferrable ||
         queue_type == MainThreadTaskQueue::QueueType::kFramePausable ||
         queue_type == MainThreadTaskQueue::QueueType::kFrameUnpausable ||
         queue_type == MainThreadTaskQueue::QueueType::kIdle);

  const std::string histogram_name =
      std::string("RendererScheduler.TimeRunningOtherFramesWhileTaskReady.") +
      (GetFrameSchedulerForQueue(queue)->IsFrameVisible() ? "Visible"
                                                          : "Hidden");
  base::UmaHistogramCustomMicrosecondsTimes(
      histogram_name, sample, base::TimeDelta::FromMicroseconds(1),
      base::TimeDelta::FromSeconds(1), 100);
  const std::string histogram_name_with_queue_type =
      histogram_name + "." + MainThreadTaskQueue::NameForQueueType(queue_type);
  base::UmaHistogramCustomMicrosecondsTimes(
      histogram_name_with_queue_type, sample,
      base::TimeDelta::FromMicroseconds(1), base::TimeDelta::FromSeconds(1),
      100);
}

const FrameScheduler* FrameInterferenceRecorder::GetFrameSchedulerForQueue(
    const MainThreadTaskQueue* queue) {
  return queue->GetFrameScheduler();
}

}  // namespace scheduler
}  // namespace blink
