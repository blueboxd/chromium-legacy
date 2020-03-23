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
                               const net::HttpRequestHeaders& headers,
                               bool want_empty = false) {
    auto config = LatestProxyConfig();
    ASSERT_TRUE(config);

    EXPECT_EQ(config->rules.type,
              net::ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME);
    EXPECT_FALSE(config->should_override_existing_config);
    EXPECT_FALSE(config->allow_non_idempotent_methods);

    EXPECT_EQ(config->connect_tunnel_headers.ToString(), headers.ToString());

    EXPECT_EQ(config->rules.proxies_for_http.size(), 0U);
    EXPECT_EQ(config->rules.proxies_for_ftp.size(), 0U);

    if (want_empty) {
      EXPECT_EQ(config->rules.proxies_for_https.size(), 0U);
    } else {
      ASSERT_EQ(config->rules.proxies_for_https.size(), 1U);
      EXPECT_EQ(GURL(config->rules.proxies_for_https.Get().ToURI()), proxy_url);
    }
  }

  IsolatedPrerenderProxyConfigurator* configurator() {
    return configurator_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<IsolatedPrerenderProxyConfigurator> configurator_;
  std::unique_ptr<TestCustomProxyConfigClient> config_client_;
};

TEST_F(IsolatedPrerenderProxyConfiguratorTest, FeaturesOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {}, {features::kIsolatePrerenders,
           data_reduction_proxy::features::kDataReductionProxyHoldback});

  configurator()->UpdateCustomProxyConfig();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(LatestProxyConfig());
}

TEST_F(IsolatedPrerenderProxyConfiguratorTest, DRPFeatureOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kIsolatePrerenders},
      {data_reduction_proxy::features::kDataReductionProxyHoldback});

  configurator()->UpdateCustomProxyConfig();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(LatestProxyConfig());
}

TEST_F(IsolatedPrerenderProxyConfiguratorTest, PrefetchFeatureOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {data_reduction_proxy::features::kDataReductionProxyHoldback},
      {features::kIsolatePrerenders});

  configurator()->UpdateCustomProxyConfig();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(LatestProxyConfig());
}

TEST_F(IsolatedPrerenderProxyConfiguratorTest, NoProxyServers) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kIsolatePrerenders,
       data_reduction_proxy::features::kDataReductionProxyHoldback},
      {});

  configurator()->UpdateProxyHosts({});
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(LatestProxyConfig());
}

TEST_F(IsolatedPrerenderProxyConfiguratorTest, ClearProxyServers) {
  GURL proxy_url("https://proxy.com");
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kIsolatePrerenders,
       data_reduction_proxy::features::kDataReductionProxyHoldback},
      {});

  configurator()->UpdateProxyHosts({proxy_url});
  base::RunLoop().RunUntilIdle();

  net::HttpRequestHeaders headers;
  VerifyLatestProxyConfig(proxy_url, headers);

  configurator()->UpdateProxyHosts({});
  base::RunLoop().RunUntilIdle();

  VerifyLatestProxyConfig(GURL(), headers, /*want_empty=*/true);
}

TEST_F(IsolatedPrerenderProxyConfiguratorTest, ValidProxyServerURL) {
  GURL proxy_url("https://proxy.com");
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kIsolatePrerenders,
       data_reduction_proxy::features::kDataReductionProxyHoldback},
      {});

  configurator()->UpdateProxyHosts({proxy_url});
  base::RunLoop().RunUntilIdle();

  net::HttpRequestHeaders headers;
  VerifyLatestProxyConfig(proxy_url, headers);
}

TEST_F(IsolatedPrerenderProxyConfiguratorTest, ValidProxyServerURLWithHeaders) {
  GURL proxy_url("https://proxy.com");
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kIsolatePrerenders,
       data_reduction_proxy::features::kDataReductionProxyHoldback},
      {});

  net::HttpRequestHeaders headers;
  headers.SetHeader("X-Testing", "Hello World");
  configurator()->UpdateTunnelHeaders(headers);
  configurator()->UpdateProxyHosts({proxy_url});

  base::RunLoop().RunUntilIdle();
  VerifyLatestProxyConfig(proxy_url, headers);
}
