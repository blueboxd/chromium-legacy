// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_watchdog_thread_v2.h"

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/bit_cast.h"
#include "base/debug/alias.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop_current.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/native_library.h"
#include "base/power_monitor/power_monitor.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "gpu/config/gpu_crash_keys.h"

namespace gpu {

GpuWatchdogThreadImplV2::GpuWatchdogThreadImplV2(base::TimeDelta timeout,
                                                 base::TimeDelta max_wait_time,
                                                 bool is_test_mode)
    : watchdog_timeout_(timeout),
      in_gpu_initialization_(true),
      max_wait_time_(max_wait_time),
      is_test_mode_(is_test_mode),
      watched_gpu_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  base::MessageLoopCurrent::Get()->AddTaskObserver(this);
#if defined(OS_WIN)
  // GetCurrentThread returns a pseudo-handle that cannot be used by one thread
  // to identify another. DuplicateHandle creates a "real" handle that can be
  // used for this purpose.
  if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
                       GetCurrentProcess(), &watched_thread_handle_,
                       THREAD_QUERY_INFORMATION, FALSE, 0)) {
    watched_thread_handle_ = nullptr;
  }
#endif

  Arm();
}

GpuWatchdogThreadImplV2::~GpuWatchdogThreadImplV2() {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());
  Stop();  // stop the watchdog thread

  base::MessageLoopCurrent::Get()->RemoveTaskObserver(this);
  base::PowerMonitor::RemoveObserver(this);
  GpuWatchdogHistogram(GpuWatchdogThreadEvent::kGpuWatchdogEnd);
#if defined(OS_WIN)
  if (watched_thread_handle_)
    CloseHandle(watched_thread_handle_);
#endif
}

// static
std::unique_ptr<GpuWatchdogThreadImplV2> GpuWatchdogThreadImplV2::Create(
    bool start_backgrounded,
    base::TimeDelta timeout,
    base::TimeDelta max_wait_time,
    bool is_test_mode) {
  auto watchdog_thread = base::WrapUnique(
      new GpuWatchdogThreadImplV2(timeout, max_wait_time, is_test_mode));
  base::Thread::Options options;
  options.timer_slack = base::TIMER_SLACK_MAXIMUM;
  watchdog_thread->StartWithOptions(options);
  if (start_backgrounded)
    watchdog_thread->OnBackgrounded();
  return watchdog_thread;
}

// static
std::unique_ptr<GpuWatchdogThreadImplV2> GpuWatchdogThreadImplV2::Create(
    bool start_backgrounded) {
  return Create(start_backgrounded, kGpuWatchdogTimeout, kMaxWaitTime, false);
}

// Do not add power observer during watchdog init, PowerMonitor might not be up
// running yet.
void GpuWatchdogThreadImplV2::AddPowerObserver() {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());

  // Forward it to the watchdog thread. Call PowerMonitor::AddObserver on the
  // watchdog thread so that OnSuspend and OnResume will be called on watchdog
  // thread.
  is_add_power_observer_called_ = true;
  task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&GpuWatchdogThreadImplV2::OnAddPowerObserver,
                                base::Unretained(this)));
}

// Android Chrome goes to the background. Called from the gpu thread.
void GpuWatchdogThreadImplV2::OnBackgrounded() {
  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuWatchdogThreadImplV2::StopWatchdogTimeoutTask,
                     base::Unretained(this), kAndroidBackgroundForeground));
}

// Android Chrome goes to the foreground. Called from the gpu thread.
void GpuWatchdogThreadImplV2::OnForegrounded() {
  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuWatchdogThreadImplV2::RestartWatchdogTimeoutTask,
                     base::Unretained(this), kAndroidBackgroundForeground));
}

// Called from the gpu thread when gpu init has completed.
void GpuWatchdogThreadImplV2::OnInitComplete() {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());

  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuWatchdogThreadImplV2::UpdateInitializationFlag,
                     base::Unretained(this)));
  Disarm();
}

// Called from the gpu thread in viz::GpuServiceImpl::~GpuServiceImpl().
// After this, no Disarm() will be called before the watchdog thread is
// destroyed. If this destruction takes too long, the watchdog timeout
// will be triggered.
void GpuWatchdogThreadImplV2::OnGpuProcessTearDown() {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());

  in_gpu_process_teardown_ = true;
  if (!IsArmed())
    Arm();
}

// Called from the gpu main thread.
void GpuWatchdogThreadImplV2::PauseWatchdog() {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());

  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuWatchdogThreadImplV2::StopWatchdogTimeoutTask,
                     base::Unretained(this), kGeneralGpuFlow));
}

// Called from the gpu main thread.
void GpuWatchdogThreadImplV2::ResumeWatchdog() {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());

  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuWatchdogThreadImplV2::RestartWatchdogTimeoutTask,
                     base::Unretained(this), kGeneralGpuFlow));
}

// Running on the watchdog thread.
// On Linux, Init() will be called twice for Sandbox Initialization. The
// watchdog is stopped and then restarted in StartSandboxLinux(). Everything
// should be the same and continue after the second init().
void GpuWatchdogThreadImplV2::Init() {
  watchdog_thread_task_runner_ = base::ThreadTaskRunnerHandle::Get();

  // Get and Invalidate weak_ptr should be done on the watchdog thread only.
  weak_ptr_ = weak_factory_.GetWeakPtr();
  base::TimeDelta timeout = watchdog_timeout_ * kInitFactor;
  task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&GpuWatchdogThreadImplV2::OnWatchdogTimeout, weak_ptr_),
      timeout);

  last_arm_disarm_counter_ = base::subtle::NoBarrier_Load(&arm_disarm_counter_);
  watchdog_start_timeticks_ = base::TimeTicks::Now();
  last_on_watchdog_timeout_timeticks_ = watchdog_start_timeticks_;

#if defined(OS_WIN)
  if (watched_thread_handle_) {
    if (base::ThreadTicks::IsSupported())
      base::ThreadTicks::WaitUntilInitialized();
    last_on_watchdog_timeout_thread_ticks_ = GetWatchedThreadTime();
    remaining_watched_thread_ticks_ = timeout;
  }
#endif
}

// Running on the watchdog thread.
void GpuWatchdogThreadImplV2::CleanUp() {
  DCHECK(watchdog_thread_task_runner_->BelongsToCurrentThread());
  weak_factory_.InvalidateWeakPtrs();
}

void GpuWatchdogThreadImplV2::ReportProgress() {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());
  InProgress();
}

void GpuWatchdogThreadImplV2::WillProcessTask(
    const base::PendingTask& pending_task,
    bool was_blocked_or_low_priority) {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());

  // The watchdog is armed at the beginning of the gpu process teardown.
  // Do not call Arm() during teardown.
  if (in_gpu_process_teardown_)
    DCHECK(IsArmed());
  else
    Arm();
}

void GpuWatchdogThreadImplV2::DidProcessTask(
    const base::PendingTask& pending_task) {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());

  // Keep the watchdog armed during tear down.
  if (in_gpu_process_teardown_)
    InProgress();
  else
    Disarm();
}

// Power Suspends. Running on the watchdog thread.
void GpuWatchdogThreadImplV2::OnSuspend() {
  StopWatchdogTimeoutTask(kPowerSuspendResume);
}

// Power Resumes. Running on the watchdog thread.
void GpuWatchdogThreadImplV2::OnResume() {
  RestartWatchdogTimeoutTask(kPowerSuspendResume);
}

// Running on the watchdog thread.
void GpuWatchdogThreadImplV2::OnAddPowerObserver() {
  DCHECK(watchdog_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(base::PowerMonitor::IsInitialized());

  is_power_observer_added_ = base::PowerMonitor::AddObserver(this);
}

// Running on the watchdog thread.
void GpuWatchdogThreadImplV2::RestartWatchdogTimeoutTask(
    PauseResumeSource source_of_request) {
  DCHECK(watchdog_thread_task_runner_->BelongsToCurrentThread());
  base::TimeDelta timeout;

  switch (source_of_request) {
    case kAndroidBackgroundForeground:
      if (!is_backgrounded_)
        return;
      is_backgrounded_ = false;
      timeout = watchdog_timeout_ * kRestartFactor;
      foregrounded_timeticks_ = base::TimeTicks::Now();
      foregrounded_event_ = true;
      num_of_timeout_after_foregrounded_ = 0;
      break;
    case kPowerSuspendResume:
      if (!in_power_suspension_)
        return;
      in_power_suspension_ = false;
      timeout = watchdog_timeout_ * kRestartFactor;
      power_resume_timeticks_ = base::TimeTicks::Now();
      power_resumed_event_ = true;
      num_of_timeout_after_power_resume_ = 0;
      break;
    case kGeneralGpuFlow:
      if (!is_paused_)
        return;
      is_paused_ = false;
      timeout = watchdog_timeout_ * kInitFactor;
      watchdog_resume_timeticks_ = base::TimeTicks::Now();
      break;
  }

  if (!is_backgrounded_ && !in_power_suspension_ && !is_paused_) {
    weak_ptr_ = weak_factory_.GetWeakPtr();
    task_runner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&GpuWatchdogThreadImplV2::OnWatchdogTimeout, weak_ptr_),
        timeout);
    last_on_watchdog_timeout_timeticks_ = base::TimeTicks::Now();
    last_arm_disarm_counter_ =
        base::subtle::NoBarrier_Load(&arm_disarm_counter_);
#if defined(OS_WIN)
    if (watched_thread_handle_) {
      last_on_watchdog_timeout_thread_ticks_ = GetWatchedThreadTime();
      remaining_watched_thread_ticks_ = timeout;
    }
#endif
  }
}

void GpuWatchdogThreadImplV2::StopWatchdogTimeoutTask(
    PauseResumeSource source_of_request) {
  DCHECK(watchdog_thread_task_runner_->BelongsToCurrentThread());

  switch (source_of_request) {
    case kAndroidBackgroundForeground:
      if (is_backgrounded_)
        return;
      is_backgrounded_ = true;
      backgrounded_timeticks_ = base::TimeTicks::Now();
      foregrounded_event_ = false;
      break;
    case kPowerSuspendResume:
      if (in_power_suspension_)
        return;
      in_power_suspension_ = true;
      power_suspend_timeticks_ = base::TimeTicks::Now();
      power_resumed_event_ = false;
      break;
    case kGeneralGpuFlow:
      if (is_paused_)
        return;
      is_paused_ = true;
      watchdog_pause_timeticks_ = base::TimeTicks::Now();
      break;
  }

  // Revoke any pending watchdog timeout task
  weak_factory_.InvalidateWeakPtrs();
}

void GpuWatchdogThreadImplV2::UpdateInitializationFlag() {
  in_gpu_initialization_ = false;
}

// Called from the gpu main thread.
// The watchdog is armed only in these three functions -
// GpuWatchdogThreadImplV2(), WillProcessTask(), and OnGpuProcessTearDown()
void GpuWatchdogThreadImplV2::Arm() {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());

  base::subtle::NoBarrier_AtomicIncrement(&arm_disarm_counter_, 1);

  // Arm/Disarm are always called in sequence. Now it's an odd number.
  DCHECK(IsArmed());
}

void GpuWatchdogThreadImplV2::Disarm() {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());

  base::subtle::NoBarrier_AtomicIncrement(&arm_disarm_counter_, 1);

  // Arm/Disarm are always called in sequence. Now it's an even number.
  DCHECK(!IsArmed());
}

void GpuWatchdogThreadImplV2::InProgress() {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());

  // Increment by 2. This is equivalent to Disarm() + Arm().
  base::subtle::NoBarrier_AtomicIncrement(&arm_disarm_counter_, 2);

  // Now it's an odd number.
  DCHECK(IsArmed());
}

bool GpuWatchdogThreadImplV2::IsArmed() {
  // It's an odd number.
  return base::subtle::NoBarrier_Load(&arm_disarm_counter_) & 1;
}

// Running on the watchdog thread.
void GpuWatchdogThreadImplV2::OnWatchdogTimeout() {
  DCHECK(watchdog_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(!is_backgrounded_);
  DCHECK(!in_power_suspension_);
  DCHECK(!is_paused_);

  // If this metric is added too early (eg. watchdog creation time), it cannot
  // be persistent. The histogram data will be lost after crash or browser exit.
  // Delay the recording of kGpuWatchdogStart until the firs
  // OnWatchdogTimeout() to ensure this metric is created in the persistent
  // memory.
  if (!is_watchdog_start_histogram_recorded) {
    is_watchdog_start_histogram_recorded = true;
    GpuWatchdogHistogram(GpuWatchdogThreadEvent::kGpuWatchdogStart);
  }

  base::subtle::Atomic32 arm_disarm_counter =
      base::subtle::NoBarrier_Load(&arm_disarm_counter_);
  GpuWatchdogTimeoutHistogram(GpuWatchdogTimeoutEvent::kTimeout);
  if (power_resumed_event_)
    num_of_timeout_after_power_resume_++;
  if (foregrounded_event_)
    num_of_timeout_after_foregrounded_++;

  // Collect all needed info for gpu hang detection.
  bool disarmed = arm_disarm_counter % 2 == 0;  // even number
  bool gpu_makes_progress = arm_disarm_counter != last_arm_disarm_counter_;
  bool watched_thread_needs_more_time =
      WatchedThreadNeedsMoreTime(disarmed || gpu_makes_progress);

  // No gpu hang is detected. Continue with another OnWatchdogTimeout task
  if (disarmed || gpu_makes_progress || watched_thread_needs_more_time) {
    last_on_watchdog_timeout_timeticks_ = base::TimeTicks::Now();
    last_arm_disarm_counter_ =
        base::subtle::NoBarrier_Load(&arm_disarm_counter_);

    task_runner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&GpuWatchdogThreadImplV2::OnWatchdogTimeout, weak_ptr_),
        watchdog_timeout_);
    return;
  }

  // An experiment for all platforms: Wait for max_wait_time_ and see if GPU
  // will response.
  GpuWatchdogTimeoutHistogram(GpuWatchdogTimeoutEvent::kTimeoutWait);
  if (GpuRespondsAfterWaiting()) {
    last_on_watchdog_timeout_timeticks_ = base::TimeTicks::Now();
    last_arm_disarm_counter_ =
        base::subtle::NoBarrier_Load(&arm_disarm_counter_);

    task_runner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&GpuWatchdogThreadImplV2::OnWatchdogTimeout, weak_ptr_),
        watchdog_timeout_);
    return;
  }

  // Still armed without any progress. GPU possibly hangs.
  GpuWatchdogTimeoutHistogram(GpuWatchdogTimeoutEvent::kKill);
  DeliberatelyTerminateToRecoverFromHang();
}

bool GpuWatchdogThreadImplV2::GpuIsAlive() {
  base::subtle::Atomic32 arm_disarm_counter =
      base::subtle::NoBarrier_Load(&arm_disarm_counter_);
  bool gpu_makes_progress = arm_disarm_counter != last_arm_disarm_counter_;

  return (gpu_makes_progress);
}

bool GpuWatchdogThreadImplV2::WatchedThreadNeedsMoreTime(
    bool no_gpu_hang_detected) {
#if defined(OS_WIN)
  if (!watched_thread_handle_)
    return false;

  // For metrics only - If count_of_more_gpu_thread_time_allowed_ > 0, we know
  // extra time was extended in the previous OnWatchdogTimeout(). Now we find
  // gpu makes progress. Record this case.
  if (no_gpu_hang_detected && count_of_more_gpu_thread_time_allowed_ > 0) {
    GpuWatchdogTimeoutHistogram(
        GpuWatchdogTimeoutEvent::kProgressAfterMoreThreadTime);
    WindowsNumOfExtraTimeoutsHistogram();
  }
  // For metrics only - The extra time was give in timeouts.
  time_in_extra_timeouts_ =
      count_of_more_gpu_thread_time_allowed_ * watchdog_timeout_;

  // Calculate how many thread ticks the watched thread spent doing the work.
  base::ThreadTicks now = GetWatchedThreadTime();
  base::TimeDelta thread_time_elapsed =
      now - last_on_watchdog_timeout_thread_ticks_;
  last_on_watchdog_timeout_thread_ticks_ = now;
  remaining_watched_thread_ticks_ -= thread_time_elapsed;

  if (no_gpu_hang_detected ||
      count_of_more_gpu_thread_time_allowed_ >=
          kMaxCountOfMoreGpuThreadTimeAllowed ||
      thread_time_elapsed < base::TimeDelta() /* bogus data */ ||
      remaining_watched_thread_ticks_ <= base::TimeDelta()) {
    // Reset the remaining thread ticks.
    remaining_watched_thread_ticks_ = watchdog_timeout_;
    count_of_more_gpu_thread_time_allowed_ = 0;
    return false;
  } else {
    count_of_more_gpu_thread_time_allowed_++;
    // Only record it once for all extenteded timeout on the same detected gpu
    // hang, so we know this is equivlent one crash in our crash reports.
    if (count_of_more_gpu_thread_time_allowed_ == 1)
      GpuWatchdogTimeoutHistogram(GpuWatchdogTimeoutEvent::kMoreThreadTime);

    return true;
  }
#else
  return false;
#endif
}

#if defined(OS_WIN)
base::ThreadTicks GpuWatchdogThreadImplV2::GetWatchedThreadTime() {
  DCHECK(watched_thread_handle_);

  if (base::ThreadTicks::IsSupported()) {
    // Note: GetForThread() might return bogus results if running on different
    // CPUs between two calls.
    return base::ThreadTicks::GetForThread(
        base::PlatformThreadHandle(watched_thread_handle_));
  } else {
    FILETIME creation_time;
    FILETIME exit_time;
    FILETIME kernel_time;
    FILETIME user_time;
    BOOL result = GetThreadTimes(watched_thread_handle_, &creation_time,
                                 &exit_time, &kernel_time, &user_time);
    if (!result)
      return base::ThreadTicks();

    // Need to bit_cast to fix alignment, then divide by 10 to convert
    // 100-nanoseconds to microseconds.
    int64_t user_time_us = bit_cast<int64_t, FILETIME>(user_time) / 10;
    int64_t kernel_time_us = bit_cast<int64_t, FILETIME>(kernel_time) / 10;

    return base::ThreadTicks() +
           base::TimeDelta::FromMicroseconds(user_time_us + kernel_time_us);
  }
}
#endif

// This is an experiment on all platforms to see whether GPU will response
// after waiting longer.
bool GpuWatchdogThreadImplV2::GpuRespondsAfterWaiting() {
  base::TimeDelta duration;
  base::TimeTicks start_timeticks = base::TimeTicks::Now();

  while (duration < max_wait_time_) {
    // Sleep for 1 seconds each time and check if the GPU makes a progress.
    base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(1));
    duration = base::TimeTicks::Now() - start_timeticks;

    if (GpuIsAlive()) {
      GpuWatchdogTimeoutHistogram(GpuWatchdogTimeoutEvent::kProgressAfterWait);
      GpuWatchdogWaitTimeHistogram(duration);
      return true;
    }
  }

  return false;
}

void GpuWatchdogThreadImplV2::DeliberatelyTerminateToRecoverFromHang() {
  DCHECK(watchdog_thread_task_runner_->BelongsToCurrentThread());
  // If this is for gpu testing, do not terminate the gpu process.
  if (is_test_mode_) {
    test_result_timeout_and_gpu_hang_.Set();
    return;
  }

#if defined(OS_WIN)
  if (IsDebuggerPresent())
    return;
#endif

  // Store variables so they're available in crash dumps to help determine the
  // cause of any hang.
  base::TimeTicks function_begin_timeticks = base::TimeTicks::Now();
  base::debug::Alias(&in_gpu_initialization_);
  base::debug::Alias(&num_of_timeout_after_power_resume_);
  base::debug::Alias(&num_of_timeout_after_foregrounded_);
  base::debug::Alias(&function_begin_timeticks);
  base::debug::Alias(&watchdog_start_timeticks_);
  base::debug::Alias(&power_suspend_timeticks_);
  base::debug::Alias(&power_resume_timeticks_);
  base::debug::Alias(&backgrounded_timeticks_);
  base::debug::Alias(&foregrounded_timeticks_);
  base::debug::Alias(&watchdog_pause_timeticks_);
  base::debug::Alias(&watchdog_resume_timeticks_);
  base::debug::Alias(&in_power_suspension_);
  base::debug::Alias(&in_gpu_process_teardown_);
  base::debug::Alias(&is_backgrounded_);
  base::debug::Alias(&is_add_power_observer_called_);
  base::debug::Alias(&is_power_observer_added_);
  base::debug::Alias(&last_on_watchdog_timeout_timeticks_);
  base::TimeDelta timeticks_elapses =
      function_begin_timeticks - last_on_watchdog_timeout_timeticks_;
  base::debug::Alias(&timeticks_elapses);
#if defined(OS_WIN)
  base::debug::Alias(&remaining_watched_thread_ticks_);
#endif

  GpuWatchdogHistogram(GpuWatchdogThreadEvent::kGpuWatchdogKill);

  crash_keys::gpu_watchdog_crashed_in_gpu_init.Set(
      in_gpu_initialization_ ? "1" : "0");

  crash_keys::gpu_watchdog_kill_after_power_resume.Set(
      WithinOneMinFromPowerResumed() ? "1" : "0");

  // Deliberately crash the process to create a crash dump.
  *((volatile int*)0) = 0xdeadface;
}

void GpuWatchdogThreadImplV2::GpuWatchdogHistogram(
    GpuWatchdogThreadEvent thread_event) {
  base::UmaHistogramEnumeration("GPU.WatchdogThread.Event.V2", thread_event);
  base::UmaHistogramEnumeration("GPU.WatchdogThread.Event", thread_event);
}

void GpuWatchdogThreadImplV2::GpuWatchdogTimeoutHistogram(
    GpuWatchdogTimeoutEvent timeout_event) {
  base::UmaHistogramEnumeration("GPU.WatchdogThread.Timeout", timeout_event);

  bool recorded = false;
  if (in_gpu_initialization_) {
    base::UmaHistogramEnumeration("GPU.WatchdogThread.Timeout.Init",
                                  timeout_event);
    recorded = true;
  }

  if (WithinOneMinFromPowerResumed()) {
    base::UmaHistogramEnumeration("GPU.WatchdogThread.Timeout.PowerResume",
                                  timeout_event);
    recorded = true;
  }

  if (WithinOneMinFromForegrounded()) {
    base::UmaHistogramEnumeration("GPU.WatchdogThread.Timeout.Foregrounded",
                                  timeout_event);
    recorded = true;
  }

  if (!recorded) {
    base::UmaHistogramEnumeration("GPU.WatchdogThread.Timeout.Normal",
                                  timeout_event);
  }
}

#if defined(OS_WIN)
void GpuWatchdogThreadImplV2::WindowsNumOfExtraTimeoutsHistogram() {
  // Record the number of timeouts the GPU main thread needs to make a progress
  // after GPU OnWatchdogTimeout() is triggered. The maximum count is 6 which
  // is more  than kMaxCountOfMoreGpuThreadTimeAllowed(4);
  constexpr int kMin = 1;
  constexpr int kMax = 6;
  constexpr int kBuckets = 6;
  int count = count_of_more_gpu_thread_time_allowed_;
  bool recorded = false;

  base::UmaHistogramCustomCounts("GPU.WatchdogThread.ExtraThreadTime", count,
                                 kMin, kMax, kBuckets);

  if (in_gpu_initialization_) {
    base::UmaHistogramCustomCounts("GPU.WatchdogThread.ExtraThreadTime.Init",
                                   count, kMin, kMax, kBuckets);
    recorded = true;
  }

  if (WithinOneMinFromPowerResumed()) {
    base::UmaHistogramCustomCounts(
        "GPU.WatchdogThread.ExtraThreadTime.PowerResume", count, kMin, kMax,
        kBuckets);
    recorded = true;
  }

  if (WithinOneMinFromForegrounded()) {
    base::UmaHistogramCustomCounts(
        "GPU.WatchdogThread.ExtraThreadTime.Foregrounded", count, kMin, kMax,
        kBuckets);
    recorded = true;
  }

  if (!recorded) {
    base::UmaHistogramCustomCounts("GPU.WatchdogThread.ExtraThreadTime.Normal",
                                   count, kMin, kMax, kBuckets);
  }
}
#endif

void GpuWatchdogThreadImplV2::GpuWatchdogWaitTimeHistogram(
    base::TimeDelta wait_time) {
#if defined(OS_WIN)
  // Add the time the GPU thread was given for full thread time.
  wait_time += time_in_extra_timeouts_;
#endif

  // Record the wait time in OnWatchdogTimeout() for the GPU main thread to
  // make a progress. The maximum recodrding time is 150 seconds because
  // Windows need to add the time spent before reaching here (max 60 sec).
  constexpr base::TimeDelta kMin = base::TimeDelta::FromSeconds(1);
  constexpr base::TimeDelta kMax = base::TimeDelta::FromSeconds(150);
  constexpr int kBuckets = 50;
  bool recorded = false;

  base::UmaHistogramCustomTimes("GPU.WatchdogThread.WaitTime", wait_time, kMin,
                                kMax, kBuckets);

  if (in_gpu_initialization_) {
    base::UmaHistogramCustomTimes("GPU.WatchdogThread.WaitTime.Init", wait_time,
                                  kMin, kMax, kBuckets);
    recorded = true;
  }

  if (WithinOneMinFromPowerResumed()) {
    base::UmaHistogramCustomTimes("GPU.WatchdogThread.WaitTime.PowerResume",
                                  wait_time, kMin, kMax, kBuckets);
    recorded = true;
  }

  if (WithinOneMinFromForegrounded()) {
    base::UmaHistogramCustomTimes("GPU.WatchdogThread.WaitTime.Foregrounded",
                                  wait_time, kMin, kMax, kBuckets);
    recorded = true;
  }

  if (!recorded) {
    base::UmaHistogramCustomTimes("GPU.WatchdogThread.WaitTime.Normal",
                                  wait_time, kMin, kMax, kBuckets);
  }
}

bool GpuWatchdogThreadImplV2::WithinOneMinFromPowerResumed() {
  size_t count = base::TimeDelta::FromSeconds(60) / watchdog_timeout_;
  return power_resumed_event_ && num_of_timeout_after_power_resume_ <= count;
}

bool GpuWatchdogThreadImplV2::WithinOneMinFromForegrounded() {
  size_t count = base::TimeDelta::FromSeconds(60) / watchdog_timeout_;
  return foregrounded_event_ && num_of_timeout_after_foregrounded_ <= count;
}

// For gpu testing only. Return whether a GPU hang was detected or not.
bool GpuWatchdogThreadImplV2::IsGpuHangDetectedForTesting() {
  DCHECK(is_test_mode_);
  return test_result_timeout_and_gpu_hang_.IsSet();
}

// This should be called on the test main thread only. It will wait until the
// power observer is added on the watchdog thread.
void GpuWatchdogThreadImplV2::WaitForPowerObserverAddedForTesting() {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());
  DCHECK(is_add_power_observer_called_);

  // Just return if it has been added.
  if (is_power_observer_added_)
    return;

  base::WaitableEvent event;
  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(&event)));
  event.Wait();
}

}  // namespace gpu
