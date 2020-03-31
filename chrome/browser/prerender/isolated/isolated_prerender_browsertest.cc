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
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_features.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_proxy_configurator.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_service.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_service_factory.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_service_workers_observer.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_tab_helper.h"
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
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_utils.h"
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
    if (update_closure_) {
      std::move(update_closure_).Run();
    }
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
    origin_server_->RegisterRequestHandler(
        base::BindRepeating(&IsolatedPrerenderBrowserTest::HandleOriginRequest,
                            base::Unretained(this)));
    EXPECT_TRUE(origin_server_->Start());

    proxy_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    proxy_server_->ServeFilesFromSourceDirectory("chrome/test/data");
    proxy_server_->RegisterRequestMonitor(base::BindRepeating(
        &IsolatedPrerenderBrowserTest::MonitorProxyResourceRequest,
        base::Unretained(this)));
    EXPECT_TRUE(proxy_server_->Start());

    config_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    config_server_->RegisterRequestHandler(
        base::BindRepeating(&IsolatedPrerenderBrowserTest::GetConfigResponse,
                            base::Unretained(this)));
    EXPECT_TRUE(config_server_->Start());
  }

  void SetUp() override {
    SetFeatures();
    InProcessBrowserTest::SetUp();
  }

  // This browsertest uses a separate method to handle enabling/disabling
  // features since order is tricky when doing different feature lists between
  // base and derived classes.
  virtual void SetFeatures() {
    scoped_feature_list_.InitWithFeatures(
        {features::kIsolatePrerenders,
         data_reduction_proxy::features::kDataReductionProxyHoldback,
         data_reduction_proxy::features::kFetchClientConfig},
        {});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Ensure the service gets created before the tests start.
    IsolatedPrerenderServiceFactory::GetForProfile(browser()->profile());

    host_resolver()->AddRule("testorigin.com", "127.0.0.1");
    host_resolver()->AddRule("badprobe.testorigin.com", "127.0.0.1");
    host_resolver()->AddRule("proxy.com", "127.0.0.1");
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    InProcessBrowserTest::SetUpCommandLine(cmd);
    // For the proxy.
    cmd->AppendSwitch("ignore-certificate-errors");
    cmd->AppendSwitchASCII(
        data_reduction_proxy::switches::kDataReductionProxyConfigURL,
        config_server_->base_url().spec());
  }

  void SetDataSaverEnabled(bool enabled) {
    data_reduction_proxy::DataReductionProxySettings::
        SetDataSaverEnabledForTesting(browser()->profile()->GetPrefs(),
                                      enabled);
  }

  content::WebContents* GetWebContents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void MakeNavigationPrediction(const GURL& doc_url,
                                const std::vector<GURL>& predicted_urls) {
    NavigationPredictorKeyedServiceFactory::GetForProfile(browser()->profile())
        ->OnPredictionUpdated(
            GetWebContents(), doc_url,
            NavigationPredictorKeyedService::PredictionSource::
                kAnchorElementsParsedFromWebPage,
            predicted_urls);
  }

  std::unique_ptr<prerender::PrerenderHandle> StartPrerender(const GURL& url) {
    prerender::PrerenderManager* prerender_manager =
        prerender::PrerenderManagerFactory::GetForBrowserContext(
            browser()->profile());

    return prerender_manager->AddPrerenderFromNavigationPredictor(
        url,
        GetWebContents()->GetController().GetDefaultSessionStorageNamespace(),
        kSize);
  }

  network::mojom::CustomProxyConfigPtr WaitForUpdatedCustomProxyConfig() {
    IsolatedPrerenderService* isolated_prerender_service =
        IsolatedPrerenderServiceFactory::GetForProfile(browser()->profile());

    base::RunLoop run_loop;
    mojo::Remote<network::mojom::CustomProxyConfigClient> client_remote;
    TestCustomProxyConfigClient config_client(
        client_remote.BindNewPipeAndPassReceiver(), run_loop.QuitClosure());
    isolated_prerender_service->proxy_configurator()
        ->AddCustomProxyConfigClient(std::move(client_remote));

    // A network change forces the config to be fetched.
    SimulateNetworkChange(network::mojom::ConnectionType::CONNECTION_3G);
    run_loop.Run();

    return std::move(config_client.config_);
  }

  void AddSuccessfulPrefetch(const GURL& url) const {
    IsolatedPrerenderTabHelper* tab_helper =
        IsolatedPrerenderTabHelper::FromWebContents(GetWebContents());

    network::mojom::URLResponseHeadPtr head =
        network::CreateURLResponseHead(net::HTTP_OK);
    head->was_fetched_via_cache = false;
    head->mime_type = "text/html";

    tab_helper->CallHandlePrefetchResponseForTesting(
        url, net::NetworkIsolationKey::CreateOpaqueAndNonTransient(),
        std::move(head),
        std::make_unique<std::string>(
            "<html><head><title>Successful prefetch</title></head></html>"));
  }

  void VerifyProxyConfig(network::mojom::CustomProxyConfigPtr config,
                         bool want_empty = false) {
    ASSERT_TRUE(config);

    EXPECT_EQ(config->rules.type,
              net::ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME);
    EXPECT_FALSE(config->should_override_existing_config);
    EXPECT_FALSE(config->allow_non_idempotent_methods);

    if (want_empty) {
      EXPECT_EQ(config->rules.proxies_for_https.size(), 0U);
    } else {
      ASSERT_EQ(config->rules.proxies_for_https.size(), 1U);
      EXPECT_EQ(GURL(config->rules.proxies_for_https.Get().ToURI()),
                GetProxyURL());
    }
  }

  GURL GetProxyURL() const { return proxy_server_->GetURL("proxy.com", "/"); }

  GURL GetOriginServerURL(const std::string& path) const {
    return origin_server_->GetURL("testorigin.com", path);
  }

  GURL GetOriginServerURLWithBadProbe(const std::string& path) const {
    return origin_server_->GetURL("badprobe.testorigin.com", path);
  }

 protected:
  base::OnceClosure on_proxy_request_closure_;

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleOriginRequest(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().spec().find("favicon") != std::string::npos)
      return nullptr;

    // If the badprobe origin is being requested, (which has to be checked using
    // the Host header since the request URL is always 127.0.0.1), check if this
    // is a probe request. The probe only requests "/" whereas the navigation
    // will request the HTML file, i.e.: "/simple.html".
    if (request.headers.find("Host")->second.find("badprobe.testorigin.com") !=
            std::string::npos &&
        request.relative_url == "/") {
      // This is an invalid response to the net stack and will cause a NetError.
      return std::make_unique<net::test_server::RawHttpResponse>("", "");
    }

    return nullptr;
  }

  void MonitorProxyResourceRequest(
      const net::test_server::HttpRequest& request) {
    // This method is called on embedded test server thread. Post the
    // information on UI thread.
    base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                   base::BindOnce(&IsolatedPrerenderBrowserTest::
                                      MonitorProxyResourceRequestOnUIThread,
                                  base::Unretained(this), request));
  }

  void MonitorProxyResourceRequestOnUIThread(
      const net::test_server::HttpRequest& request) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    std::vector<std::string> request_lines =
        base::SplitString(request.all_headers, "\r\n", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);

    EXPECT_EQ(request_lines[0], "CONNECT testorigin.com:443 HTTP/1.1");

    bool found_chrome_proxy_header = false;
    for (const std::string& header : request_lines) {
      if (header.find("chrome-proxy") != std::string::npos &&
          header.find("s=secretsessionkey") != std::string::npos) {
        found_chrome_proxy_header = true;
      }
    }
    EXPECT_TRUE(found_chrome_proxy_header);

    if (on_proxy_request_closure_) {
      std::move(on_proxy_request_closure_).Run();
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
    valid_secure_proxy->set_host(GetProxyURL().host());
    valid_secure_proxy->set_port(GetProxyURL().EffectiveIntPort());
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
  void OnPrerenderStop(prerender::PrerenderHandle* handle) override {}

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> proxy_server_;
  std::unique_ptr<net::EmbeddedTestServer> origin_server_;
  std::unique_ptr<net::EmbeddedTestServer> config_server_;
};

IN_PROC_BROWSER_TEST_F(
    IsolatedPrerenderBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(ServiceWorkerRegistrationIsObserved)) {
  SetDataSaverEnabled(true);

  // Load a page that registers a service worker.
  ui_test_utils::NavigateToURL(
      browser(),
      GetOriginServerURL("/service_worker/create_service_worker.html"));
  EXPECT_EQ("DONE", EvalJs(GetWebContents(),
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
  auto client_config = WaitForUpdatedCustomProxyConfig();
  VerifyProxyConfig(std::move(client_config));
}

IN_PROC_BROWSER_TEST_F(IsolatedPrerenderBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(ConnectProxyEndtoEnd)) {
  SetDataSaverEnabled(true);
  ui_test_utils::NavigateToURL(browser(), GetOriginServerURL("/simple.html"));
  WaitForUpdatedCustomProxyConfig();

  base::RunLoop run_loop;
  on_proxy_request_closure_ = run_loop.QuitClosure();

  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {GURL("https://testorigin.com/")});

  // This run loop will quit when a valid CONNECT request is made to the proxy
  // server.
  run_loop.Run();

  // The embedded test server will return a 400 for all CONNECT requests by
  // default. Ensure that the request didn't fallback to a direct connection.
  IsolatedPrerenderTabHelper* tab_helper =
      IsolatedPrerenderTabHelper::FromWebContents(GetWebContents());
  EXPECT_EQ(tab_helper->metrics().prefetch_attempted_count, 1U);
  EXPECT_EQ(tab_helper->metrics().prefetch_successful_count, 0U);
}

class ProbingEnabledIsolatedPrerenderBrowserTest
    : public IsolatedPrerenderBrowserTest {
 public:
  void SetFeatures() override {
    IsolatedPrerenderBrowserTest::SetFeatures();
    scoped_feature_list_.InitAndEnableFeature(
        features::kIsolatePrerendersMustProbeOrigin);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class ProbingDisabledIsolatedPrerenderBrowserTest
    : public IsolatedPrerenderBrowserTest {
 public:
  void SetFeatures() override {
    IsolatedPrerenderBrowserTest::SetFeatures();
    scoped_feature_list_.InitAndDisableFeature(
        features::kIsolatePrerendersMustProbeOrigin);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// These tests use a separate embedded test server from |origin_server_| because
// we need to test against a bad server for the probe disabled case, and ensure
// that no probe occurs and the prefetched page can still be used. Therefore,
// |origin_server_for_probing_| is only started when probing is enabled.

IN_PROC_BROWSER_TEST_F(ProbingEnabledIsolatedPrerenderBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(ProbeGood)) {
  GURL url = GetOriginServerURL("/simple.html");

  AddSuccessfulPrefetch(url);

  ui_test_utils::NavigateToURL(browser(), url);

  content::NavigationEntry* entry =
      GetWebContents()->GetController().GetVisibleEntry();
  EXPECT_EQ(content::PAGE_TYPE_NORMAL, entry->GetPageType());

  // If served from the origin test server, the title would be "OK", but the
  // title in the prefetched is "Successful prefetch".
  EXPECT_EQ(base::UTF8ToUTF16("Successful prefetch"),
            GetWebContents()->GetTitle());
}

IN_PROC_BROWSER_TEST_F(ProbingEnabledIsolatedPrerenderBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(ProbeBad)) {
  GURL url = GetOriginServerURLWithBadProbe("/simple.html");

  AddSuccessfulPrefetch(url);

  ui_test_utils::NavigateToURL(browser(), url);

  // The navigation won't be intercepted so it will be served from the test
  // server directly. If served from the origin test server, the title would be
  // "OK", but the title in the prefetched is "Successful prefetch".
  EXPECT_EQ(base::UTF8ToUTF16("OK"), GetWebContents()->GetTitle());
}

IN_PROC_BROWSER_TEST_F(ProbingDisabledIsolatedPrerenderBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(NoProbe)) {
  // Use the bad probe url to ensure the probe is not being used.
  GURL url = GetOriginServerURLWithBadProbe("/simple.html");

  AddSuccessfulPrefetch(url);

  ui_test_utils::NavigateToURL(browser(), url);

  content::NavigationEntry* entry =
      GetWebContents()->GetController().GetVisibleEntry();
  EXPECT_EQ(content::PAGE_TYPE_NORMAL, entry->GetPageType());

  // If served from the origin test server, the title would be "OK", but the
  // title in the prefetched is "Successful prefetch".
  EXPECT_EQ(base::UTF8ToUTF16("Successful prefetch"),
            GetWebContents()->GetTitle());
}
