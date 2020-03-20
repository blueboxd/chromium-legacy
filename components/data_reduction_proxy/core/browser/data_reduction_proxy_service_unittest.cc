// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_service_client_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_mutable_config_values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_prefs.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_network_quality_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {
namespace {

std::string CreateEncodedConfig(
    const std::vector<DataReductionProxyServer> proxy_servers) {
  ClientConfig config;
  config.set_session_key("session");
  for (const auto& proxy_server : proxy_servers) {
    ProxyServer* config_proxy =
        config.mutable_proxy_config()->add_http_proxy_servers();
    net::HostPortPair host_port_pair =
        proxy_server.proxy_server().host_port_pair();
    config_proxy->set_scheme(ProxyServer_ProxyScheme_HTTP);
    config_proxy->set_host(host_port_pair.host());
    config_proxy->set_port(host_port_pair.port());
  }
  return EncodeConfig(config);
}
}  // namespace

class DataReductionProxyServiceTest : public testing::Test {
 public:
  void SetUp() override {
    RegisterSimpleProfilePrefs(prefs_.registry());
  }

  void RequestCallback(int err) {}

  PrefService* prefs() { return &prefs_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;

 private:
  TestingPrefServiceSimple prefs_;
};

class TestCustomProxyConfigClient
    : public network::mojom::CustomProxyConfigClient {
 public:
  TestCustomProxyConfigClient(
      mojo::PendingReceiver<network::mojom::CustomProxyConfigClient>
          pending_receiver)
      : receiver_(this, std::move(pending_receiver)) {}

  // network::mojom::CustomProxyConfigClient implementation:
  void OnCustomProxyConfigUpdated(
      network::mojom::CustomProxyConfigPtr proxy_config) override {
    config = std::move(proxy_config);
  }

  void MarkProxiesAsBad(base::TimeDelta bypass_duration,
                        const net::ProxyList& bad_proxies,
                        MarkProxiesAsBadCallback callback) override {}

  void ClearBadProxiesCache() override { num_clear_cache_calls++; }

  network::mojom::CustomProxyConfigPtr config;
  int num_clear_cache_calls = 0;

 private:
  mojo::Receiver<network::mojom::CustomProxyConfigClient> receiver_;
};

TEST_F(DataReductionProxyServiceTest, TestResetBadProxyListOnDisableDataSaver) {
  std::unique_ptr<DataReductionProxyTestContext> drp_test_context =
      DataReductionProxyTestContext::Builder()
          .SkipSettingsInitialization()
          .Build();

  drp_test_context->SetDataReductionProxyEnabled(true);
  drp_test_context->InitSettings();

  mojo::Remote<network::mojom::CustomProxyConfigClient> client_remote;
  TestCustomProxyConfigClient client(
      client_remote.BindNewPipeAndPassReceiver());
  drp_test_context->data_reduction_proxy_service()->AddCustomProxyConfigClient(
      std::move(client_remote));
  base::RunLoop().RunUntilIdle();

  // Turn Data Saver off.
  drp_test_context->SetDataReductionProxyEnabled(false);
  base::RunLoop().RunUntilIdle();

  // Verify that the bad proxy cache was cleared.
  EXPECT_EQ(1, client.num_clear_cache_calls);
}

TEST_F(DataReductionProxyServiceTest, HoldbackConfiguresProxies) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kDataReductionProxyHoldback);

  std::unique_ptr<DataReductionProxyTestContext> drp_test_context =
      DataReductionProxyTestContext::Builder()
          .SkipSettingsInitialization()
          .Build();

  EXPECT_TRUE(drp_test_context->test_params()->proxies_for_http().size() > 0);
  EXPECT_FALSE(drp_test_context->test_params()
                   ->proxies_for_http()
                   .front()
                   .proxy_server()
                   .is_direct());
}

TEST_F(DataReductionProxyServiceTest, TestCustomProxyConfigClient) {
  std::unique_ptr<DataReductionProxyTestContext> drp_test_context =
      DataReductionProxyTestContext::Builder().WithConfigClient().Build();
  drp_test_context->SetDataReductionProxyEnabled(true);
  drp_test_context->test_network_quality_tracker()
      ->ReportEffectiveConnectionTypeForTesting(
          net::EFFECTIVE_CONNECTION_TYPE_4G);
  DataReductionProxyService* service =
      drp_test_context->data_reduction_proxy_service();

  auto proxy_server = net::ProxyServer::FromPacString("PROXY foo");
  service->config_client()->ApplySerializedConfig(
      CreateEncodedConfig({DataReductionProxyServer(proxy_server)}));

  mojo::Remote<network::mojom::CustomProxyConfigClient> client_remote;
  TestCustomProxyConfigClient client(
      client_remote.BindNewPipeAndPassReceiver());
  service->AddCustomProxyConfigClient(std::move(client_remote));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(client.config);
}

TEST_F(DataReductionProxyServiceTest, TestCustomProxyConfigUpdatedOnECTChange) {
  std::unique_ptr<DataReductionProxyTestContext> drp_test_context =
      DataReductionProxyTestContext::Builder().Build();
  drp_test_context->SetDataReductionProxyEnabled(true);
  drp_test_context->test_network_quality_tracker()
      ->ReportEffectiveConnectionTypeForTesting(
          net::EFFECTIVE_CONNECTION_TYPE_4G);

  mojo::Remote<network::mojom::CustomProxyConfigClient> client_remote;
  TestCustomProxyConfigClient client(
      client_remote.BindNewPipeAndPassReceiver());
  drp_test_context->data_reduction_proxy_service()->AddCustomProxyConfigClient(
      std::move(client_remote));
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(client.config);
}

TEST_F(DataReductionProxyServiceTest,
       TestCustomProxyConfigUpdatedOnHeaderChange) {
  std::unique_ptr<DataReductionProxyTestContext> drp_test_context =
      DataReductionProxyTestContext::Builder().Build();
  drp_test_context->SetDataReductionProxyEnabled(true);
  DataReductionProxyService* service =
      drp_test_context->data_reduction_proxy_service();

  mojo::Remote<network::mojom::CustomProxyConfigClient> client_remote;
  TestCustomProxyConfigClient client(
      client_remote.BindNewPipeAndPassReceiver());
  service->AddCustomProxyConfigClient(std::move(client_remote));
  base::RunLoop().RunUntilIdle();

  std::string value;
  ASSERT_FALSE(client.config);
}

TEST_F(DataReductionProxyServiceTest,
       TestCustomProxyConfigUpdatedOnProxyChange) {
  std::unique_ptr<DataReductionProxyTestContext> drp_test_context =
      DataReductionProxyTestContext::Builder().WithConfigClient().Build();
  drp_test_context->SetDataReductionProxyEnabled(true);
  DataReductionProxyService* service =
      drp_test_context->data_reduction_proxy_service();

  service->config()->UpdateConfigForTesting(true, true, true);

  auto proxy_server1 = net::ProxyServer::FromPacString("PROXY foo");
  service->config_client()->ApplySerializedConfig(
      CreateEncodedConfig({DataReductionProxyServer(proxy_server1)}));

  mojo::Remote<network::mojom::CustomProxyConfigClient> client_remote;
  TestCustomProxyConfigClient client(
      client_remote.BindNewPipeAndPassReceiver());
  service->AddCustomProxyConfigClient(std::move(client_remote));
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(client.config);
}

TEST_F(DataReductionProxyServiceTest,
       TestCustomProxyConfigHasAlternateProxyListOfCoreProxies) {
  std::unique_ptr<DataReductionProxyTestContext> drp_test_context =
      DataReductionProxyTestContext::Builder().WithConfigClient().Build();
  drp_test_context->SetDataReductionProxyEnabled(true);
  DataReductionProxyService* service =
      drp_test_context->data_reduction_proxy_service();
  service->config()->UpdateConfigForTesting(true, true, true);

  auto core_proxy_server = net::ProxyServer::FromPacString("PROXY foo");
  auto second_proxy_server = net::ProxyServer::FromPacString("PROXY bar");
  service->config_client()->ApplySerializedConfig(
      CreateEncodedConfig({DataReductionProxyServer(core_proxy_server),
                           DataReductionProxyServer(second_proxy_server)}));

  mojo::Remote<network::mojom::CustomProxyConfigClient> client_remote;
  TestCustomProxyConfigClient client(
      client_remote.BindNewPipeAndPassReceiver());
  service->AddCustomProxyConfigClient(std::move(client_remote));
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(client.config);
}

TEST_F(DataReductionProxyServiceTest, TestCustomProxyConfigProperties) {
  std::unique_ptr<DataReductionProxyTestContext> drp_test_context =
      DataReductionProxyTestContext::Builder().Build();
  drp_test_context->SetDataReductionProxyEnabled(true);
  DataReductionProxyService* service =
      drp_test_context->data_reduction_proxy_service();
  service->config()->UpdateConfigForTesting(true, true, true);

  mojo::Remote<network::mojom::CustomProxyConfigClient> client_remote;
  TestCustomProxyConfigClient client(
      client_remote.BindNewPipeAndPassReceiver());
  service->AddCustomProxyConfigClient(std::move(client_remote));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(client.config);
}

}  // namespace data_reduction_proxy
