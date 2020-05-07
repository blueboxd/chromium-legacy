// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <memory>
#include <set>
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
#include "chrome/browser/chrome_notification_types.h"
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
#include "chrome/browser/prerender/isolated/isolated_prerender_test_utils.h"
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
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
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

class AuthChallengeObserver : public content::NotificationObserver {
 public:
  explicit AuthChallengeObserver(content::WebContents* web_contents) {
    registrar_.Add(this, chrome::NOTIFICATION_AUTH_NEEDED,
                   content::Source<content::NavigationController>(
                       &web_contents->GetController()));
  }
  ~AuthChallengeObserver() override = default;

  bool GotAuthChallenge() const { return got_auth_challenge_; }

  void Reset() { got_auth_challenge_ = false; }

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    got_auth_challenge_ |= type == chrome::NOTIFICATION_AUTH_NEEDED;
  }

 private:
  content::NotificationRegistrar registrar_;
  bool got_auth_challenge_ = false;
};

// Runs a closure when all expected URLs have been fetched successfully.
class TestTabHelperObserver : public IsolatedPrerenderTabHelper::Observer {
 public:
  explicit TestTabHelperObserver(IsolatedPrerenderTabHelper* tab_helper)
      : tab_helper_(tab_helper) {
    tab_helper_->AddObserverForTesting(this);
  }
  ~TestTabHelperObserver() { tab_helper_->RemoveObserverForTesting(this); }

  void SetOnPrefetchSuccessfulClosure(base::OnceClosure closure) {
    on_successful_prefetch_closure_ = std::move(closure);
  }

  void SetOnPrefetchErrorClosure(base::OnceClosure closure) {
    on_prefetch_error_closure_ = std::move(closure);
  }

  void SetExpectedSuccessfulURLs(const std::set<GURL>& expected_urls) {
    expected_successful_prefetch_urls_ = expected_urls;
  }

  void SetExpectedPrefetchErrors(
      const std::set<std::pair<GURL, int>> expected_prefetch_errors) {
    expected_prefetch_errors_ = expected_prefetch_errors;
  }

  // IsolatedPrerenderTabHelper::Observer:
  void OnPrefetchCompletedSuccessfully(const GURL& url) override {
    auto it = expected_successful_prefetch_urls_.find(url);
    if (it != expected_successful_prefetch_urls_.end()) {
      expected_successful_prefetch_urls_.erase(it);
    }

    if (!expected_successful_prefetch_urls_.empty())
      return;

    if (!on_successful_prefetch_closure_)
      return;

    std::move(on_successful_prefetch_closure_).Run();
  }

  void OnPrefetchCompletedWithError(const GURL& url, int code) override {
    std::pair<GURL, int> error_pair = {url, code};
    auto it = expected_prefetch_errors_.find(error_pair);
    if (it != expected_prefetch_errors_.end()) {
      expected_prefetch_errors_.erase(it);
    }

    if (!expected_prefetch_errors_.empty())
      return;

    if (!on_prefetch_error_closure_)
      return;

    std::move(on_prefetch_error_closure_).Run();
  }

 private:
  IsolatedPrerenderTabHelper* tab_helper_;

  base::OnceClosure on_successful_prefetch_closure_;
  std::set<GURL> expected_successful_prefetch_urls_;

  base::OnceClosure on_prefetch_error_closure_;
  std::set<std::pair<GURL, int>> expected_prefetch_errors_;
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
      public prerender::PrerenderHandle::Observer,
      public net::test_server::EmbeddedTestServerConnectionListener {
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
    proxy_server_->RegisterRequestHandler(
        base::BindRepeating(&IsolatedPrerenderBrowserTest::HandleProxyRequest,
                            base::Unretained(this)));
    proxy_server_->SetConnectionListener(this);
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

    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();

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
    cmd->AppendSwitch("force-enable-metrics-reporting");
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

  void VerifyUKMEntry(const GURL& url,
                      const std::string& metric_name,
                      base::Optional<int64_t> expected_value) {
    SCOPED_TRACE(metric_name);

    auto entries = ukm_recorder_->GetEntriesByName(
        ukm::builders::PrefetchProxy::kEntryName);
    ASSERT_EQ(1U, entries.size());

    const auto* entry = entries.front();

    ukm_recorder_->ExpectEntrySourceHasUrl(entry, url);

    const int64_t* value =
        ukm::TestUkmRecorder::GetEntryMetric(entry, metric_name);
    ASSERT_EQ(value != nullptr, expected_value.has_value());

    if (!expected_value.has_value())
      return;

    EXPECT_EQ(*value, expected_value.value());
  }

  size_t OriginServerRequestCount() const {
    base::RunLoop().RunUntilIdle();
    return origin_server_request_count_;
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
  base::OnceClosure on_proxy_tunnel_done_closure_;

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleOriginRequest(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().spec().find("favicon") != std::string::npos)
      return nullptr;

    base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                   base::BindOnce(&IsolatedPrerenderBrowserTest::
                                      MonitorOriginResourceRequestOnUIThread,
                                  base::Unretained(this), request));

    if (request.relative_url == "/auth_challenge") {
      std::unique_ptr<net::test_server::BasicHttpResponse> resp =
          std::make_unique<net::test_server::BasicHttpResponse>();
      resp->set_code(net::HTTP_UNAUTHORIZED);
      resp->AddCustomHeader("www-authenticate", "Basic realm=\"test\"");
      return resp;
    }

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

  void OnProxyTunnelDone() {
    proxy_tunnel_.reset();
    if (on_proxy_tunnel_done_closure_) {
      std::move(on_proxy_tunnel_done_closure_).Run();
    }
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleProxyRequest(
      const net::test_server::HttpRequest& request) {
    if (request.all_headers.find("CONNECT auth_challenge.com:443") !=
        std::string::npos) {
      std::unique_ptr<net::test_server::BasicHttpResponse> resp =
          std::make_unique<net::test_server::BasicHttpResponse>();
      resp->set_code(net::HTTP_UNAUTHORIZED);
      resp->AddCustomHeader("www-authenticate", "Basic realm=\"test\"");
      return resp;
    }

    std::vector<std::string> request_lines =
        base::SplitString(request.all_headers, "\r\n", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
    DCHECK(!request_lines.empty());

    std::vector<std::string> request_line =
        base::SplitString(request_lines[0], " ", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
    DCHECK_EQ(3U, request_line.size());
    EXPECT_EQ("CONNECT", request_line[0]);
    EXPECT_EQ("HTTP/1.1", request_line[2]);

    GURL request_origin("https://" + request_line[1]);
    EXPECT_TRUE("testorigin.com" == request_origin.host() ||
                "badprobe.testorigin.com" == request_origin.host());

    bool found_chrome_proxy_header = false;
    for (const std::string& header : request_lines) {
      if (header.find("chrome-proxy") != std::string::npos &&
          header.find("s=secretsessionkey") != std::string::npos) {
        found_chrome_proxy_header = true;
      }
    }
    EXPECT_TRUE(found_chrome_proxy_header);

    proxy_tunnel_ = std::make_unique<TestProxyTunnelConnection>();
    proxy_tunnel_->SetOnDoneCallback(
        base::BindOnce(&IsolatedPrerenderBrowserTest::OnProxyTunnelDone,
                       base::Unretained(this)));

    EXPECT_TRUE(proxy_tunnel_->ConnectToPeerOnLocalhost(
        request_origin.EffectiveIntPort()));

    // This method is called on embedded test server thread. Post the
    // information on UI thread.
    base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                   base::BindOnce(&IsolatedPrerenderBrowserTest::
                                      MonitorProxyResourceRequestOnUIThread,
                                  base::Unretained(this), request));

    std::unique_ptr<net::test_server::BasicHttpResponse> resp =
        std::make_unique<net::test_server::BasicHttpResponse>();
    resp->set_code(net::HTTP_OK);
    return resp;
  }

  void MonitorProxyResourceRequestOnUIThread(
      const net::test_server::HttpRequest& request) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    if (on_proxy_request_closure_) {
      std::move(on_proxy_request_closure_).Run();
    }
  }

  void MonitorOriginResourceRequestOnUIThread(
      const net::test_server::HttpRequest& request) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    origin_server_request_count_++;
  }

  // Called when |config_server_| receives a request for config fetch.
  std::unique_ptr<net::test_server::HttpResponse> GetConfigResponse(
      const net::test_server::HttpRequest& request) {
    data_reduction_proxy::ClientConfig config =
        data_reduction_proxy::CreateClientConfig("secretsessionkey", 1000, 0);

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

  // net::test_server::EmbeddedTestServerConnectionListener:
  void ReadFromSocket(const net::StreamSocket& socket, int rv) override {}
  std::unique_ptr<net::StreamSocket> AcceptedSocket(
      std::unique_ptr<net::StreamSocket> socket) override {
    return socket;
  }
  void OnResponseCompletedSuccessfully(
      std::unique_ptr<net::StreamSocket> socket) override {
    if (proxy_tunnel_) {
      // PostTask starting the proxy so that we are more confident that the
      // prefetch has had ample time to process the response from the embedded
      // test server.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&TestProxyTunnelConnection::StartProxy,
                         proxy_tunnel_->GetWeakPtr(), std::move(socket)));
    }
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
  std::unique_ptr<net::EmbeddedTestServer> proxy_server_;
  std::unique_ptr<net::EmbeddedTestServer> origin_server_;
  std::unique_ptr<net::EmbeddedTestServer> config_server_;

  // Lives on |proxy_server_|'s IO Thread.
  std::unique_ptr<TestProxyTunnelConnection> proxy_tunnel_;

  size_t origin_server_request_count_ = 0;
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

IN_PROC_BROWSER_TEST_F(
    IsolatedPrerenderBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(NoAuthChallenges_FromProxy)) {
  SetDataSaverEnabled(true);
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  WaitForUpdatedCustomProxyConfig();

  std::unique_ptr<AuthChallengeObserver> auth_observer =
      std::make_unique<AuthChallengeObserver>(GetWebContents());

  // Do a positive test first to make sure we get an auth challenge under these
  // circumstances.
  ui_test_utils::NavigateToURL(browser(),
                               GetOriginServerURL("/auth_challenge"));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(auth_observer->GotAuthChallenge());

  // Test that a proxy auth challenge does not show a dialog.
  auth_observer->Reset();
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {GURL("https://auth_challenge.com/")});
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(auth_observer->GotAuthChallenge());
}

IN_PROC_BROWSER_TEST_F(
    IsolatedPrerenderBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(NoAuthChallenges_FromOrigin)) {
  SetDataSaverEnabled(true);
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  WaitForUpdatedCustomProxyConfig();

  GURL auth_challenge_url = GetOriginServerURL("/auth_challenge");

  std::unique_ptr<AuthChallengeObserver> auth_observer =
      std::make_unique<AuthChallengeObserver>(GetWebContents());

  // Do a positive test first to make sure we get an auth challenge under these
  // circumstances.
  ui_test_utils::NavigateToURL(browser(), auth_challenge_url);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(auth_observer->GotAuthChallenge());

  // Test that an origin auth challenge does not show a dialog.
  auth_observer->Reset();

  IsolatedPrerenderTabHelper* tab_helper =
      IsolatedPrerenderTabHelper::FromWebContents(GetWebContents());
  TestTabHelperObserver tab_helper_observer(tab_helper);

  base::RunLoop run_loop;
  tab_helper_observer.SetOnPrefetchErrorClosure(run_loop.QuitClosure());
  tab_helper_observer.SetExpectedPrefetchErrors(
      {{auth_challenge_url, net::HTTP_UNAUTHORIZED}});

  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {auth_challenge_url});

  run_loop.Run();

  EXPECT_FALSE(auth_observer->GotAuthChallenge());
}

IN_PROC_BROWSER_TEST_F(IsolatedPrerenderBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(ConnectProxyEndtoEnd)) {
  SetDataSaverEnabled(true);
  ui_test_utils::NavigateToURL(browser(), GetOriginServerURL("/simple.html"));
  WaitForUpdatedCustomProxyConfig();

  IsolatedPrerenderTabHelper* tab_helper =
      IsolatedPrerenderTabHelper::FromWebContents(GetWebContents());
  TestTabHelperObserver tab_helper_observer(tab_helper);

  GURL prefetch_url = GetOriginServerURL("/title2.html");

  base::RunLoop run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(run_loop.QuitClosure());
  tab_helper_observer.SetExpectedSuccessfulURLs({prefetch_url});

  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {prefetch_url});

  // This run loop will quit when the prefetch response has been successfully
  // done and processed.
  run_loop.Run();

  EXPECT_EQ(tab_helper->srp_metrics().prefetch_attempted_count_, 1U);
  EXPECT_EQ(tab_helper->srp_metrics().prefetch_successful_count_, 1U);

  size_t starting_origin_request_count = OriginServerRequestCount();

  ui_test_utils::NavigateToURL(browser(), prefetch_url);
  EXPECT_EQ(base::UTF8ToUTF16("Title Of Awesomeness"),
            GetWebContents()->GetTitle());

  // The origin server should not have served this request.
  EXPECT_EQ(starting_origin_request_count, OriginServerRequestCount());
}

IN_PROC_BROWSER_TEST_F(IsolatedPrerenderBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(PrefetchingUKM)) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      "isolated-prerender-unlimited-prefetches");

  GURL url = GetOriginServerURL("/simple.html");
  SetDataSaverEnabled(true);
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForUpdatedCustomProxyConfig();

  IsolatedPrerenderTabHelper* tab_helper =
      IsolatedPrerenderTabHelper::FromWebContents(GetWebContents());

  GURL eligible_link_1 = GetOriginServerURL("/title1.html");
  GURL eligible_link_2 = GetOriginServerURL("/title2.html");
  GURL eligible_link_3 = GetOriginServerURL("/title3.html");

  TestTabHelperObserver tab_helper_observer(tab_helper);
  tab_helper_observer.SetExpectedSuccessfulURLs(
      {eligible_link_1, eligible_link_2, eligible_link_3});

  base::RunLoop run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(run_loop.QuitClosure());

  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {
                                        eligible_link_1,
                                        eligible_link_2,
                                        GURL("http://not-eligible.com/1"),
                                        GURL("http://not-eligible.com/2"),
                                        GURL("http://not-eligible.com/3"),
                                        eligible_link_3,
                                    });

  // This run loop will quit when all the prefetch responses have been
  // successfully done and processed.
  run_loop.Run();

  // Navigate again to trigger UKM recording.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  base::RunLoop().RunUntilIdle();

  // This bit mask records which links were eligible for prefetching with
  // respect to their order in the navigation prediction. The LSB corresponds to
  // the first index in the prediction, and is set if that url was eligible.
  // Given the above URLs, they map to each bit accordingly:
  //
  // Note: The only difference between eligible and non-eligible urls is the
  // scheme.
  //
  //  (eligible)                   https://testorigin.com/1
  //  (eligible)                https://testorigin.com/2  |
  //  (not eligible)        http://not-eligible.com/1  |  |
  //  (not eligible)     http://not-eligible.com/2  |  |  |
  //  (not eligible)  http://not-eligible.com/3  |  |  |  |
  //  (eligible)    https://testorigin.com/3  |  |  |  |  |
  //                                       |  |  |  |  |  |
  //                                       V  V  V  V  V  V
  // int64_t expected_bitmask =        0b  1  0  0  0  1  1;

  constexpr int64_t expected_bitmask = 0b100011;

  using UkmEntry = ukm::builders::PrefetchProxy;
  VerifyUKMEntry(url, UkmEntry::kordered_eligible_pages_bitmaskName,
                 expected_bitmask);
  VerifyUKMEntry(url, UkmEntry::kprefetch_eligible_countName, 3);
  VerifyUKMEntry(url, UkmEntry::kprefetch_attempted_countName, 3);
  VerifyUKMEntry(url, UkmEntry::kprefetch_successful_countName, 3);

  // This UKM should not be recorded until the following page load.
  VerifyUKMEntry(url, UkmEntry::kprefetch_usageName, base::nullopt);
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

IN_PROC_BROWSER_TEST_F(ProbingEnabledIsolatedPrerenderBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(ProbeGood)) {
  SetDataSaverEnabled(true);
  GURL starting_page = GetOriginServerURL("/simple.html");
  ui_test_utils::NavigateToURL(browser(), starting_page);
  WaitForUpdatedCustomProxyConfig();

  IsolatedPrerenderTabHelper* tab_helper =
      IsolatedPrerenderTabHelper::FromWebContents(GetWebContents());

  GURL eligible_link = GetOriginServerURL("/title2.html");

  TestTabHelperObserver tab_helper_observer(tab_helper);
  tab_helper_observer.SetExpectedSuccessfulURLs({eligible_link});

  base::RunLoop run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(run_loop.QuitClosure());

  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {eligible_link});

  // This run loop will quit when all the prefetch responses have been
  // successfully done and processed.
  run_loop.Run();

  // Navigate to the prefetched page, this also triggers UKM recording.
  size_t starting_origin_request_count = OriginServerRequestCount();
  ui_test_utils::NavigateToURL(browser(), eligible_link);

  // Only the probe should have hit the origin server.
  EXPECT_EQ(starting_origin_request_count + 1, OriginServerRequestCount());

  EXPECT_EQ(base::UTF8ToUTF16("Title Of Awesomeness"),
            GetWebContents()->GetTitle());

  ASSERT_TRUE(tab_helper->after_srp_metrics());
  ASSERT_TRUE(tab_helper->after_srp_metrics()->prefetch_status_.has_value());
  // 1 is the value of "prefetch used, probe success". The test does not
  // reference the enum directly to ensure that casting the enum to an int went
  // cleanly, and to provide an extra review point if the value should ever
  // accidentally change in the future, which it never should.
  EXPECT_EQ(1, static_cast<int>(
                   tab_helper->after_srp_metrics()->prefetch_status_.value()));

  base::Optional<base::TimeDelta> probe_latency =
      tab_helper->after_srp_metrics()->probe_latency_;
  ASSERT_TRUE(probe_latency.has_value());
  EXPECT_GT(probe_latency.value(), base::TimeDelta());
}

IN_PROC_BROWSER_TEST_F(ProbingEnabledIsolatedPrerenderBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(ProbeBad)) {
  SetDataSaverEnabled(true);
  GURL starting_page = GetOriginServerURL("/simple.html");
  ui_test_utils::NavigateToURL(browser(), starting_page);
  WaitForUpdatedCustomProxyConfig();

  IsolatedPrerenderTabHelper* tab_helper =
      IsolatedPrerenderTabHelper::FromWebContents(GetWebContents());

  GURL eligible_link_bad_probe = GetOriginServerURLWithBadProbe("/title2.html");

  TestTabHelperObserver tab_helper_observer(tab_helper);
  tab_helper_observer.SetExpectedSuccessfulURLs({eligible_link_bad_probe});

  base::RunLoop run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(run_loop.QuitClosure());

  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {eligible_link_bad_probe});

  // This run loop will quit when all the prefetch responses have been
  // successfully done and processed.
  run_loop.Run();

  // Navigate to the prefetched page, this also triggers UKM recording.
  size_t starting_origin_request_count = OriginServerRequestCount();
  ui_test_utils::NavigateToURL(browser(), eligible_link_bad_probe);

  // The probe and a request for the page should have hit the origin server,
  // since the prefetched page couldn't be used.
  EXPECT_EQ(starting_origin_request_count + 2, OriginServerRequestCount());

  EXPECT_EQ(base::UTF8ToUTF16("Title Of Awesomeness"),
            GetWebContents()->GetTitle());

  ASSERT_TRUE(tab_helper->after_srp_metrics());
  ASSERT_TRUE(tab_helper->after_srp_metrics()->prefetch_status_.has_value());
  // 2 is the value of "prefetch used, probe failed". The test does not
  // reference the enum directly to ensure that casting the enum to an int went
  // cleanly, and to provide an extra review point if the value should ever
  // accidentally change in the future, which it never should.
  EXPECT_EQ(2, static_cast<int>(
                   tab_helper->after_srp_metrics()->prefetch_status_.value()));

  base::Optional<base::TimeDelta> probe_latency =
      tab_helper->after_srp_metrics()->probe_latency_;
  ASSERT_TRUE(probe_latency.has_value());
  EXPECT_GT(probe_latency.value(), base::TimeDelta());
}

IN_PROC_BROWSER_TEST_F(ProbingDisabledIsolatedPrerenderBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(NoProbe)) {
  SetDataSaverEnabled(true);
  GURL starting_page = GetOriginServerURL("/simple.html");
  ui_test_utils::NavigateToURL(browser(), starting_page);
  WaitForUpdatedCustomProxyConfig();

  IsolatedPrerenderTabHelper* tab_helper =
      IsolatedPrerenderTabHelper::FromWebContents(GetWebContents());

  GURL eligible_link = GetOriginServerURL("/title2.html");

  TestTabHelperObserver tab_helper_observer(tab_helper);
  tab_helper_observer.SetExpectedSuccessfulURLs({eligible_link});

  base::RunLoop run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(run_loop.QuitClosure());

  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {eligible_link});

  // This run loop will quit when all the prefetch responses have been
  // successfully done and processed.
  run_loop.Run();

  // Navigate to the prefetched page, this also triggers UKM recording.
  size_t starting_origin_request_count = OriginServerRequestCount();
  ui_test_utils::NavigateToURL(browser(), eligible_link);

  // No probe should have been made, and the page was prefetched so do not
  // expect additional origin server requests.
  EXPECT_EQ(starting_origin_request_count, OriginServerRequestCount());

  EXPECT_EQ(base::UTF8ToUTF16("Title Of Awesomeness"),
            GetWebContents()->GetTitle());

  ASSERT_TRUE(tab_helper->after_srp_metrics());
  ASSERT_TRUE(tab_helper->after_srp_metrics()->prefetch_status_.has_value());
  // 0 is the value of "prefetch used, no probe". The test does not
  // reference the enum directly to ensure that casting the enum to an int went
  // cleanly, and to provide an extra review point if the value should ever
  // accidentally change in the future, which it never should.
  EXPECT_EQ(0, static_cast<int>(
                   tab_helper->after_srp_metrics()->prefetch_status_.value()));

  base::Optional<base::TimeDelta> probe_latency =
      tab_helper->after_srp_metrics()->probe_latency_;
  EXPECT_FALSE(probe_latency.has_value());
}
