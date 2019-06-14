// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_watchdog_thread_v2.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"

namespace gpu {

namespace {
#if defined(CYGPROFILE_INSTRUMENTATION)
const int kGpuTimeoutInMs = 30000;
#elif defined(OS_WIN) || defined(OS_MACOSX)
const int kGpuTimeoutInMs = 15000;
#else
const int kGpuTimeoutInMs = 10000;
#endif
}  // namespace

GpuWatchdogThreadImplV2::GpuWatchdogThreadImplV2()
    : timeout_(base::TimeDelta::FromMilliseconds(kGpuTimeoutInMs)),
      task_observer_(this),
      watched_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_factory_(this) {}

GpuWatchdogThreadImplV2::~GpuWatchdogThreadImplV2() {
  DCHECK(watched_task_runner_->BelongsToCurrentThread());
  Stop();
}

// static
std::unique_ptr<GpuWatchdogThreadImplV2> GpuWatchdogThreadImplV2::Create(
    bool start_backgrounded) {
  NOTREACHED();  // Not ready yet

  auto watchdog_thread = base::WrapUnique(new GpuWatchdogThreadImplV2);
  base::Thread::Options options;
  options.timer_slack = base::TIMER_SLACK_MAXIMUM;
  watchdog_thread->StartWithOptions(options);
  if (start_backgrounded)
    watchdog_thread->OnBackgrounded();
  return watchdog_thread;
}

void GpuWatchdogThreadImplV2::AddPowerObserver() {}

void GpuWatchdogThreadImplV2::OnBackgrounded() {}

void GpuWatchdogThreadImplV2::OnForegrounded() {}

void GpuWatchdogThreadImplV2::ReportProgress() {}

void GpuWatchdogThreadImplV2::Init() {}

void GpuWatchdogThreadImplV2::CleanUp() {
  weak_factory_.InvalidateWeakPtrs();
}

GpuWatchdogThreadImplV2::GpuWatchdogTaskObserver::GpuWatchdogTaskObserver(
    GpuWatchdogThreadImplV2* watchdog)
    : watchdog_(watchdog) {}

GpuWatchdogThreadImplV2::GpuWatchdogTaskObserver::~GpuWatchdogTaskObserver() =
    default;

void GpuWatchdogThreadImplV2::GpuWatchdogTaskObserver::WillProcessTask(
    const base::PendingTask& pending_task) {}

void GpuWatchdogThreadImplV2::GpuWatchdogTaskObserver::DidProcessTask(
    const base::PendingTask& pending_task) {
  watchdog_->Disarm();
}

void GpuWatchdogThreadImplV2::Arm() {}

void GpuWatchdogThreadImplV2::Disarm() {}

void GpuWatchdogThreadImplV2::OnSuspend() {}

void GpuWatchdogThreadImplV2::OnResume() {}

void GpuWatchdogThreadImplV2::DeliberatelyTerminateToRecoverFromHang() {
  // Store variables so they're available in crash dumps to help determine the
  // cause of any hang.

  // Deliberately crash the process to create a crash dump.
  *((volatile int*)0) = 0xdeadface;
}

}  // namespace gpu
