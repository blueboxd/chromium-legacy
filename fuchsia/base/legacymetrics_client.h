// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_BASE_LEGACYMETRICS_CLIENT_H_
#define FUCHSIA_BASE_LEGACYMETRICS_CLIENT_H_

#include <fuchsia/legacymetrics/cpp/fidl.h>

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "fuchsia/base/legacymetrics_user_event_recorder.h"

namespace cr_fuchsia {

// Used to report events & histogram data to the
// fuchsia.legacymetrics.MetricsRecorder service.
// LegacyMetricsClient must be Start()ed on an IO-capable sequence.
// Cannot be used in conjunction with other metrics reporting services.
// Must be constructed, used, and destroyed on the same sequence.
class LegacyMetricsClient {
 public:
  using ReportAdditionalMetricsCallback = base::RepeatingCallback<void(
      std::vector<fuchsia::legacymetrics::Event>*)>;

  LegacyMetricsClient();

  ~LegacyMetricsClient();

  explicit LegacyMetricsClient(const LegacyMetricsClient&) = delete;
  LegacyMetricsClient& operator=(const LegacyMetricsClient&) = delete;

  // Starts buffering data and schedules metric reporting after every
  // |report_interval|.
  void Start(base::TimeDelta report_interval);

  // Sets a |callback| to be invoked just prior to reporting, allowing users to
  // report additional custom metrics.
  // Must be called before Start().
  void SetReportAdditionalMetricsCallback(
      ReportAdditionalMetricsCallback callback);

 private:
  void ScheduleNextReport();
  void Report();
  void OnMetricsRecorderDisconnected(zx_status_t status);

  base::TimeDelta report_interval_;
  ReportAdditionalMetricsCallback report_additional_callback_;
  std::unique_ptr<LegacyMetricsUserActionRecorder> user_events_recorder_;

  fuchsia::legacymetrics::MetricsRecorderPtr metrics_recorder_;
  base::RetainingOneShotTimer timer_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace cr_fuchsia

#endif  // FUCHSIA_BASE_LEGACYMETRICS_CLIENT_H_
