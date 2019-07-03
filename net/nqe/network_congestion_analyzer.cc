// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <net/nqe/network_congestion_analyzer.h>

namespace net {

namespace nqe {

namespace internal {

NetworkCongestionAnalyzer::NetworkCongestionAnalyzer()
    : recent_active_hosts_count_(0u) {}
NetworkCongestionAnalyzer::~NetworkCongestionAnalyzer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

size_t NetworkCongestionAnalyzer::GetActiveHostsCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return recent_active_hosts_count_;
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
  if (recent_downlink_per_packet_time_ms_ != base::nullopt) {
    recent_queue_length_ = static_cast<float>(delay_ms) /
                           recent_downlink_per_packet_time_ms_.value();
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
