// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/nqe/network_congestion_analyzer.h"
#include <map>
#include <unordered_map>

#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "net/nqe/network_quality.h"
#include "net/nqe/observation_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace nqe {

namespace internal {

namespace {
constexpr float kEpsilon = 0.001f;

// Verify that the network queueing delay is computed correctly based on RTT
// and downlink throughput observations.
TEST(NetworkCongestionAnalyzerTest, TestComputingQueueingDelay) {
  NetworkCongestionAnalyzer analyzer;
  std::map<uint64_t, CanonicalStats> recent_rtt_stats;
  std::map<uint64_t, CanonicalStats> historical_rtt_stats;
  int32_t downlink_kbps = nqe::internal::INVALID_RTT_THROUGHPUT;

  // Checks that no result is updated when providing empty RTT observations and
  // an invalid downlink throughput observation.
  analyzer.ComputeRecentQueueingDelay(recent_rtt_stats, historical_rtt_stats,
                                      downlink_kbps);
  EXPECT_TRUE(analyzer.recent_queueing_delay().is_zero());

  const uint64_t host_1 = 0x101010UL;
  const uint64_t host_2 = 0x202020UL;
  // Checks that the queueing delay is updated based on hosts with valid RTT
  // observations. For example, the computation should be done by using data
  // from host 1 only because host 2 does not provide a valid min RTT value.
  std::map<int32_t, int32_t> recent_stat_host_1 = {{kStatVal0p, 1100}};
  std::map<int32_t, int32_t> historical_stat_host_1 = {{kStatVal0p, 600}};
  CanonicalStats recent_rtt_host_1 =
      CanonicalStats(recent_stat_host_1, 1400, 5);
  CanonicalStats historical_rtt_host_1 =
      CanonicalStats(historical_stat_host_1, 1400, 15);

  std::map<int32_t, int32_t> recent_stat_host_2 = {{kStatVal0p, 1200}};
  std::map<int32_t, int32_t> historical_stat_host_2 = {{kStatVal50p, 1200}};
  CanonicalStats recent_rtt_host_2 =
      CanonicalStats(recent_stat_host_2, 1600, 3);
  CanonicalStats historical_rtt_host_2 =
      CanonicalStats(historical_stat_host_2, 1600, 8);
  recent_rtt_stats.emplace(host_1, recent_rtt_host_1);
  recent_rtt_stats.emplace(host_2, recent_rtt_host_2);
  historical_rtt_stats.emplace(host_1, historical_rtt_host_1);
  historical_rtt_stats.emplace(host_2, historical_rtt_host_2);

  analyzer.ComputeRecentQueueingDelay(recent_rtt_stats, historical_rtt_stats,
                                      downlink_kbps);
  EXPECT_EQ(800, analyzer.recent_queueing_delay().InMilliseconds());

  // Checks that the queueing delay is updated correctly based on all hosts when
  // RTT observations and the throughput observation are valid.
  historical_rtt_stats[host_2].canonical_pcts[kStatVal0p] = 1000;
  downlink_kbps = 120;
  analyzer.ComputeRecentQueueingDelay(recent_rtt_stats, historical_rtt_stats,
                                      downlink_kbps);
  EXPECT_EQ(700, analyzer.recent_queueing_delay().InMilliseconds());
  EXPECT_NEAR(7.0, analyzer.recent_queue_length().value_or(0), kEpsilon);
}

}  // namespace

}  // namespace internal

}  // namespace nqe

}  // namespace net