// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/resolve_context.h"

#include <stdint.h>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "net/base/address_list.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_session.h"
#include "net/dns/dns_socket_pool.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver_source.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/dns_query_type.h"
#include "net/socket/socket_test_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/url_request/url_request_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class ResolveContextTest : public TestWithTaskEnvironment {
 public:
  ResolveContextTest() = default;

  scoped_refptr<DnsSession> CreateDnsSession(const DnsConfig& config) {
    auto null_random_callback =
        base::BindRepeating([](int, int) -> int { IMMEDIATE_CRASH(); });
    std::unique_ptr<DnsSocketPool> dns_socket_pool =
        DnsSocketPool::CreateNull(socket_factory_.get(), null_random_callback);

    return base::MakeRefCounted<DnsSession>(config, std::move(dns_socket_pool),
                                            null_random_callback,
                                            nullptr /* netlog */);
  }

 private:
  std::unique_ptr<MockClientSocketFactory> socket_factory_ =
      std::make_unique<MockClientSocketFactory>();
};

DnsConfig CreateDnsConfig(int num_servers, int num_doh_servers) {
  DnsConfig config;
  for (int i = 0; i < num_servers; ++i) {
    IPEndPoint dns_endpoint(IPAddress(192, 168, 1, static_cast<uint8_t>(i)),
                            dns_protocol::kDefaultPort);
    config.nameservers.push_back(dns_endpoint);
  }
  for (int i = 0; i < num_doh_servers; ++i) {
    std::string server_template(
        base::StringPrintf("https://mock.http/doh_test_%d{?dns}", i));
    config.dns_over_https_servers.push_back(DnsConfig::DnsOverHttpsServerConfig(
        server_template, true /* is_post */));
  }

  return config;
}

// Simulate a new session with the same pointer as an old deleted session by
// invalidating WeakPtrs.
TEST_F(ResolveContextTest, ReusedSessionPointer) {
  DnsConfig config =
      CreateDnsConfig(1 /* num_servers */, 3 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);

  URLRequestContext request_context;
  ResolveContext context(&request_context, true /* enable_caching */);
  context.InvalidateCaches(session.get());

  // Mark probe success for the "original" (pre-invalidation) session.
  context.SetProbeSuccess(1u, true, session.get());
  ASSERT_TRUE(context.GetDohServerAvailability(1u, session.get()));

  // Simulate session destruction and recreation on the same pointer.
  session->InvalidateWeakPtrsForTesting();

  // Expect |session| should now be treated as a new session, not matching
  // |context|'s "current" session. Expect availability from the "old" session
  // should not be read and SetProbeSuccess() should have no effect because the
  // "new" session has not yet been marked as "current" through
  // InvalidateCaches().
  EXPECT_FALSE(context.GetDohServerAvailability(1u, session.get()));
  context.SetProbeSuccess(1u, true, session.get());
  EXPECT_FALSE(context.GetDohServerAvailability(1u, session.get()));
}

TEST_F(ResolveContextTest, DohServerAvailability_InitialAvailability) {
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);

  URLRequestContext request_context;
  ResolveContext context(&request_context, true /* enable_caching */);
  context.InvalidateCaches(session.get());

  EXPECT_EQ(context.NumAvailableDohServers(session.get()), 0u);
  EXPECT_EQ(base::nullopt,
            context.DohServerIndexToUse(0u, DnsConfig::SecureDnsMode::AUTOMATIC,
                                        session.get()));
}

TEST_F(ResolveContextTest, DohServerAvailability_ProbeSuccess) {
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);

  URLRequestContext request_context;
  ResolveContext context(&request_context, true /* enable_caching */);
  context.InvalidateCaches(session.get());

  ASSERT_EQ(context.NumAvailableDohServers(session.get()), 0u);

  context.SetProbeSuccess(1, true /* success */, session.get());
  EXPECT_EQ(context.NumAvailableDohServers(session.get()), 1u);
  EXPECT_THAT(context.DohServerIndexToUse(
                  0u, DnsConfig::SecureDnsMode::AUTOMATIC, session.get()),
              testing::Optional(1u));
}

TEST_F(ResolveContextTest, DohServerAvailability_ProbeFailure) {
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);

  URLRequestContext request_context;
  ResolveContext context(&request_context, true /* enable_caching */);
  context.InvalidateCaches(session.get());

  context.SetProbeSuccess(1, true /* success */, session.get());
  ASSERT_EQ(context.NumAvailableDohServers(session.get()), 1u);

  context.SetProbeSuccess(1, false /* success */, session.get());
  EXPECT_EQ(context.NumAvailableDohServers(session.get()), 0u);
  EXPECT_EQ(base::nullopt,
            context.DohServerIndexToUse(0u, DnsConfig::SecureDnsMode::AUTOMATIC,
                                        session.get()));
}

TEST_F(ResolveContextTest, DohServerAvailability_NoCurrentSession) {
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);

  URLRequestContext request_context;
  ResolveContext context(&request_context, true /* enable_caching */);

  context.SetProbeSuccess(1u, true, session.get());

  EXPECT_EQ(base::nullopt,
            context.DohServerIndexToUse(0, DnsConfig::SecureDnsMode::AUTOMATIC,
                                        session.get()));
  EXPECT_EQ(0u, context.NumAvailableDohServers(session.get()));
  EXPECT_FALSE(context.GetDohServerAvailability(1, session.get()));
}

TEST_F(ResolveContextTest, DohServerAvailability_DifferentSession) {
  DnsConfig config1 =
      CreateDnsConfig(1 /* num_servers */, 3 /* num_doh_servers */);
  scoped_refptr<DnsSession> session1 = CreateDnsSession(config1);

  DnsConfig config2 =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session2 = CreateDnsSession(config2);

  URLRequestContext request_context;
  ResolveContext context(&request_context, true /* enable_caching */);
  context.InvalidateCaches(session2.get());

  // Use current session to set a probe result.
  context.SetProbeSuccess(1u, true, session2.get());

  EXPECT_EQ(base::nullopt,
            context.DohServerIndexToUse(0, DnsConfig::SecureDnsMode::AUTOMATIC,
                                        session1.get()));
  EXPECT_EQ(0u, context.NumAvailableDohServers(session1.get()));
  EXPECT_FALSE(context.GetDohServerAvailability(1u, session1.get()));

  // Different session for SetProbeResult should have no effect.
  ASSERT_TRUE(context.GetDohServerAvailability(1u, session2.get()));
  context.SetProbeSuccess(1u, false, session1.get());
  EXPECT_TRUE(context.GetDohServerAvailability(1u, session2.get()));
}

TEST_F(ResolveContextTest, DohServerIndexToUse) {
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);

  URLRequestContext request_context;
  ResolveContext context(&request_context, true /* enable_caching */);
  context.InvalidateCaches(session.get());

  context.SetProbeSuccess(0u, true /* success */, session.get());
  EXPECT_THAT(context.DohServerIndexToUse(
                  0u, DnsConfig::SecureDnsMode::AUTOMATIC, session.get()),
              testing::Optional(0u));
  EXPECT_THAT(context.DohServerIndexToUse(
                  1u, DnsConfig::SecureDnsMode::AUTOMATIC, session.get()),
              testing::Optional(0u));
}

TEST_F(ResolveContextTest, DohServerIndexToUse_NoneEligible) {
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);

  URLRequestContext request_context;
  ResolveContext context(&request_context, true /* enable_caching */);
  context.InvalidateCaches(session.get());

  EXPECT_EQ(base::nullopt,
            context.DohServerIndexToUse(0u, DnsConfig::SecureDnsMode::AUTOMATIC,
                                        session.get()));
  EXPECT_EQ(base::nullopt,
            context.DohServerIndexToUse(1u, DnsConfig::SecureDnsMode::AUTOMATIC,
                                        session.get()));
}

TEST_F(ResolveContextTest, DohServerIndexToUse_SecureMode) {
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);

  URLRequestContext request_context;
  ResolveContext context(&request_context, true /* enable_caching */);
  context.InvalidateCaches(session.get());

  EXPECT_THAT(context.DohServerIndexToUse(0u, DnsConfig::SecureDnsMode::SECURE,
                                          session.get()),
              testing::Optional(0u));
  EXPECT_THAT(context.DohServerIndexToUse(1u, DnsConfig::SecureDnsMode::SECURE,
                                          session.get()),
              testing::Optional(1u));
}

class TestDnsObserver : public NetworkChangeNotifier::DNSObserver {
 public:
  void OnDNSChanged() override { ++dns_changed_calls_; }

  int dns_changed_calls() const { return dns_changed_calls_; }

 private:
  int dns_changed_calls_ = 0;
};

TEST_F(ResolveContextTest, DohServerAvailabilityNotification) {
  test::ScopedMockNetworkChangeNotifier mock_network_change_notifier;
  TestDnsObserver config_observer;
  NetworkChangeNotifier::AddDNSObserver(&config_observer);

  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);

  URLRequestContext request_context;
  ResolveContext context(&request_context, true /* enable_caching */);
  context.InvalidateCaches(session.get());

  base::RunLoop().RunUntilIdle();  // Notifications are async.
  EXPECT_EQ(0, config_observer.dns_changed_calls());

  // Expect notification on first available DoH server.
  context.SetProbeSuccess(0, true /* success */, session.get());
  base::RunLoop().RunUntilIdle();  // Notifications are async.
  EXPECT_EQ(1, config_observer.dns_changed_calls());

  // No notifications as additional servers are available or unavailable.
  context.SetProbeSuccess(1, true /* success */, session.get());
  context.SetProbeSuccess(0, false /* success */, session.get());
  base::RunLoop().RunUntilIdle();  // Notifications are async.
  EXPECT_EQ(1, config_observer.dns_changed_calls());

  // Expect notification on last server unavailable.
  context.SetProbeSuccess(1, false /* success */, session.get());
  base::RunLoop().RunUntilIdle();  // Notifications are async.
  EXPECT_EQ(2, config_observer.dns_changed_calls());

  NetworkChangeNotifier::RemoveDNSObserver(&config_observer);
}

TEST_F(ResolveContextTest, HostCacheInvalidation) {
  ResolveContext context(nullptr /* url_request_context */,
                         true /* enable_caching */);

  base::TimeTicks now;
  HostCache::Key key("example.com", DnsQueryType::UNSPECIFIED, 0,
                     HostResolverSource::ANY, NetworkIsolationKey());
  context.host_cache()->Set(
      key,
      HostCache::Entry(OK, AddressList(), HostCache::Entry::SOURCE_UNKNOWN),
      now, base::TimeDelta::FromSeconds(10));
  ASSERT_TRUE(context.host_cache()->Lookup(key, now));

  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);
  context.InvalidateCaches(session.get());

  EXPECT_FALSE(context.host_cache()->Lookup(key, now));

  // Re-add to the host cache and now add some DoH server status.
  context.host_cache()->Set(
      key,
      HostCache::Entry(OK, AddressList(), HostCache::Entry::SOURCE_UNKNOWN),
      now, base::TimeDelta::FromSeconds(10));
  context.SetProbeSuccess(0u, true /* success */, session.get());
  ASSERT_TRUE(context.host_cache()->Lookup(key, now));
  ASSERT_TRUE(context.GetDohServerAvailability(0u, session.get()));

  // Invalidate again.
  DnsConfig config2 =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session2 = CreateDnsSession(config2);
  context.InvalidateCaches(session2.get());

  EXPECT_FALSE(context.host_cache()->Lookup(key, now));
  EXPECT_FALSE(context.GetDohServerAvailability(0u, session.get()));
  EXPECT_FALSE(context.GetDohServerAvailability(0u, session2.get()));
}

TEST_F(ResolveContextTest, HostCacheInvalidation_SameSession) {
  ResolveContext context(nullptr /* url_request_context */,
                         true /* enable_caching */);
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);

  // Initial invalidation just to set the session.
  context.InvalidateCaches(session.get());

  // Add to the host cache and add some DoH server status.
  base::TimeTicks now;
  HostCache::Key key("example.com", DnsQueryType::UNSPECIFIED, 0,
                     HostResolverSource::ANY, NetworkIsolationKey());
  context.host_cache()->Set(
      key,
      HostCache::Entry(OK, AddressList(), HostCache::Entry::SOURCE_UNKNOWN),
      now, base::TimeDelta::FromSeconds(10));
  context.SetProbeSuccess(0u, true /* success */, session.get());
  ASSERT_TRUE(context.host_cache()->Lookup(key, now));
  ASSERT_TRUE(context.GetDohServerAvailability(0u, session.get()));

  // Invalidate again with the same session.
  context.InvalidateCaches(session.get());

  // Expect host cache to be invalidated but not the per-session data.
  EXPECT_FALSE(context.host_cache()->Lookup(key, now));
  EXPECT_TRUE(context.GetDohServerAvailability(0u, session.get()));
}

TEST_F(ResolveContextTest, Failures_Consecutive) {
  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);
  context.InvalidateCaches(session.get());

  // Expect server preference to change after |config.attempts| failures.
  for (int i = 0; i < config.attempts; i++) {
    EXPECT_EQ(context.ClassicServerIndexToUse(0u /* starting_server */,
                                              session.get()),
              0u);
    EXPECT_EQ(context.ClassicServerIndexToUse(1u /* starting_server */,
                                              session.get()),
              1u);
    context.RecordServerFailure(1u /* server_index */,
                                false /* is_doh_server */, session.get());
  }
  EXPECT_EQ(
      context.ClassicServerIndexToUse(0u /* starting_server */, session.get()),
      0u);
  EXPECT_EQ(
      context.ClassicServerIndexToUse(1u /* starting_server */, session.get()),
      0u);

  // Expect failures to be reset on successful request.
  context.RecordServerSuccess(1u /* server_index */, false /* is_doh_server */,
                              session.get());
  EXPECT_EQ(
      context.ClassicServerIndexToUse(0u /* starting_server */, session.get()),
      0u);
  EXPECT_EQ(
      context.ClassicServerIndexToUse(1u /* starting_server */, session.get()),
      1u);
}

TEST_F(ResolveContextTest, Failures_NonConsecutive) {
  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);
  context.InvalidateCaches(session.get());

  for (int i = 0; i < config.attempts - 1; i++) {
    EXPECT_EQ(context.ClassicServerIndexToUse(1u /* starting_server */,
                                              session.get()),
              1u);
    context.RecordServerFailure(1u /* server_index */,
                                false /* is_doh_server */, session.get());
  }
  EXPECT_EQ(
      context.ClassicServerIndexToUse(1u /* starting_server */, session.get()),
      1u);

  context.RecordServerSuccess(1u /* server_index */, false /* is_doh_server */,
                              session.get());
  EXPECT_EQ(
      context.ClassicServerIndexToUse(1u /* starting_server */, session.get()),
      1u);

  // Expect server stay preferred through non-consecutive failures.
  context.RecordServerFailure(1u /* server_index */, false /* is_doh_server */,
                              session.get());
  EXPECT_EQ(
      context.ClassicServerIndexToUse(1u /* starting_server */, session.get()),
      1u);
}

TEST_F(ResolveContextTest, Failures_NoSession) {
  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);

  // No expected change from recording failures.
  for (int i = 0; i < config.attempts; i++) {
    EXPECT_EQ(context.ClassicServerIndexToUse(1u /* starting_server */,
                                              session.get()),
              1u);
    context.RecordServerFailure(1u /* server_index */,
                                false /* is_doh_server */, session.get());
  }
  EXPECT_EQ(
      context.ClassicServerIndexToUse(1u /* starting_server */, session.get()),
      1u);
}

TEST_F(ResolveContextTest, Failures_DifferentSession) {
  DnsConfig config1 =
      CreateDnsConfig(1 /* num_servers */, 3 /* num_doh_servers */);
  scoped_refptr<DnsSession> session1 = CreateDnsSession(config1);

  DnsConfig config2 =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session2 = CreateDnsSession(config2);

  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);
  context.InvalidateCaches(session2.get());

  // No change from recording failures to wrong session.
  for (int i = 0; i < config1.attempts; i++) {
    EXPECT_EQ(context.ClassicServerIndexToUse(1u /* starting_server */,
                                              session2.get()),
              1u);
    context.RecordServerFailure(1u /* server_index */,
                                false /* is_doh_server */, session1.get());
  }
  EXPECT_EQ(
      context.ClassicServerIndexToUse(1u /* starting_server */, session2.get()),
      1u);
}

// Test 2 of 3 servers failing.
TEST_F(ResolveContextTest, TwoFailures) {
  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);
  DnsConfig config =
      CreateDnsConfig(3 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);
  context.InvalidateCaches(session.get());

  // Expect server preference to change after |config.attempts| failures.
  for (int i = 0; i < config.attempts; i++) {
    EXPECT_EQ(context.ClassicServerIndexToUse(0u /* starting_server */,
                                              session.get()),
              0u);
    EXPECT_EQ(context.ClassicServerIndexToUse(1u /* starting_server */,
                                              session.get()),
              1u);
    EXPECT_EQ(context.ClassicServerIndexToUse(2u /* starting_server */,
                                              session.get()),
              2u);
    context.RecordServerFailure(0u /* server_index */,
                                false /* is_doh_server */, session.get());
    context.RecordServerFailure(1u /* server_index */,
                                false /* is_doh_server */, session.get());
  }
  EXPECT_EQ(
      context.ClassicServerIndexToUse(0u /* starting_server */, session.get()),
      2u);
  EXPECT_EQ(
      context.ClassicServerIndexToUse(1u /* starting_server */, session.get()),
      2u);
  EXPECT_EQ(
      context.ClassicServerIndexToUse(2u /* starting_server */, session.get()),
      2u);

  // Expect failures to be reset on successful request.
  context.RecordServerSuccess(0u /* server_index */, false /* is_doh_server */,
                              session.get());
  context.RecordServerSuccess(1u /* server_index */, false /* is_doh_server */,
                              session.get());
  EXPECT_EQ(
      context.ClassicServerIndexToUse(0u /* starting_server */, session.get()),
      0u);
  EXPECT_EQ(
      context.ClassicServerIndexToUse(1u /* starting_server */, session.get()),
      1u);
  EXPECT_EQ(
      context.ClassicServerIndexToUse(2u /* starting_server */, session.get()),
      2u);
}

TEST_F(ResolveContextTest, DohFailures_Consecutive) {
  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);
  context.InvalidateCaches(session.get());

  context.SetProbeSuccess(1u /* doh_server_index */, true /* success */,
                          session.get());

  for (size_t i = 0; i < ResolveContext::kAutomaticModeFailureLimit; i++) {
    EXPECT_THAT(context.DohServerIndexToUse(1u /* starting_doh_server_index */,
                                            DnsConfig::SecureDnsMode::AUTOMATIC,
                                            session.get()),
                testing::Optional(1u));
    EXPECT_EQ(1u, context.NumAvailableDohServers(session.get()));
    context.RecordServerFailure(1u /* server_index */, true /* is_doh_server */,
                                session.get());
  }
  EXPECT_EQ(context.DohServerIndexToUse(1u /* starting_doh_server_index */,
                                        DnsConfig::SecureDnsMode::AUTOMATIC,
                                        session.get()),
            base::nullopt);
  EXPECT_EQ(0u, context.NumAvailableDohServers(session.get()));
}

TEST_F(ResolveContextTest, DohFailures_NonConsecutive) {
  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);
  context.InvalidateCaches(session.get());

  context.SetProbeSuccess(1, true /* success */, session.get());

  for (size_t i = 0; i < ResolveContext::kAutomaticModeFailureLimit - 1; i++) {
    EXPECT_THAT(context.DohServerIndexToUse(1u /* starting_doh_server_index */,
                                            DnsConfig::SecureDnsMode::AUTOMATIC,
                                            session.get()),
                testing::Optional(1u));
    EXPECT_EQ(1u, context.NumAvailableDohServers(session.get()));
    context.RecordServerFailure(1u /* server_index */, true /* is_doh_server */,
                                session.get());
  }
  EXPECT_THAT(context.DohServerIndexToUse(1u /* starting_doh_server_index */,
                                          DnsConfig::SecureDnsMode::AUTOMATIC,
                                          session.get()),
              testing::Optional(1u));
  EXPECT_EQ(1u, context.NumAvailableDohServers(session.get()));

  context.RecordServerSuccess(1u /* server_index */, true /* is_doh_server */,
                              session.get());
  EXPECT_THAT(context.DohServerIndexToUse(1u /* starting_doh_server_index */,
                                          DnsConfig::SecureDnsMode::AUTOMATIC,
                                          session.get()),
              testing::Optional(1u));
  EXPECT_EQ(1u, context.NumAvailableDohServers(session.get()));

  context.RecordServerFailure(1u /* server_index */, true /* is_doh_server */,
                              session.get());
  EXPECT_EQ(context.DohServerIndexToUse(1u /* starting_doh_server_index */,
                                        DnsConfig::SecureDnsMode::AUTOMATIC,
                                        session.get()),
            base::nullopt);
  EXPECT_EQ(0u, context.NumAvailableDohServers(session.get()));
}

TEST_F(ResolveContextTest, DohFailures_NoSession) {
  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);

  context.SetProbeSuccess(1u /* doh_server_index */, true /* success */,
                          session.get());

  // No expected change from recording failures.
  for (size_t i = 0; i < ResolveContext::kAutomaticModeFailureLimit; i++) {
    EXPECT_EQ(0u, context.NumAvailableDohServers(session.get()));
    context.RecordServerFailure(1u /* server_index */, true /* is_doh_server */,
                                session.get());
  }
  EXPECT_EQ(0u, context.NumAvailableDohServers(session.get()));
}

TEST_F(ResolveContextTest, DohFailures_DifferentSession) {
  DnsConfig config1 =
      CreateDnsConfig(1 /* num_servers */, 3 /* num_doh_servers */);
  scoped_refptr<DnsSession> session1 = CreateDnsSession(config1);

  DnsConfig config2 =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session2 = CreateDnsSession(config2);

  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);
  context.InvalidateCaches(session2.get());

  context.SetProbeSuccess(1u /* doh_server_index */, true /* success */,
                          session2.get());
  ASSERT_EQ(1u, context.NumAvailableDohServers(session2.get()));

  // No change from recording failures to wrong session.
  for (size_t i = 0; i < ResolveContext::kAutomaticModeFailureLimit; i++) {
    EXPECT_EQ(1u, context.NumAvailableDohServers(session2.get()));
    context.RecordServerFailure(1u /* server_index */, true /* is_doh_server */,
                                session1.get());
  }
  EXPECT_EQ(1u, context.NumAvailableDohServers(session2.get()));
}

// Test 2 of 3 DoH servers failing.
TEST_F(ResolveContextTest, TwoDohFailures) {
  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 3 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);
  context.InvalidateCaches(session.get());

  context.SetProbeSuccess(0u /* doh_server_index */, true /* success */,
                          session.get());
  context.SetProbeSuccess(1u /* doh_server_index */, true /* success */,
                          session.get());
  context.SetProbeSuccess(2u /* doh_server_index */, true /* success */,
                          session.get());

  // Expect server preference to change after |config.attempts| failures.
  for (int i = 0; i < config.attempts; i++) {
    EXPECT_THAT(context.DohServerIndexToUse(0u /* starting_doh_server_index */,
                                            DnsConfig::SecureDnsMode::AUTOMATIC,
                                            session.get()),
                testing::Optional(0u));
    EXPECT_THAT(context.DohServerIndexToUse(1u /* starting_doh_server_index */,
                                            DnsConfig::SecureDnsMode::AUTOMATIC,
                                            session.get()),
                testing::Optional(1u));
    EXPECT_THAT(context.DohServerIndexToUse(2u /* starting_doh_server_index */,
                                            DnsConfig::SecureDnsMode::AUTOMATIC,
                                            session.get()),
                testing::Optional(2u));
    context.RecordServerFailure(0u /* server_index */, true /* is_doh_server */,
                                session.get());
    context.RecordServerFailure(1u /* server_index */, true /* is_doh_server */,
                                session.get());
  }
  EXPECT_THAT(context.DohServerIndexToUse(0u /* starting_doh_server_index */,
                                          DnsConfig::SecureDnsMode::AUTOMATIC,
                                          session.get()),
              testing::Optional(2u));
  EXPECT_THAT(context.DohServerIndexToUse(1u /* starting_doh_server_index */,
                                          DnsConfig::SecureDnsMode::AUTOMATIC,
                                          session.get()),
              testing::Optional(2u));
  EXPECT_THAT(context.DohServerIndexToUse(2u /* starting_doh_server_index */,
                                          DnsConfig::SecureDnsMode::AUTOMATIC,
                                          session.get()),
              testing::Optional(2u));
}

// Expect default calculated timeout to be within 10ms of |DnsConfig::timeout|.
TEST_F(ResolveContextTest, Timeout_Default) {
  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);
  context.InvalidateCaches(session.get());

  base::TimeDelta delta =
      context.NextClassicTimeout(0 /* server_index */, 0 /* attempt */,
                                 session.get()) -
      config.timeout;
  EXPECT_LE(delta, base::TimeDelta::FromMilliseconds(10));
  delta = context.NextDohTimeout(0 /* doh_server_index */, session.get()) -
          config.timeout;
  EXPECT_LE(delta, base::TimeDelta::FromMilliseconds(10));
}

// Expect short calculated timeout to be within 10ms of |DnsConfig::timeout|.
TEST_F(ResolveContextTest, Timeout_ShortConfigured) {
  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  config.timeout = base::TimeDelta::FromMilliseconds(15);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);
  context.InvalidateCaches(session.get());

  base::TimeDelta delta =
      context.NextClassicTimeout(0 /* server_index */, 0 /* attempt */,
                                 session.get()) -
      config.timeout;
  EXPECT_LE(delta, base::TimeDelta::FromMilliseconds(10));
  delta = context.NextDohTimeout(0 /* doh_server_index */, session.get()) -
          config.timeout;
  EXPECT_LE(delta, base::TimeDelta::FromMilliseconds(10));
}

// Expect long calculated timeout to be equal to |DnsConfig::timeout|.
// (Default max timeout is 5 seconds, so NextTimeout should return exactly
// the config timeout.)
TEST_F(ResolveContextTest, Timeout_LongConfigured) {
  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  config.timeout = base::TimeDelta::FromSeconds(15);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);
  context.InvalidateCaches(session.get());

  EXPECT_EQ(context.NextClassicTimeout(0 /* server_index */, 0 /* attempt */,
                                       session.get()),
            config.timeout);
  EXPECT_EQ(context.NextDohTimeout(0 /* doh_server_index */, session.get()),
            config.timeout);
}

// Expect timeouts to increase on recording long round-trip times.
TEST_F(ResolveContextTest, Timeout_LongRtt) {
  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);
  context.InvalidateCaches(session.get());

  for (int i = 0; i < 50; ++i) {
    context.RecordRtt(0u /* server_index */, false /* is_doh_server */,
                      base::TimeDelta::FromMinutes(10), OK, session.get());
    context.RecordRtt(1u /* server_index */, true /* is_doh_server */,
                      base::TimeDelta::FromMinutes(10), OK, session.get());
  }

  // Expect servers with high recorded RTT to have increased timeouts (>10ms).
  base::TimeDelta delta =
      context.NextClassicTimeout(0u /* server_index */, 0 /* attempt */,
                                 session.get()) -
      config.timeout;
  EXPECT_GT(delta, base::TimeDelta::FromMilliseconds(10));
  delta = context.NextDohTimeout(1u, session.get()) - config.timeout;
  EXPECT_GT(delta, base::TimeDelta::FromMilliseconds(10));

  // Servers without recorded RTT expected to remain the same (<=10ms).
  delta = context.NextClassicTimeout(1u /* server_index */, 0 /* attempt */,
                                     session.get()) -
          config.timeout;
  EXPECT_LE(delta, base::TimeDelta::FromMilliseconds(10));
  delta = context.NextDohTimeout(0u /* doh_server_index */, session.get()) -
          config.timeout;
  EXPECT_LE(delta, base::TimeDelta::FromMilliseconds(10));
}

// Expect recording round-trip times to have no affect on timeout without a
// current session.
TEST_F(ResolveContextTest, Timeout_NoSession) {
  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);

  for (int i = 0; i < 50; ++i) {
    context.RecordRtt(0u /* server_index */, false /* is_doh_server */,
                      base::TimeDelta::FromMinutes(10), OK, session.get());
    context.RecordRtt(1u /* server_index */, true /* is_doh_server */,
                      base::TimeDelta::FromMinutes(10), OK, session.get());
  }

  base::TimeDelta delta =
      context.NextClassicTimeout(0u /* server_index */, 0 /* attempt */,
                                 session.get()) -
      config.timeout;
  EXPECT_LE(delta, base::TimeDelta::FromMilliseconds(10));
  delta = context.NextDohTimeout(1u /* doh_server_index */, session.get()) -
          config.timeout;
  EXPECT_LE(delta, base::TimeDelta::FromMilliseconds(10));
}

// Expect recording round-trip times to have no affect on timeout without a
// current session.
TEST_F(ResolveContextTest, Timeout_DifferentSession) {
  DnsConfig config1 =
      CreateDnsConfig(1 /* num_servers */, 3 /* num_doh_servers */);
  scoped_refptr<DnsSession> session1 = CreateDnsSession(config1);

  DnsConfig config2 =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session2 = CreateDnsSession(config2);

  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);
  context.InvalidateCaches(session2.get());

  // Record RTT's to increase timeouts for current session.
  for (int i = 0; i < 50; ++i) {
    context.RecordRtt(0u /* server_index */, false /* is_doh_server */,
                      base::TimeDelta::FromMinutes(10), OK, session2.get());
    context.RecordRtt(1u /* server_index */, true /* is_doh_server */,
                      base::TimeDelta::FromMinutes(10), OK, session2.get());
  }

  // Expect normal short timeouts for other session.
  base::TimeDelta delta =
      context.NextClassicTimeout(0u /* server_index */, 0 /* attempt */,
                                 session1.get()) -
      config1.timeout;
  EXPECT_LE(delta, base::TimeDelta::FromMilliseconds(10));
  delta = context.NextDohTimeout(0u /* doh_server_index */, session1.get()) -
          config1.timeout;
  EXPECT_LE(delta, base::TimeDelta::FromMilliseconds(10));

  // Recording RTT's for other session should have no effect on current session
  // timeouts.
  base::TimeDelta timeout = context.NextClassicTimeout(
      0u /* server_index */, 0 /* attempt */, session2.get());
  for (int i = 0; i < 50; ++i) {
    context.RecordRtt(0u /* server_index */, false /* is_doh_server */,
                      base::TimeDelta::FromMilliseconds(1), OK, session1.get());
  }
  EXPECT_EQ(timeout,
            context.NextClassicTimeout(0u /* server_index */, 0 /* attempt */,
                                       session2.get()));
}

// Ensures that reported negative RTT values don't cause a crash. Regression
// test for https://crbug.com/753568.
TEST_F(ResolveContextTest, NegativeRtt) {
  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);
  context.InvalidateCaches(session.get());

  context.RecordRtt(0 /* server_index */, false /* is_doh_server */,
                    base::TimeDelta::FromMilliseconds(-1), OK /* rv */,
                    session.get());
  context.RecordRtt(0 /* server_index */, true /* is_doh_server */,
                    base::TimeDelta::FromMilliseconds(-1), OK /* rv */,
                    session.get());
}

}  // namespace
}  // namespace net
