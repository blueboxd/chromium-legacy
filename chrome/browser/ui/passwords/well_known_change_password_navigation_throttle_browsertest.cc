// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/well_known_change_password_navigation_throttle.h"

#include <map>
#include <utility>
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/url_constants.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/cert/x509_certificate.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {
using chrome::kWellKnownChangePasswordPath;
using chrome::kWellKnownNotExistingResourcePath;
using content::NavigationThrottle;
using content::TestNavigationObserver;
using net::test_server::BasicHttpResponse;
using net::test_server::DelayedHttpResponse;
using net::test_server::EmbeddedTestServer;
using net::test_server::EmbeddedTestServerHandle;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

// ServerResponse describes how a server should respond to a given path.
struct ServerResponse {
  net::HttpStatusCode status_code;
  std::vector<std::pair<std::string, std::string>> headers;
  int resolve_time_in_milliseconds;
};

// The NavigationThrottle is making 2 requests in parallel. With this config we
// simulate the different orders for the arrival of the responses. The value
// represents the delay in milliseconds.
struct ResponseDelayParams {
  int change_password_delay;
  int not_exist_delay;
};

}  // namespace

class WellKnownChangePasswordNavigationThrottleBrowserTest
    : public CertVerifierBrowserTest,
      public testing::WithParamInterface<ResponseDelayParams> {
 public:
  WellKnownChangePasswordNavigationThrottleBrowserTest() {
    feature_list_.InitAndEnableFeature(
        password_manager::features::kWellKnownChangePassword);
    test_server_->RegisterRequestHandler(base::BindRepeating(
        &WellKnownChangePasswordNavigationThrottleBrowserTest::HandleRequest,
        base::Unretained(this)));
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(test_server_->InitializeAndListen());
    test_server_->StartAcceptingConnections();
  }

 protected:
  // Navigates to |kWellKnownChangePasswordPath| from the mock server. It waits
  // until the navigation to |expected_path| happened.
  void TestNavigationThrottle(const std::string& expected_path);

  // Whitelist all https certs for the |test_server_|.
  void AddHttpsCertificate() {
    auto cert = test_server_->GetCertificate();
    net::CertVerifyResult verify_result;
    verify_result.cert_status = 0;
    verify_result.verified_cert = cert;
    mock_cert_verifier()->AddResultForCert(cert.get(), verify_result, net::OK);
  }

  // Maps a path to a ServerResponse config object.
  base::flat_map<std::string, ServerResponse> path_response_map_;
  std::unique_ptr<EmbeddedTestServer> test_server_ =
      std::make_unique<EmbeddedTestServer>(EmbeddedTestServer::TYPE_HTTPS);

 private:
  // Returns a response for the given request. Uses |path_response_map_| to
  // construct the response. Returns nullptr when the path is not defined in
  // |path_response_map_|.
  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request);

  base::test::ScopedFeatureList feature_list_;
};

std::unique_ptr<HttpResponse>
WellKnownChangePasswordNavigationThrottleBrowserTest::HandleRequest(
    const HttpRequest& request) {
  GURL absolute_url = test_server_->GetURL(request.relative_url);
  std::string path = absolute_url.path();
  auto it = path_response_map_.find(absolute_url.path_piece());
  if (it == path_response_map_.end())
    return nullptr;
  const ServerResponse& config = it->second;
  auto http_response = std::make_unique<DelayedHttpResponse>(
      base::TimeDelta::FromMilliseconds(config.resolve_time_in_milliseconds));
  http_response->set_code(config.status_code);
  http_response->set_content_type("text/plain");
  for (auto header_pair : config.headers) {
    http_response->AddCustomHeader(header_pair.first, header_pair.second);
  }
  return http_response;
}

void WellKnownChangePasswordNavigationThrottleBrowserTest::
    TestNavigationThrottle(const std::string& expected_path) {
  AddHttpsCertificate();
  GURL url = test_server_->GetURL(kWellKnownChangePasswordPath);
  GURL expected_url = test_server_->GetURL(expected_path);

  NavigateParams params(browser(), url,
                        ui::PageTransition::PAGE_TRANSITION_LINK);
  TestNavigationObserver observer(expected_url);
  observer.WatchExistingWebContents();
  Navigate(&params);
  observer.Wait();

  EXPECT_EQ(observer.last_navigation_url(), expected_url);
}

IN_PROC_BROWSER_TEST_P(WellKnownChangePasswordNavigationThrottleBrowserTest,
                       SupportForChangePassword) {
  auto response_delays = GetParam();
  path_response_map_[kWellKnownChangePasswordPath] = {
      net::HTTP_OK, {}, response_delays.change_password_delay};
  path_response_map_[kWellKnownNotExistingResourcePath] = {
      net::HTTP_NOT_FOUND, {}, response_delays.not_exist_delay};

  TestNavigationThrottle(kWellKnownChangePasswordPath);
}

IN_PROC_BROWSER_TEST_P(WellKnownChangePasswordNavigationThrottleBrowserTest,
                       SupportForChangePassword_WithRedirect) {
  auto response_delays = GetParam();
  path_response_map_[kWellKnownChangePasswordPath] = {
      net::HTTP_PERMANENT_REDIRECT,
      {std::make_pair("Location", "/change-password")},
      response_delays.change_password_delay};
  path_response_map_[kWellKnownNotExistingResourcePath] = {
      net::HTTP_NOT_FOUND, {}, response_delays.not_exist_delay};
  path_response_map_["/change-password"] = {net::HTTP_OK, {}, 0};

  TestNavigationThrottle(/*expected_path=*/"/change-password");
}

IN_PROC_BROWSER_TEST_P(WellKnownChangePasswordNavigationThrottleBrowserTest,
                       SupportForChangePassword_PartialContent) {
  auto response_delays = GetParam();
  path_response_map_[kWellKnownChangePasswordPath] = {
      net::HTTP_PARTIAL_CONTENT, {}, response_delays.change_password_delay};
  path_response_map_[kWellKnownNotExistingResourcePath] = {
      net::HTTP_NOT_FOUND, {}, response_delays.not_exist_delay};

  TestNavigationThrottle(/*expected_path=*/kWellKnownChangePasswordPath);
}

IN_PROC_BROWSER_TEST_P(WellKnownChangePasswordNavigationThrottleBrowserTest,
                       SupportForChangePassword_WithRedirectToNotFoundPage) {
  auto response_delays = GetParam();
  path_response_map_[kWellKnownChangePasswordPath] = {
      net::HTTP_PERMANENT_REDIRECT,
      {std::make_pair("Location", "/change-password")},
      response_delays.change_password_delay};
  path_response_map_[kWellKnownNotExistingResourcePath] = {
      net::HTTP_PERMANENT_REDIRECT,
      {std::make_pair("Location", "/not-found")},
      response_delays.not_exist_delay};
  path_response_map_["/change-password"] = {net::HTTP_OK, {}, 0};
  path_response_map_["/not-found"] = {net::HTTP_NOT_FOUND, {}, 0};

  TestNavigationThrottle(/*expected_path=*/"/change-password");
}

IN_PROC_BROWSER_TEST_P(WellKnownChangePasswordNavigationThrottleBrowserTest,
                       NoSupportForChangePassword_NotFound) {
  auto response_delays = GetParam();
  path_response_map_[kWellKnownChangePasswordPath] = {
      net::HTTP_NOT_FOUND, {}, response_delays.change_password_delay};
  path_response_map_[kWellKnownNotExistingResourcePath] = {
      net::HTTP_NOT_FOUND, {}, response_delays.not_exist_delay};
  path_response_map_["/"] = {net::HTTP_OK, {}, 0};

  TestNavigationThrottle(/*expected_path=*/"/");
}

// Single page applications often return 200 for all paths
IN_PROC_BROWSER_TEST_P(WellKnownChangePasswordNavigationThrottleBrowserTest,
                       NoSupportForChangePassword_Ok) {
  auto response_delays = GetParam();
  path_response_map_[kWellKnownChangePasswordPath] = {
      net::HTTP_OK, {}, response_delays.change_password_delay};
  path_response_map_[kWellKnownNotExistingResourcePath] = {
      net::HTTP_OK, {}, response_delays.not_exist_delay};
  path_response_map_["/"] = {net::HTTP_OK, {}, 0};

  TestNavigationThrottle(/*expected_path=*/"/");
}

IN_PROC_BROWSER_TEST_P(WellKnownChangePasswordNavigationThrottleBrowserTest,
                       NoSupportForChangePassword_WithRedirectToNotFoundPage) {
  auto response_delays = GetParam();
  path_response_map_[kWellKnownChangePasswordPath] = {
      net::HTTP_PERMANENT_REDIRECT,
      {std::make_pair("Location", "/not-found")},
      response_delays.change_password_delay};
  path_response_map_[kWellKnownNotExistingResourcePath] = {
      net::HTTP_PERMANENT_REDIRECT,
      {std::make_pair("Location", "/not-found")},
      response_delays.not_exist_delay};
  path_response_map_["/"] = {net::HTTP_OK, {}, 0};
  path_response_map_["/not-found"] = {net::HTTP_NOT_FOUND, {}, 0};

  TestNavigationThrottle(/*expected_path=*/"/");
}

IN_PROC_BROWSER_TEST_P(WellKnownChangePasswordNavigationThrottleBrowserTest,
                       NoSupportForChangePassword_WillFailRequest) {
  auto response_delays = GetParam();
  path_response_map_[kWellKnownChangePasswordPath] = {
      net::HTTP_PERMANENT_REDIRECT,
      {std::make_pair("Location", "/change-password")},
      response_delays.change_password_delay};
  path_response_map_[kWellKnownNotExistingResourcePath] = {
      net::HTTP_NOT_FOUND, {}, response_delays.not_exist_delay};

  // Make request fail.
  scoped_refptr<net::X509Certificate> cert = test_server_->GetCertificate();
  net::CertVerifyResult verify_result;
  verify_result.cert_status = 0;
  verify_result.verified_cert = cert;
  mock_cert_verifier()->AddResultForCert(cert.get(), verify_result,
                                         net::ERR_BLOCKED_BY_CLIENT);

  GURL url = test_server_->GetURL(kWellKnownChangePasswordPath);
  NavigateParams params(browser(), url,
                        ui::PageTransition::PAGE_TRANSITION_LINK);
  Navigate(&params);
  TestNavigationObserver observer(params.navigated_or_inserted_contents);
  observer.Wait();

  EXPECT_EQ(observer.last_navigation_url(), url);
}

constexpr ResponseDelayParams kDelayParams[] = {{0, 1}, {1, 0}};

INSTANTIATE_TEST_SUITE_P(All,
                         WellKnownChangePasswordNavigationThrottleBrowserTest,
                         ::testing::ValuesIn(kDelayParams));
