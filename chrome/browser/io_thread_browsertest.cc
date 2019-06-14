// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/features.h"
#include "net/base/filename_util.h"
#include "net/base/host_port_pair.h"
#include "net/cert/ct_verifier.h"
#include "net/http/http_auth_preferences.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/simple_connection_listener.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class TestNetworkQualityObserver
    : public network::NetworkQualityTracker::EffectiveConnectionTypeObserver {
 public:
  explicit TestNetworkQualityObserver(network::NetworkQualityTracker* tracker)
      : run_loop_wait_effective_connection_type_(
            net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN),
        tracker_(tracker),
        effective_connection_type_(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {
    tracker_->AddEffectiveConnectionTypeObserver(this);
  }

  ~TestNetworkQualityObserver() override {
    tracker_->RemoveEffectiveConnectionTypeObserver(this);
  }

  // NetworkQualityTracker::EffectiveConnectionTypeObserver implementation:
  void OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType type) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    net::EffectiveConnectionType queried_type =
        tracker_->GetEffectiveConnectionType();
    EXPECT_EQ(type, queried_type);

    effective_connection_type_ = type;
    if (effective_connection_type_ != run_loop_wait_effective_connection_type_)
      return;
    if (run_loop_)
      run_loop_->Quit();
  }

  void WaitForNotification(
      net::EffectiveConnectionType run_loop_wait_effective_connection_type) {
    if (effective_connection_type_ == run_loop_wait_effective_connection_type)
      return;
    ASSERT_NE(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
              run_loop_wait_effective_connection_type);
    run_loop_.reset(new base::RunLoop());
    run_loop_wait_effective_connection_type_ =
        run_loop_wait_effective_connection_type;
    run_loop_->Run();
    run_loop_.reset();
  }

 private:
  net::EffectiveConnectionType run_loop_wait_effective_connection_type_;
  std::unique_ptr<base::RunLoop> run_loop_;
  network::NetworkQualityTracker* tracker_;
  net::EffectiveConnectionType effective_connection_type_;

  DISALLOW_COPY_AND_ASSIGN(TestNetworkQualityObserver);
};

void CheckEffectiveConnectionType(net::EffectiveConnectionType expected) {
  TestNetworkQualityObserver network_quality_observer(
      g_browser_process->network_quality_tracker());
  network_quality_observer.WaitForNotification(expected);
}

class IOThreadBrowserTest : public InProcessBrowserTest {
 public:
  IOThreadBrowserTest() {}
  ~IOThreadBrowserTest() override {}

  void SetUp() override {
    // Must start listening (And get a port for the proxy) before calling
    // SetUp(). Use two phase EmbeddedTestServer setup for proxy tests.
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDown() override {
    // Need to stop this before |connection_listener_| is destroyed.
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InProcessBrowserTest::TearDown();
  }
};

class IOThreadEctCommandLineBrowserTest : public IOThreadBrowserTest {
 public:
  IOThreadEctCommandLineBrowserTest() {}
  ~IOThreadEctCommandLineBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII("--force-effective-connection-type",
                                    "Slow-2G");
  }
};

IN_PROC_BROWSER_TEST_F(IOThreadEctCommandLineBrowserTest,
                       ForceECTFromCommandLine) {
  CheckEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
}

class IOThreadEctFieldTrialBrowserTest : public IOThreadBrowserTest {
 public:
  IOThreadEctFieldTrialBrowserTest() {}
  ~IOThreadEctFieldTrialBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    variations::testing::ClearAllVariationParams();
    std::map<std::string, std::string> variation_params;
    variation_params["force_effective_connection_type"] = "2G";
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        net::features::kNetworkQualityEstimator, variation_params);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(IOThreadEctFieldTrialBrowserTest,
                       ForceECTUsingFieldTrial) {
  CheckEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_2G);
}

class IOThreadEctFieldTrialAndCommandLineBrowserTest
    : public IOThreadEctFieldTrialBrowserTest {
 public:
  IOThreadEctFieldTrialAndCommandLineBrowserTest() {}
  ~IOThreadEctFieldTrialAndCommandLineBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IOThreadEctFieldTrialBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII("--force-effective-connection-type",
                                    "Slow-2G");
  }
};

IN_PROC_BROWSER_TEST_F(IOThreadEctFieldTrialAndCommandLineBrowserTest,
                       ECTFromCommandLineOverridesFieldTrial) {
  CheckEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
}

class IOThreadBrowserTestWithHangingPacRequest : public IOThreadBrowserTest {
 public:
  IOThreadBrowserTestWithHangingPacRequest() {}
  ~IOThreadBrowserTestWithHangingPacRequest() override {}

  void SetUpOnMainThread() override {
    // This must be created after the main message loop has been set up.
    // Waits for one connection.  Additional connections are fine.
    connection_listener_ =
        std::make_unique<net::test_server::SimpleConnectionListener>(
            1, net::test_server::SimpleConnectionListener::
                   ALLOW_ADDITIONAL_CONNECTIONS);

    embedded_test_server()->SetConnectionListener(connection_listener_.get());

    IOThreadBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kProxyPacUrl, embedded_test_server()->GetURL("/hung").spec());
  }

 protected:
  std::unique_ptr<net::test_server::SimpleConnectionListener>
      connection_listener_;
};

// Make sure that the SystemURLRequestContext is shut down correctly when
// there's an in-progress PAC script fetch.
IN_PROC_BROWSER_TEST_F(IOThreadBrowserTestWithHangingPacRequest, Shutdown) {
  // Request that should hang while trying to request the PAC script.
  // Enough requests are created on startup that this probably isn't needed, but
  // best to be safe.
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL("http://blah/");
  auto simple_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), TRAFFIC_ANNOTATION_FOR_TESTS);

  auto* context_manager = g_browser_process->system_network_context_manager();
  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      context_manager->GetURLLoaderFactory(),
      base::BindOnce([](std::unique_ptr<std::string> body) {
        ADD_FAILURE() << "This request should never complete.";
      }));

  connection_listener_->WaitForConnections();
}

}  // namespace
