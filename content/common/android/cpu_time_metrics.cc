// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/android/cpu_time_metrics.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/message_loop/message_loop_current.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/process/process_metrics.h"
#include "base/task/task_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"

#include <memory>

namespace content {
namespace {

// Histogram macros expect an enum class with kMaxValue. Because
// content::ProcessType cannot be migrated to this style at the moment, we
// specify a separate version here. Keep in sync with content::ProcessType.
// TODO(eseckler): Replace with content::ProcessType after its migration.
enum class ProcessTypeForUma {
  kUnknown = 1,
  kBrowser,
  kRenderer,
  kPluginDeprecated,
  kWorkerDeprecated,
  kUtility,
  kZygote,
  kSandboxHelper,
  kGpu,
  kPpapiPlugin,
  kPpapiBroker,
  kMaxValue = kPpapiBroker,
};

static_assert(static_cast<int>(ProcessTypeForUma::kMaxValue) ==
                  PROCESS_TYPE_PPAPI_BROKER,
              "ProcessTypeForUma and CurrentProcessType() require updating");

ProcessTypeForUma CurrentProcessType() {
  std::string process_type =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessType);
  if (process_type.empty())
    return ProcessTypeForUma::kBrowser;
  if (process_type == switches::kRendererProcess)
    return ProcessTypeForUma::kRenderer;
  if (process_type == switches::kUtilityProcess)
    return ProcessTypeForUma::kUtility;
  if (process_type == switches::kSandboxIPCProcess)
    return ProcessTypeForUma::kSandboxHelper;
  if (process_type == switches::kGpuProcess)
    return ProcessTypeForUma::kGpu;
  if (process_type == switches::kPpapiPluginProcess)
    return ProcessTypeForUma::kPpapiPlugin;
  if (process_type == switches::kPpapiBrokerProcess)
    return ProcessTypeForUma::kPpapiBroker;
  NOTREACHED() << "Unexpected process type: " << process_type;
  return ProcessTypeForUma::kUnknown;
}

// Samples the process's CPU time after a specific number of task were executed
// on the current thread (process main). The number of tasks is a crude proxy
// for CPU activity within this process. We sample more frequently when the
// process is more active, thus ensuring we lose little CPU time attribution
// when the process is terminated, even after it was very active.
class ProcessCpuTimeTaskObserver : public base::TaskObserver {
 public:
  ProcessCpuTimeTaskObserver()
      : process_metrics_(base::ProcessMetrics::CreateCurrentProcessMetrics()),
        process_type_(CurrentProcessType()) {}

  void WillProcessTask(const base::PendingTask& pending_task,
                       bool was_blocked_or_low_priority) override {}

  void DidProcessTask(const base::PendingTask& pending_task) override {
    task_counter_++;
    if (task_counter_ == kReportAfterEveryNTasks) {
      CollectAndReportProcessCpuTime();
      task_counter_ = 0;
    }
  }

  void CollectAndReportProcessCpuTime() {
    // GetCumulativeCPUUsage() may return a negative value if sampling failed.
    base::TimeDelta cumulative_cpu_time =
        process_metrics_->GetCumulativeCPUUsage();
    base::TimeDelta cpu_time_delta = cumulative_cpu_time - reported_cpu_time_;
    if (cpu_time_delta > base::TimeDelta()) {
      UMA_HISTOGRAM_SCALED_ENUMERATION(
          "Power.CpuTimeSecondsPerProcessType", process_type_,
          cpu_time_delta.InMicroseconds(), base::Time::kMicrosecondsPerSecond);
      reported_cpu_time_ = cumulative_cpu_time;
    }
  }

 private:
  // Sample CPU time after every 100th task to balance overhead of sampling and
  // loss at process termination.
  static constexpr int kReportAfterEveryNTasks = 100;

  int task_counter_ = 0;
  std::unique_ptr<base::ProcessMetrics> process_metrics_;
  ProcessTypeForUma process_type_;
  base::TimeDelta reported_cpu_time_;
};

}  // namespace

void SetupCpuTimeMetrics() {
  static base::NoDestructor<ProcessCpuTimeTaskObserver> task_observer;
  base::MessageLoopCurrent::Get()->AddTaskObserver(task_observer.get());
}

}  // namespace content
