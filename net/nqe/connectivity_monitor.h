// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_NQE_CONNECTIVITY_MONITOR_H_
#define NET_NQE_CONNECTIVITY_MONITOR_H_

#include <set>

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "net/url_request/url_request.h"

namespace net {

// ConnectivityMonitor is driven by NetworkQualityEstimator and is used to
// monitor progress of active URLRequests. If all active requests fail to make
// progress for a certain time interval, this will log accordingly and may
// report the problem to the operating system as a potential hint to fall back
// onto a more responsive network.
class NET_EXPORT_PRIVATE ConnectivityMonitor {
 public:
  // Constructs a new ConnectivityMonitor as below, but with builtin default
  // TimeDelta values.
  explicit ConnectivityMonitor();

  // Constructs a new ConnectivityMonitor which assumes the current network has
  // lost connectivity if it observes no request progress over a duration of at
  // least `inactivity_threshold`. This observation will only occur at most once
  // every `min_observation_interval`.
  ConnectivityMonitor(base::TimeDelta inactivity_threshold,
                      base::TimeDelta min_failure_logging_interval);
  ConnectivityMonitor(const ConnectivityMonitor&) = delete;
  ConnectivityMonitor& operator=(const ConnectivityMonitor&) = delete;
  ~ConnectivityMonitor();

  // Registers a new `request` to be tracked by the ConnectivityMonitor. Called
  // just before the request's first header bytes hit the wire.
  void TrackNewRequest(const URLRequest& request);

  // Notifies the ConnectivityMonitor that progress has been made toward
  // `request` completion. This means that some response bytes were received,
  // and for a newly tracked request, the first call to this method signifies
  // receipt of at least the first response header bytes.
  void NotifyRequestProgress(const URLRequest& request);

  // Indicates that `request` has been completed or is about to be destroyed,
  // regardless of success or failure. If `request` was being tracked by this
  // ConnectivityMonitor, it must no longer be tracked after this call.
  void NotifyRequestCompleted(const URLRequest& request);

  // Notifies the monitor of a change in the system's network configuration. As
  // an example, this may be called when an Android device switches its default
  // network from WiFi to mobile data.
  void NotifyConnectionTypeChanged(NetworkChangeNotifier::ConnectionType type);

  size_t num_active_requests_for_testing() const {
    return active_requests_.size();
  }

  // Returns the amount of time since the ConnectivityMonitor first observed the
  // current lapse in connectivity, if any.
  base::Optional<base::TimeDelta> GetTimeSinceLastFailureForTesting();

  // Registers a callback to hook into any time an activity deadline is reached.
  void SetNextDeadlineCallbackForTesting(base::OnceClosure callback);

  // Registers a callback to hook into the code path for OS reporting. Allows
  // tests to effectively observe the OS reporting event.
  void SetReportCallbackForTesting(base::OnceClosure callback);

 private:
  void ScheduleNextActivityDeadline(base::TimeDelta delay);
  void OnActivityDeadlineExceeded();
  void ReportConnectivityFailure();

  const base::TimeDelta inactivity_threshold_;
  const base::TimeDelta min_failure_logging_interval_;

  base::OnceClosure next_deadline_callback_for_testing_;
  base::OnceClosure report_callback_for_testing_;

  std::set<const URLRequest*> active_requests_;
  base::CancelableOnceClosure next_activity_deadline_;
  base::Optional<base::TimeTicks> time_last_failure_observed_;
};

}  // namespace net

#endif  // NET_NQE_CONNECTIVITY_MONITOR_H_
