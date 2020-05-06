// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_WATCHDOG_THREAD_H_
#define GPU_IPC_SERVICE_GPU_WATCHDOG_THREAD_H_

#include "base/atomicops.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/power_monitor/power_observer.h"
#include "base/task/task_observer.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "gpu/ipc/common/gpu_watchdog_timeout.h"
#include "gpu/ipc/service/gpu_ipc_service_export.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/progress_reporter.h"

namespace gpu {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GpuWatchdogThreadEvent {
  kGpuWatchdogStart,
  kGpuWatchdogKill,
  kGpuWatchdogEnd,
  kMaxValue = kGpuWatchdogEnd,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GpuWatchdogTimeoutEvent {
  // Recorded each time OnWatchdogTimeout() is called.
  kTimeout,
  // Recorded when a GPU main thread is killed for a detected hang.
  kKill,
  // Window only: Recorded when a hang is detected but we allow the GPU main
  // thread to continue until it spent the full
  // thread time doing the work.
  kMoreThreadTime,
  // Windows only: The GPU makes progress after givenmore thread time. The GPU
  // main thread is not killed.
  kProgressAfterMoreThreadTime,
  // A gpu hang is detected but watchdog waits for 60 seconds before taking
  // action.
  kTimeoutWait,
  // The GPU makes progress within 60 sec in OnWatchdogTimeout(). The GPU main
  // thread is not killed.
  kProgressAfterWait,
  // Just continue if it's not on the TTY of our host X11 server.
  kContinueOnNonHostServerTty,
  // Windows only: After detecting GPU hang and continuing running through
  // OnGpuWatchdogTimeout for the max cycles, the GPU main thread still cannot
  // get the full thread time.
  kLessThanFullThreadTimeAfterCapped,
  // Windows only: The GPU main thread went through the
  // kLessThanFullThreadTimeAfterCapped stage before the process is killed.
  kKillOnLessThreadTime,
  // OnWatchdogTimeout() is called long after the expected time. The GPU is not
  // killed this time because of the slow system.
  kSlowWatchdogThread,
  kMaxValue = kSlowWatchdogThread,
};

// A thread that intermitently sends tasks to a group of watched message loops
// and deliberately crashes if one of them does not respond after a timeout.
class GPU_IPC_SERVICE_EXPORT GpuWatchdogThread : public base::Thread,
                                                 public base::PowerObserver,
                                                 public gl::ProgressReporter {
 public:
  ~GpuWatchdogThread() override;

  // Must be called after a PowerMonitor has been created. Can be called from
  // any thread.
  virtual void AddPowerObserver() = 0;

  // Notifies the watchdog when Chrome is backgrounded / foregrounded. Should
  // only be used if Chrome is completely backgrounded and not expected to
  // render (all windows backgrounded and not producing frames).
  virtual void OnBackgrounded() = 0;
  virtual void OnForegrounded() = 0;

  // The watchdog starts armed to catch startup hangs, and needs to be disarmed
  // once init is complete, before executing tasks.
  virtual void OnInitComplete() = 0;

  // Notifies the watchdog when the GPU child process is being destroyed.
  // This function is called directly from
  // viz::GpuServiceImpl::~GpuServiceImpl()
  virtual void OnGpuProcessTearDown() = 0;

  // Pause the GPU watchdog to stop the timeout task. If the current heavy task
  // is not running on the GPU driver, the watchdog can be paused to avoid
  // unneeded crash.
  virtual void PauseWatchdog() = 0;
  // Continue the watchdog after a pause.
  virtual void ResumeWatchdog() = 0;

  virtual void GpuWatchdogHistogram(GpuWatchdogThreadEvent thread_event) = 0;

  // For gpu testing only. Return status for the watchdog tests
  virtual bool IsGpuHangDetectedForTesting() = 0;

  virtual void WaitForPowerObserverAddedForTesting() {}

 protected:
  GpuWatchdogThread();

 private:
  DISALLOW_COPY_AND_ASSIGN(GpuWatchdogThread);
};

class GPU_IPC_SERVICE_EXPORT GpuWatchdogThreadImplV1
    : public GpuWatchdogThread {
 public:
  ~GpuWatchdogThreadImplV1() override;

  static std::unique_ptr<GpuWatchdogThreadImplV1> Create(
      bool start_backgrounded);

  // Implements GpuWatchdogThread.
  void AddPowerObserver() override;
  void OnBackgrounded() override;
  void OnForegrounded() override;
  void OnInitComplete() override {}
  void OnGpuProcessTearDown() override {}
  void ResumeWatchdog() override {}
  void PauseWatchdog() override {}
  void GpuWatchdogHistogram(GpuWatchdogThreadEvent thread_event) override;
  bool IsGpuHangDetectedForTesting() override;

  // gl::ProgressReporter implementation:
  void ReportProgress() override;

 protected:
  void Init() override;
  void CleanUp() override;

 private:
  // An object of this type intercepts the reception and completion of all tasks
  // on the watched thread and checks whether the watchdog is armed.
  class GpuWatchdogTaskObserver : public base::TaskObserver {
   public:
    explicit GpuWatchdogTaskObserver(GpuWatchdogThreadImplV1* watchdog);
    ~GpuWatchdogTaskObserver() override;

    // Implements TaskObserver.
    void WillProcessTask(const base::PendingTask& pending_task,
                         bool was_blocked_or_low_priority) override;
    void DidProcessTask(const base::PendingTask& pending_task) override;

   private:
    GpuWatchdogThreadImplV1* watchdog_;
  };

  // A helper class which allows multiple clients to suspend/resume the
  // watchdog thread. As we need to suspend resume on both background /
  // foreground events as well as power events, this class manages a ref-count
  // of suspend requests.
  class SuspensionCounter {
   public:
    SuspensionCounter(GpuWatchdogThreadImplV1* watchdog_thread);

    class SuspensionCounterRef {
     public:
      explicit SuspensionCounterRef(SuspensionCounter* counter);
      ~SuspensionCounterRef();

     private:
      SuspensionCounter* counter_;
    };

    // This class must outlive SuspensionCounterRefs.
    std::unique_ptr<SuspensionCounterRef> Take();

    // Used to update the |watchdog_thread_sequence_checker_|.
    void OnWatchdogThreadStopped();

    bool HasRefs() const;

   private:
    void OnAddRef();
    void OnReleaseRef();
    GpuWatchdogThreadImplV1* watchdog_thread_;
    uint32_t suspend_count_ = 0;

    SEQUENCE_CHECKER(watchdog_thread_sequence_checker_);
  };
  GpuWatchdogThreadImplV1();

  void CheckArmed();

  void OnAcknowledge();
  void OnCheck(bool after_suspend);
  void OnCheckTimeout();
  // Do not change the function name. It is used for [GPU HANG] carsh reports.
  void DeliberatelyTerminateToRecoverFromHang();

  void OnAddPowerObserver();

  // Implement PowerObserver.
  void OnSuspend() override;
  void OnResume() override;

  // Handle background/foreground.
  void OnBackgroundedOnWatchdogThread();
  void OnForegroundedOnWatchdogThread();

  void SuspendStateChanged();

#if defined(OS_WIN)
  base::ThreadTicks GetWatchedThreadTime();
#endif

#if defined(USE_X11)
  void UpdateActiveTTY();
#endif

  scoped_refptr<base::SingleThreadTaskRunner> watched_task_runner_;
  base::TimeDelta timeout_;
  bool armed_;
  GpuWatchdogTaskObserver task_observer_;

  // |awaiting_acknowledge_| is only ever read on the watched thread, but may
  // be modified on either the watched or watchdog thread. Reads/writes should
  // be careful to ensure that appropriate synchronization is used.
  base::subtle::Atomic32 awaiting_acknowledge_;

  // True if the watchdog should wait for a certain amount of CPU to be used
  // before killing the process.
  bool use_thread_cpu_time_;

  // The number of consecutive acknowledgements that had a latency less than
  // 50ms.
  int responsive_acknowledge_count_;

#if defined(OS_WIN)
  void* watched_thread_handle_;
  base::ThreadTicks arm_cpu_time_;

  // This measures the time that the system has been running, in units of 100
  // ns.
  ULONGLONG arm_interrupt_time_;
#endif

  // Time after which it's assumed that the computer has been suspended since
  // the task was posted.
  base::Time suspension_timeout_;

  SuspensionCounter suspension_counter_;
  std::unique_ptr<SuspensionCounter::SuspensionCounterRef> power_suspend_ref_;
  std::unique_ptr<SuspensionCounter::SuspensionCounterRef>
      background_suspend_ref_;

  // The time the last OnSuspend and OnResume was called.
  base::Time suspend_time_;
  base::Time resume_time_;

  // This is the time the last check was sent.
  base::Time check_time_;
  base::TimeTicks check_timeticks_;

  // The time in the last OnCheckTimeout()
  base::TimeTicks last_timeout_timeticks_;

  // After GPU hang detected, whether the GPU thread is allowed to continue due
  // to not spending enough thread time.
  bool more_gpu_thread_time_allowed_ = false;

  // whether GpuWatchdogThreadEvent::kGpuWatchdogStart has been recorded.
  bool is_watchdog_start_histogram_recorded = false;

#if defined(USE_X11)
  FILE* tty_file_;
  int host_tty_;
  int active_tty_ = -1;
  int last_active_tty_ = -1;
#endif

  base::WeakPtrFactory<GpuWatchdogThreadImplV1> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GpuWatchdogThreadImplV1);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_WATCHDOG_THREAD_H_
