// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_session.h"

#include <stdint.h>

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/metrics/bucket_ranges.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sample_vector.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_socket_pool.h"
#include "net/dns/dns_util.h"
#include "net/dns/resolve_context.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/datagram_client_socket.h"
#include "net/socket/stream_socket.h"

namespace net {

namespace {

// Set min timeout, in case we are talking to a local DNS proxy.
const base::TimeDelta kMinTimeout = base::TimeDelta::FromMilliseconds(10);

// Default maximum timeout between queries, even with exponential backoff.
// (Can be overridden by field trial.)
const base::TimeDelta kDefaultMaxTimeout = base::TimeDelta::FromSeconds(5);

// Maximum RTT that will fit in the RTT histograms.
const base::TimeDelta kRttMax = base::TimeDelta::FromSeconds(30);
// Number of buckets in the histogram of observed RTTs.
const size_t kRttBucketCount = 350;
// Target percentile in the RTT histogram used for retransmission timeout.
const int kRttPercentile = 99;
// Number of samples to seed the histogram with.
const base::HistogramBase::Count kNumSeeds = 2;

class RttBuckets : public base::BucketRanges {
 public:
  RttBuckets() : base::BucketRanges(kRttBucketCount + 1) {
    base::Histogram::InitializeBucketRanges(
        1,
        base::checked_cast<base::HistogramBase::Sample>(
            kRttMax.InMilliseconds()),
        this);
  }
};

static RttBuckets* GetRttBuckets() {
  static base::NoDestructor<RttBuckets> buckets;
  return buckets.get();
}

}  // namespace

// Runtime statistics of DNS server.
struct DnsSession::ServerStats {
  ServerStats(base::TimeDelta rtt_estimate, RttBuckets* buckets)
      : last_failure_count(0),
        rtt_histogram(std::make_unique<base::SampleVector>(buckets)) {
    // Seed histogram with 2 samples at |rtt_estimate| timeout.
    rtt_histogram->Accumulate(
        static_cast<base::HistogramBase::Sample>(rtt_estimate.InMilliseconds()),
        kNumSeeds);
  }

  // Count of consecutive failures after last success.
  int last_failure_count;

  // Last time when server returned failure or timeout.
  base::TimeTicks last_failure;
  // Last time when server returned success.
  base::TimeTicks last_success;

  // A histogram of observed RTT .
  std::unique_ptr<base::SampleVector> rtt_histogram;

  DISALLOW_COPY_AND_ASSIGN(ServerStats);
};

DnsSession::SocketLease::SocketLease(
    scoped_refptr<DnsSession> session,
    size_t server_index,
    std::unique_ptr<DatagramClientSocket> socket)
    : session_(session),
      server_index_(server_index),
      socket_(std::move(socket)) {}

DnsSession::SocketLease::~SocketLease() {
  session_->FreeSocket(server_index_, std::move(socket_));
}

DnsSession::DnsSession(const DnsConfig& config,
                       std::unique_ptr<DnsSocketPool> socket_pool,
                       const RandIntCallback& rand_int_callback,
                       NetLog* net_log)
    : config_(config),
      socket_pool_(std::move(socket_pool)),
      rand_callback_(base::Bind(rand_int_callback,
                                0,
                                std::numeric_limits<uint16_t>::max())),
      net_log_(net_log),
      server_index_(0) {
  socket_pool_->Initialize(&config_.nameservers, net_log);
  UMA_HISTOGRAM_CUSTOM_COUNTS("AsyncDNS.ServerCount",
                              config_.nameservers.size(), 1, 10, 11);
  UpdateTimeouts(NetworkChangeNotifier::GetConnectionType());
  InitializeServerStats();
}

DnsSession::~DnsSession() = default;

void DnsSession::UpdateTimeouts(NetworkChangeNotifier::ConnectionType type) {
  initial_timeout_ = GetTimeDeltaForConnectionTypeFromFieldTrialOrDefault(
      "AsyncDnsInitialTimeoutMsByConnectionType", config_.timeout, type);
  max_timeout_ = GetTimeDeltaForConnectionTypeFromFieldTrialOrDefault(
      "AsyncDnsMaxTimeoutMsByConnectionType", kDefaultMaxTimeout, type);
}

void DnsSession::InitializeServerStats() {
  server_stats_.clear();
  for (size_t i = 0; i < config_.nameservers.size(); ++i) {
    server_stats_.push_back(
        std::make_unique<ServerStats>(initial_timeout_, GetRttBuckets()));
  }

  doh_server_stats_.clear();
  for (size_t i = 0; i < config_.dns_over_https_servers.size(); ++i) {
    doh_server_stats_.push_back(
        std::make_unique<ServerStats>(initial_timeout_, GetRttBuckets()));
  }
}

uint16_t DnsSession::NextQueryId() const {
  return static_cast<uint16_t>(rand_callback_.Run());
}

size_t DnsSession::FirstServerIndex(bool doh_server) {
  if (doh_server)
    return 0u;

  size_t index = ServerIndexToUse(server_index_);
  if (config_.rotate)
    server_index_ = (server_index_ + 1) % config_.nameservers.size();
  return index;
}

size_t DnsSession::ServerIndexToUse(size_t starting_server) {
  DCHECK_LT(starting_server, config_.nameservers.size());
  size_t index = starting_server;
  base::TimeTicks oldest_server_failure;
  base::Optional<size_t> oldest_server_failure_index;

  do {
    // If number of failures on this server doesn't exceed number of allowed
    // attempts, return its index.
    if (server_stats_[index]->last_failure_count < config_.attempts) {
      return index;
    }
    // Track oldest failed server.
    base::TimeTicks cur_server_failure = server_stats_[index]->last_failure;
    if (!oldest_server_failure_index.has_value() ||
        cur_server_failure < oldest_server_failure) {
      oldest_server_failure = cur_server_failure;
      oldest_server_failure_index = index;
    }
    index = (index + 1) % config_.nameservers.size();
  } while (index != starting_server);

  // If we are here it means that there are no successful servers, so we have
  // to use one that has failed least recently.
  DCHECK(oldest_server_failure_index.has_value());
  return oldest_server_failure_index.value();
}

base::TimeTicks DnsSession::GetLastDohFailure(size_t server_index) {
  return GetServerStats(server_index, true /* is_doh_server */)->last_failure;
}

int DnsSession::GetLastDohFailureCount(size_t server_index) {
  return GetServerStats(server_index, true /* is_doh_server */)
      ->last_failure_count;
}

DnsSession::ServerStats* DnsSession::GetServerStats(size_t server_index,
                                                    bool is_doh_server) {
  DCHECK_GE(server_index, 0u);
  if (!is_doh_server) {
    DCHECK_LT(server_index, config_.nameservers.size());
    return server_stats_[server_index].get();
  } else {
    DCHECK_LT(server_index, config_.dns_over_https_servers.size());
    return doh_server_stats_[server_index].get();
  }
}

void DnsSession::RecordServerFailure(size_t server_index,
                                     bool is_doh_server,
                                     ResolveContext* resolve_context) {
  ServerStats* stats = GetServerStats(server_index, is_doh_server);
  ++(stats->last_failure_count);
  stats->last_failure = base::TimeTicks::Now();

  if (is_doh_server &&
      stats->last_failure_count >= kAutomaticModeFailureLimit) {
    resolve_context->SetProbeSuccess(server_index, false /* success */, this);
  }
}

void DnsSession::RecordServerSuccess(size_t server_index, bool is_doh_server) {
  ServerStats* stats = GetServerStats(server_index, is_doh_server);

  // DoH queries can be sent using more than one URLRequestContext. A success
  // from one URLRequestContext shouldn't zero out failures that may be
  // consistently occurring for another URLRequestContext.
  if (!is_doh_server)
    stats->last_failure_count = 0;
  stats->last_failure = base::TimeTicks();
  stats->last_success = base::TimeTicks::Now();
}

void DnsSession::RecordRtt(size_t server_index,
                           bool is_doh_server,
                           bool is_validated_doh_server,
                           base::TimeDelta rtt,
                           int rv) {
  RecordRttForUma(server_index, is_doh_server, is_validated_doh_server, rtt,
                  rv);

  ServerStats* stats = GetServerStats(server_index, is_doh_server);

  // RTT values shouldn't be less than 0, but it shouldn't cause a crash if
  // they are anyway, so clip to 0. See https://crbug.com/753568.
  if (rtt < base::TimeDelta())
    rtt = base::TimeDelta();

  // Histogram-based method.
  stats->rtt_histogram->Accumulate(
      base::saturated_cast<base::HistogramBase::Sample>(rtt.InMilliseconds()),
      1);
}

base::TimeDelta DnsSession::NextTimeout(size_t server_index, int attempt) {
  return NextTimeoutHelper(
      GetServerStats(server_index, false /* is _doh_server */),
      attempt / config_.nameservers.size());
}

base::TimeDelta DnsSession::NextDohTimeout(size_t doh_server_index) {
  return NextTimeoutHelper(
      GetServerStats(doh_server_index, true /* is _doh_server */),
      0 /* num_backoffs */);
}

base::TimeDelta DnsSession::NextTimeoutHelper(ServerStats* server_stats,
                                              int num_backoffs) {
  // Respect initial timeout (from config or field trial) if it exceeds max.
  if (initial_timeout_ > max_timeout_)
    return initial_timeout_;

  static_assert(std::numeric_limits<base::HistogramBase::Count>::is_signed,
                "histogram base count assumed to be signed");

  // Use fixed percentile of observed samples.
  const base::SampleVector& samples = *server_stats->rtt_histogram;

  base::HistogramBase::Count total = samples.TotalCount();
  base::HistogramBase::Count remaining_count = kRttPercentile * total / 100;
  size_t index = 0;
  while (remaining_count > 0 && index < GetRttBuckets()->size()) {
    remaining_count -= samples.GetCountAtIndex(index);
    ++index;
  }

  base::TimeDelta timeout =
      base::TimeDelta::FromMilliseconds(GetRttBuckets()->range(index));

  timeout = std::max(timeout, kMinTimeout);

  return std::min(timeout * (1 << num_backoffs), max_timeout_);
}

// Allocate a socket, already connected to the server address.
std::unique_ptr<DnsSession::SocketLease> DnsSession::AllocateSocket(
    size_t server_index,
    const NetLogSource& source) {
  std::unique_ptr<DatagramClientSocket> socket;

  socket = socket_pool_->AllocateSocket(server_index);
  if (!socket.get())
    return std::unique_ptr<SocketLease>();

  socket->NetLog().BeginEventReferencingSource(NetLogEventType::SOCKET_IN_USE,
                                               source);

  SocketLease* lease = new SocketLease(this, server_index, std::move(socket));
  return std::unique_ptr<SocketLease>(lease);
}

std::unique_ptr<StreamSocket> DnsSession::CreateTCPSocket(
    size_t server_index,
    const NetLogSource& source) {
  return socket_pool_->CreateTCPSocket(server_index, source);
}

void DnsSession::InvalidateWeakPtrsForTesting() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

// Release a socket.
void DnsSession::FreeSocket(size_t server_index,
                            std::unique_ptr<DatagramClientSocket> socket) {
  DCHECK(socket.get());

  socket->NetLog().EndEvent(NetLogEventType::SOCKET_IN_USE);

  socket_pool_->FreeSocket(server_index, std::move(socket));
}

void DnsSession::RecordRttForUma(size_t server_index,
                                 bool is_doh_server,
                                 bool is_validated_doh_server,
                                 base::TimeDelta rtt,
                                 int rv) {
  std::string query_type;
  std::string provider_id;
  if (is_doh_server) {
    // Secure queries are validated if the DoH server state is available.
    if (is_validated_doh_server)
      query_type = "SecureValidated";
    else
      query_type = "SecureNotValidated";
    provider_id = GetDohProviderIdForHistogramFromDohConfig(
        config_.dns_over_https_servers[server_index]);
  } else {
    DCHECK(!is_validated_doh_server);
    query_type = "Insecure";
    provider_id = GetDohProviderIdForHistogramFromNameserver(
        config_.nameservers[server_index]);
  }
  if (rv == OK || rv == ERR_NAME_NOT_RESOLVED) {
    base::UmaHistogramMediumTimes(
        base::StringPrintf("Net.DNS.DnsTransaction.%s.%s.SuccessTime",
                           query_type.c_str(), provider_id.c_str()),
        rtt);
  } else {
    base::UmaHistogramMediumTimes(
        base::StringPrintf("Net.DNS.DnsTransaction.%s.%s.FailureTime",
                           query_type.c_str(), provider_id.c_str()),
        rtt);
    if (is_doh_server) {
      base::UmaHistogramSparse(
          base::StringPrintf("Net.DNS.DnsTransaction.%s.%s.FailureError",
                             query_type.c_str(), provider_id.c_str()),
          std::abs(rv));
    }
  }
}

}  // namespace net
