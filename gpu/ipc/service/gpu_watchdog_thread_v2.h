// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_WATCHDOG_THREAD_V2_H_
#define GPU_IPC_SERVICE_GPU_WATCHDOG_THREAD_V2_H_

#include "gpu/ipc/service/gpu_watchdog_thread.h"

namespace gpu {

class GPU_IPC_SERVICE_EXPORT GpuWatchdogThreadImplV2
    : public GpuWatchdogThread,
      public base::MessageLoopCurrent::TaskObserver {
 public:
  static std::unique_ptr<GpuWatchdogThreadImplV2> Create(
      bool start_backgrounded);
  ~GpuWatchdogThreadImplV2() override;

  // Implements GpuWatchdogThread.
  void AddPowerObserver() override;
  void OnBackgrounded() override;
  void OnForegrounded() override;
  void OnInitComplete() override;

  // Implements gl::ProgressReporter.
  void ReportProgress() override;

 protected:
  // Implements base::Thread.
  void Init() override;
  void CleanUp() override;

 private:
  GpuWatchdogThreadImplV2();
  void Arm();
  void Disarm();
  void InProgress();
  void OnWatchdogTimeout();

  // Implements base::PowerObserver.
  void OnSuspend() override;
  void OnResume() override;

  // Implements MessageLoopCurrent::TaskObserver.
  void WillProcessTask(const base::PendingTask& pending_task) override;
  void DidProcessTask(const base::PendingTask& pending_task) override;

  // Implements GpuWatchdogThread.
  void DeliberatelyTerminateToRecoverFromHang() override;

  // This counter is only written on the gpu thread, and read on the watchdog
  // thread.
  base::subtle::Atomic32 arm_disarm_counter_ = 0;
  // The counter number read in the last OnWatchdogTimeout() on the watchdog
  // thread.
  int32_t last_arm_disarm_counter_ = 0;

  // Timeout on the watchdog thread to check if gpu hangs
  base::TimeDelta watchdog_timeout_;

  scoped_refptr<base::SingleThreadTaskRunner> watched_task_runner_;

  base::WeakPtrFactory<GpuWatchdogThreadImplV2> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuWatchdogThreadImplV2);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_WATCHDOG_THREAD_V2_H_
