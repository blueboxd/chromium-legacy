// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_watchdog_thread_v2.h"

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/debug/alias.h"
#include "base/message_loop/message_loop_current.h"
#include "base/power_monitor/power_monitor.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "gpu/config/gpu_crash_keys.h"

namespace gpu {

GpuWatchdogThreadImplV2::GpuWatchdogThreadImplV2(base::TimeDelta timeout,
                                                 bool is_test_mode)
    : watchdog_timeout_(timeout),
      is_test_mode_(is_test_mode),
      watched_gpu_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  base::MessageLoopCurrent::Get()->AddTaskObserver(this);
  Arm();
}

GpuWatchdogThreadImplV2::~GpuWatchdogThreadImplV2() {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());
  Stop();  // stop the watchdog thread

  base::MessageLoopCurrent::Get()->RemoveTaskObserver(this);
  base::PowerMonitor::RemoveObserver(this);
  GpuWatchdogHistogram(GpuWatchdogThreadEvent::kGpuWatchdogEnd);
}

// static
std::unique_ptr<GpuWatchdogThreadImplV2> GpuWatchdogThreadImplV2::Create(
    bool start_backgrounded,
    base::TimeDelta timeout,
    bool is_test_mode) {
  auto watchdog_thread =
      base::WrapUnique(new GpuWatchdogThreadImplV2(timeout, is_test_mode));
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
  return Create(start_backgrounded, kGpuWatchdogTimeout, false);
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

// Called from the gpu thread.
void GpuWatchdogThreadImplV2::OnBackgrounded() {
  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuWatchdogThreadImplV2::OnWatchdogBackgrounded,
                     base::Unretained(this)));
}

// Called from the gpu thread.
void GpuWatchdogThreadImplV2::OnForegrounded() {
  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuWatchdogThreadImplV2::OnWatchdogForegrounded,
                     base::Unretained(this)));
}

// Called from the gpu thread when gpu init has completed.
void GpuWatchdogThreadImplV2::OnInitComplete() {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());
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

// Running on the watchdog thread.
// On Linux, Init() will be called twice for Sandbox Initialization. The
// watchdog is stopped and then restarted in StartSandboxLinux(). Everything
// should be the same and continue after the second init().
void GpuWatchdogThreadImplV2::Init() {
  watchdog_thread_task_runner_ = base::ThreadTaskRunnerHandle::Get();

  // Get and Invalidate weak_ptr should be done on the watchdog thread only.
  weak_ptr_ = weak_factory_.GetWeakPtr();
  task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&GpuWatchdogThreadImplV2::OnWatchdogTimeout, weak_ptr_),
      watchdog_timeout_ * kInitFactor);

  last_arm_disarm_counter_ = base::subtle::NoBarrier_Load(&arm_disarm_counter_);
  watchdog_start_timeticks_ = base::TimeTicks::Now();
  last_on_watchdog_timeout_timeticks_ = watchdog_start_timeticks_;
  last_on_watchdog_timeout_time_ = base::Time::Now();

  GpuWatchdogHistogram(GpuWatchdogThreadEvent::kGpuWatchdogStart);
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
    const base::PendingTask& pending_task) {
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

// Running on the watchdog thread.
void GpuWatchdogThreadImplV2::OnSuspend() {
  DCHECK(watchdog_thread_task_runner_->BelongsToCurrentThread());
  in_power_suspension_ = true;
  // Revoke any pending watchdog timeout task
  weak_factory_.InvalidateWeakPtrs();
  suspend_timeticks_ = base::TimeTicks::Now();
}

// Running on the watchdog thread.
void GpuWatchdogThreadImplV2::OnResume() {
  DCHECK(watchdog_thread_task_runner_->BelongsToCurrentThread());

  in_power_suspension_ = false;
  RestartWatchdogTimeoutTask();
  resume_timeticks_ = base::TimeTicks::Now();
  is_first_timeout_after_power_resume = true;
}

// Running on the watchdog thread.
void GpuWatchdogThreadImplV2::OnAddPowerObserver() {
  DCHECK(watchdog_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(base::PowerMonitor::IsInitialized());

  is_power_observer_added_ = base::PowerMonitor::AddObserver(this);
}

// Running on the watchdog thread.
void GpuWatchdogThreadImplV2::OnWatchdogBackgrounded() {
  DCHECK(watchdog_thread_task_runner_->BelongsToCurrentThread());

  is_backgrounded_ = true;
  // Revoke any pending watchdog timeout task
  weak_factory_.InvalidateWeakPtrs();
  backgrounded_timeticks_ = base::TimeTicks::Now();
}

// Running on the watchdog thread.
void GpuWatchdogThreadImplV2::OnWatchdogForegrounded() {
  DCHECK(watchdog_thread_task_runner_->BelongsToCurrentThread());

  is_backgrounded_ = false;
  RestartWatchdogTimeoutTask();
  foregrounded_timeticks_ = base::TimeTicks::Now();
}

// Running on the watchdog thread.
void GpuWatchdogThreadImplV2::RestartWatchdogTimeoutTask() {
  DCHECK(watchdog_thread_task_runner_->BelongsToCurrentThread());

  if (!is_backgrounded_ && !in_power_suspension_) {
    // Make the timeout twice long. The system/gpu might be very slow right
    // after resume or foregrounded.
    weak_ptr_ = weak_factory_.GetWeakPtr();
    task_runner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&GpuWatchdogThreadImplV2::OnWatchdogTimeout, weak_ptr_),
        watchdog_timeout_ * kRestartFactor);
    last_on_watchdog_timeout_timeticks_ = base::TimeTicks::Now();
    last_on_watchdog_timeout_time_ = base::Time::Now();
  }
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
  base::subtle::Atomic32 arm_disarm_counter =
      base::subtle::NoBarrier_Load(&arm_disarm_counter_);

  // disarmed is true if it's an even number.
  bool disarmed = arm_disarm_counter % 2 == 0;
  bool gpu_makes_progress = arm_disarm_counter != last_arm_disarm_counter_;
  last_arm_disarm_counter_ = arm_disarm_counter;

  // No gpu hang is detected. Continue with another OnWatchdogTimeout
  if (disarmed || gpu_makes_progress) {
    last_on_watchdog_timeout_timeticks_ = base::TimeTicks::Now();
    last_on_watchdog_timeout_time_ = base::Time::Now();
    is_first_timeout_after_power_resume = false;

    task_runner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&GpuWatchdogThreadImplV2::OnWatchdogTimeout, weak_ptr_),
        watchdog_timeout_);
    return;
  }

  // Still armed without any progress. GPU possibly hangs.
  DeliberatelyTerminateToRecoverFromHang();
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
  base::TimeTicks current_timeticks = base::TimeTicks::Now();
  base::debug::Alias(&current_timeticks);
  base::debug::Alias(&watchdog_start_timeticks_);
  base::debug::Alias(&suspend_timeticks_);
  base::debug::Alias(&resume_timeticks_);
  base::debug::Alias(&backgrounded_timeticks_);
  base::debug::Alias(&foregrounded_timeticks_);
  base::debug::Alias(&in_power_suspension_);
  base::debug::Alias(&in_gpu_process_teardown_);
  base::debug::Alias(&is_backgrounded_);
  base::debug::Alias(&is_add_power_observer_called_);
  base::debug::Alias(&is_power_observer_added_);
  base::debug::Alias(&last_on_watchdog_timeout_timeticks_);
  base::TimeDelta timeticks_elapses =
      current_timeticks - last_on_watchdog_timeout_timeticks_;
  base::debug::Alias(&timeticks_elapses);

  // If clock_time_elapses is much longer than time_elapses, it might be a sign
  // of a busy system.
  base::Time current_time = base::Time::Now();
  base::TimeDelta time_elapses = current_time - last_on_watchdog_timeout_time_;
  base::debug::Alias(&current_time);
  base::debug::Alias(&last_on_watchdog_timeout_time_);
  base::debug::Alias(&time_elapses);

  GpuWatchdogHistogram(GpuWatchdogThreadEvent::kGpuWatchdogKill);

  crash_keys::gpu_watchdog_kill_after_power_resume.Set(
      is_first_timeout_after_power_resume ? "1" : "0");

  // Deliberately crash the process to create a crash dump.
  *((volatile int*)0) = 0xdeadface;
}

void GpuWatchdogThreadImplV2::GpuWatchdogHistogram(
    GpuWatchdogThreadEvent thread_event) {
  UMA_HISTOGRAM_ENUMERATION("GPU.WatchdogThread.Event.V2", thread_event);
  UMA_HISTOGRAM_ENUMERATION("GPU.WatchdogThread.Event", thread_event);
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
