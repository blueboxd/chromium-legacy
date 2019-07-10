// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_NQE_NETWORK_CONGESTION_ANALYZER_H_
#define NET_NQE_NETWORK_CONGESTION_ANALYZER_H_

#include <cmath>
#include <map>
#include <unordered_map>

#include "base/macros.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/nqe/network_quality.h"
#include "net/nqe/network_quality_estimator_util.h"
#include "net/nqe/observation_buffer.h"
#include "net/nqe/throughput_analyzer.h"

namespace net {

namespace nqe {

namespace internal {

// NetworkCongestionAnalyzer holds the network congestion information, such
// as the recent packet queue length in the mobile edge, recent queueing delay,
// recent downlink throughput.
class NET_EXPORT_PRIVATE NetworkCongestionAnalyzer {
 public:
  NetworkCongestionAnalyzer();
  ~NetworkCongestionAnalyzer();

  // Returns the number of hosts that are involved in the last attempt of
  // computing the recent queueing delay. These hosts are recent active hosts.
  size_t GetActiveHostsCount() const;

  // Notifies |this| that the headers of |request| are about to be sent.
  void NotifyStartTransaction(const URLRequest& request);

  // Notifies |this| that the response body of |request| has been received.
  void NotifyRequestCompleted(const URLRequest& request);

  // Computes the recent queueing delay. Records the observed queueing delay
  // samples for all recent active hosts. The mean queueing delay is recorded in
  // |recent_queueing_delay_|. |recent_rtt_stats| has all canonical statistic
  // values of recent observations for active hosts. |historical_rtt_stats| has
  // all canonical statistic values of historical observations for active hosts.
  // If |downlink_kbps| is valid, updates |recent_downlink_throughput_kbps_| and
  // |recent_downlink_per_packet_time_ms_| based on its value. If
  // |recent_downlink_per_packet_time_ms_| has a valid value, updates
  // |recent_queue_length_| as well.
  void ComputeRecentQueueingDelay(
      const std::map<nqe::internal::IPHash, nqe::internal::CanonicalStats>&
          recent_rtt_stats,
      const std::map<nqe::internal::IPHash, nqe::internal::CanonicalStats>&
          historical_rtt_stats,
      const int32_t downlink_kbps);

  base::Optional<float> recent_queue_length() const {
    return recent_queue_length_;
  }

  base::TimeDelta recent_queueing_delay() const {
    return recent_queueing_delay_;
  }

 private:
  // Starts tracking the peak queueing delay for |request|.
  void TrackPeakQueueingDelayBegin(const URLRequest* request);

  // Returns the peak queueing delay observed by |request|. Also removes the
  // record that belongs to |request| in the map. If the result is unavailable,
  // returns nullopt.
  base::Optional<base::TimeDelta> TrackPeakQueueingDelayEnd(
      const URLRequest* request);

  // Sets the |recent_downlink_throughput_kbps_| with the estimated downlink
  // throughput in kbps. Also, computes the time frame (in millisecond) to
  // transmit one TCP packet (1500 Bytes) under this downlink throughput.
  void set_recent_downlink_throughput_kbps(const int32_t downlink_kbps);

  base::Optional<int32_t> recent_downlink_throughput_kbps() const {
    return recent_downlink_throughput_kbps_;
  }

  // Recent downstream throughput estimate. It is the median of most recent
  // downstream throughput observations (in kilobits per second).
  base::Optional<int32_t> recent_downlink_throughput_kbps_;
  // Time of transmitting one 1500-Byte TCP packet under
  // |recent_downlink_throughput_kbps_|.
  base::Optional<int32_t> recent_downlink_per_packet_time_ms_;

  // The estimate of packet queue length. Computation is done based on the last
  // observed downlink throughput.
  base::Optional<float> recent_queue_length_;
  // The estimate of queueing delay induced by packet queue.
  base::TimeDelta recent_queueing_delay_;

  // Mapping between URL requests to the observed queueing delay observations.
  // The default value is nullopt.
  typedef std::unordered_map<const URLRequest*, base::Optional<base::TimeDelta>>
      RequestPeakDelay;

  // This map maintains the mapping from in-flight URL requests to the peak
  // queueing delay observed by requests.
  RequestPeakDelay request_peak_delay_;

  // Counts the number of hosts involved in the last attempt of computing the
  // recent queueing delay.
  size_t recent_active_hosts_count_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(NetworkCongestionAnalyzer);
};

}  // namespace internal

}  // namespace nqe

}  // namespace net

#endif  // NET_NQE_NETWORK_CONGESTION_ANALYZER_H_
