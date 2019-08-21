// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/system_network_context_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "base/deferred_sequenced_task_runner.h"
#include "base/feature_list.h"
#include "base/optional.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/dns_util.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/system_connector.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/network_service_util.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/user_agent.h"
#include "net/socket/client_socket_pool_manager.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace {

void GetStubResolverConfig(
    bool* insecure_stub_resolver_enabled,
    net::DnsConfig::SecureDnsMode* secure_dns_mode,
    base::Optional<std::vector<network::mojom::DnsOverHttpsServerPtr>>*
        dns_over_https_servers) {
  dns_over_https_servers->reset();

  SystemNetworkContextManager::GetStubResolverConfigForTesting(
      insecure_stub_resolver_enabled, secure_dns_mode, dns_over_https_servers);
}

// Checks that the values returned by GetStubResolverConfigForTesting() match
// |async_dns_feature_enabled| (With empty DNS over HTTPS prefs). Then sets
// various DoH modes and DoH template strings and makes sure the settings are
// respected.
void RunStubResolverConfigTests(bool async_dns_feature_enabled) {
  // Check initial state.
  bool insecure_stub_resolver_enabled = !async_dns_feature_enabled;
  net::DnsConfig::SecureDnsMode secure_dns_mode;
  base::Optional<std::vector<network::mojom::DnsOverHttpsServerPtr>>
      dns_over_https_servers;
  GetStubResolverConfig(&insecure_stub_resolver_enabled, &secure_dns_mode,
                        &dns_over_https_servers);
  EXPECT_EQ(async_dns_feature_enabled, insecure_stub_resolver_enabled);
  EXPECT_EQ(net::DnsConfig::SecureDnsMode::OFF, secure_dns_mode);
  EXPECT_FALSE(dns_over_https_servers.has_value());

  std::string good_post_template = "https://foo.test/";
  std::string good_get_template = "https://bar.test/dns-query{?dns}";
  std::string bad_template = "dns-query{?dns}";
  std::string good_then_bad_template = good_get_template + " " + bad_template;
  std::string bad_then_good_template = bad_template + " " + good_get_template;
  std::string multiple_good_templates =
      "  " + good_get_template + "   " + good_post_template + "  ";

  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kDnsOverHttpsMode, kDnsOverHttpsModeSecure);
  local_state->SetString(prefs::kDnsOverHttpsTemplates, bad_template);
  GetStubResolverConfig(&insecure_stub_resolver_enabled, &secure_dns_mode,
                        &dns_over_https_servers);
  EXPECT_EQ(async_dns_feature_enabled, insecure_stub_resolver_enabled);
  EXPECT_EQ(net::DnsConfig::SecureDnsMode::SECURE, secure_dns_mode);
  EXPECT_FALSE(dns_over_https_servers.has_value());

  local_state->SetString(prefs::kDnsOverHttpsTemplates, good_post_template);
  GetStubResolverConfig(&insecure_stub_resolver_enabled, &secure_dns_mode,
                        &dns_over_https_servers);
  EXPECT_EQ(async_dns_feature_enabled, insecure_stub_resolver_enabled);
  EXPECT_EQ(net::DnsConfig::SecureDnsMode::SECURE, secure_dns_mode);
  ASSERT_TRUE(dns_over_https_servers.has_value());
  ASSERT_EQ(1u, dns_over_https_servers->size());
  EXPECT_EQ(good_post_template, dns_over_https_servers->at(0)->server_template);
  EXPECT_EQ(true, dns_over_https_servers->at(0)->use_post);

  local_state->SetString(prefs::kDnsOverHttpsMode, kDnsOverHttpsModeAutomatic);
  local_state->SetString(prefs::kDnsOverHttpsTemplates, bad_template);
  GetStubResolverConfig(&insecure_stub_resolver_enabled, &secure_dns_mode,
                        &dns_over_https_servers);
  EXPECT_EQ(async_dns_feature_enabled, insecure_stub_resolver_enabled);
  EXPECT_EQ(net::DnsConfig::SecureDnsMode::AUTOMATIC, secure_dns_mode);
  EXPECT_FALSE(dns_over_https_servers.has_value());

  local_state->SetString(prefs::kDnsOverHttpsTemplates, good_then_bad_template);
  GetStubResolverConfig(&insecure_stub_resolver_enabled, &secure_dns_mode,
                        &dns_over_https_servers);
  EXPECT_EQ(async_dns_feature_enabled, insecure_stub_resolver_enabled);
  EXPECT_EQ(net::DnsConfig::SecureDnsMode::AUTOMATIC, secure_dns_mode);
  ASSERT_TRUE(dns_over_https_servers.has_value());
  ASSERT_EQ(1u, dns_over_https_servers->size());
  EXPECT_EQ(good_get_template, dns_over_https_servers->at(0)->server_template);
  EXPECT_FALSE(dns_over_https_servers->at(0)->use_post);

  local_state->SetString(prefs::kDnsOverHttpsTemplates, bad_then_good_template);
  GetStubResolverConfig(&insecure_stub_resolver_enabled, &secure_dns_mode,
                        &dns_over_https_servers);
  EXPECT_EQ(async_dns_feature_enabled, insecure_stub_resolver_enabled);
  EXPECT_EQ(net::DnsConfig::SecureDnsMode::AUTOMATIC, secure_dns_mode);
  ASSERT_TRUE(dns_over_https_servers.has_value());
  ASSERT_EQ(1u, dns_over_https_servers->size());
  EXPECT_EQ(good_get_template, dns_over_https_servers->at(0)->server_template);
  EXPECT_FALSE(dns_over_https_servers->at(0)->use_post);

  local_state->SetString(prefs::kDnsOverHttpsTemplates,
                         multiple_good_templates);
  GetStubResolverConfig(&insecure_stub_resolver_enabled, &secure_dns_mode,
                        &dns_over_https_servers);
  EXPECT_EQ(async_dns_feature_enabled, insecure_stub_resolver_enabled);
  EXPECT_EQ(net::DnsConfig::SecureDnsMode::AUTOMATIC, secure_dns_mode);
  ASSERT_TRUE(dns_over_https_servers.has_value());
  ASSERT_EQ(2u, dns_over_https_servers->size());
  EXPECT_EQ(good_get_template, dns_over_https_servers->at(0)->server_template);
  EXPECT_FALSE(dns_over_https_servers->at(0)->use_post);
  EXPECT_EQ(good_post_template, dns_over_https_servers->at(1)->server_template);
  EXPECT_TRUE(dns_over_https_servers->at(1)->use_post);

  local_state->SetString(prefs::kDnsOverHttpsMode, kDnsOverHttpsModeOff);
  local_state->SetString(prefs::kDnsOverHttpsTemplates, good_get_template);
  GetStubResolverConfig(&insecure_stub_resolver_enabled, &secure_dns_mode,
                        &dns_over_https_servers);
  EXPECT_EQ(async_dns_feature_enabled, insecure_stub_resolver_enabled);
  EXPECT_EQ(net::DnsConfig::SecureDnsMode::OFF, secure_dns_mode);
  EXPECT_FALSE(dns_over_https_servers.has_value());

  local_state->SetString(prefs::kDnsOverHttpsMode, "no_match");
  GetStubResolverConfig(&insecure_stub_resolver_enabled, &secure_dns_mode,
                        &dns_over_https_servers);
  EXPECT_EQ(async_dns_feature_enabled, insecure_stub_resolver_enabled);
  EXPECT_EQ(net::DnsConfig::SecureDnsMode::OFF, secure_dns_mode);
  EXPECT_FALSE(dns_over_https_servers.has_value());

  // Test case with policy BuiltInDnsClientEnabled enabled. The DoH fields
  // should be unaffected.
  local_state->Set(prefs::kBuiltInDnsClientEnabled,
                   base::Value(!async_dns_feature_enabled));
  GetStubResolverConfig(&insecure_stub_resolver_enabled, &secure_dns_mode,
                        &dns_over_https_servers);
  EXPECT_EQ(!async_dns_feature_enabled, insecure_stub_resolver_enabled);
  EXPECT_EQ(net::DnsConfig::SecureDnsMode::OFF, secure_dns_mode);
  EXPECT_FALSE(dns_over_https_servers.has_value());
}

}  // namespace

using SystemNetworkContextManagerBrowsertest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(SystemNetworkContextManagerBrowsertest,
                       StubResolverDefaultConfig) {
  RunStubResolverConfigTests(base::FeatureList::IsEnabled(features::kAsyncDns));
}

IN_PROC_BROWSER_TEST_F(SystemNetworkContextManagerBrowsertest,
                       StaticAuthParams) {
  // Test defaults.
  network::mojom::HttpAuthStaticParamsPtr static_params =
      SystemNetworkContextManager::GetHttpAuthStaticParamsForTesting();
  EXPECT_THAT(static_params->supported_schemes,
              testing::ElementsAre("basic", "digest", "ntlm", "negotiate"));
  EXPECT_EQ("", static_params->gssapi_library_name);

  // Test that prefs are reflected in params.

  PrefService* local_state = g_browser_process->local_state();

  local_state->SetString(prefs::kAuthSchemes, "basic");
  static_params =
      SystemNetworkContextManager::GetHttpAuthStaticParamsForTesting();
  EXPECT_THAT(static_params->supported_schemes, testing::ElementsAre("basic"));

#if defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  const char dev_null[] = "/dev/null";
  local_state->SetString(prefs::kGSSAPILibraryName, dev_null);
  static_params =
      SystemNetworkContextManager::GetHttpAuthStaticParamsForTesting();
  EXPECT_EQ(dev_null, static_params->gssapi_library_name);
#endif
}

IN_PROC_BROWSER_TEST_F(SystemNetworkContextManagerBrowsertest, AuthParams) {
  // Test defaults.
  network::mojom::HttpAuthDynamicParamsPtr dynamic_params =
      SystemNetworkContextManager::GetHttpAuthDynamicParamsForTesting();
  EXPECT_EQ(false, dynamic_params->negotiate_disable_cname_lookup);
  EXPECT_EQ(false, dynamic_params->enable_negotiate_port);
  EXPECT_EQ("", dynamic_params->server_allowlist);
  EXPECT_EQ("", dynamic_params->delegate_allowlist);
  EXPECT_FALSE(dynamic_params->delegate_by_kdc_policy);

  PrefService* local_state = g_browser_process->local_state();

  local_state->SetBoolean(prefs::kDisableAuthNegotiateCnameLookup, true);
  dynamic_params =
      SystemNetworkContextManager::GetHttpAuthDynamicParamsForTesting();
  EXPECT_EQ(true, dynamic_params->negotiate_disable_cname_lookup);
  EXPECT_EQ(false, dynamic_params->enable_negotiate_port);
  EXPECT_EQ("", dynamic_params->server_allowlist);
  EXPECT_EQ("", dynamic_params->delegate_allowlist);
  EXPECT_FALSE(dynamic_params->delegate_by_kdc_policy);

  local_state->SetBoolean(prefs::kEnableAuthNegotiatePort, true);
  dynamic_params =
      SystemNetworkContextManager::GetHttpAuthDynamicParamsForTesting();
  EXPECT_EQ(true, dynamic_params->negotiate_disable_cname_lookup);
  EXPECT_EQ(true, dynamic_params->enable_negotiate_port);
  EXPECT_EQ("", dynamic_params->server_allowlist);
  EXPECT_EQ("", dynamic_params->delegate_allowlist);
  EXPECT_FALSE(dynamic_params->delegate_by_kdc_policy);

  const char kServerAllowList[] = "foo";
  local_state->SetString(prefs::kAuthServerWhitelist, kServerAllowList);
  dynamic_params =
      SystemNetworkContextManager::GetHttpAuthDynamicParamsForTesting();
  EXPECT_EQ(true, dynamic_params->negotiate_disable_cname_lookup);
  EXPECT_EQ(true, dynamic_params->enable_negotiate_port);
  EXPECT_EQ(kServerAllowList, dynamic_params->server_allowlist);
  EXPECT_EQ("", dynamic_params->delegate_allowlist);

  const char kDelegateAllowList[] = "bar, baz";
  local_state->SetString(prefs::kAuthNegotiateDelegateWhitelist,
                         kDelegateAllowList);
  dynamic_params =
      SystemNetworkContextManager::GetHttpAuthDynamicParamsForTesting();
  EXPECT_EQ(true, dynamic_params->negotiate_disable_cname_lookup);
  EXPECT_EQ(true, dynamic_params->enable_negotiate_port);
  EXPECT_EQ(kServerAllowList, dynamic_params->server_allowlist);
  EXPECT_EQ(kDelegateAllowList, dynamic_params->delegate_allowlist);
  EXPECT_FALSE(dynamic_params->delegate_by_kdc_policy);

#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
  local_state->SetBoolean(prefs::kAuthNegotiateDelegateByKdcPolicy, true);
  dynamic_params =
      SystemNetworkContextManager::GetHttpAuthDynamicParamsForTesting();
  EXPECT_EQ(true, dynamic_params->negotiate_disable_cname_lookup);
  EXPECT_EQ(true, dynamic_params->enable_negotiate_port);
  EXPECT_EQ(kServerAllowList, dynamic_params->server_allowlist);
  EXPECT_EQ(kDelegateAllowList, dynamic_params->delegate_allowlist);
  EXPECT_TRUE(dynamic_params->delegate_by_kdc_policy);
#endif  // defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
  // The kerberos.enabled pref is false and the device is not Active Directory
  // managed by default.
  EXPECT_EQ(false, dynamic_params->allow_gssapi_library_load);
  local_state->SetBoolean(prefs::kKerberosEnabled, true);
  dynamic_params =
      SystemNetworkContextManager::GetHttpAuthDynamicParamsForTesting();
  EXPECT_EQ(true, dynamic_params->allow_gssapi_library_load);
#endif  // defined(OS_CHROMEOS)
}

IN_PROC_BROWSER_TEST_F(SystemNetworkContextManagerBrowsertest,
                       DefaultMaxConnectionsPerProxy) {
  int max_connections = 0;
  if (content::IsInProcessNetworkService()) {
    base::RunLoop run_loop;
    content::GetNetworkTaskRunner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&run_loop, &max_connections] {
          max_connections =
              net::ClientSocketPoolManager::max_sockets_per_proxy_server(
                  net::HttpNetworkSession::NORMAL_SOCKET_POOL);
          run_loop.Quit();
        }));
    run_loop.Run();
  } else {
    network::mojom::NetworkServiceTestPtr network_service_test;
    content::GetSystemConnector()->BindInterface(
        content::mojom::kNetworkServiceName, &network_service_test);

    mojo::ScopedAllowSyncCallForTesting allow_sync_call;

    bool available =
        network_service_test->GetMaxConnectionsPerProxy(&max_connections);
    EXPECT_TRUE(available);
  }
  EXPECT_EQ(net::DefaultMaxValues::kDefaultMaxSocketsPerProxyServer,
            max_connections);
}

class SystemNetworkContextManagerMaxConnectionsPerProxyBrowsertest
    : public SystemNetworkContextManagerBrowsertest {
 public:
  const int kTestMaxConnectionsPerProxy = 42;

  SystemNetworkContextManagerMaxConnectionsPerProxyBrowsertest() = default;
  ~SystemNetworkContextManagerMaxConnectionsPerProxyBrowsertest() override =
      default;

  void SetUpInProcessBrowserTestFixture() override {
    EXPECT_CALL(provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
    policy::PolicyMap policies;
    policies.Set(policy::key::kMaxConnectionsPerProxy,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD,
                 std::make_unique<base::Value>(kTestMaxConnectionsPerProxy),
                 /*external_data_fetcher=*/nullptr);
    provider_.UpdateChromePolicy(policies);
  }

 private:
  policy::MockConfigurationPolicyProvider provider_;
};

// Flaky on Linux Tests. https://crbug.com/993059
IN_PROC_BROWSER_TEST_F(
    SystemNetworkContextManagerMaxConnectionsPerProxyBrowsertest,
    DISABLED_MaxConnectionsPerProxy) {
  network::mojom::NetworkServiceTestPtr network_service_test;
  content::GetSystemConnector()->BindInterface(
      content::mojom::kNetworkServiceName, &network_service_test);

  mojo::ScopedAllowSyncCallForTesting allow_sync_call;

  int max_connections = 0;
  bool available =
      network_service_test->GetMaxConnectionsPerProxy(&max_connections);
  EXPECT_TRUE(available);
  EXPECT_EQ(kTestMaxConnectionsPerProxy, max_connections);
}

class SystemNetworkContextManagerStubResolverBrowsertest
    : public SystemNetworkContextManagerBrowsertest,
      public testing::WithParamInterface<bool> {
 public:
  SystemNetworkContextManagerStubResolverBrowsertest() {
    scoped_feature_list_.InitWithFeatureState(features::kAsyncDns, GetParam());
  }
  ~SystemNetworkContextManagerStubResolverBrowsertest() override {}

  void SetUpOnMainThread() override {}

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(SystemNetworkContextManagerStubResolverBrowsertest,
                       StubResolverConfig) {
  RunStubResolverConfigTests(GetParam());
}

INSTANTIATE_TEST_SUITE_P(,
                         SystemNetworkContextManagerStubResolverBrowsertest,
                         ::testing::Values(false, true));

class SystemNetworkContextManagerFreezeQUICUaBrowsertest
    : public SystemNetworkContextManagerBrowsertest,
      public testing::WithParamInterface<bool> {
 public:
  SystemNetworkContextManagerFreezeQUICUaBrowsertest() {
    scoped_feature_list_.InitWithFeatureState(blink::features::kFreezeUserAgent,
                                              GetParam());
  }
  ~SystemNetworkContextManagerFreezeQUICUaBrowsertest() override {}

  void SetUpOnMainThread() override {}

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

bool ContainsSubstring(std::string super, std::string sub) {
  return super.find(sub) != std::string::npos;
}

IN_PROC_BROWSER_TEST_P(SystemNetworkContextManagerFreezeQUICUaBrowsertest,
                       QUICUaConfig) {
  network::mojom::NetworkContextParamsPtr network_context_params =
      g_browser_process->system_network_context_manager()
          ->CreateDefaultNetworkContextParams();

  std::string quic_ua = network_context_params->quic_user_agent_id;

  if (GetParam()) {  // if the UA Freeze feature is turned on
    EXPECT_EQ("", quic_ua);
  } else {
    EXPECT_TRUE(ContainsSubstring(quic_ua, chrome::GetChannelName()));
    EXPECT_TRUE(ContainsSubstring(
        quic_ua, version_info::GetProductNameAndVersionForUserAgent()));
    EXPECT_TRUE(ContainsSubstring(quic_ua, content::BuildOSCpuInfo(false)));
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         SystemNetworkContextManagerFreezeQUICUaBrowsertest,
                         ::testing::Values(true, false));
