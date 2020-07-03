// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/throughput_ukm_reporter.h"

#include "cc/trees/ukm_manager.h"

namespace cc {

namespace {
// Collect UKM once per kNumberOfSamplesToReport UMA reports.
constexpr unsigned kNumberOfSamplesToReport = 100u;
}  // namespace

ThroughputUkmReporter::ThroughputUkmReporter(UkmManager* ukm_manager)
    : ukm_manager_(ukm_manager) {
  DCHECK(ukm_manager_);
  // TODO(crbug.com/1040634): Setting it to 1 such that the first sample is
  // ignored. TThis is because the universal tracker is active during the page
  // load and the first sample is heavily biased by loading as a result.
  samples_to_next_event_[static_cast<int>(
      FrameSequenceTrackerType::kUniversal)] = 1;
}

ThroughputUkmReporter::~ThroughputUkmReporter() = default;

void ThroughputUkmReporter::ReportThroughputUkm(
    const base::Optional<int>& slower_throughput_percent,
    const base::Optional<int>& impl_throughput_percent,
    const base::Optional<int>& main_throughput_percent,
    FrameSequenceTrackerType type) {
  // It is possible that when a tab shuts down, the ukm_manager_ owned by the
  // LayerTreeHostImpl is cleared, and yet we try to report to UKM here. In this
  // case, the |ukm_manager_| here is null.
  if (!ukm_manager_)
    return;
  if (samples_to_next_event_[static_cast<int>(type)] == 0) {
    // Sample every 100 events. Using the Universal tracker as an example
    // which reports UMA every 5s, then the system collects UKM once per
    // 100*5 = 500 seconds. This number may need to be tuned to not throttle
    // the UKM system.
    samples_to_next_event_[static_cast<int>(type)] = kNumberOfSamplesToReport;
    if (impl_throughput_percent) {
      ukm_manager_->RecordThroughputUKM(
          type, FrameSequenceMetrics::ThreadType::kCompositor,
          impl_throughput_percent.value());
    }
    if (main_throughput_percent) {
      ukm_manager_->RecordThroughputUKM(type,
                                        FrameSequenceMetrics::ThreadType::kMain,
                                        main_throughput_percent.value());
    }
    ukm_manager_->RecordThroughputUKM(type,
                                      FrameSequenceMetrics::ThreadType::kSlower,
                                      slower_throughput_percent.value());
  }
  DCHECK_GT(samples_to_next_event_[static_cast<int>(type)], 0u);
  samples_to_next_event_[static_cast<int>(type)]--;
}

void ThroughputUkmReporter::ReportAggregateThroughput(
    AggregationType aggregation_type,
    int throughput) {
  if (samples_for_aggregated_report_ == 0) {
    samples_for_aggregated_report_ = kNumberOfSamplesToReport;
    ukm_manager_->RecordAggregateThroughput(aggregation_type, throughput);
  }
  DCHECK_GT(samples_for_aggregated_report_, 0u);
  --samples_for_aggregated_report_;
}

void ThroughputUkmReporter::ComputeUniversalThroughput(
    FrameSequenceMetrics* metrics) {
  last_impl_percent_ = metrics->impl_throughput().DroppedFramePercent();
  last_main_percent_ = metrics->main_throughput().DroppedFramePercent();
  last_aggregated_percent_ =
      metrics->aggregated_throughput().DroppedFramePercent();
}

bool ThroughputUkmReporter::HasThroughputData() const {
  return last_aggregated_percent_.has_value();
}

int ThroughputUkmReporter::TakeLastAggregatedPercent() {
  int ret_value = last_aggregated_percent_.value();
  DCHECK(ret_value >= 0 && ret_value <= 100);
  last_aggregated_percent_ = base::nullopt;
  return ret_value;
}

int ThroughputUkmReporter::TakeLastImplPercent() {
  int ret_value = last_impl_percent_.value();
  DCHECK(ret_value >= 0 && ret_value <= 100);
  last_impl_percent_ = base::nullopt;
  return ret_value;
}

base::Optional<int> ThroughputUkmReporter::TakeLastMainPercent() {
  base::Optional<int> ret_value = last_main_percent_;
  DCHECK(!ret_value || (ret_value.value() >= 0 && ret_value.value() <= 100));
  last_main_percent_ = base::nullopt;
  return ret_value;
}

uint32_t ThroughputUkmReporter::GetSamplesToNextEventForTesting(int index) {
  DCHECK_LT(index, static_cast<int>(FrameSequenceTrackerType::kMaxType));
  return samples_to_next_event_[index];
}

}  // namespace cc
