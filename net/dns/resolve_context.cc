// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/resolve_context.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/metrics/bucket_ranges.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/sample_vector.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "net/base/network_change_notifier.h"
#include "net/dns/dns_session.h"
#include "net/dns/dns_util.h"
#include "net/dns/host_cache.h"

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

base::TimeDelta GetDefaultTimeout(const DnsConfig& config) {
  NetworkChangeNotifier::ConnectionType type =
      NetworkChangeNotifier::GetConnectionType();
  return GetTimeDeltaForConnectionTypeFromFieldTrialOrDefault(
      "AsyncDnsInitialTimeoutMsByConnectionType", config.timeout, type);
}

base::TimeDelta GetMaxTimeout() {
  NetworkChangeNotifier::ConnectionType type =
      NetworkChangeNotifier::GetConnectionType();
  return GetTimeDeltaForConnectionTypeFromFieldTrialOrDefault(
      "AsyncDnsMaxTimeoutMsByConnectionType", kDefaultMaxTimeout, type);
}

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
struct ResolveContext::ServerStats {
  ServerStats(base::TimeDelta rtt_estimate, RttBuckets* buckets)
      : last_failure_count(0),
        rtt_histogram(std::make_unique<base::SampleVector>(buckets)) {
    // Seed histogram with 2 samples at |rtt_estimate| timeout.
    rtt_histogram->Accumulate(base::checked_cast<base::HistogramBase::Sample>(
                                  rtt_estimate.InMilliseconds()),
                              kNumSeeds);
  }

  // Count of consecutive failures after last success.
  int last_failure_count;

  // True if any success has ever been recorded for this server for the current
  // connection.
  bool current_connection_success = false;

  // Last time when server returned failure or timeout.
  base::TimeTicks last_failure;
  // Last time when server returned success.
  base::TimeTicks last_success;

  // A histogram of observed RTT .
  std::unique_ptr<base::SampleVector> rtt_histogram;
};

ResolveContext::ResolveContext(URLRequestContext* url_request_context,
                               bool enable_caching)
    : url_request_context_(url_request_context),
      host_cache_(enable_caching ? HostCache::CreateDefaultCache() : nullptr) {
  max_timeout_ = GetMaxTimeout();
}

ResolveContext::~ResolveContext() = default;

size_t ResolveContext::FirstServerIndex(bool doh_server,
                                        const DnsSession* session) {
  if (!IsCurrentSession(session))
    return 0u;

  // DoH first server doesn't rotate, so always return 0u.
  if (doh_server)
    return 0u;

  size_t index = ClassicServerIndexToUse(classic_server_index_, session);
  if (current_session_->config().rotate) {
    classic_server_index_ = (classic_server_index_ + 1) %
                            current_session_->config().nameservers.size();
  }
  return index;
}

size_t ResolveContext::ClassicServerIndexToUse(
    size_t classic_starting_server_index,
    const DnsSession* session) {
  if (!IsCurrentSession(session))
    return classic_starting_server_index;

  CHECK_LT(classic_starting_server_index, classic_server_stats_.size());
  size_t index = classic_starting_server_index;
  base::TimeTicks oldest_server_failure;
  base::Optional<size_t> oldest_server_failure_index;

  do {
    CHECK_LT(index, classic_server_stats_.size());

    // If number of failures on this server doesn't exceed number of allowed
    // attempts, return its index.
    if (classic_server_stats_[index].last_failure_count <
        current_session_->config().attempts) {
      return index;
    }
    // Track oldest failed server.
    base::TimeTicks cur_server_failure =
        classic_server_stats_[index].last_failure;
    if (!oldest_server_failure_index.has_value() ||
        cur_server_failure < oldest_server_failure) {
      oldest_server_failure = cur_server_failure;
      oldest_server_failure_index = index;
    }
    index = (index + 1) % current_session_->config().nameservers.size();
  } while (index != classic_starting_server_index);

  // If we are here it means that there are no successful servers, so we have
  // to use one that has failed least recently.
  DCHECK(oldest_server_failure_index.has_value());
  return oldest_server_failure_index.value();
}

base::Optional<size_t> ResolveContext::DohServerIndexToUse(
    size_t starting_doh_server_index,
    DnsConfig::SecureDnsMode secure_dns_mode,
    const DnsSession* session) {
  if (!IsCurrentSession(session))
    return base::nullopt;

  CHECK_LT(starting_doh_server_index, doh_server_stats_.size());
  size_t index = starting_doh_server_index;
  base::TimeTicks oldest_server_failure;
  base::Optional<size_t> oldest_available_server_failure_index;

  do {
    CHECK_LT(index, doh_server_stats_.size());
    // For a server to be considered "available", the server must have a
    // successful probe status if we are in AUTOMATIC mode.
    if (secure_dns_mode == DnsConfig::SecureDnsMode::SECURE ||
        GetDohServerAvailability(index, session)) {
      // If number of failures on this server doesn't exceed |config_.attempts|,
      // return its index. |config_.attempts| will generally be more restrictive
      // than |kAutomaticModeFailureLimit|, although this is not guaranteed.
      if (doh_server_stats_[index].last_failure_count <
          session->config().attempts) {
        return index;
      }
      // Track oldest failed available server.
      base::TimeTicks cur_server_failure =
          doh_server_stats_[index].last_failure;
      if (!oldest_available_server_failure_index.has_value() ||
          cur_server_failure < oldest_server_failure) {
        oldest_server_failure = cur_server_failure;
        oldest_available_server_failure_index = index;
      }
    }
    index = (index + 1) % session->config().dns_over_https_servers.size();
  } while (index != starting_doh_server_index);

  // If we are here it means that there are either no available DoH servers or
  // that all available DoH servers have at least |config_.attempts| consecutive
  // failures. In the latter case, we'll return the available DoH server that
  // failed least recently. In the former case we return nullopt.
  return oldest_available_server_failure_index;
}

bool ResolveContext::GetDohServerAvailability(size_t doh_server_index,
                                              const DnsSession* session) const {
  if (!IsCurrentSession(session))
    return false;

  CHECK_LT(doh_server_index, doh_server_stats_.size());
  return ServerStatsToDohAvailability(doh_server_stats_[doh_server_index]);
}

size_t ResolveContext::NumAvailableDohServers(const DnsSession* session) const {
  if (!IsCurrentSession(session))
    return 0;

  return std::count_if(doh_server_stats_.cbegin(), doh_server_stats_.cend(),
                       &ServerStatsToDohAvailability);
}

void ResolveContext::RecordServerFailure(size_t server_index,
                                         bool is_doh_server,
                                         const DnsSession* session) {
  if (!IsCurrentSession(session))
    return;

  size_t num_available_doh_servers_before = NumAvailableDohServers(session);

  ServerStats* stats = GetServerStats(server_index, is_doh_server);
  ++(stats->last_failure_count);
  stats->last_failure = base::TimeTicks::Now();

  size_t num_available_doh_servers_now = NumAvailableDohServers(session);
  if (num_available_doh_servers_now < num_available_doh_servers_before) {
    NotifyDohStatusObserversOfUnavailable(false /* network_change */);

    // TODO(crbug.com/1022059): Consider figuring out some way to only for the
    // first context enabling DoH or the last context disabling DoH.
    if (num_available_doh_servers_now == 0)
      NetworkChangeNotifier::TriggerNonSystemDnsChange();
  }
}

void ResolveContext::RecordServerSuccess(size_t server_index,
                                         bool is_doh_server,
                                         const DnsSession* session) {
  if (!IsCurrentSession(session))
    return;

  bool doh_available_before = NumAvailableDohServers(session) > 0;

  ServerStats* stats = GetServerStats(server_index, is_doh_server);
  stats->last_failure_count = 0;
  stats->current_connection_success = true;
  stats->last_failure = base::TimeTicks();
  stats->last_success = base::TimeTicks::Now();

  // TODO(crbug.com/1022059): Consider figuring out some way to only for the
  // first context enabling DoH or the last context disabling DoH.
  bool doh_available_now = NumAvailableDohServers(session) > 0;
  if (doh_available_before != doh_available_now)
    NetworkChangeNotifier::TriggerNonSystemDnsChange();
}

void ResolveContext::RecordRtt(size_t server_index,
                               bool is_doh_server,
                               base::TimeDelta rtt,
                               int rv,
                               const DnsSession* session) {
  if (!IsCurrentSession(session))
    return;

  RecordRttForUma(server_index, is_doh_server, rtt, rv, session);

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

base::TimeDelta ResolveContext::NextClassicTimeout(size_t classic_server_index,
                                                   int attempt,
                                                   const DnsSession* session) {
  if (!IsCurrentSession(session))
    return std::min(GetDefaultTimeout(session->config()), max_timeout_);

  return NextTimeoutHelper(
      GetServerStats(classic_server_index, false /* is _doh_server */),
      attempt / current_session_->config().nameservers.size());
}

base::TimeDelta ResolveContext::NextDohTimeout(size_t doh_server_index,
                                               const DnsSession* session) {
  if (!IsCurrentSession(session))
    return std::min(GetDefaultTimeout(session->config()), max_timeout_);

  return NextTimeoutHelper(
      GetServerStats(doh_server_index, true /* is _doh_server */),
      0 /* num_backoffs */);
}

void ResolveContext::RegisterDohStatusObserver(DohStatusObserver* observer) {
  DCHECK(observer);
  doh_status_observers_.AddObserver(observer);
}

void ResolveContext::UnregisterDohStatusObserver(
    const DohStatusObserver* observer) {
  DCHECK(observer);
  doh_status_observers_.RemoveObserver(observer);
}

void ResolveContext::InvalidateCachesAndPerSessionData(
    const DnsSession* new_session,
    bool network_change) {
  if (host_cache_)
    host_cache_->Invalidate();

  // DNS config is constant for any given session, so if the current session is
  // unchanged, any per-session data is safe to keep, even if it's dependent on
  // a specific config.
  if (new_session && new_session == current_session_.get())
    return;

  current_session_.reset();
  classic_server_stats_.clear();
  doh_server_stats_.clear();
  initial_timeout_ = base::TimeDelta();
  max_timeout_ = GetMaxTimeout();

  if (!new_session) {
    NotifyDohStatusObserversOfSessionChanged();
    return;
  }

  current_session_ = new_session->GetWeakPtr();

  initial_timeout_ = GetDefaultTimeout(current_session_->config());

  for (size_t i = 0; i < new_session->config().nameservers.size(); ++i) {
    classic_server_stats_.emplace_back(initial_timeout_, GetRttBuckets());
  }
  for (size_t i = 0; i < new_session->config().dns_over_https_servers.size();
       ++i) {
    doh_server_stats_.emplace_back(initial_timeout_, GetRttBuckets());
  }

  CHECK_EQ(new_session->config().nameservers.size(),
           classic_server_stats_.size());
  CHECK_EQ(new_session->config().dns_over_https_servers.size(),
           doh_server_stats_.size());

  NotifyDohStatusObserversOfSessionChanged();

  if (!doh_server_stats_.empty())
    NotifyDohStatusObserversOfUnavailable(network_change);
}

bool ResolveContext::IsCurrentSession(const DnsSession* session) const {
  CHECK(session);
  if (session == current_session_.get()) {
    CHECK_EQ(current_session_->config().nameservers.size(),
             classic_server_stats_.size());
    CHECK_EQ(current_session_->config().dns_over_https_servers.size(),
             doh_server_stats_.size());
    return true;
  }

  return false;
}

ResolveContext::ServerStats* ResolveContext::GetServerStats(
    size_t server_index,
    bool is_doh_server) {
  if (!is_doh_server) {
    CHECK_LT(server_index, classic_server_stats_.size());
    return &classic_server_stats_[server_index];
  } else {
    CHECK_LT(server_index, doh_server_stats_.size());
    return &doh_server_stats_[server_index];
  }
}

base::TimeDelta ResolveContext::NextTimeoutHelper(ServerStats* server_stats,
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

void ResolveContext::RecordRttForUma(size_t server_index,
                                     bool is_doh_server,
                                     base::TimeDelta rtt,
                                     int rv,
                                     const DnsSession* session) {
  DCHECK(IsCurrentSession(session));

  std::string query_type;
  std::string provider_id;
  if (is_doh_server) {
    // Secure queries are validated if the DoH server state is available.
    if (GetDohServerAvailability(server_index, session))
      query_type = "SecureValidated";
    else
      query_type = "SecureNotValidated";
    provider_id = GetDohProviderIdForHistogramFromDohConfig(
        current_session_->config().dns_over_https_servers[server_index]);
  } else {
    query_type = "Insecure";
    provider_id = GetDohProviderIdForHistogramFromNameserver(
        current_session_->config().nameservers[server_index]);
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

void ResolveContext::NotifyDohStatusObserversOfSessionChanged() {
  for (auto& observer : doh_status_observers_)
    observer.OnSessionChanged();
}

void ResolveContext::NotifyDohStatusObserversOfUnavailable(
    bool network_change) {
  for (auto& observer : doh_status_observers_)
    observer.OnDohServerUnavailable(network_change);
}

// static
bool ResolveContext::ServerStatsToDohAvailability(
    const ResolveContext::ServerStats& stats) {
  return stats.last_failure_count < kAutomaticModeFailureLimit &&
         stats.current_connection_success;
}

}  // namespace net
