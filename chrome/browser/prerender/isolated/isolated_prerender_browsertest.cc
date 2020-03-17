// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_features.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_proxy_configurator.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_service.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_service_factory.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_service_workers_observer.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_url_loader_interceptor.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/prerender/prerender_handle.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_service_client_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

constexpr gfx::Size kSize(640, 480);

void SimulateNetworkChange(network::mojom::ConnectionType type) {
  if (!content::IsInProcessNetworkService()) {
    mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
    content::GetNetworkService()->BindTestInterface(
        network_service_test.BindNewPipeAndPassReceiver());
    base::RunLoop run_loop;
    network_service_test->SimulateNetworkChange(type, run_loop.QuitClosure());
    run_loop.Run();
    return;
  }
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::ConnectionType(type));
}

class TestCustomProxyConfigClient
    : public network::mojom::CustomProxyConfigClient {
 public:
  explicit TestCustomProxyConfigClient(
      mojo::PendingReceiver<network::mojom::CustomProxyConfigClient>
          pending_receiver,
      base::OnceClosure update_closure)
      : receiver_(this, std::move(pending_receiver)),
        update_closure_(std::move(update_closure)) {}

  // network::mojom::CustomProxyConfigClient:
  void OnCustomProxyConfigUpdated(
      network::mojom::CustomProxyConfigPtr proxy_config) override {
    config_ = std::move(proxy_config);
    std::move(update_closure_).Run();
  }
  void MarkProxiesAsBad(base::TimeDelta bypass_duration,
                        const net::ProxyList& bad_proxies,
                        MarkProxiesAsBadCallback callback) override {}
  void ClearBadProxiesCache() override {}

  network::mojom::CustomProxyConfigPtr config_;

 private:
  mojo::Receiver<network::mojom::CustomProxyConfigClient> receiver_;
  base::OnceClosure update_closure_;
};

}  // namespace

// Occasional flakes on Windows (https://crbug.com/1045971).
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
#define DISABLE_ON_WIN_MAC_CHROMEOS(x) DISABLED_##x
#else
#define DISABLE_ON_WIN_MAC_CHROMEOS(x) x
#endif

class IsolatedPrerenderBrowserTest
    : public InProcessBrowserTest,
      public prerender::PrerenderHandle::Observer {
 public:
  IsolatedPrerenderBrowserTest() {
    origin_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    origin_server_->ServeFilesFromSourceDirectory("chrome/test/data");
    origin_server_->RegisterRequestMonitor(base::BindRepeating(
        &IsolatedPrerenderBrowserTest::MonitorResourceRequest,
        base::Unretained(this)));
    EXPECT_TRUE(origin_server_->Start());

    config_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    config_server_->RegisterRequestHandler(
        base::BindRepeating(&IsolatedPrerenderBrowserTest::GetConfigResponse,
                            base::Unretained(this)));
    EXPECT_TRUE(config_server_->Start());
  }

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kIsolatePrerenders,
         data_reduction_proxy::features::kDataReductionProxyHoldback,
         data_reduction_proxy::features::kFetchClientConfig},
        {});

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Ensure the service gets created before the tests start.
    IsolatedPrerenderServiceFactory::GetForProfile(browser()->profile());
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    InProcessBrowserTest::SetUpCommandLine(cmd);
    cmd->AppendSwitchASCII("host-rules", "MAP * 127.0.0.1");
    cmd->AppendSwitchASCII(
        data_reduction_proxy::switches::kDataReductionProxyConfigURL,
        config_server_->base_url().spec());
  }

  void SetDataSaverEnabled(bool enabled) {
    data_reduction_proxy::DataReductionProxySettings::
        SetDataSaverEnabledForTesting(browser()->profile()->GetPrefs(),
                                      enabled);
  }

  std::unique_ptr<prerender::PrerenderHandle> StartPrerender(const GURL& url) {
    prerender::PrerenderManager* prerender_manager =
        prerender::PrerenderManagerFactory::GetForBrowserContext(
            browser()->profile());

    return prerender_manager->AddPrerenderFromNavigationPredictor(
        url,
        browser()
            ->tab_strip_model()
            ->GetActiveWebContents()
            ->GetController()
            .GetDefaultSessionStorageNamespace(),
        kSize);
  }

  void VerifyProxyConfig(network::mojom::CustomProxyConfigPtr config,
                         bool want_empty = false) {
    ASSERT_TRUE(config);

    EXPECT_EQ(config->rules.type,
              net::ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME);
    EXPECT_FALSE(config->should_override_existing_config);
    EXPECT_FALSE(config->allow_non_idempotent_methods);
    EXPECT_FALSE(config->assume_https_proxies_support_quic);
    EXPECT_TRUE(config->can_use_proxy_on_http_url_redirect_cycles);

    EXPECT_TRUE(config->pre_cache_headers.IsEmpty());
    EXPECT_TRUE(config->post_cache_headers.IsEmpty());

    EXPECT_EQ(config->rules.proxies_for_http.size(), 0U);
    EXPECT_EQ(config->rules.proxies_for_ftp.size(), 0U);

    if (want_empty) {
      EXPECT_EQ(config->rules.proxies_for_https.size(), 0U);
    } else {
      ASSERT_EQ(config->rules.proxies_for_https.size(), 1U);
      EXPECT_EQ(GURL(config->rules.proxies_for_https.Get().ToURI()),
                GURL("https://prefetch-proxy.com:443/"));
    }
  }

  GURL GetOriginServerURL(const std::string& path) const {
    return origin_server_->GetURL("testorigin.com", path);
  }

  size_t origin_server_request_with_cookies() const {
    return origin_server_request_with_cookies_;
  }

 protected:
  base::OnceClosure waiting_for_resource_request_closure_;

 private:
  void MonitorResourceRequest(const net::test_server::HttpRequest& request) {
    // This method is called on embedded test server thread. Post the
    // information on UI thread.
    base::PostTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(
            &IsolatedPrerenderBrowserTest::MonitorResourceRequestOnUIThread,
            base::Unretained(this), request));
  }

  void MonitorResourceRequestOnUIThread(
      const net::test_server::HttpRequest& request) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // Don't care about favicons.
    if (request.GetURL().spec().find("favicon") != std::string::npos)
      return;

    if (request.headers.find("Cookie") != request.headers.end()) {
      origin_server_request_with_cookies_++;
    }
  }

  // Called when |config_server_| receives a request for config fetch.
  std::unique_ptr<net::test_server::HttpResponse> GetConfigResponse(
      const net::test_server::HttpRequest& request) {
    data_reduction_proxy::ClientConfig config =
        data_reduction_proxy::CreateConfig(
            "secretsessionkey", 1000, 0,
            data_reduction_proxy::ProxyServer_ProxyScheme_HTTP,
            "proxy-host.net", 80,
            data_reduction_proxy::ProxyServer_ProxyScheme_HTTP, "fallback.net",
            80, 0.5f, false);

    data_reduction_proxy::PrefetchProxyConfig_Proxy* valid_secure_proxy =
        config.mutable_prefetch_proxy_config()->add_proxy_list();
    valid_secure_proxy->set_type(
        data_reduction_proxy::PrefetchProxyConfig_Proxy_Type_CONNECT);
    valid_secure_proxy->set_host("prefetch-proxy.com");
    valid_secure_proxy->set_port(443);
    valid_secure_proxy->set_scheme(
        data_reduction_proxy::PrefetchProxyConfig_Proxy_Scheme_HTTPS);

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_content(config.SerializeAsString());
    response->set_content_type("text/plain");
    return response;
  }

  // prerender::PrerenderHandle::Observer:
  void OnPrerenderStart(prerender::PrerenderHandle* handle) override {}
  void OnPrerenderStopLoading(prerender::PrerenderHandle* handle) override {}
  void OnPrerenderDomContentLoaded(
      prerender::PrerenderHandle* handle) override {}
  void OnPrerenderNetworkBytesChanged(
      prerender::PrerenderHandle* handle) override {}
  void OnPrerenderStop(prerender::PrerenderHandle* handle) override {
    if (waiting_for_resource_request_closure_) {
      std::move(waiting_for_resource_request_closure_).Run();
    }
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> origin_server_;
  std::unique_ptr<net::EmbeddedTestServer> config_server_;
  size_t origin_server_request_with_cookies_ = 0;
};

IN_PROC_BROWSER_TEST_F(IsolatedPrerenderBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(PrerenderIsIsolated)) {
  SetDataSaverEnabled(true);

  base::HistogramTester histogram_tester;

  ASSERT_TRUE(content::SetCookie(browser()->profile(), GetOriginServerURL("/"),
                                 "testing"));

  // Do a prerender to the same origin and expect that the cookies are not used.
  std::unique_ptr<prerender::PrerenderHandle> handle =
      StartPrerender(GetOriginServerURL("/simple.html"));
  ASSERT_TRUE(handle);

  // Wait for the prerender to complete before checking.
  if (!handle->IsFinishedLoading()) {
    handle->SetObserver(this);
    base::RunLoop run_loop;
    waiting_for_resource_request_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0U, origin_server_request_with_cookies());

  histogram_tester.ExpectUniqueSample(
      "Prerender.FinalStatus",
      prerender::FINAL_STATUS_NOSTATE_PREFETCH_FINISHED, 1);

  // Navigate to the same origin and expect it to have cookies.
  // Note: This check needs to come after the prerender, otherwise the prerender
  // will be canceled because the origin was recently loaded.
  ui_test_utils::NavigateToURL(browser(), GetOriginServerURL("/simple.html"));
  EXPECT_EQ(1U, origin_server_request_with_cookies());
}

IN_PROC_BROWSER_TEST_F(
    IsolatedPrerenderBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(ServiceWorkerRegistrationIsObserved)) {
  SetDataSaverEnabled(true);

  // Load a page that registers a service worker.
  ui_test_utils::NavigateToURL(
      browser(),
      GetOriginServerURL("/service_worker/create_service_worker.html"));
  EXPECT_EQ("DONE", EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                           "register('network_fallback_worker.js');"));

  IsolatedPrerenderService* isolated_prerender_service =
      IsolatedPrerenderServiceFactory::GetForProfile(browser()->profile());
  EXPECT_EQ(base::Optional<bool>(true),
            isolated_prerender_service->service_workers_observer()
                ->IsServiceWorkerRegisteredForOrigin(
                    url::Origin::Create(GetOriginServerURL("/"))));
  EXPECT_EQ(base::Optional<bool>(false),
            isolated_prerender_service->service_workers_observer()
                ->IsServiceWorkerRegisteredForOrigin(
                    url::Origin::Create(GURL("https://unregistered.com"))));
}

IN_PROC_BROWSER_TEST_F(IsolatedPrerenderBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(DRPClientConfigPlumbing)) {
  SetDataSaverEnabled(true);

  IsolatedPrerenderService* isolated_prerender_service =
      IsolatedPrerenderServiceFactory::GetForProfile(browser()->profile());

  base::RunLoop run_loop;
  mojo::Remote<network::mojom::CustomProxyConfigClient> client_remote;
  TestCustomProxyConfigClient config_client(
      client_remote.BindNewPipeAndPassReceiver(), run_loop.QuitClosure());
  isolated_prerender_service->proxy_configurator()->AddCustomProxyConfigClient(
      std::move(client_remote));
  base::RunLoop().RunUntilIdle();

  // A network change forces the config to be fetched.
  SimulateNetworkChange(network::mojom::ConnectionType::CONNECTION_3G);
  run_loop.Run();

  VerifyProxyConfig(std::move(config_client.config_));
}
