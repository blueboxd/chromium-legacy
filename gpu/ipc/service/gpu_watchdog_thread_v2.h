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

  // Implements GpuWatchdogThread
  void AddPowerObserver() override;
  void OnBackgrounded() override;
  void OnForegrounded() override;

  // Implements gl::ProgressReporter
  void ReportProgress() override;

 protected:
  // Implements base::Thread
  void Init() override;
  void CleanUp() override;

 private:
  GpuWatchdogThreadImplV2();
  void Arm();
  void Disarm();

  // Implements base::PowerObserver
  void OnSuspend() override;
  void OnResume() override;

  // Implements MessageLoopCurrent::TaskObserver.
  void WillProcessTask(const base::PendingTask& pending_task) override;
  void DidProcessTask(const base::PendingTask& pending_task) override;

  // Implements GpuWatchdogThread
  void DeliberatelyTerminateToRecoverFromHang() override;

  base::TimeDelta timeout_;
  scoped_refptr<base::SingleThreadTaskRunner> watched_task_runner_;

  base::WeakPtrFactory<GpuWatchdogThreadImplV2> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuWatchdogThreadImplV2);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_WATCHDOG_THREAD_V2_H_
