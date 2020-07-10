// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/battery/android_battery_metrics.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/power_monitor/power_monitor.h"

namespace {

void Report30SecondDrain(int capacity_consumed) {
  // Drain over the last 30 seconds in uAh. We assume a max current of 10A which
  // translates to a little under 100mAh capacity drain over 30 seconds.
  UMA_HISTOGRAM_COUNTS_100000("Power.ForegroundBatteryDrain.30Seconds",
                              capacity_consumed);
}

void ReportAveragedDrain(int capacity_consumed, int num_sampling_periods) {
  // Averaged drain over 30 second intervals in uAh. We assume a max current of
  // 10A which translates to a little under 100mAh capacity drain over 30
  // seconds.
  static const char kName[] = "Power.ForegroundBatteryDrain.30SecondsAvg";
  STATIC_HISTOGRAM_POINTER_BLOCK(
      kName,
      AddCount(capacity_consumed / num_sampling_periods, num_sampling_periods),
      base::Histogram::FactoryGet(
          kName, /*min_value=*/1, /*max_value=*/100000, /*bucket_count=*/50,
          base::HistogramBase::kUmaTargetedHistogramFlag));
}

}  // namespace

AndroidBatteryMetrics::AndroidBatteryMetrics()
    : app_state_listener_(base::android::ApplicationStatusListener::New(
          base::BindRepeating(&AndroidBatteryMetrics::OnAppStateChanged,
                              base::Unretained(this)))) {
  base::PowerMonitor::AddObserver(this);
}

AndroidBatteryMetrics::~AndroidBatteryMetrics() {
  base::PowerMonitor::RemoveObserver(this);
}

void AndroidBatteryMetrics::OnAppStateChanged(
    base::android::ApplicationState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  app_state_ = state;
  UpdateDrainMetricsEnabled();
}

void AndroidBatteryMetrics::OnPowerStateChange(bool on_battery_power) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_battery_power_ = on_battery_power;
  UpdateDrainMetricsEnabled();
}

void AndroidBatteryMetrics::UpdateDrainMetricsEnabled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // We want to attribute battery drain to Chrome while it is in the foreground.
  // Battery drain will only be reflected in remaining battery capacity when the
  // device is not on a charger.
  bool should_be_enabled =
      app_state_ == base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES &&
      on_battery_power_;

  if (should_be_enabled && !drain_metrics_timer_.IsRunning()) {
    // Capture first capacity measurement and enable the repeating timer.
    last_remaining_capacity_uah_ =
        base::PowerMonitor::GetRemainingBatteryCapacity();
    skipped_timers_ = 0;

    drain_metrics_timer_.Start(FROM_HERE, kDrainMetricsInterval, this,
                               &AndroidBatteryMetrics::CaptureAndReportDrain);
  } else if (!should_be_enabled && drain_metrics_timer_.IsRunning()) {
    // Capture one last measurement before disabling the timer.
    CaptureAndReportDrain();
    drain_metrics_timer_.Stop();
  }
}

void AndroidBatteryMetrics::CaptureAndReportDrain() {
  int remaining_capacity_uah =
      base::PowerMonitor::GetRemainingBatteryCapacity();

  if (remaining_capacity_uah >= last_remaining_capacity_uah_) {
    // No change in battery capacity, or it increased. The latter could happen
    // if we detected the switch off battery power to a charger late, or if the
    // device reports bogus values. We don't change last_remaining_capacity_uah_
    // here to avoid overreporting in case of fluctuating values.
    skipped_timers_++;
    Report30SecondDrain(0);
    return;
  }

  // Report the consumed capacity delta over the last 30 seconds.
  int capacity_consumed = last_remaining_capacity_uah_ - remaining_capacity_uah;
  Report30SecondDrain(capacity_consumed);

  // Also record drain over 30 second intervals, but averaged since the last
  // time we recorded an increase (or started recording samples). Because the
  // underlying battery capacity counter is often low-resolution (usually
  // between .5 and 50 mAh), it may only increment after multiple sampling
  // points.
  ReportAveragedDrain(capacity_consumed, skipped_timers_ + 1);

  // Also track the total capacity consumed in a single-bucket-histogram,
  // emitting one sample for every 100 uAh drained.
  static constexpr base::Histogram::Sample kSampleFactor = 100;
  UMA_HISTOGRAM_SCALED_EXACT_LINEAR("Power.ForegroundBatteryDrain",
                                    /*sample=*/1, capacity_consumed,
                                    /*sample_max=*/1, kSampleFactor);

  last_remaining_capacity_uah_ = remaining_capacity_uah;
  skipped_timers_ = 0;
}
