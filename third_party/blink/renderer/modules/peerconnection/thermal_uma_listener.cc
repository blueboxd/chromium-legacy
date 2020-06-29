// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/thermal_uma_listener.h"

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/power_monitor/power_observer.h"
#include "base/sequenced_task_runner.h"

namespace blink {

namespace {

const base::TimeDelta kStatsReportingPeriod = base::TimeDelta::FromMinutes(1);

enum class ThermalStateUMA {
  kNominal = 0,
  kFair = 1,
  kSerious = 2,
  kCritical = 3,
  kMaxValue = kCritical,
};

ThermalStateUMA ToThermalStateUMA(
    base::PowerObserver::DeviceThermalState state) {
  switch (state) {
    case base::PowerObserver::DeviceThermalState::kNominal:
      return ThermalStateUMA::kNominal;
    case base::PowerObserver::DeviceThermalState::kFair:
      return ThermalStateUMA::kFair;
    case base::PowerObserver::DeviceThermalState::kSerious:
      return ThermalStateUMA::kSerious;
    case base::PowerObserver::DeviceThermalState::kCritical:
      return ThermalStateUMA::kCritical;
    default:
      NOTREACHED();
      return ThermalStateUMA::kNominal;
  }
}

}  // namespace

// static
std::unique_ptr<ThermalUmaListener> ThermalUmaListener::Create(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  std::unique_ptr<ThermalUmaListener> listener =
      std::make_unique<ThermalUmaListener>(std::move(task_runner));
  listener->ScheduleReport();
  return listener;
}

ThermalUmaListener::ThermalUmaListener(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)),
      current_thermal_state_(base::PowerObserver::DeviceThermalState::kUnknown),
      weak_ptr_factor_(this) {
  DCHECK(task_runner_);
}

void ThermalUmaListener::OnThermalMeasurement(
    base::PowerObserver::DeviceThermalState measurement) {
  base::AutoLock crit(lock_);
  current_thermal_state_ = measurement;
}

void ThermalUmaListener::ScheduleReport() {
  task_runner_->PostDelayedTask(FROM_HERE,
                                base::BindOnce(&ThermalUmaListener::ReportStats,
                                               weak_ptr_factor_.GetWeakPtr()),
                                kStatsReportingPeriod);
}

void ThermalUmaListener::ReportStats() {
  {
    base::AutoLock crit(lock_);
    if (current_thermal_state_ !=
        base::PowerObserver::DeviceThermalState::kUnknown) {
      UMA_HISTOGRAM_ENUMERATION("WebRTC.PeerConnection.ThermalState",
                                ToThermalStateUMA(current_thermal_state_));
    }
  }
  ScheduleReport();
}

}  // namespace blink
