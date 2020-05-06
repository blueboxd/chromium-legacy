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
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop_current.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/native_library.h"
#include "base/power_monitor/power_monitor.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "gpu/config/gpu_crash_keys.h"
#include "gpu/config/gpu_finch_features.h"

namespace gpu {

GpuWatchdogThreadImplV2::GpuWatchdogThreadImplV2(
    base::TimeDelta timeout,
    int max_extra_cycles_before_kill,
    bool is_test_mode)
    : watchdog_timeout_(timeout),
      in_gpu_initialization_(true),
      max_extra_cycles_before_kill_(max_extra_cycles_before_kill),
      is_test_mode_(is_test_mode),
      watched_gpu_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  base::MessageLoopCurrent::Get()->AddTaskObserver(this);
  num_of_processors_ = base::SysInfo::NumberOfProcessors();

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

#if defined(USE_X11)
  tty_file_ = base::OpenFile(
      base::FilePath(FILE_PATH_LITERAL("/sys/class/tty/tty0/active")), "r");
  UpdateActiveTTY();
  host_tty_ = active_tty_;
#endif

  Arm();
}

GpuWatchdogThreadImplV2::~GpuWatchdogThreadImplV2() {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());
  // Stop() might take too long and the watchdog timeout is triggered.
  // Disarm first before calling Stop() to avoid a crash.
  if (IsArmed())
    Disarm();
  PauseWatchdog();

  Stop();  // stop the watchdog thread

  base::MessageLoopCurrent::Get()->RemoveTaskObserver(this);
  base::PowerMonitor::RemoveObserver(this);
  GpuWatchdogHistogram(GpuWatchdogThreadEvent::kGpuWatchdogEnd);
#if defined(OS_WIN)
  if (watched_thread_handle_)
    CloseHandle(watched_thread_handle_);
#endif

#if defined(USE_X11)
  if (tty_file_)
    fclose(tty_file_);
#endif
}

// static
std::unique_ptr<GpuWatchdogThreadImplV2> GpuWatchdogThreadImplV2::Create(
    bool start_backgrounded,
    base::TimeDelta timeout,
    int max_extra_cycles_before_kill,
    bool is_test_mode) {
  auto watchdog_thread = base::WrapUnique(new GpuWatchdogThreadImplV2(
      timeout, max_extra_cycles_before_kill, is_test_mode));
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
  base::TimeDelta gpu_watchdog_timeout = kGpuWatchdogTimeout;
  int max_extra_cycles_before_kill = kMaxExtraCyclesBeforeKill;

  if (base::FeatureList::IsEnabled(features::kGpuWatchdogV2NewTimeout)) {
    const char kNewTimeOutParam[] = "new_time_out";
    const char kMaxExtraCyclesBeforeKillParam[] =
        "max_extra_cycles_before_kill";
#if defined(OS_WIN) || defined(OS_MACOSX)
    constexpr int kFinchMaxExtraCyclesBeforeKill = 1;
#else
    constexpr int kFinchMaxExtraCyclesBeforeKill = 2;
#endif

    int timeout = base::GetFieldTrialParamByFeatureAsInt(
        features::kGpuWatchdogV2NewTimeout, kNewTimeOutParam,
        kGpuWatchdogTimeout.InSeconds());
    gpu_watchdog_timeout = base::TimeDelta::FromSeconds(timeout);

    max_extra_cycles_before_kill = base::GetFieldTrialParamByFeatureAsInt(
        features::kGpuWatchdogV2NewTimeout, kMaxExtraCyclesBeforeKillParam,
        kFinchMaxExtraCyclesBeforeKill);
  }

  return Create(start_backgrounded, gpu_watchdog_timeout,
                max_extra_cycles_before_kill, false);
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

  last_arm_disarm_counter_ = ReadArmDisarmCounter();
  watchdog_start_timeticks_ = base::TimeTicks::Now();
  last_on_watchdog_timeout_timeticks_ = watchdog_start_timeticks_;
  next_on_watchdog_timeout_time_ = base::Time::Now() + timeout;

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
    next_on_watchdog_timeout_time_ = base::Time::Now() + timeout;
    last_arm_disarm_counter_ = ReadArmDisarmCounter();
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

base::subtle::Atomic32 GpuWatchdogThreadImplV2::ReadArmDisarmCounter() {
  return base::subtle::NoBarrier_Load(&arm_disarm_counter_);
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

  auto arm_disarm_counter = ReadArmDisarmCounter();
  GpuWatchdogTimeoutHistogram(GpuWatchdogTimeoutEvent::kTimeout);
  if (power_resumed_event_)
    num_of_timeout_after_power_resume_++;
  if (foregrounded_event_)
    num_of_timeout_after_foregrounded_++;

#if defined(USE_X11)
  UpdateActiveTTY();
#endif

  // Collect all needed info for gpu hang detection.
  bool disarmed = arm_disarm_counter % 2 == 0;  // even number
  bool gpu_makes_progress = arm_disarm_counter != last_arm_disarm_counter_;
  bool no_gpu_hang = disarmed || gpu_makes_progress || SlowWatchdogThread();

  bool watched_thread_needs_more_time =
      WatchedThreadNeedsMoreThreadTime(no_gpu_hang);
  no_gpu_hang = no_gpu_hang || watched_thread_needs_more_time ||
                ContinueOnNonHostX11ServerTty();

  bool allows_extra_timeout = WatchedThreadGetsExtraTimeout(no_gpu_hang);
  no_gpu_hang = no_gpu_hang || allows_extra_timeout;

  // No gpu hang. Continue with another OnWatchdogTimeout task.
  if (no_gpu_hang) {
    last_on_watchdog_timeout_timeticks_ = base::TimeTicks::Now();
    next_on_watchdog_timeout_time_ = base::Time::Now() + watchdog_timeout_;
    last_arm_disarm_counter_ = ReadArmDisarmCounter();

    task_runner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&GpuWatchdogThreadImplV2::OnWatchdogTimeout, weak_ptr_),
        watchdog_timeout_);
    return;
  }

  // Still armed without any progress. GPU possibly hangs.
  GpuWatchdogTimeoutHistogram(GpuWatchdogTimeoutEvent::kKill);
#if defined(OS_WIN)
  if (less_than_full_thread_time_after_capped_)
    GpuWatchdogTimeoutHistogram(GpuWatchdogTimeoutEvent::kKillOnLessThreadTime);
#endif

  DeliberatelyTerminateToRecoverFromHang();
}

bool GpuWatchdogThreadImplV2::SlowWatchdogThread() {
  // If it takes 15 more seconds than the expected time between two
  // OnWatchdogTimeout() calls, the system is considered slow and it's not a GPU
  // hang.
  bool slow_watchdog_thread =
      (base::Time::Now() - next_on_watchdog_timeout_time_) >=
      base::TimeDelta::FromSeconds(15);

  // Record this case only when a GPU hang is detected and the thread is slow.
  if (slow_watchdog_thread)
    GpuWatchdogTimeoutHistogram(GpuWatchdogTimeoutEvent::kSlowWatchdogThread);

  return slow_watchdog_thread;
}

bool GpuWatchdogThreadImplV2::WatchedThreadNeedsMoreThreadTime(
    bool no_gpu_hang_detected) {
#if defined(OS_WIN)
  if (!watched_thread_handle_)
    return false;

  // We allow extra thread time. When that runs out, we extend extra timeout
  // cycles. Now, we are extending extra timeout cycles. Don't add extra thread
  // time.
  if (count_of_extra_cycles_ > 0)
    return false;

  WatchedThreadNeedsMoreThreadTimeHistogram(
      no_gpu_hang_detected,
      /*start_of_more_thread_time*/ false);

  if (!no_gpu_hang_detected && count_of_more_gpu_thread_time_allowed_ >=
                                   kMaxCountOfMoreGpuThreadTimeAllowed) {
    less_than_full_thread_time_after_capped_ = true;
  } else {
    less_than_full_thread_time_after_capped_ = false;
  }

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
    // This is the start of allowing more thread time.
    if (count_of_more_gpu_thread_time_allowed_ == 0) {
      WatchedThreadNeedsMoreThreadTimeHistogram(
          no_gpu_hang_detected, /*start_of_more_thread_time*/ true);
    }
    count_of_more_gpu_thread_time_allowed_++;

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

bool GpuWatchdogThreadImplV2::WatchedThreadGetsExtraTimeout(bool no_gpu_hang) {
  if (max_extra_cycles_before_kill_ == 0)
    return false;

  // We want to record histograms even if there is no gpu hang.
  bool allows_more_timeouts = false;
  WatchedThreadGetsExtraTimeoutHistogram(no_gpu_hang);

  if (no_gpu_hang) {
    if (count_of_extra_cycles_ > 0) {
      count_of_extra_cycles_ = 0;
    }
  } else if (count_of_extra_cycles_ < max_extra_cycles_before_kill_) {
    count_of_extra_cycles_++;
    allows_more_timeouts = true;
  }

  return allows_more_timeouts;
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
  base::debug::Alias(&max_extra_cycles_before_kill_);
#if defined(OS_WIN)
  base::debug::Alias(&remaining_watched_thread_ticks_);
  base::debug::Alias(&less_than_full_thread_time_after_capped_);
#endif

  GpuWatchdogHistogram(GpuWatchdogThreadEvent::kGpuWatchdogKill);

  crash_keys::gpu_watchdog_crashed_in_gpu_init.Set(
      in_gpu_initialization_ ? "1" : "0");

  crash_keys::gpu_watchdog_kill_after_power_resume.Set(
      WithinOneMinFromPowerResumed() ? "1" : "0");

  crash_keys::num_of_processors.Set(base::NumberToString(num_of_processors_));

  // Check the arm_disarm_counter value one more time.
  auto last_arm_disarm_counter = ReadArmDisarmCounter();
  base::debug::Alias(&last_arm_disarm_counter);

  // Deliberately crash the process to create a crash dump.
  *static_cast<volatile int*>(nullptr) = 0x1337;
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
void GpuWatchdogThreadImplV2::RecordExtraThreadTimeHistogram() {
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

void GpuWatchdogThreadImplV2::
    RecordNumOfUsersWaitingWithExtraThreadTimeHistogram(int count) {
  constexpr int kMax = 4;

  base::UmaHistogramExactLinear("GPU.WatchdogThread.ExtraThreadTime.NumOfUsers",
                                count, kMax);
}

void GpuWatchdogThreadImplV2::WatchedThreadNeedsMoreThreadTimeHistogram(
    bool no_gpu_hang_detected,
    bool start_of_more_thread_time) {
  if (start_of_more_thread_time) {
    // This is the start of allowing more thread time. Only record it once for
    // all following timeouts on the same detected gpu hang, so we know this
    // is equivlent one crash in our crash reports.
    GpuWatchdogTimeoutHistogram(GpuWatchdogTimeoutEvent::kMoreThreadTime);
    RecordNumOfUsersWaitingWithExtraThreadTimeHistogram(0);
  } else {
    if (count_of_more_gpu_thread_time_allowed_ > 0) {
      if (no_gpu_hang_detected) {
        // If count_of_more_gpu_thread_time_allowed_ > 0, we know extra time was
        // extended in the previous OnWatchdogTimeout(). Now we find gpu makes
        // progress. Record this case.
        GpuWatchdogTimeoutHistogram(
            GpuWatchdogTimeoutEvent::kProgressAfterMoreThreadTime);
        RecordExtraThreadTimeHistogram();
      } else {
        if (count_of_more_gpu_thread_time_allowed_ >=
            kMaxCountOfMoreGpuThreadTimeAllowed) {
          GpuWatchdogTimeoutHistogram(
              GpuWatchdogTimeoutEvent::kLessThanFullThreadTimeAfterCapped);
        }
      }

      // Records the number of users who are still waiting. We can use this
      // number to calculate the number of users who had already quit.
      RecordNumOfUsersWaitingWithExtraThreadTimeHistogram(
          count_of_more_gpu_thread_time_allowed_);

      // Used by GPU.WatchdogThread.WaitTime later
      time_in_wait_for_full_thread_time_ =
          count_of_more_gpu_thread_time_allowed_ * watchdog_timeout_;
    }
  }
}
#endif

void GpuWatchdogThreadImplV2::WatchedThreadGetsExtraTimeoutHistogram(
    bool no_gpu_hang) {
  constexpr int kMax = 60;
  if (count_of_extra_cycles_ == 0 && !no_gpu_hang) {
    GpuWatchdogTimeoutHistogram(GpuWatchdogTimeoutEvent::kTimeoutWait);
    base::UmaHistogramExactLinear("GPU.WatchdogThread.WaitTime.NumOfUsers", 0,
                                  kMax);
  } else if (count_of_extra_cycles_ > 0) {
    int count = watchdog_timeout_.InSeconds() * count_of_extra_cycles_;
    base::UmaHistogramExactLinear("GPU.WatchdogThread.WaitTime.NumOfUsers",
                                  count, kMax);
    if (no_gpu_hang) {
      GpuWatchdogTimeoutHistogram(GpuWatchdogTimeoutEvent::kProgressAfterWait);
      base::UmaHistogramExactLinear(
          "GPU.WatchdogThread.WaitTime.ProgressAfterWait", count, kMax);

#if defined(OS_WIN)
      // Add the time the GPU thread was given for the full thread time up to 60
      // seconds. GPU.WatchdogThread.WaitTime is essentially equal to
      // GPU.WatchdogThread.WaitTime.ProgressAfterWait on non-Windows systems.
      base::TimeDelta wait_time = base::TimeDelta::FromSeconds(count);
      wait_time += time_in_wait_for_full_thread_time_;

      constexpr base::TimeDelta kMinTime = base::TimeDelta::FromSeconds(1);
      constexpr base::TimeDelta kMaxTime = base::TimeDelta::FromSeconds(150);
      constexpr int kBuckets = 50;

      // The time the GPU main thread takes to finish a task after a "hang" is
      // dectedted.
      base::UmaHistogramCustomTimes("GPU.WatchdogThread.WaitTime", wait_time,
                                    kMinTime, kMaxTime, kBuckets);
#endif
    }
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

#if defined(USE_X11)
void GpuWatchdogThreadImplV2::UpdateActiveTTY() {
  last_active_tty_ = active_tty_;

  active_tty_ = -1;
  char tty_string[8] = {0};
  if (tty_file_ && !fseek(tty_file_, 0, SEEK_SET) &&
      fread(tty_string, 1, 7, tty_file_)) {
    int tty_number;
    if (sscanf(tty_string, "tty%d\n", &tty_number) == 1) {
      active_tty_ = tty_number;
    }
  }
}
#endif

bool GpuWatchdogThreadImplV2::ContinueOnNonHostX11ServerTty() {
#if defined(USE_X11)
  if (host_tty_ == -1 || active_tty_ == -1)
    return false;

  // Don't crash if we're not on the TTY of our host X11 server.
  if (active_tty_ != host_tty_) {
    // Only record for the time there is a change on TTY
    if (last_active_tty_ == active_tty_) {
      GpuWatchdogTimeoutHistogram(
          GpuWatchdogTimeoutEvent::kContinueOnNonHostServerTty);
    }
    return true;
  }
#endif
  return false;
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
