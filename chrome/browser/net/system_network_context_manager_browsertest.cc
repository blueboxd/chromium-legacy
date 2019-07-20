// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/system_network_context_manager.h"

#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/optional.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/user_agent.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace {

void GetStubResolverConfig(
    bool* stub_resolver_enabled,
    base::Optional<std::vector<network::mojom::DnsOverHttpsServerPtr>>*
        dns_over_https_servers) {
  dns_over_https_servers->reset();

  SystemNetworkContextManager::GetStubResolverConfigForTesting(
      stub_resolver_enabled, dns_over_https_servers);
}

// Checks the values returned by GetStubResolverConfigForTesting() match
// |async_dns_feature_enabled| (With empty DNS over HTTPS prefs). Then sets
// various DNS over HTTPS servers, and makes sure the settings are respected.
void RunStubResolverConfigTests(bool async_dns_feature_enabled) {
  // Check initial state.
  bool stub_resolver_enabled = !async_dns_feature_enabled;
  base::Optional<std::vector<network::mojom::DnsOverHttpsServerPtr>>
      dns_over_https_servers;
  GetStubResolverConfig(&stub_resolver_enabled, &dns_over_https_servers);
  EXPECT_EQ(async_dns_feature_enabled, stub_resolver_enabled);
  EXPECT_FALSE(dns_over_https_servers.has_value());

  // Check state after setting various DNS over HTTPS preferences.

  // The POST template is only valid for POSTs, though the GET template is
  // technically valid for both POSTs and GETs.
  const char kGoodPostTemplate[] = "https://foo.test/";
  const char kGoodGetTemplate[] = "https://bar.test/dns-query{?dns}";
  const char kBadTemplate[] = "dns-query{?dns}";

  const char kPost[] = "POST";
  // The code actually looks for POST and not-POST, but may as well use "GET"
  // for not-POST.
  const char kGet[] = "GET";

  PrefService* local_state = g_browser_process->local_state();
  base::Value servers(base::Value::Type::LIST);
  base::Value methods(base::Value::Type::LIST);

  // Test cases with server and method length mismatches. This shouldn't happen
  // at steady state, but can happen during pref changes.

  servers.GetList().push_back(base::Value(kGoodGetTemplate));
  local_state->Set(prefs::kDnsOverHttpsServers, servers);
  local_state->Set(prefs::kDnsOverHttpsServerMethods, methods);
  GetStubResolverConfig(&stub_resolver_enabled, &dns_over_https_servers);
  EXPECT_EQ(async_dns_feature_enabled, stub_resolver_enabled);
  EXPECT_FALSE(dns_over_https_servers.has_value());
  servers.GetList().clear();
  methods.GetList().clear();

  methods.GetList().push_back(base::Value(kPost));
  local_state->Set(prefs::kDnsOverHttpsServers, servers);
  local_state->Set(prefs::kDnsOverHttpsServerMethods, methods);
  GetStubResolverConfig(&stub_resolver_enabled, &dns_over_https_servers);
  EXPECT_EQ(async_dns_feature_enabled, stub_resolver_enabled);
  EXPECT_FALSE(dns_over_https_servers.has_value());
  servers.GetList().clear();
  methods.GetList().clear();

  // Test case with incorrect server type.
  servers.GetList().push_back(base::Value(15));
  methods.GetList().push_back(base::Value(kPost));
  local_state->Set(prefs::kDnsOverHttpsServers, servers);
  local_state->Set(prefs::kDnsOverHttpsServerMethods, methods);
  GetStubResolverConfig(&stub_resolver_enabled, &dns_over_https_servers);
  EXPECT_EQ(async_dns_feature_enabled, stub_resolver_enabled);
  EXPECT_FALSE(dns_over_https_servers.has_value());
  servers.GetList().clear();
  methods.GetList().clear();

  // Test case with incorrect method type.
  servers.GetList().push_back(base::Value(kGoodGetTemplate));
  methods.GetList().push_back(base::Value(3.14));
  local_state->Set(prefs::kDnsOverHttpsServers, servers);
  local_state->Set(prefs::kDnsOverHttpsServerMethods, methods);
  GetStubResolverConfig(&stub_resolver_enabled, &dns_over_https_servers);
  EXPECT_EQ(async_dns_feature_enabled, stub_resolver_enabled);
  EXPECT_FALSE(dns_over_https_servers.has_value());
  servers.GetList().clear();
  methods.GetList().clear();

  // Test case with one bad template.
  servers.GetList().push_back(base::Value(kBadTemplate));
  methods.GetList().push_back(base::Value(kPost));
  local_state->Set(prefs::kDnsOverHttpsServers, servers);
  local_state->Set(prefs::kDnsOverHttpsServerMethods, methods);
  GetStubResolverConfig(&stub_resolver_enabled, &dns_over_https_servers);
  EXPECT_EQ(async_dns_feature_enabled, stub_resolver_enabled);
  EXPECT_FALSE(dns_over_https_servers.has_value());
  servers.GetList().clear();
  methods.GetList().clear();

  // Test case with one good template.
  servers.GetList().push_back(base::Value(kGoodPostTemplate));
  methods.GetList().push_back(base::Value(kPost));
  local_state->Set(prefs::kDnsOverHttpsServers, servers);
  local_state->Set(prefs::kDnsOverHttpsServerMethods, methods);
  GetStubResolverConfig(&stub_resolver_enabled, &dns_over_https_servers);
  EXPECT_EQ(true, stub_resolver_enabled);
  ASSERT_TRUE(dns_over_https_servers.has_value());
  ASSERT_EQ(1u, dns_over_https_servers->size());
  EXPECT_EQ(kGoodPostTemplate, dns_over_https_servers->at(0)->server_template);
  EXPECT_EQ(true, dns_over_https_servers->at(0)->use_post);
  servers.GetList().clear();
  methods.GetList().clear();

  // Test case with one good template, one bad one.
  servers.GetList().push_back(base::Value(kGoodGetTemplate));
  methods.GetList().push_back(base::Value(kGet));
  servers.GetList().push_back(base::Value(kBadTemplate));
  methods.GetList().push_back(base::Value(kPost));
  local_state->Set(prefs::kDnsOverHttpsServers, servers);
  local_state->Set(prefs::kDnsOverHttpsServerMethods, methods);
  GetStubResolverConfig(&stub_resolver_enabled, &dns_over_https_servers);
  EXPECT_EQ(true, stub_resolver_enabled);
  ASSERT_TRUE(dns_over_https_servers.has_value());
  ASSERT_EQ(1u, dns_over_https_servers->size());
  EXPECT_EQ(kGoodGetTemplate, dns_over_https_servers->at(0)->server_template);
  EXPECT_EQ(false, dns_over_https_servers->at(0)->use_post);
  servers.GetList().clear();
  methods.GetList().clear();

  // Test case with two good templates.
  servers.GetList().push_back(base::Value(kGoodPostTemplate));
  methods.GetList().push_back(base::Value(kPost));
  servers.GetList().push_back(base::Value(kGoodGetTemplate));
  methods.GetList().push_back(base::Value(kGet));
  local_state->Set(prefs::kDnsOverHttpsServers, servers);
  local_state->Set(prefs::kDnsOverHttpsServerMethods, methods);
  GetStubResolverConfig(&stub_resolver_enabled, &dns_over_https_servers);
  EXPECT_EQ(true, stub_resolver_enabled);
  ASSERT_TRUE(dns_over_https_servers.has_value());
  ASSERT_EQ(2u, dns_over_https_servers->size());
  EXPECT_EQ(kGoodPostTemplate, dns_over_https_servers->at(0)->server_template);
  EXPECT_EQ(true, dns_over_https_servers->at(0)->use_post);
  EXPECT_EQ(kGoodGetTemplate, dns_over_https_servers->at(1)->server_template);
  EXPECT_EQ(false, dns_over_https_servers->at(1)->use_post);
  servers.GetList().clear();
  methods.GetList().clear();

  // Test case with policy BuiltInDnsClientEnabled enabled.
  local_state->Set(prefs::kDnsOverHttpsServers, servers);
  local_state->Set(prefs::kDnsOverHttpsServerMethods, methods);
  GetStubResolverConfig(&stub_resolver_enabled, &dns_over_https_servers);
  EXPECT_EQ(async_dns_feature_enabled, stub_resolver_enabled);
  EXPECT_FALSE(dns_over_https_servers.has_value());
  local_state->Set(prefs::kBuiltInDnsClientEnabled,
                   base::Value(!async_dns_feature_enabled));
  GetStubResolverConfig(&stub_resolver_enabled, &dns_over_https_servers);
  EXPECT_EQ(!async_dns_feature_enabled, stub_resolver_enabled);
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
  EXPECT_EQ("", dynamic_params->server_whitelist);
  EXPECT_EQ("", dynamic_params->delegate_whitelist);
  EXPECT_FALSE(dynamic_params->delegate_by_kdc_policy);

  PrefService* local_state = g_browser_process->local_state();

  local_state->SetBoolean(prefs::kDisableAuthNegotiateCnameLookup, true);
  dynamic_params =
      SystemNetworkContextManager::GetHttpAuthDynamicParamsForTesting();
  EXPECT_EQ(true, dynamic_params->negotiate_disable_cname_lookup);
  EXPECT_EQ(false, dynamic_params->enable_negotiate_port);
  EXPECT_EQ("", dynamic_params->server_whitelist);
  EXPECT_EQ("", dynamic_params->delegate_whitelist);
  EXPECT_FALSE(dynamic_params->delegate_by_kdc_policy);

  local_state->SetBoolean(prefs::kEnableAuthNegotiatePort, true);
  dynamic_params =
      SystemNetworkContextManager::GetHttpAuthDynamicParamsForTesting();
  EXPECT_EQ(true, dynamic_params->negotiate_disable_cname_lookup);
  EXPECT_EQ(true, dynamic_params->enable_negotiate_port);
  EXPECT_EQ("", dynamic_params->server_whitelist);
  EXPECT_EQ("", dynamic_params->delegate_whitelist);
  EXPECT_FALSE(dynamic_params->delegate_by_kdc_policy);

  const char kServerWhiteList[] = "foo";
  local_state->SetString(prefs::kAuthServerWhitelist, kServerWhiteList);
  dynamic_params =
      SystemNetworkContextManager::GetHttpAuthDynamicParamsForTesting();
  EXPECT_EQ(true, dynamic_params->negotiate_disable_cname_lookup);
  EXPECT_EQ(true, dynamic_params->enable_negotiate_port);
  EXPECT_EQ(kServerWhiteList, dynamic_params->server_whitelist);
  EXPECT_EQ("", dynamic_params->delegate_whitelist);

  const char kDelegateWhiteList[] = "bar, baz";
  local_state->SetString(prefs::kAuthNegotiateDelegateWhitelist,
                         kDelegateWhiteList);
  dynamic_params =
      SystemNetworkContextManager::GetHttpAuthDynamicParamsForTesting();
  EXPECT_EQ(true, dynamic_params->negotiate_disable_cname_lookup);
  EXPECT_EQ(true, dynamic_params->enable_negotiate_port);
  EXPECT_EQ(kServerWhiteList, dynamic_params->server_whitelist);
  EXPECT_EQ(kDelegateWhiteList, dynamic_params->delegate_whitelist);
  EXPECT_FALSE(dynamic_params->delegate_by_kdc_policy);

#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
  local_state->SetBoolean(prefs::kAuthNegotiateDelegateByKdcPolicy, true);
  dynamic_params =
      SystemNetworkContextManager::GetHttpAuthDynamicParamsForTesting();
  EXPECT_EQ(true, dynamic_params->negotiate_disable_cname_lookup);
  EXPECT_EQ(true, dynamic_params->enable_negotiate_port);
  EXPECT_EQ(kServerWhiteList, dynamic_params->server_whitelist);
  EXPECT_EQ(kDelegateWhiteList, dynamic_params->delegate_whitelist);
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
