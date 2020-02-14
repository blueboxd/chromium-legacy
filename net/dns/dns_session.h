// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_SESSION_H_
#define NET_DNS_DNS_SESSION_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "net/base/rand_callback.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_socket_pool.h"

namespace net {

class DatagramClientSocket;
class NetLog;
struct NetLogSource;
class ResolveContext;
class StreamSocket;

// Number of failures allowed before a DoH server is designated 'unavailable'.
// In AUTOMATIC mode, non-probe DoH queries should not be sent to DoH servers
// that have reached this limit.
// This limit is different from the failure limit that governs insecure async
// resolver bypass in several ways: the failures need not be consecutive,
// NXDOMAIN responses are never counted as failures, and the outcome of
// fallback queries is not taken into account.
const int kAutomaticModeFailureLimit = 10;

// Session parameters and state shared between DNS transactions.
// Ref-counted so that DnsClient::Request can keep working in absence of
// DnsClient. A DnsSession must be recreated when DnsConfig changes.
class NET_EXPORT_PRIVATE DnsSession : public base::RefCounted<DnsSession> {
 public:
  typedef base::Callback<int()> RandCallback;

  class NET_EXPORT_PRIVATE SocketLease {
   public:
    SocketLease(scoped_refptr<DnsSession> session,
                size_t server_index,
                std::unique_ptr<DatagramClientSocket> socket);
    ~SocketLease();

    size_t server_index() const { return server_index_; }

    DatagramClientSocket* socket() { return socket_.get(); }

   private:
    scoped_refptr<DnsSession> session_;
    size_t server_index_;
    std::unique_ptr<DatagramClientSocket> socket_;

    DISALLOW_COPY_AND_ASSIGN(SocketLease);
  };

  DnsSession(const DnsConfig& config,
             std::unique_ptr<DnsSocketPool> socket_pool,
             const RandIntCallback& rand_int_callback,
             NetLog* net_log);

  const DnsConfig& config() const { return config_; }
  NetLog* net_log() const { return net_log_; }

  void UpdateTimeouts(NetworkChangeNotifier::ConnectionType type);
  void InitializeServerStats();

  // Return the next random query ID.
  uint16_t NextQueryId() const;

  // TODO(crbug.com/1045507): Rework the server index selection logic and
  // interface to not be susceptible to race conditions on server
  // availability/failure-tracking changing between attempts. As-is, this code
  // can easily result in working servers getting skipped and failing servers
  // getting extra attempts (further inflating the failure tracking).

  // Return the (potentially rotating) index of the first configured server (to
  // be passed to [Doh]ServerIndexToUse()).
  size_t FirstServerIndex(bool doh_server);

  // Find the index of a non-DoH server to use for this attempt.  Starts from
  // |starting_server| and finds the first eligible server (wrapping around as
  // necessary) below failure limits, or if no eligible servers are below
  // failure limits, the one with the oldest last failure.
  size_t ServerIndexToUse(size_t starting_server);

  // TODO(b/1022059): Remove once all server stats are moved to ResolveContext.
  base::TimeTicks GetLastDohFailure(size_t server_index);
  int GetLastDohFailureCount(size_t server_index);

  // Record that server failed to respond (due to SRV_FAIL or timeout). If
  // |is_doh_server| and the number of failures has surpassed a threshold,
  // sets the DoH probe state to unavailable.
  void RecordServerFailure(size_t server_index,
                           bool is_doh_server,
                           ResolveContext* resolve_context);

  // Record that server responded successfully.
  void RecordServerSuccess(size_t server_index, bool is_doh_server);

  // Record how long it took to receive a response from the server.
  void RecordRtt(size_t server_index,
                 bool is_doh_server,
                 bool is_validated_doh_server,
                 base::TimeDelta rtt,
                 int rv);

  // Return the timeout for the next query. |attempt| counts from 0 and is used
  // for exponential backoff.
  base::TimeDelta NextTimeout(size_t server_index, int attempt);

  // Return the timeout for the next DoH query.
  base::TimeDelta NextDohTimeout(size_t doh_server_index);

  // Allocate a socket, already connected to the server address.
  // When the SocketLease is destroyed, the socket will be freed.
  std::unique_ptr<SocketLease> AllocateSocket(size_t server_index,
                                              const NetLogSource& source);

  // Creates a StreamSocket from the factory for a transaction over TCP. These
  // sockets are not pooled.
  std::unique_ptr<StreamSocket> CreateTCPSocket(size_t server_index,
                                                const NetLogSource& source);

  base::WeakPtr<DnsSession> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void InvalidateWeakPtrsForTesting();

 private:
  friend class base::RefCounted<DnsSession>;
  struct ServerStats;
  ~DnsSession();

  // Release a socket.
  void FreeSocket(size_t server_index,
                  std::unique_ptr<DatagramClientSocket> socket);

  // Returns the ServerStats for the designated server. Returns nullptr if no
  // ServerStats found.
  ServerStats* GetServerStats(size_t server_index, bool is_doh_server);

  // Return the timeout for the next query.
  base::TimeDelta NextTimeoutHelper(ServerStats* server_stats, int attempt);

  // Record the time to perform a query.
  void RecordRttForUma(size_t server_index,
                       bool is_doh_server,
                       bool is_validated_doh_server,
                       base::TimeDelta rtt,
                       int rv);

  const DnsConfig config_;
  std::unique_ptr<DnsSocketPool> socket_pool_;
  RandCallback rand_callback_;
  NetLog* net_log_;

  // Current index into |config_.nameservers| to begin resolution with.
  int server_index_;

  base::TimeDelta initial_timeout_;
  base::TimeDelta max_timeout_;

  // TODO(crbug.com/1022059): Move all handling of ServerStats (both for DoH and
  // non-DoH) to ResolveContext.

  // Track runtime statistics of each insecure DNS server.
  std::vector<std::unique_ptr<ServerStats>> server_stats_;

  // Track runtime statistics of each DoH server.
  std::vector<std::unique_ptr<ServerStats>> doh_server_stats_;

  base::WeakPtrFactory<DnsSession> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DnsSession);
};

}  // namespace net

#endif  // NET_DNS_DNS_SESSION_H_
