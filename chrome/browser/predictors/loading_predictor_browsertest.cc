// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/predictors/loading_test_util.h"
#include "chrome/browser/predictors/preconnect_manager.h"
#include "chrome/browser/predictors/predictors_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_test_job.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace predictors {

const char kChromiumUrl[] = "http://chromium.org";
const char kInvalidLongUrl[] =
    "http://"
    "illegally-long-hostname-over-255-characters-should-not-send-an-ipc-"
    "message-to-the-browser-"
    "00000000000000000000000000000000000000000000000000000000000000000000000000"
    "00000000000000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000.org";

const char kHtmlSubresourcesPath[] = "/predictors/html_subresources.html";
// The embedded test server runs on test.com.
// kHtmlSubresourcesPath contains high priority resources from baz.com and
// foo.com. kHtmlSubresourcesPath also contains a low priority resource from
// bar.com.
const char* const kHtmlSubresourcesHosts[] = {"test.com", "baz.com", "foo.com"};

std::string GetPathWithPortReplacement(const std::string& path, uint16_t port) {
  std::string string_port = base::StringPrintf("%d", port);
  return net::test_server::GetFilePathWithReplacements(
      path, {{"REPLACE_WITH_PORT", string_port}});
}

GURL GetDataURLWithContent(const std::string& content) {
  std::string encoded_content;
  base::Base64Encode(content, &encoded_content);
  std::string data_uri_content = "data:text/html;base64," + encoded_content;
  return GURL(data_uri_content);
}

// Helper class to track and allow waiting for ResourcePrefetchPredictor
// initialization. WARNING: OnPredictorInitialized event will not be fired if
// ResourcePrefetchPredictor is initialized before the observer creation.
class PredictorInitializer : public TestObserver {
 public:
  explicit PredictorInitializer(ResourcePrefetchPredictor* predictor)
      : TestObserver(predictor), predictor_(predictor) {}

  void EnsurePredictorInitialized() {
    if (predictor_->initialization_state_ ==
        ResourcePrefetchPredictor::INITIALIZED) {
      return;
    }

    if (predictor_->initialization_state_ ==
        ResourcePrefetchPredictor::NOT_INITIALIZED) {
      predictor_->StartInitialization();
    }

    run_loop_.Run();
  }

  void OnPredictorInitialized() override { run_loop_.Quit(); }

 private:
  ResourcePrefetchPredictor* predictor_;
  base::RunLoop run_loop_;
  DISALLOW_COPY_AND_ASSIGN(PredictorInitializer);
};

// Keeps track of incoming connections being accepted or read from and exposes
// that info to the tests.
// A port being reused is currently considered an error.
// If a test needs to verify multiple connections are opened in sequence, that
// will need to be changed.
class ConnectionTracker {
 public:
  ConnectionTracker() {}

  void AcceptedSocketWithPort(uint16_t port) {
    EXPECT_FALSE(base::Contains(sockets_, port));
    sockets_[port] = SocketStatus::kAccepted;
    CheckAccepted();
    first_accept_loop_.Quit();
  }

  void ReadFromSocketWithPort(uint16_t port) {
    EXPECT_TRUE(base::Contains(sockets_, port));
    sockets_[port] = SocketStatus::kReadFrom;
    first_read_loop_.Quit();
  }

  // Returns the number of sockets that were accepted by the server.
  size_t GetAcceptedSocketCount() const { return sockets_.size(); }

  // Returns the number of sockets that were read from by the server.
  size_t GetReadSocketCount() const {
    size_t read_sockets = 0;
    for (const auto& socket : sockets_) {
      if (socket.second == SocketStatus::kReadFrom)
        ++read_sockets;
    }
    return read_sockets;
  }

  void WaitUntilFirstConnectionAccepted() { first_accept_loop_.Run(); }
  void WaitUntilFirstConnectionRead() { first_read_loop_.Run(); }

  // The UI thread will wait for exactly |num_connections| items in |sockets_|.
  // This method expects the server will not accept more than |num_connections|
  // connections. |num_connections| must be greater than 0.
  void WaitForAcceptedConnections(size_t num_connections) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(!num_accepted_connections_loop_);
    DCHECK_GT(num_connections, 0u);
    base::RunLoop run_loop;
    EXPECT_GE(num_connections, sockets_.size());
    num_accepted_connections_loop_ = &run_loop;
    num_accepted_connections_needed_ = num_connections;
    CheckAccepted();
    // Note that the previous call to CheckAccepted can quit this run loop
    // before this call, which will make this call a no-op.
    run_loop.Run();
    EXPECT_EQ(num_connections, sockets_.size());
  }

  // Helper function to stop the waiting for sockets to be accepted for
  // WaitForAcceptedConnections. |num_accepted_connections_loop_| spins
  // until |num_accepted_connections_needed_| sockets are accepted by the test
  // server. The values will be null/0 if the loop is not running.
  void CheckAccepted() {
    // |num_accepted_connections_loop_| null implies
    // |num_accepted_connections_needed_| == 0.
    DCHECK(num_accepted_connections_loop_ ||
           num_accepted_connections_needed_ == 0);
    if (!num_accepted_connections_loop_ ||
        num_accepted_connections_needed_ != sockets_.size()) {
      return;
    }

    num_accepted_connections_loop_->Quit();
    num_accepted_connections_needed_ = 0;
    num_accepted_connections_loop_ = nullptr;
  }

  void ResetCounts() { sockets_.clear(); }

 private:
  enum class SocketStatus { kAccepted, kReadFrom };

  base::RunLoop first_accept_loop_;
  base::RunLoop first_read_loop_;

  // Port -> SocketStatus.
  using SocketContainer = std::map<uint16_t, SocketStatus>;
  SocketContainer sockets_;

  // If |num_accepted_connections_needed_| is non zero, then the object is
  // waiting for |num_accepted_connections_needed_| sockets to be accepted
  // before quitting the |num_accepted_connections_loop_|.
  size_t num_accepted_connections_needed_ = 0;
  base::RunLoop* num_accepted_connections_loop_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ConnectionTracker);
};

// Gets notified by the EmbeddedTestServer on incoming connections being
// accepted or read from and transfers this information to ConnectionTracker.
class ConnectionListener
    : public net::test_server::EmbeddedTestServerConnectionListener {
 public:
  // This class should be constructed on the browser UI thread.
  explicit ConnectionListener(ConnectionTracker* tracker)
      : task_runner_(base::ThreadTaskRunnerHandle::Get()), tracker_(tracker) {}

  ~ConnectionListener() override {}

  // Get called from the EmbeddedTestServer thread to be notified that
  // a connection was accepted.
  void AcceptedSocket(const net::StreamSocket& connection) override {
    uint16_t port = GetPort(connection);
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ConnectionTracker::AcceptedSocketWithPort,
                                  base::Unretained(tracker_), port));
  }

  // Get called from the EmbeddedTestServer thread to be notified that
  // a connection was read from.
  void ReadFromSocket(const net::StreamSocket& connection, int rv) override {
    // Don't log a read if no data was transferred. This case often happens if
    // the sockets of the test server are being flushed and disconnected.
    if (rv <= 0)
      return;
    uint16_t port = GetPort(connection);
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ConnectionTracker::ReadFromSocketWithPort,
                                  base::Unretained(tracker_), port));
  }

 private:
  static uint16_t GetPort(const net::StreamSocket& connection) {
    // Get the remote port of the peer, since the local port will always be the
    // port the test server is listening on. This isn't strictly correct - it's
    // possible for multiple peers to connect with the same remote port but
    // different remote IPs - but the tests here assume that connections to the
    // test server (running on localhost) will always come from localhost, and
    // thus the peer port is all that's needed to distinguish two connections.
    // This also would be problematic if the OS reused ports, but that's not
    // something to worry about for these tests.
    net::IPEndPoint address;
    EXPECT_EQ(net::OK, connection.GetPeerAddress(&address));
    return address.port();
  }

  // Task runner associated with the browser UI thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // This pointer should be only accessed on the browser UI thread.
  ConnectionTracker* tracker_;
};

class TestPreconnectManagerObserver : public PreconnectManager::Observer {
 public:
  explicit TestPreconnectManagerObserver(
      PreconnectManager* preconnect_manager_) {
    preconnect_manager_->SetObserverForTesting(this);
  }

  void OnPreconnectUrl(const GURL& url,
                       int num_sockets,
                       bool allow_credentials) override {
    preconnect_url_attempts_.insert(url.GetOrigin());
  }

  void OnPreresolveFinished(const GURL& url, bool success) override {
    if (success)
      successful_dns_lookups_.insert(url.host());
    else
      unsuccessful_dns_lookups_.insert(url.host());
    CheckForWaitingLoop();
  }

  void OnProxyLookupFinished(const GURL& url, bool success) override {
    GURL origin = url.GetOrigin();
    if (success)
      successful_proxy_lookups_.insert(origin);
    else
      unsuccessful_proxy_lookups_.insert(origin);
    CheckForWaitingLoop();
  }

  void WaitUntilHostLookedUp(const std::string& host) {
    wait_event_ = WaitEvent::kDns;
    DCHECK(waiting_on_dns_.empty());
    waiting_on_dns_ = host;
    Wait();
  }

  void WaitUntilProxyLookedUp(const GURL& url) {
    wait_event_ = WaitEvent::kProxy;
    DCHECK(waiting_on_proxy_.is_empty());
    waiting_on_proxy_ = url;
    Wait();
  }

  bool HasOriginAttemptedToPreconnect(const GURL& origin) {
    DCHECK_EQ(origin, origin.GetOrigin());
    return base::Contains(preconnect_url_attempts_, origin);
  }

  bool HasHostBeenLookedUp(const std::string& host) {
    return base::Contains(successful_dns_lookups_, host) ||
           base::Contains(unsuccessful_dns_lookups_, host);
  }

  bool HostFound(const std::string& host) {
    return base::Contains(successful_dns_lookups_, host);
  }

  bool HasProxyBeenLookedUp(const GURL& url) {
    return base::Contains(successful_proxy_lookups_, url.GetOrigin()) ||
           base::Contains(unsuccessful_proxy_lookups_, url.GetOrigin());
  }

  bool ProxyFound(const GURL& url) {
    return base::Contains(successful_proxy_lookups_, url.GetOrigin());
  }

 private:
  enum class WaitEvent { kNone, kDns, kProxy };

  void Wait() {
    base::RunLoop run_loop;
    DCHECK(!run_loop_);
    run_loop_ = &run_loop;
    CheckForWaitingLoop();
    run_loop.Run();
  }

  void CheckForWaitingLoop() {
    switch (wait_event_) {
      case WaitEvent::kNone:
        return;
      case WaitEvent::kDns:
        if (!HasHostBeenLookedUp(waiting_on_dns_))
          return;
        waiting_on_dns_ = std::string();
        break;
      case WaitEvent::kProxy:
        if (!HasProxyBeenLookedUp(waiting_on_proxy_))
          return;
        waiting_on_proxy_ = GURL();
        break;
    }
    DCHECK(run_loop_);
    run_loop_->Quit();
    run_loop_ = nullptr;
    wait_event_ = WaitEvent::kNone;
  }

  WaitEvent wait_event_ = WaitEvent::kNone;
  base::RunLoop* run_loop_ = nullptr;

  std::string waiting_on_dns_;
  std::set<std::string> successful_dns_lookups_;
  std::set<std::string> unsuccessful_dns_lookups_;

  GURL waiting_on_proxy_;
  std::set<GURL> successful_proxy_lookups_;
  std::set<GURL> unsuccessful_proxy_lookups_;

  std::set<GURL> preconnect_url_attempts_;
};

class LoadingPredictorBrowserTest : public InProcessBrowserTest {
 public:
  LoadingPredictorBrowserTest() {}
  ~LoadingPredictorBrowserTest() override {}

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kLoadingOnlyLearnHighPriorityResources);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    connection_tracker_ = std::make_unique<ConnectionTracker>();
    connection_listener_ =
        std::make_unique<ConnectionListener>(connection_tracker_.get());
    embedded_test_server()->SetConnectionListener(connection_listener_.get());
    embedded_test_server()->StartAcceptingConnections();

    loading_predictor_ =
        LoadingPredictorFactory::GetForProfile(browser()->profile());
    ASSERT_TRUE(loading_predictor_);
    preconnect_manager_observer_ =
        std::make_unique<TestPreconnectManagerObserver>(
            loading_predictor_->preconnect_manager());
    PredictorInitializer initializer(
        loading_predictor_->resource_prefetch_predictor());
    initializer.EnsurePredictorInitialized();
  }

  // Navigates to an URL without blocking until the navigation finishes.
  // Returns an observer that can be used to wait for the navigation
  // completion.
  // This function creates a new tab for each navigation that allows multiple
  // simultaneous navigations and avoids triggering the reload behavior.
  std::unique_ptr<content::TestNavigationManager> NavigateToURLAsync(
      const GURL& url) {
    chrome::NewTab(browser());
    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    DCHECK(tab);
    auto observer = std::make_unique<content::TestNavigationManager>(tab, url);
    tab->GetController().LoadURL(url, content::Referrer(),
                                 ui::PAGE_TRANSITION_TYPED, std::string());
    return observer;
  }

  void ResetNetworkState() {
    auto* network_context = content::BrowserContext::GetDefaultStoragePartition(
                                browser()->profile())
                                ->GetNetworkContext();
    base::RunLoop clear_host_cache_loop;
    base::RunLoop close_all_connections_loop;
    network_context->ClearHostCache(nullptr,
                                    clear_host_cache_loop.QuitClosure());
    network_context->CloseAllConnections(
        close_all_connections_loop.QuitClosure());
    clear_host_cache_loop.Run();
    close_all_connections_loop.Run();

    connection_tracker()->ResetCounts();
  }

  void ResetPredictorState() {
    loading_predictor_->resource_prefetch_predictor()->DeleteAllUrls();
  }

  std::unique_ptr<PreconnectPrediction> GetPreconnectPrediction(
      const GURL& url) {
    auto prediction = std::make_unique<PreconnectPrediction>();
    bool has_prediction = loading_predictor_->resource_prefetch_predictor()
                              ->PredictPreconnectOrigins(url, prediction.get());
    if (!has_prediction)
      return nullptr;
    return prediction;
  }

  LoadingPredictor* loading_predictor() { return loading_predictor_; }

  TestPreconnectManagerObserver* preconnect_manager_observer() {
    return preconnect_manager_observer_.get();
  }

  ConnectionTracker* connection_tracker() { return connection_tracker_.get(); }

 private:
  LoadingPredictor* loading_predictor_ = nullptr;
  std::unique_ptr<ConnectionListener> connection_listener_;
  std::unique_ptr<ConnectionTracker> connection_tracker_;
  std::unique_ptr<TestPreconnectManagerObserver> preconnect_manager_observer_;
  base::test::ScopedFeatureList scoped_feature_list_;
  DISALLOW_COPY_AND_ASSIGN(LoadingPredictorBrowserTest);
};

// Tests that a navigation triggers the LoadingPredictor.
IN_PROC_BROWSER_TEST_F(LoadingPredictorBrowserTest, SimpleNavigation) {
  GURL url = embedded_test_server()->GetURL("/nocontent");
  auto observer = NavigateToURLAsync(url);
  EXPECT_TRUE(observer->WaitForRequestStart());
  EXPECT_EQ(1u, loading_predictor()->GetActiveNavigationsSizeForTesting());
  // Checking GetActiveHintsSizeForTesting() is racy since the active hint
  // is removed after the preconnect finishes. Instead check for total
  // hints activated.
  EXPECT_LE(1u, loading_predictor()->GetTotalHintsActivatedForTesting());
  EXPECT_GE(2u, loading_predictor()->GetTotalHintsActivatedForTesting());
  observer->WaitForNavigationFinished();
  EXPECT_EQ(0u, loading_predictor()->GetActiveNavigationsSizeForTesting());
  EXPECT_EQ(0u, loading_predictor()->GetActiveHintsSizeForTesting());
  EXPECT_LE(1u, loading_predictor()->GetTotalHintsActivatedForTesting());
  EXPECT_GE(2u, loading_predictor()->GetTotalHintsActivatedForTesting());
}

// Tests that two concurrenct navigations are recorded correctly by the
// predictor.
IN_PROC_BROWSER_TEST_F(LoadingPredictorBrowserTest, TwoConcurrentNavigations) {
  GURL url1 = embedded_test_server()->GetURL("/echo-raw?1");
  GURL url2 = embedded_test_server()->GetURL("/echo-raw?2");
  auto observer1 = NavigateToURLAsync(url1);
  auto observer2 = NavigateToURLAsync(url2);
  EXPECT_TRUE(observer1->WaitForRequestStart());
  EXPECT_TRUE(observer2->WaitForRequestStart());
  EXPECT_EQ(2u, loading_predictor()->GetActiveNavigationsSizeForTesting());
  // Checking GetActiveHintsSizeForTesting() is racy since the active hint
  // is removed after the preconnect finishes. Instead check for total
  // hints activated.
  EXPECT_LE(2u, loading_predictor()->GetTotalHintsActivatedForTesting());
  EXPECT_GE(4u, loading_predictor()->GetTotalHintsActivatedForTesting());
  observer1->WaitForNavigationFinished();
  observer2->WaitForNavigationFinished();
  EXPECT_EQ(0u, loading_predictor()->GetActiveNavigationsSizeForTesting());
  EXPECT_EQ(0u, loading_predictor()->GetActiveHintsSizeForTesting());
  EXPECT_LE(2u, loading_predictor()->GetTotalHintsActivatedForTesting());
  EXPECT_GE(4u, loading_predictor()->GetTotalHintsActivatedForTesting());
}

// Tests that two navigations to the same URL are deduplicated.
IN_PROC_BROWSER_TEST_F(LoadingPredictorBrowserTest,
                       TwoNavigationsToTheSameURL) {
  GURL url = embedded_test_server()->GetURL("/nocontent");
  auto observer1 = NavigateToURLAsync(url);
  auto observer2 = NavigateToURLAsync(url);
  EXPECT_TRUE(observer1->WaitForRequestStart());
  EXPECT_TRUE(observer2->WaitForRequestStart());
  EXPECT_EQ(2u, loading_predictor()->GetActiveNavigationsSizeForTesting());
  // Checking GetActiveHintsSizeForTesting() is racy since the active hint
  // is removed after the preconnect finishes. Instead check for total
  // hints activated. The total hints activated may be only 1 if the second
  // navigation arrives before the first preconnect finishes. However, if the
  // second navigation arrives later, then two hints may get activated.
  EXPECT_LE(1u, loading_predictor()->GetTotalHintsActivatedForTesting());
  EXPECT_GE(4u, loading_predictor()->GetTotalHintsActivatedForTesting());
  observer1->WaitForNavigationFinished();
  observer2->WaitForNavigationFinished();
  EXPECT_EQ(0u, loading_predictor()->GetActiveNavigationsSizeForTesting());
  EXPECT_EQ(0u, loading_predictor()->GetActiveHintsSizeForTesting());
  EXPECT_LE(1u, loading_predictor()->GetTotalHintsActivatedForTesting());
  EXPECT_GE(4u, loading_predictor()->GetTotalHintsActivatedForTesting());
}

// Tests that the LoadingPredictor doesn't record non-http(s) navigations.
IN_PROC_BROWSER_TEST_F(LoadingPredictorBrowserTest, NonHttpNavigation) {
  std::string content = "<body>Hello world!</body>";
  GURL url = GetDataURLWithContent(content);
  auto observer = NavigateToURLAsync(url);
  EXPECT_TRUE(observer->WaitForRequestStart());
  EXPECT_EQ(0u, loading_predictor()->GetActiveNavigationsSizeForTesting());
  EXPECT_EQ(0u, loading_predictor()->GetActiveHintsSizeForTesting());
}

// Tests that the LoadingPredictor doesn't preconnect to non-http(s) urls.
IN_PROC_BROWSER_TEST_F(LoadingPredictorBrowserTest,
                       PrepareForPageLoadNonHttpScheme) {
  std::string content = "<body>Hello world!</body>";
  GURL url = GetDataURLWithContent(content);
  ui_test_utils::NavigateToURL(browser(), url);
  // Ensure that no backgound task would make a host lookup or attempt to
  // preconnect.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(preconnect_manager_observer()->HasHostBeenLookedUp(url.host()));
  EXPECT_FALSE(preconnect_manager_observer()->HasHostBeenLookedUp(""));
  EXPECT_FALSE(preconnect_manager_observer()->HasOriginAttemptedToPreconnect(
      url.GetOrigin()));
  EXPECT_FALSE(
      preconnect_manager_observer()->HasOriginAttemptedToPreconnect(GURL()));
}

// Tests that the LoadingPredictor preconnects to the main frame origin even if
// it doesn't have any prediction for this origin.
IN_PROC_BROWSER_TEST_F(LoadingPredictorBrowserTest,
                       PrepareForPageLoadWithoutPrediction) {
  // Navigate the first time to fill the HTTP cache.
  GURL url = embedded_test_server()->GetURL(
      "test.com", GetPathWithPortReplacement(kHtmlSubresourcesPath,
                                             embedded_test_server()->port()));
  ui_test_utils::NavigateToURL(browser(), url);
  ResetNetworkState();
  ResetPredictorState();

  auto observer = NavigateToURLAsync(url);
  EXPECT_TRUE(observer->WaitForRequestStart());
  preconnect_manager_observer()->WaitUntilHostLookedUp(url.host());
  EXPECT_TRUE(preconnect_manager_observer()->HostFound(url.host()));
  // We should preconnect only 2 sockets for the main frame host.
  const size_t expected_connections = 2;
  connection_tracker()->WaitForAcceptedConnections(expected_connections);
  EXPECT_EQ(expected_connections,
            connection_tracker()->GetAcceptedSocketCount());
  // No reads since all resources should be cached.
  EXPECT_EQ(0u, connection_tracker()->GetReadSocketCount());
}

// Tests that the LoadingPredictor has a prediction for a host after navigating
// to it.
IN_PROC_BROWSER_TEST_F(LoadingPredictorBrowserTest, LearnFromNavigation) {
  base::test::ScopedFeatureList scoped_feature_list;
  GURL url = embedded_test_server()->GetURL(
      "test.com", GetPathWithPortReplacement(kHtmlSubresourcesPath,
                                             embedded_test_server()->port()));
  std::vector<PreconnectRequest> requests;
  for (auto* const host : kHtmlSubresourcesHosts)
    requests.emplace_back(embedded_test_server()->GetURL(host, "/"), 1);

  ui_test_utils::NavigateToURL(browser(), url);
  auto prediction = GetPreconnectPrediction(url);
  ASSERT_TRUE(prediction);
  EXPECT_EQ(prediction->is_redirected, false);
  EXPECT_EQ(prediction->host, url.host());
  EXPECT_THAT(prediction->requests,
              testing::UnorderedElementsAreArray(requests));
}

// Tests that the LoadingPredictor has a prediction for a host after navigating
// to it. Disables kLoadingOnlyLearnHighPriorityResources.
IN_PROC_BROWSER_TEST_F(
    LoadingPredictorBrowserTest,
    LearnFromNavigation_DisablekLoadingOnlyLearnHighPriorityResources) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kLoadingOnlyLearnHighPriorityResources);

  GURL url = embedded_test_server()->GetURL(
      "test.com", GetPathWithPortReplacement(kHtmlSubresourcesPath,
                                             embedded_test_server()->port()));
  std::vector<PreconnectRequest> requests;
  for (auto* const host : kHtmlSubresourcesHosts)
    requests.emplace_back(embedded_test_server()->GetURL(host, "/"), 1);

  // When kLoadingOnlyLearnHighPriorityResources is disabled, loading data
  // collector should learn the loading of low priority resources hosted on
  // bar.com as well.
  requests.emplace_back(embedded_test_server()->GetURL("bar.com", "/"), 1);

  ui_test_utils::NavigateToURL(browser(), url);
  auto prediction = GetPreconnectPrediction(url);
  ASSERT_TRUE(prediction);
  EXPECT_EQ(prediction->is_redirected, false);
  EXPECT_EQ(prediction->host, url.host());
  EXPECT_THAT(prediction->requests,
              testing::UnorderedElementsAreArray(requests));
}

// Tests that the LoadingPredictor correctly learns from navigations with
// redirect.
IN_PROC_BROWSER_TEST_F(LoadingPredictorBrowserTest,
                       LearnFromNavigationWithRedirect) {
  GURL redirect_url = embedded_test_server()->GetURL(
      "test.com", GetPathWithPortReplacement(kHtmlSubresourcesPath,
                                             embedded_test_server()->port()));
  GURL original_url = embedded_test_server()->GetURL(
      "redirect.com",
      base::StringPrintf("/server-redirect?%s", redirect_url.spec().c_str()));
  std::vector<PreconnectRequest> requests;
  for (auto* const host : kHtmlSubresourcesHosts)
    requests.emplace_back(embedded_test_server()->GetURL(host, "/"), 1);

  ui_test_utils::NavigateToURL(browser(), original_url);
  // The predictor can correctly predict hosts by |redirect_url|
  auto prediction = GetPreconnectPrediction(redirect_url);
  ASSERT_TRUE(prediction);
  EXPECT_EQ(prediction->is_redirected, false);
  EXPECT_EQ(prediction->host, redirect_url.host());
  EXPECT_THAT(prediction->requests,
              testing::UnorderedElementsAreArray(requests));
  // The predictor needs minimum two redirect hits to be confident in the
  // redirect.
  prediction = GetPreconnectPrediction(original_url);
  EXPECT_FALSE(prediction);

  // The predictor will start predict a redirect after the second navigation.
  ui_test_utils::NavigateToURL(browser(), original_url);
  prediction = GetPreconnectPrediction(original_url);
  ASSERT_TRUE(prediction);
  EXPECT_EQ(prediction->is_redirected, true);
  EXPECT_EQ(prediction->host, redirect_url.host());
  EXPECT_THAT(prediction->requests,
              testing::UnorderedElementsAreArray(requests));
}

// Tests that the LoadingPredictor performs preresolving/preconnecting for a
// navigation which it has a prediction for.
IN_PROC_BROWSER_TEST_F(LoadingPredictorBrowserTest,
                       PrepareForPageLoadWithPrediction) {
  // Navigate the first time to fill the predictor's database and the HTTP
  // cache.
  GURL url = embedded_test_server()->GetURL(
      "test.com", GetPathWithPortReplacement(kHtmlSubresourcesPath,
                                             embedded_test_server()->port()));
  ui_test_utils::NavigateToURL(browser(), url);
  ResetNetworkState();

  auto observer = NavigateToURLAsync(url);
  EXPECT_TRUE(observer->WaitForRequestStart());
  for (auto* const host : kHtmlSubresourcesHosts) {
    GURL url(base::StringPrintf("http://%s", host));
    preconnect_manager_observer()->WaitUntilHostLookedUp(url.host());
    EXPECT_TRUE(preconnect_manager_observer()->HostFound(url.host()));
  }
  // 2 connections to the main frame host + 1 connection per host for others.
  const size_t expected_connections = base::size(kHtmlSubresourcesHosts) + 1;
  connection_tracker()->WaitForAcceptedConnections(expected_connections);
  EXPECT_EQ(expected_connections,
            connection_tracker()->GetAcceptedSocketCount());
  // No reads since all resources should be cached.
  EXPECT_EQ(0u, connection_tracker()->GetReadSocketCount());
}

// Tests that a host requested by <link rel="dns-prefetch"> is looked up.
IN_PROC_BROWSER_TEST_F(LoadingPredictorBrowserTest, DnsPrefetch) {
  ui_test_utils::NavigateToURL(browser(), embedded_test_server()->GetURL(
                                              "/predictor/dns_prefetch.html"));
  preconnect_manager_observer()->WaitUntilHostLookedUp(
      GURL(kChromiumUrl).host());
  EXPECT_FALSE(preconnect_manager_observer()->HasHostBeenLookedUp(
      GURL(kInvalidLongUrl).host()));
  EXPECT_TRUE(
      preconnect_manager_observer()->HostFound(GURL(kChromiumUrl).host()));
}

// Tests that preconnect warms up a socket connection to a test server.
// Note: This test uses a data URI to serve the preconnect hint, to make sure
// that the network stack doesn't just re-use its connection to the test server.
IN_PROC_BROWSER_TEST_F(LoadingPredictorBrowserTest, PreconnectNonCors) {
  GURL preconnect_url = embedded_test_server()->base_url();
  std::string preconnect_content =
      "<link rel=\"preconnect\" href=\"" + preconnect_url.spec() + "\">";
  ui_test_utils::NavigateToURL(browser(),
                               GetDataURLWithContent(preconnect_content));
  connection_tracker()->WaitUntilFirstConnectionAccepted();
  EXPECT_EQ(1u, connection_tracker()->GetAcceptedSocketCount());
  EXPECT_EQ(0u, connection_tracker()->GetReadSocketCount());
}

// Tests that preconnect warms up a socket connection to a test server,
// and that that socket is later used when fetching a resource.
// Note: This test uses a data URI to serve the preconnect hint, to make sure
// that the network stack doesn't just re-use its connection to the test server.
IN_PROC_BROWSER_TEST_F(LoadingPredictorBrowserTest, PreconnectAndFetchNonCors) {
  GURL preconnect_url = embedded_test_server()->base_url();
  // First navigation to content with a preconnect hint.
  std::string preconnect_content =
      "<link rel=\"preconnect\" href=\"" + preconnect_url.spec() + "\">";
  ui_test_utils::NavigateToURL(browser(),
                               GetDataURLWithContent(preconnect_content));
  connection_tracker()->WaitUntilFirstConnectionAccepted();
  EXPECT_EQ(1u, connection_tracker()->GetAcceptedSocketCount());
  EXPECT_EQ(0u, connection_tracker()->GetReadSocketCount());

  // Second navigation to content with an img.
  std::string img_content =
      "<img src=\"" + preconnect_url.spec() + "test.gif\">";
  ui_test_utils::NavigateToURL(browser(), GetDataURLWithContent(img_content));
  connection_tracker()->WaitUntilFirstConnectionRead();
  EXPECT_EQ(1u, connection_tracker()->GetAcceptedSocketCount());
  EXPECT_EQ(1u, connection_tracker()->GetReadSocketCount());
}

// Tests that preconnect warms up a CORS connection to a test
// server, and that socket is later used when fetching a CORS resource.
// Note: This test uses a data URI to serve the preconnect hint, to make sure
// that the network stack doesn't just re-use its connection to the test server.
IN_PROC_BROWSER_TEST_F(LoadingPredictorBrowserTest, PreconnectAndFetchCors) {
  GURL preconnect_url = embedded_test_server()->base_url();
  // First navigation to content with a preconnect hint.
  std::string preconnect_content = "<link rel=\"preconnect\" href=\"" +
                                   preconnect_url.spec() + "\" crossorigin>";
  ui_test_utils::NavigateToURL(browser(),
                               GetDataURLWithContent(preconnect_content));
  connection_tracker()->WaitUntilFirstConnectionAccepted();
  EXPECT_EQ(1u, connection_tracker()->GetAcceptedSocketCount());
  EXPECT_EQ(0u, connection_tracker()->GetReadSocketCount());

  // Second navigation to content with a font.
  std::string font_content = "<script>var font = new FontFace('FontA', 'url(" +
                             preconnect_url.spec() +
                             "test.woff2)');font.load();</script>";
  ui_test_utils::NavigateToURL(browser(), GetDataURLWithContent(font_content));
  connection_tracker()->WaitUntilFirstConnectionRead();
  EXPECT_EQ(1u, connection_tracker()->GetAcceptedSocketCount());
  EXPECT_EQ(1u, connection_tracker()->GetReadSocketCount());
}

// Tests that preconnect warms up a non-CORS connection to a test
// server, but that socket is not used when fetching a CORS resource.
// Note: This test uses a data URI to serve the preconnect hint, to make sure
// that the network stack doesn't just re-use its connection to the test server.
IN_PROC_BROWSER_TEST_F(LoadingPredictorBrowserTest,
                       PreconnectNonCorsAndFetchCors) {
  GURL preconnect_url = embedded_test_server()->base_url();
  // First navigation to content with a preconnect hint.
  std::string preconnect_content =
      "<link rel=\"preconnect\" href=\"" + preconnect_url.spec() + "\">";
  ui_test_utils::NavigateToURL(browser(),
                               GetDataURLWithContent(preconnect_content));
  connection_tracker()->WaitUntilFirstConnectionAccepted();
  EXPECT_EQ(1u, connection_tracker()->GetAcceptedSocketCount());
  EXPECT_EQ(0u, connection_tracker()->GetReadSocketCount());

  // Second navigation to content with a font.
  std::string font_content = "<script>var font = new FontFace('FontA', 'url(" +
                             preconnect_url.spec() +
                             "test.woff2)');font.load();</script>";
  ui_test_utils::NavigateToURL(browser(), GetDataURLWithContent(font_content));
  connection_tracker()->WaitUntilFirstConnectionRead();
  EXPECT_EQ(2u, connection_tracker()->GetAcceptedSocketCount());
  EXPECT_EQ(1u, connection_tracker()->GetReadSocketCount());
}

// Tests that preconnect warms up a CORS connection to a test server,
// but that socket is not used when fetching a non-CORS resource.
// Note: This test uses a data URI to serve the preconnect hint, to make sure
// that the network stack doesn't just re-use its connection to the test server.
IN_PROC_BROWSER_TEST_F(LoadingPredictorBrowserTest,
                       PreconnectCorsAndFetchNonCors) {
  GURL preconnect_url = embedded_test_server()->base_url();
  // First navigation to content with a preconnect hint.
  std::string preconnect_content = "<link rel=\"preconnect\" href=\"" +
                                   preconnect_url.spec() + "\" crossorigin>";
  ui_test_utils::NavigateToURL(browser(),
                               GetDataURLWithContent(preconnect_content));
  connection_tracker()->WaitUntilFirstConnectionAccepted();
  EXPECT_EQ(1u, connection_tracker()->GetAcceptedSocketCount());
  EXPECT_EQ(0u, connection_tracker()->GetReadSocketCount());

  // Second navigation to content with an img.
  std::string img_content =
      "<img src=\"" + preconnect_url.spec() + "test.gif\">";
  ui_test_utils::NavigateToURL(browser(), GetDataURLWithContent(img_content));
  connection_tracker()->WaitUntilFirstConnectionRead();
  EXPECT_EQ(2u, connection_tracker()->GetAcceptedSocketCount());
  EXPECT_EQ(1u, connection_tracker()->GetReadSocketCount());
}

class LoadingPredictorBrowserTestWithProxy
    : public LoadingPredictorBrowserTest {
 public:
  void SetUp() override {
    pac_script_server_ = std::make_unique<net::EmbeddedTestServer>();
    pac_script_server_->AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(pac_script_server_->InitializeAndListen());
    LoadingPredictorBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    LoadingPredictorBrowserTest::SetUpOnMainThread();
    // This will make all dns requests fail.
    host_resolver()->ClearRules();

    pac_script_server_->StartAcceptingConnections();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    GURL pac_url = pac_script_server_->GetURL(GetPathWithPortReplacement(
        "/predictors/proxy.pac", embedded_test_server()->port()));
    command_line->AppendSwitchASCII(switches::kProxyPacUrl, pac_url.spec());
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> pac_script_server_;
};

IN_PROC_BROWSER_TEST_F(LoadingPredictorBrowserTestWithProxy,
                       PrepareForPageLoadWithoutPrediction) {
  // Navigate the first time to fill the HTTP cache.
  GURL url = embedded_test_server()->GetURL(
      "test.com", GetPathWithPortReplacement(kHtmlSubresourcesPath,
                                             embedded_test_server()->port()));
  ui_test_utils::NavigateToURL(browser(), url);
  ResetNetworkState();
  ResetPredictorState();

  auto observer = NavigateToURLAsync(url);
  EXPECT_TRUE(observer->WaitForRequestStart());
  preconnect_manager_observer()->WaitUntilProxyLookedUp(url);
  EXPECT_TRUE(preconnect_manager_observer()->ProxyFound(url));
  // We should preconnect only 2 sockets for the main frame host.
  const size_t expected_connections = 2;
  connection_tracker()->WaitForAcceptedConnections(expected_connections);
  EXPECT_EQ(expected_connections,
            connection_tracker()->GetAcceptedSocketCount());
  // No reads since all resources should be cached.
  EXPECT_EQ(0u, connection_tracker()->GetReadSocketCount());
}

IN_PROC_BROWSER_TEST_F(LoadingPredictorBrowserTestWithProxy,
                       PrepareForPageLoadWithPrediction) {
  // Navigate the first time to fill the predictor's database and the HTTP
  // cache.
  GURL url = embedded_test_server()->GetURL(
      "test.com", GetPathWithPortReplacement(kHtmlSubresourcesPath,
                                             embedded_test_server()->port()));
  ui_test_utils::NavigateToURL(browser(), url);
  ResetNetworkState();

  auto observer = NavigateToURLAsync(url);
  EXPECT_TRUE(observer->WaitForRequestStart());
  for (auto* const host : kHtmlSubresourcesHosts) {
    GURL url = embedded_test_server()->GetURL(host, "/");
    preconnect_manager_observer()->WaitUntilProxyLookedUp(url);
    EXPECT_TRUE(preconnect_manager_observer()->ProxyFound(url));
  }
  // 2 connections to the main frame host + 1 connection per host for others.
  const size_t expected_connections = base::size(kHtmlSubresourcesHosts) + 1;
  connection_tracker()->WaitForAcceptedConnections(expected_connections);
  EXPECT_EQ(expected_connections,
            connection_tracker()->GetAcceptedSocketCount());
  // No reads since all resources should be cached.
  EXPECT_EQ(0u, connection_tracker()->GetReadSocketCount());
}

}  // namespace predictors
