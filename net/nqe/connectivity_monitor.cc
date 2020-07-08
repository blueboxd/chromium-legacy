// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/nqe/connectivity_monitor.h"

#include <algorithm>
#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/base/features.h"

#if defined(OS_ANDROID)
#include "net/android/network_library.h"
#endif

namespace net {

namespace {

// The default time threshold (in milliseconds) of network inactivity after
// which a URLRequest is treated as a potential indication of connection
// failure.
constexpr base::FeatureParam<int> kDefaultInactivityThresholdMs{
    &features::kReportPoorConnectivity, "inactivity_threshold_ms", 2500};

// If the ConnectivityMonitor observes a potential connectivity problem, it will
// refrain from doing so again until either a network change has occurred or
// a specified time interval has elapsed. This is the default time interval for
// that behavior.
constexpr base::TimeDelta kDefaultMinFailureLoggingInterval{
    base::TimeDelta::FromSeconds(45)};

}  // namespace

ConnectivityMonitor::ConnectivityMonitor()
    : ConnectivityMonitor(base::TimeDelta::FromMilliseconds(
                              kDefaultInactivityThresholdMs.Get()),
                          kDefaultMinFailureLoggingInterval) {}

ConnectivityMonitor::ConnectivityMonitor(
    base::TimeDelta inactivity_threshold,
    base::TimeDelta min_failure_logging_interval)
    : inactivity_threshold_(inactivity_threshold),
      min_failure_logging_interval_(min_failure_logging_interval) {}

ConnectivityMonitor::~ConnectivityMonitor() = default;

void ConnectivityMonitor::TrackNewRequest(const URLRequest& request) {
  active_requests_.insert(&request);
  if (next_activity_deadline_.IsCancelled()) {
    // This must be the only active request, so start a new deadline timer.
    ScheduleNextActivityDeadline(inactivity_threshold_);
  }
}

void ConnectivityMonitor::NotifyRequestProgress(const URLRequest& request) {
  auto it = active_requests_.find(&request);
  if (it == active_requests_.end())
    return;

  ScheduleNextActivityDeadline(inactivity_threshold_);
}

void ConnectivityMonitor::NotifyRequestCompleted(const URLRequest& request) {
  // Stop tracking this request and cancel monitoring if it was the last one.
  active_requests_.erase(&request);
  if (active_requests_.empty())
    next_activity_deadline_.Cancel();
}

void ConnectivityMonitor::NotifyConnectionTypeChanged(
    NetworkChangeNotifier::ConnectionType type) {
  if (time_last_failure_observed_) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "NQE.ConnectivityMonitor.TimeToSwitchNetworks",
        base::TimeTicks::Now() - *time_last_failure_observed_);
  }

  active_requests_.clear();
  next_activity_deadline_.Cancel();
  time_last_failure_observed_.reset();
}

void ConnectivityMonitor::ScheduleNextActivityDeadline(base::TimeDelta delay) {
  next_activity_deadline_.Reset(
      base::BindOnce(&ConnectivityMonitor::OnActivityDeadlineExceeded,
                     base::Unretained(this)));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, next_activity_deadline_.callback(), delay);
}

void ConnectivityMonitor::OnActivityDeadlineExceeded() {
  if (active_requests_.empty())
    return;

  const base::TimeTicks now = base::TimeTicks::Now();
  if (time_last_failure_observed_ &&
      now - *time_last_failure_observed_ < min_failure_logging_interval_) {
    // We've already hit a connectivity failure too recently on this connection.
    // Don't do anything but reschedule a new deadline in case there's still no
    // network activity between now and then.
    ScheduleNextActivityDeadline(
        (*time_last_failure_observed_ + min_failure_logging_interval_) - now);
    return;
  }

  // If we reach this point, there must still be at least one active URLRequest,
  // and no URLRequests have made progress since this deadline was set. The
  // time elapsed since then must be at least |inactivity_threshold_|, thus we
  // consider this invocation to signal a network failure.
  time_last_failure_observed_ = now;
  if (next_deadline_callback_for_testing_)
    std::move(next_deadline_callback_for_testing_).Run();
  if (base::FeatureList::IsEnabled(features::kReportPoorConnectivity))
    ReportConnectivityFailure();
}

void ConnectivityMonitor::ReportConnectivityFailure() {
  DCHECK(base::FeatureList::IsEnabled(features::kReportPoorConnectivity));

  if (report_callback_for_testing_) {
    std::move(report_callback_for_testing_).Run();
    return;
  }

  // TODO(crbug.com/1079380): Actually inform the OS on platforms other than
  // Android as well.
  DLOG(ERROR) << "The current network appears to be unresponsive.";
#if defined(OS_ANDROID)
  net::android::ReportBadDefaultNetwork();
#endif
}

void ConnectivityMonitor::SetNextDeadlineCallbackForTesting(
    base::OnceClosure callback) {
  next_deadline_callback_for_testing_ = std::move(callback);
}

void ConnectivityMonitor::SetReportCallbackForTesting(
    base::OnceClosure callback) {
  report_callback_for_testing_ = std::move(callback);
}

base::Optional<base::TimeDelta>
ConnectivityMonitor::GetTimeSinceLastFailureForTesting() {
  if (!time_last_failure_observed_)
    return base::nullopt;

  return base::TimeTicks::Now() - *time_last_failure_observed_;
}

}  // namespace net
