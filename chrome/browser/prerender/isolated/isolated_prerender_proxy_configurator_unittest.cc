// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/isolated/isolated_prerender_proxy_configurator.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/host_port_pair.h"
#include "net/http/http_request_headers.h"
#include "net/proxy_resolution/proxy_config.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class TestCustomProxyConfigClient
    : public network::mojom::CustomProxyConfigClient {
 public:
  explicit TestCustomProxyConfigClient(
      mojo::PendingReceiver<network::mojom::CustomProxyConfigClient>
          pending_receiver)
      : receiver_(this, std::move(pending_receiver)) {}

  // network::mojom::CustomProxyConfigClient:
  void OnCustomProxyConfigUpdated(
      network::mojom::CustomProxyConfigPtr proxy_config) override {
    config_ = std::move(proxy_config);
  }
  void MarkProxiesAsBad(base::TimeDelta bypass_duration,
                        const net::ProxyList& bad_proxies,
                        MarkProxiesAsBadCallback callback) override {}
  void ClearBadProxiesCache() override {}

  network::mojom::CustomProxyConfigPtr config_;

 private:
  mojo::Receiver<network::mojom::CustomProxyConfigClient> receiver_;
};

class IsolatedPrerenderProxyConfiguratorTest : public testing::Test {
 public:
  IsolatedPrerenderProxyConfiguratorTest()
      : configurator_(std::make_unique<IsolatedPrerenderProxyConfigurator>()) {}
  ~IsolatedPrerenderProxyConfiguratorTest() override = default;

  void SetUp() override {
    mojo::Remote<network::mojom::CustomProxyConfigClient> client_remote;
    config_client_ = std::make_unique<TestCustomProxyConfigClient>(
        client_remote.BindNewPipeAndPassReceiver());
    configurator_->AddCustomProxyConfigClient(std::move(client_remote));
    base::RunLoop().RunUntilIdle();
  }

  network::mojom::CustomProxyConfigPtr LatestProxyConfig() {
    return std::move(config_client_->config_);
  }

  void VerifyLatestProxyConfig(const GURL& proxy_url,
                               const net::HttpRequestHeaders& headers) {
    auto config = LatestProxyConfig();
    ASSERT_TRUE(config);

    EXPECT_EQ(config->rules.type,
              net::ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME);
    EXPECT_FALSE(config->should_override_existing_config);
    EXPECT_FALSE(config->allow_non_idempotent_methods);
    EXPECT_FALSE(config->assume_https_proxies_support_quic);
    EXPECT_TRUE(config->can_use_proxy_on_http_url_redirect_cycles);

    EXPECT_TRUE(config->pre_cache_headers.IsEmpty());
    EXPECT_TRUE(config->post_cache_headers.IsEmpty());
    EXPECT_EQ(config->connect_tunnel_headers.ToString(), headers.ToString());

    EXPECT_EQ(config->rules.proxies_for_http.size(), 0U);
    EXPECT_EQ(config->rules.proxies_for_ftp.size(), 0U);

    ASSERT_EQ(config->rules.proxies_for_https.size(), 1U);
    EXPECT_EQ(GURL(config->rules.proxies_for_https.Get().ToURI()), proxy_url);
  }

  IsolatedPrerenderProxyConfigurator* configurator() {
    return configurator_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<IsolatedPrerenderProxyConfigurator> configurator_;
  std::unique_ptr<TestCustomProxyConfigClient> config_client_;
};

TEST_F(IsolatedPrerenderProxyConfiguratorTest, BothFeaturesOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {}, {features::kIsolatedPrerenderUsesProxy,
           data_reduction_proxy::features::kDataReductionProxyHoldback});

  configurator()->UpdateCustomProxyConfig();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(LatestProxyConfig());
}

TEST_F(IsolatedPrerenderProxyConfiguratorTest, DRPFeatureOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kIsolatedPrerenderUsesProxy},
      {data_reduction_proxy::features::kDataReductionProxyHoldback});

  configurator()->UpdateCustomProxyConfig();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(LatestProxyConfig());
}

TEST_F(IsolatedPrerenderProxyConfiguratorTest, ProxyFeatureOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {data_reduction_proxy::features::kDataReductionProxyHoldback},
      {features::kIsolatedPrerenderUsesProxy});

  configurator()->UpdateCustomProxyConfig();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(LatestProxyConfig());
}

TEST_F(IsolatedPrerenderProxyConfiguratorTest, NoProxyServer) {
  base::test::ScopedFeatureList drp_scoped_feature_list;
  drp_scoped_feature_list.InitAndEnableFeature(
      data_reduction_proxy::features::kDataReductionProxyHoldback);

  base::test::ScopedFeatureList proxy_scoped_feature_list;
  proxy_scoped_feature_list.InitAndEnableFeature(
      features::kIsolatedPrerenderUsesProxy);

  configurator()->UpdateCustomProxyConfig();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(LatestProxyConfig());
}

TEST_F(IsolatedPrerenderProxyConfiguratorTest, InvalidProxyServerURL_NoScheme) {
  base::test::ScopedFeatureList drp_scoped_feature_list;
  drp_scoped_feature_list.InitAndEnableFeature(
      data_reduction_proxy::features::kDataReductionProxyHoldback);

  base::test::ScopedFeatureList proxy_scoped_feature_list;
  proxy_scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIsolatedPrerenderUsesProxy, {{"proxy_server_url", "invalid"}});

  configurator()->UpdateCustomProxyConfig();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(LatestProxyConfig());
}

TEST_F(IsolatedPrerenderProxyConfiguratorTest, InvalidProxyServerURL_NoHost) {
  base::test::ScopedFeatureList drp_scoped_feature_list;
  drp_scoped_feature_list.InitAndEnableFeature(
      data_reduction_proxy::features::kDataReductionProxyHoldback);

  base::test::ScopedFeatureList proxy_scoped_feature_list;
  proxy_scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIsolatedPrerenderUsesProxy,
      {{"proxy_server_url", "https://"}});

  configurator()->UpdateCustomProxyConfig();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(LatestProxyConfig());
}

TEST_F(IsolatedPrerenderProxyConfiguratorTest, ValidProxyServerURL) {
  GURL proxy_url("https://proxy.com");
  base::test::ScopedFeatureList drp_scoped_feature_list;
  drp_scoped_feature_list.InitAndEnableFeature(
      data_reduction_proxy::features::kDataReductionProxyHoldback);

  base::test::ScopedFeatureList proxy_scoped_feature_list;
  proxy_scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIsolatedPrerenderUsesProxy,
      {{"proxy_server_url", proxy_url.spec()}});

  configurator()->UpdateCustomProxyConfig();
  base::RunLoop().RunUntilIdle();

  net::HttpRequestHeaders headers;
  VerifyLatestProxyConfig(proxy_url, headers);
}

TEST_F(IsolatedPrerenderProxyConfiguratorTest, ValidProxyServerURLWithHeaders) {
  GURL proxy_url("https://proxy.com");
  base::test::ScopedFeatureList drp_scoped_feature_list;
  drp_scoped_feature_list.InitAndEnableFeature(
      data_reduction_proxy::features::kDataReductionProxyHoldback);

  base::test::ScopedFeatureList proxy_scoped_feature_list;
  proxy_scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIsolatedPrerenderUsesProxy,
      {{"proxy_server_url", proxy_url.spec()}});

  net::HttpRequestHeaders headers;
  headers.SetHeader("X-Testing", "Hello World");
  configurator()->UpdateTunnelHeaders(headers);

  base::RunLoop().RunUntilIdle();
  VerifyLatestProxyConfig(proxy_url, headers);
}
