// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include <net/nqe/network_congestion_analyzer.h>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "net/url_request/url_request.h"

namespace net {

namespace {

// The threshold for the observed peak queueing delay in milliseconds.
// A peak queueing delay is HIGH if it exceeds this threshold. The value is the
// 98th percentile value of the peak queueing delay observed by all requests.
static constexpr int64_t kHighQueueingDelayMsec = 5000;

// The minimal time interval between two consecutive empty queue observations
// when the number of in-flight requests is relatively low (i.e. 2). This time
// interval is required so that a new measurement period could start.
static constexpr int64_t kMinEmptyQueueObservingTimeMsec = 1500;

}  // namespace

namespace nqe {

namespace internal {

NetworkCongestionAnalyzer::NetworkCongestionAnalyzer(
    const base::TickClock* tick_clock)
    : tick_clock_(tick_clock),
      recent_active_hosts_count_(0u),
      count_inflight_requests_for_peak_queueing_delay_(0u),
      peak_count_inflight_requests_measurement_period_(0u) {
  DCHECK(tick_clock_);
}

NetworkCongestionAnalyzer::~NetworkCongestionAnalyzer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

size_t NetworkCongestionAnalyzer::GetActiveHostsCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return recent_active_hosts_count_;
}

void NetworkCongestionAnalyzer::NotifyStartTransaction(
    const URLRequest& request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Starts tracking the peak queueing delay after |request| starts.
  TrackPeakQueueingDelayBegin(&request);
}

void NetworkCongestionAnalyzer::NotifyRequestCompleted(
    const URLRequest& request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Ends tracking of the peak queueing delay.
  base::Optional<base::TimeDelta> peak_observed_delay =
      TrackPeakQueueingDelayEnd(&request);
  if (peak_observed_delay.has_value()) {
    // Records the peak queueing delay keyed by the request priority.
    base::UmaHistogramMediumTimes(
        "ResourceScheduler.PeakObservedQueueingDelay.Priority" +
            base::NumberToString(request.priority()),
        peak_observed_delay.value());

    // Records the peak queueing delay for all types of requests.
    UMA_HISTOGRAM_MEDIUM_TIMES("ResourceScheduler.PeakObservedQueueingDelay",
                               peak_observed_delay.value());
  }
}

void NetworkCongestionAnalyzer::TrackPeakQueueingDelayBegin(
    const URLRequest* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Returns if |request| has already been tracked.
  if (request_peak_delay_.find(request) != request_peak_delay_.end())
    return;

  request_peak_delay_[request] = base::nullopt;
}

base::Optional<base::TimeDelta>
NetworkCongestionAnalyzer::TrackPeakQueueingDelayEnd(
    const URLRequest* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto request_delay = request_peak_delay_.find(request);
  if (request_delay == request_peak_delay_.end())
    return base::nullopt;

  base::Optional<base::TimeDelta> peak_delay = request_delay->second;
  request_peak_delay_.erase(request_delay);
  return peak_delay;
}

void NetworkCongestionAnalyzer::ComputeRecentQueueingDelay(
    const std::map<nqe::internal::IPHash, nqe::internal::CanonicalStats>&
        recent_rtt_stats,
    const std::map<nqe::internal::IPHash, nqe::internal::CanonicalStats>&
        historical_rtt_stats,
    const int32_t downlink_kbps) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Updates downlink throughput if a new valid observation comes.
  if (downlink_kbps != nqe::internal::INVALID_RTT_THROUGHPUT)
    set_recent_downlink_throughput_kbps(downlink_kbps);
  if (recent_rtt_stats.empty())
    return;

  int32_t delay_sample_sum = 0;
  recent_active_hosts_count_ = 0u;
  for (const auto& host_stats : recent_rtt_stats) {
    nqe::internal::IPHash host = host_stats.first;
    // Skip hosts that do not have historical statistics.
    if (historical_rtt_stats.find(host) == historical_rtt_stats.end())
      continue;

    // Skip hosts that have one or fewer RTT samples or do not have the min
    // value. They cannot provide an effective queueing delay sample.
    if (historical_rtt_stats.at(host).observation_count <= 1 ||
        historical_rtt_stats.at(host).canonical_pcts.find(kStatVal0p) ==
            historical_rtt_stats.at(host).canonical_pcts.end())
      continue;

    ++recent_active_hosts_count_;
    delay_sample_sum +=
        recent_rtt_stats.at(host).most_recent_val -
        historical_rtt_stats.at(host).canonical_pcts.at(kStatVal0p);
  }

  if (recent_active_hosts_count_ == 0u)
    return;

  DCHECK_LT(0u, recent_active_hosts_count_);

  int32_t delay_ms =
      delay_sample_sum / static_cast<int>(recent_active_hosts_count_);
  recent_queueing_delay_ = base::TimeDelta::FromMilliseconds(delay_ms);

  // Updates the peak queueing delay for all tracked in-flight requests.
  for (auto& it : request_peak_delay_) {
    if (it.second.has_value()) {
      it.second = std::max(it.second.value(), recent_queueing_delay_);
    } else {
      it.second = recent_queueing_delay_;
    }
  }

  if (recent_downlink_per_packet_time_ms_ != base::nullopt) {
    recent_queue_length_ = static_cast<float>(delay_ms) /
                           recent_downlink_per_packet_time_ms_.value();
  }
}

bool NetworkCongestionAnalyzer::ShouldStartNewMeasurement(
    const base::TimeDelta& delay,
    size_t count_inflight_requests) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The queue is not empty if either the queueing delay is high or the number
  // of in-flight requests is high.
  if (delay > base::TimeDelta::FromMilliseconds(500) ||
      count_inflight_requests >= 3) {
    observing_empty_queue_timestamp_ = base::nullopt;
    return false;
  }

  // Starts a new measurement period immediately if there is very few number of
  // in-flight requests.
  if (count_inflight_requests <= 1) {
    observing_empty_queue_timestamp_ = base::nullopt;
    return true;
  }

  base::TimeTicks now = tick_clock_->NowTicks();
  // Requires a sufficient time interval between consecutive empty queue
  // observations to claim the queue is empty.
  if (observing_empty_queue_timestamp_.has_value()) {
    if (now - observing_empty_queue_timestamp_.value() >=
        base::TimeDelta::FromMilliseconds(kMinEmptyQueueObservingTimeMsec)) {
      observing_empty_queue_timestamp_ = base::nullopt;
      return true;
    }
  } else {
    observing_empty_queue_timestamp_ = now;
  }
  return false;
}

void NetworkCongestionAnalyzer::UpdatePeakDelayMapping(
    const base::TimeDelta& delay,
    size_t count_inflight_requests) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Discards an abnormal observation. This high queueing delay is likely
  // caused by retransmission packets from a previous measurement period.
  if (delay >= base::TimeDelta::FromSeconds(20))
    return;

  if (ShouldStartNewMeasurement(delay, count_inflight_requests)) {
    // This is the logic to export the tracked mapping data from the last
    // measurement period.
    // Updates the count of in-flight requests that would likely cause a high
    // network queueing delay later.
    if (peak_queueing_delay_ >=
        base::TimeDelta::FromMilliseconds(kHighQueueingDelayMsec)) {
      count_inflight_requests_causing_high_delay_ =
          count_inflight_requests_for_peak_queueing_delay_;
    }

    // Resets the tracked data for the new measurement period.
    peak_queueing_delay_ = delay;
    count_inflight_requests_for_peak_queueing_delay_ = count_inflight_requests;
    peak_count_inflight_requests_measurement_period_ = count_inflight_requests;
  } else {
    // This is the logic to update the tracking data.
    // First, updates the pending peak count of in-flight requests if a higher
    // number of in-flight requests is observed.
    // Second, updates the peak queueing delay and the peak count of inflight
    // requests if a higher queueing delay is observed. The new peak queueing
    // delay should be mapped to the peak count of in-flight requests that are
    // observed before within this measurement period.
    peak_count_inflight_requests_measurement_period_ =
        std::max(peak_count_inflight_requests_measurement_period_,
                 count_inflight_requests);

    if (delay > peak_queueing_delay_) {
      // Updates the peak queueing delay and the count of in-flight requests
      // that are responsible for the delay.
      peak_queueing_delay_ = delay;
      count_inflight_requests_for_peak_queueing_delay_ =
          peak_count_inflight_requests_measurement_period_;
    }
  }
}

void NetworkCongestionAnalyzer::set_recent_downlink_throughput_kbps(
    const int32_t downlink_kbps) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  recent_downlink_throughput_kbps_ = downlink_kbps;
  // Time in msec to transmit one TCP packet (1500 Bytes).
  // |recent_downlink_per_packet_time_ms_| = 1500 * 8 /
  // |recent_downlink_throughput_kbps_|.
  recent_downlink_per_packet_time_ms_ = 12000 / downlink_kbps;
}

}  // namespace internal

}  // namespace nqe

}  // namespace net
