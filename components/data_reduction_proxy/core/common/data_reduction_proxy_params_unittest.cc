// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"

#include <stddef.h>

#include <map>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/optional.h"
#include "base/test/scoped_feature_list.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_type_info.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/proxy_server.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "base/system/sys_info.h"
#endif

namespace data_reduction_proxy {

class DataReductionProxyParamsTest : public testing::Test {};

TEST_F(DataReductionProxyParamsTest, EverythingDefined) {
  TestDataReductionProxyParams params;
  std::vector<DataReductionProxyServer> expected_proxies;

  // Both the origin and fallback proxy must have type CORE.
  expected_proxies.push_back(DataReductionProxyServer(net::ProxyServer::FromURI(
      "https://proxy.googlezip.net:443", net::ProxyServer::SCHEME_HTTP)));
  expected_proxies.push_back(DataReductionProxyServer(net::ProxyServer::FromURI(
      "compress.googlezip.net:80", net::ProxyServer::SCHEME_HTTP)));

  EXPECT_EQ(expected_proxies, params.proxies_for_http());

  EXPECT_FALSE(
      params.FindConfiguredDataReductionProxy(net::ProxyServer::FromURI(
          "unrelated.proxy.net:80", net::ProxyServer::SCHEME_HTTP)));

  base::Optional<DataReductionProxyTypeInfo> first_info =
      params.FindConfiguredDataReductionProxy(
          expected_proxies[0].proxy_server());
  ASSERT_TRUE(first_info);
  EXPECT_EQ(expected_proxies, first_info->proxy_servers);
  EXPECT_EQ(0U, first_info->proxy_index);

  base::Optional<DataReductionProxyTypeInfo> second_info =
      params.FindConfiguredDataReductionProxy(
          expected_proxies[1].proxy_server());
  ASSERT_TRUE(second_info);
  EXPECT_EQ(expected_proxies, second_info->proxy_servers);
  EXPECT_EQ(1U, second_info->proxy_index);
}

TEST_F(DataReductionProxyParamsTest, Flags) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxy, "http://ovveride-1.com/");
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyFallback, "http://ovveride-2.com/");
  TestDataReductionProxyParams params;

  std::vector<DataReductionProxyServer> expected_proxies;
  expected_proxies.push_back(DataReductionProxyServer(net::ProxyServer::FromURI(
      "http://ovveride-1.com/", net::ProxyServer::SCHEME_HTTP)));
  expected_proxies.push_back(DataReductionProxyServer(net::ProxyServer::FromURI(
      "http://ovveride-2.com/", net::ProxyServer::SCHEME_HTTP)));

  EXPECT_EQ(expected_proxies, params.proxies_for_http());

  // The default proxies shouldn't be recognized as Data Reduction Proxies.
  EXPECT_FALSE(
      params.FindConfiguredDataReductionProxy(net::ProxyServer::FromURI(
          "https://proxy.googlezip.net:443", net::ProxyServer::SCHEME_HTTP)));
  EXPECT_FALSE(
      params.FindConfiguredDataReductionProxy(net::ProxyServer::FromURI(
          "compress.googlezip.net:80", net::ProxyServer::SCHEME_HTTP)));

  base::Optional<DataReductionProxyTypeInfo> first_info =
      params.FindConfiguredDataReductionProxy(
          expected_proxies[0].proxy_server());
  ASSERT_TRUE(first_info);
  EXPECT_EQ(expected_proxies, first_info->proxy_servers);
  EXPECT_EQ(0U, first_info->proxy_index);

  base::Optional<DataReductionProxyTypeInfo> second_info =
      params.FindConfiguredDataReductionProxy(
          expected_proxies[1].proxy_server());
  ASSERT_TRUE(second_info);
  EXPECT_EQ(expected_proxies, second_info->proxy_servers);
  EXPECT_EQ(1U, second_info->proxy_index);
}

TEST_F(DataReductionProxyParamsTest, AreServerExperimentsEnabled) {
  const struct {
    std::string test_case;
    std::string trial_group_value;
    bool disable_flag_set;
    bool expected;
  } tests[] = {
      {
          "Field trial not set",
          "Enabled_42",
          false,
          true,
      },
      {
          "Field trial not set, flag set",
          "",
          true,
          false,
      },
      {
          "Enabled",
          "Enabled",
          false,
          true,
      },
      {
          "Enabled via field trial but disabled via flag",
          "Enabled",
          true,
          false,
      },
      {
          "Disabled via field trial",
          "Disabled",
          false,
          false,
      },
  };

  for (const auto& test : tests) {
    base::test::ScopedFeatureList scoped_feature_list;

    std::map<std::string, std::string> variation_params;
    std::string exp_name;

    if (test.trial_group_value != "Disabled") {
      exp_name = "foobar";
      variation_params[params::GetDataSaverServerExperimentsOptionName()] =
          exp_name;

      scoped_feature_list.InitWithFeaturesAndParameters(
          {{data_reduction_proxy::features::
                kDataReductionProxyServerExperiments,
            {variation_params}}},
          {});
    }

    base::CommandLine::ForCurrentProcess()->InitFromArgv(0, nullptr);
    if (test.disable_flag_set) {
      exp_name = "";
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kDataReductionProxyServerExperimentsDisabled, "");
    } else {
      base::CommandLine::ForCurrentProcess()->RemoveSwitch(
          switches::kDataReductionProxyServerExperimentsDisabled);
    }
    EXPECT_EQ(exp_name, params::GetDataSaverServerExperiments())
        << test.test_case;
  }
}


TEST_F(DataReductionProxyParamsTest, PromoFieldTrial) {
  const struct {
    std::string trial_group_name;
    bool expected_enabled;
  } tests[] = {
      {"Enabled", true},
      {"Enabled_Control", true},
      {"Disabled", false},
      {"enabled", false},
  };

  for (const auto& test : tests) {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.Init();

    ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
        "DataCompressionProxyPromoVisibility", test.trial_group_name));
    EXPECT_EQ(test.expected_enabled, params::IsIncludedInPromoFieldTrial())
        << test.trial_group_name;
  }
}

TEST_F(DataReductionProxyParamsTest, FREPromoFieldTrial) {
  const struct {
    std::string trial_group_name;
    bool expected_enabled;
  } tests[] = {
      {"Enabled", true},
      {"Enabled_Control", true},
      {"Disabled", false},
      {"enabled", false},
  };

  for (const auto& test : tests) {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.Init();

    ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
        "DataReductionProxyFREPromo", test.trial_group_name));
    EXPECT_EQ(test.expected_enabled, params::IsIncludedInFREPromoFieldTrial())
        << test.trial_group_name;
  }
}

TEST_F(DataReductionProxyParamsTest, LowMemoryPromoFeature) {
  const struct {
    bool expected_in_field_trial;
  } tests[] = {
      {false}, {true},
  };

  for (const auto& test : tests) {
    base::test::ScopedFeatureList scoped_feature_list;
    if (test.expected_in_field_trial) {
      scoped_feature_list.InitAndDisableFeature(
          features::kDataReductionProxyLowMemoryDevicePromo);
    } else {
      scoped_feature_list.InitAndEnableFeature(
          features::kDataReductionProxyLowMemoryDevicePromo);
    }

#if defined(OS_ANDROID)
    EXPECT_EQ(test.expected_in_field_trial && base::SysInfo::IsLowEndDevice(),
              params::IsIncludedInPromoFieldTrial());
    EXPECT_EQ(test.expected_in_field_trial && base::SysInfo::IsLowEndDevice(),
              params::IsIncludedInFREPromoFieldTrial());
#else
    EXPECT_FALSE(params::IsIncludedInPromoFieldTrial());
    EXPECT_FALSE(params::IsIncludedInFREPromoFieldTrial());
#endif
  }
}

TEST_F(DataReductionProxyParamsTest, GetConfigServiceURL) {
  const struct {
    std::string test_case;
    std::string flag_value;
    GURL expected;
  } tests[] = {
      {
          "Nothing set", "",
          GURL("https://datasaver.googleapis.com/v1/clientConfigs"),
      },
      {
          "Only command line set", "http://commandline.config-service/",
          GURL("http://commandline.config-service/"),
      },
  };

  for (const auto& test : tests) {
    // Reset all flags.
    base::CommandLine::ForCurrentProcess()->InitFromArgv(0, nullptr);
    if (!test.flag_value.empty()) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kDataReductionProxyConfigURL, test.flag_value);
    }
    EXPECT_EQ(test.expected, params::GetConfigServiceURL()) << test.test_case;
  }
}


TEST(DataReductionProxyParamsStandaloneTest, OverrideProxiesForHttp) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyHttpProxies,
      "http://override-first.net;http://override-second.net");
  DataReductionProxyParams params;

  // Overriding proxies must have type UNSPECIFIED_TYPE.
  std::vector<DataReductionProxyServer> expected_override_proxies_for_http;
  expected_override_proxies_for_http.push_back(
      DataReductionProxyServer(net::ProxyServer::FromURI(
          "http://override-first.net", net::ProxyServer::SCHEME_HTTP)));
  expected_override_proxies_for_http.push_back(
      DataReductionProxyServer(net::ProxyServer::FromURI(
          "http://override-second.net", net::ProxyServer::SCHEME_HTTP)));

  EXPECT_EQ(expected_override_proxies_for_http, params.proxies_for_http());
}

}  // namespace data_reduction_proxy
