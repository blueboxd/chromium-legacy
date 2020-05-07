// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/wilco_dtc_supportd/wilco_dtc_supportd_web_request_service.h"

#include <map>
#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/wilco_dtc_supportd/mojo_utils.h"
#include "chrome/browser/chromeos/wilco_dtc_supportd/wilco_dtc_supportd_network_context.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/services/wilco_dtc_supportd/public/mojom/wilco_dtc_supportd.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

// Performs a request once using WilcoDctSupportdWebRequestService, waiting for
// the result to be available.
class ServiceRequestPerformer {
 public:
  explicit ServiceRequestPerformer(
      WilcoDtcSupportdWebRequestService* web_request_service)
      : web_request_service_(web_request_service) {
    DCHECK(web_request_service_);
  }

  ~ServiceRequestPerformer() = default;

  ServiceRequestPerformer(const ServiceRequestPerformer&) = delete;
  ServiceRequestPerformer& operator=(const ServiceRequestPerformer&) = delete;

  void PerformRequest(const GURL& url) {
    ASSERT_FALSE(request_performed_);

    request_performed_ = true;

    base::RunLoop run_loop;
    web_request_service_->PerformRequest(
        wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestHttpMethod::kGet,
        url, {}, "",
        base::BindOnce(
            [](base::OnceClosure callback,
               wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus*
                   out_status,
               int* out_response, mojo::ScopedHandle* out_response_handle,
               wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus
                   status,
               int response, mojo::ScopedHandle response_handle) {
              *out_status = status;
              *out_response = response;
              *out_response_handle = std::move(response_handle);
              std::move(callback).Run();
            },
            run_loop.QuitClosure(), &status_, &response_, &response_handle_));
    run_loop.Run();
  }

  wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus status() const {
    DCHECK(request_performed_);
    return status_;
  }

  int response() const {
    DCHECK(request_performed_);
    return response_;
  }

  std::string response_body_release() {
    DCHECK(request_performed_);
    base::ReadOnlySharedMemoryMapping response_shared_memory;
    return MojoUtils::GetStringPieceFromMojoHandle(std::move(response_handle_),
                                                   &response_shared_memory)
        .as_string();
  }

 private:
  bool request_performed_ = false;

  // Not owned.
  WilcoDtcSupportdWebRequestService* const web_request_service_;

  // Results of the request:
  wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus status_ =
      wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus::kOk;
  int response_ = 0;
  mojo::ScopedHandle response_handle_;
};

// Performs a request once using network::SharedURLLoaderFactory, waiting for
// the result to be available.
class ContextRequestPerformer {
 public:
  explicit ContextRequestPerformer(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : url_loader_factory_(url_loader_factory) {
    DCHECK(url_loader_factory_);
  }

  ~ContextRequestPerformer() = default;

  ContextRequestPerformer(const ContextRequestPerformer&) = delete;
  ContextRequestPerformer& operator=(const ContextRequestPerformer&) = delete;

  void PerformRequest(const GURL& url) {
    ASSERT_FALSE(request_performed_);
    request_performed_ = true;

    auto request = std::make_unique<network::ResourceRequest>();
    request->url = url;
    // TODO(crbug.com/993801): This probably isn't needed here and can be
    // removed if the test sets an appropriate site_for_cookies instead.
    request->force_ignore_site_for_cookies = true;

    auto url_loader = network::SimpleURLLoader::Create(
        std::move(request), TRAFFIC_ANNOTATION_FOR_TESTS);

    url_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory_.get(), url_loader_test_helper_.GetCallback());
    url_loader_test_helper_.WaitForCallback();
  }

  const std::string* response_body() const {
    DCHECK(request_performed_);
    return url_loader_test_helper_.response_body();
  }

 private:
  bool request_performed_ = false;

  scoped_refptr<network::SharedURLLoaderFactory> const url_loader_factory_;

  content::SimpleURLLoaderTestHelper url_loader_test_helper_;
};

}  // namespace

class WilcoDtcSupportdNetworkContextTest : public InProcessBrowserTest {
 public:
  WilcoDtcSupportdNetworkContextTest()
      : embedded_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  ~WilcoDtcSupportdNetworkContextTest() override = default;

  WilcoDtcSupportdNetworkContextTest(
      const WilcoDtcSupportdNetworkContextTest&) = delete;
  WilcoDtcSupportdNetworkContextTest& operator=(
      const WilcoDtcSupportdNetworkContextTest&) = delete;

  void SetUpOnMainThread() override {
    auto network_context_impl =
        std::make_unique<WilcoDtcSupportdNetworkContextImpl>();
    network_context_impl_ptr_ = network_context_impl.get();
    web_request_service_ = std::make_unique<WilcoDtcSupportdWebRequestService>(
        std::move(network_context_impl));
  }

  void TearDownOnMainThread() override {
    network_context_impl_ptr_ = nullptr;
    web_request_service_.reset();
  }

  void StartServer() {
    embedded_test_server()->AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->StartAcceptingConnections();
  }

  net::EmbeddedTestServer* embedded_test_server() {
    return &embedded_test_server_;
  }

  WilcoDtcSupportdNetworkContextImpl* network_context_impl() {
    DCHECK(network_context_impl_ptr_);
    return network_context_impl_ptr_;
  }

  WilcoDtcSupportdWebRequestService* web_request_service() {
    DCHECK(web_request_service_);
    return web_request_service_.get();
  }

 private:
  net::EmbeddedTestServer embedded_test_server_;

  // Owned by |web_request_service_|.
  WilcoDtcSupportdNetworkContextImpl* network_context_impl_ptr_ = nullptr;

  std::unique_ptr<WilcoDtcSupportdWebRequestService> web_request_service_;
};

// Verifies that by default it's not allowed to perform web requests to the
// local host.
IN_PROC_BROWSER_TEST_F(WilcoDtcSupportdNetworkContextTest,
                       LocalHostRequestsAreNotAllowed) {
  ASSERT_NO_FATAL_FAILURE(StartServer());

  {
    ServiceRequestPerformer request_performer(web_request_service());
    ASSERT_NO_FATAL_FAILURE(request_performer.PerformRequest(
        embedded_test_server()->GetURL("/echo")));
    EXPECT_EQ(request_performer.status(),
              wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus::
                  kNetworkError);
    EXPECT_EQ(request_performer.response(), 0);
    EXPECT_EQ(request_performer.response_body_release(), "");
  }
}

// Verifies that client receives non-empty body in case of HTTP error.
IN_PROC_BROWSER_TEST_F(WilcoDtcSupportdNetworkContextTest,
                       NonEmpyResponseBodyOnNetworkError) {
  ASSERT_NO_FATAL_FAILURE(StartServer());

  web_request_service()->set_allow_local_requests_for_testing(true /* allow */);

  {
    ServiceRequestPerformer request_performer(web_request_service());
    ASSERT_NO_FATAL_FAILURE(request_performer.PerformRequest(
        embedded_test_server()->GetURL("/echo?status=500")));
    EXPECT_EQ(request_performer.status(),
              wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus::
                  kHttpError);
    EXPECT_EQ(request_performer.response(), 500);
    EXPECT_EQ(request_performer.response_body_release(), "Echo");
  }
}

// Verifies that client continues request if client SSL certifient was not
// requested by server.
IN_PROC_BROWSER_TEST_F(WilcoDtcSupportdNetworkContextTest,
                       NoClientCertificate) {
  ASSERT_NO_FATAL_FAILURE(StartServer());

  web_request_service()->set_allow_local_requests_for_testing(true /* allow */);

  {
    ServiceRequestPerformer request_performer(web_request_service());
    ASSERT_NO_FATAL_FAILURE(request_performer.PerformRequest(
        embedded_test_server()->GetURL("/echo")));
    EXPECT_EQ(request_performer.status(),
              wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus::kOk);
    EXPECT_EQ(request_performer.response(), 200);
    EXPECT_EQ(request_performer.response_body_release(), "Echo");
  }
}

// Verifies that client continues request if client SSL certifient was
// requested by server as optional.
IN_PROC_BROWSER_TEST_F(WilcoDtcSupportdNetworkContextTest,
                       OptionalClientCertificate) {
  net::SSLServerConfig ssl_server_config;
  ssl_server_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::OPTIONAL_CLIENT_CERT;
  embedded_test_server()->SetSSLConfig(
      net::EmbeddedTestServer::ServerCertificate::CERT_OK, ssl_server_config);

  ASSERT_NO_FATAL_FAILURE(StartServer());

  web_request_service()->set_allow_local_requests_for_testing(true /* allow */);

  {
    ServiceRequestPerformer request_performer(web_request_service());
    ASSERT_NO_FATAL_FAILURE(request_performer.PerformRequest(
        embedded_test_server()->GetURL("/echo")));
    EXPECT_EQ(request_performer.status(),
              wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus::kOk);
    EXPECT_EQ(request_performer.response(), 200);
    EXPECT_EQ(request_performer.response_body_release(), "Echo");
  }
}

// Verifies that client does not continue request if client SSL certifient was
// requested by server as required.
IN_PROC_BROWSER_TEST_F(WilcoDtcSupportdNetworkContextTest,
                       RequireClientCertificate) {
  net::SSLServerConfig ssl_server_config;
  ssl_server_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  embedded_test_server()->SetSSLConfig(
      net::EmbeddedTestServer::ServerCertificate::CERT_OK, ssl_server_config);

  ASSERT_NO_FATAL_FAILURE(StartServer());

  web_request_service()->set_allow_local_requests_for_testing(true /* allow */);

  {
    ServiceRequestPerformer request_performer(web_request_service());
    ASSERT_NO_FATAL_FAILURE(request_performer.PerformRequest(
        embedded_test_server()->GetURL("/echo")));
    EXPECT_EQ(request_performer.status(),
              wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus::
                  kNetworkError);
    EXPECT_EQ(request_performer.response(), 0);
    EXPECT_EQ(request_performer.response_body_release(), "");
  }
}

// Verifies that WilcoDtcSupportdNetworkContext reuses valid connection to
// networking service.
IN_PROC_BROWSER_TEST_F(WilcoDtcSupportdNetworkContextTest,
                       ReuseValidNetworkingServiceConnection) {
  ASSERT_NO_FATAL_FAILURE(StartServer());

  web_request_service()->set_allow_local_requests_for_testing(true /* allow */);

  {
    ServiceRequestPerformer request_performer(web_request_service());
    ASSERT_NO_FATAL_FAILURE(request_performer.PerformRequest(
        embedded_test_server()->GetURL("/echo")));
    EXPECT_EQ(request_performer.status(),
              wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus::kOk);
    EXPECT_EQ(request_performer.response(), 200);
    EXPECT_EQ(request_performer.response_body_release(), "Echo");
  }

  {
    ServiceRequestPerformer request_performer(web_request_service());
    ASSERT_NO_FATAL_FAILURE(request_performer.PerformRequest(
        embedded_test_server()->GetURL("/echo")));
    EXPECT_EQ(request_performer.status(),
              wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus::kOk);
    EXPECT_EQ(request_performer.response(), 200);
    EXPECT_EQ(request_performer.response_body_release(), "Echo");
  }
}

// Verifies that WilcoDtcSupportdNetworkContext reconnects to network service
// on network service crash.
IN_PROC_BROWSER_TEST_F(WilcoDtcSupportdNetworkContextTest,
                       CrashNetworkingService) {
  ASSERT_NO_FATAL_FAILURE(StartServer());

  web_request_service()->set_allow_local_requests_for_testing(true /* allow */);

  {
    ServiceRequestPerformer request_performer(web_request_service());
    ASSERT_NO_FATAL_FAILURE(request_performer.PerformRequest(
        embedded_test_server()->GetURL("/echo")));
    EXPECT_EQ(request_performer.status(),
              wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus::kOk);
    EXPECT_EQ(request_performer.response(), 200);
    EXPECT_EQ(request_performer.response_body_release(), "Echo");
  }

  SimulateNetworkServiceCrash();
  network_context_impl()->FlushForTesting();

  {
    ServiceRequestPerformer request_performer(web_request_service());
    ASSERT_NO_FATAL_FAILURE(request_performer.PerformRequest(
        embedded_test_server()->GetURL("/echo")));
    EXPECT_EQ(request_performer.status(),
              wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus::kOk);
    EXPECT_EQ(request_performer.response(), 200);
    EXPECT_EQ(request_performer.response_body_release(), "Echo");
  }
}

// Verifies that WilcoDtcSupportdWebRequestService
//     1) niether saves nor sends cookies;
//     2) uses neither profile nor system network context cookies.
IN_PROC_BROWSER_TEST_F(WilcoDtcSupportdNetworkContextTest, NoCookies) {
  ASSERT_NO_FATAL_FAILURE(StartServer());

  web_request_service()->set_allow_local_requests_for_testing(true /* allow */);

  const std::map<std::string, scoped_refptr<network::SharedURLLoaderFactory>>
      url_loader_factories{
          {"profile", browser()->profile()->GetURLLoaderFactory()},
          {"system", g_browser_process->system_network_context_manager()
                         ->GetSharedURLLoaderFactory()}};

  for (const auto& iter : url_loader_factories) {
    LOG(INFO)
        << "Verifying that wilco web request service does not use cookies from "
        << iter.first << " network context.";

    const std::string kCookies = "foo=bar";

    // Setup cookies for non wilco network context.
    {
      ContextRequestPerformer context_request_performer(iter.second);
      ASSERT_NO_FATAL_FAILURE(context_request_performer.PerformRequest(
          embedded_test_server()->GetURL("/echoheader?cookie")));
      ASSERT_TRUE(context_request_performer.response_body());
      EXPECT_EQ(*context_request_performer.response_body(), "None");
    }
    {
      ContextRequestPerformer context_request_performer(iter.second);
      ASSERT_NO_FATAL_FAILURE(context_request_performer.PerformRequest(
          embedded_test_server()->GetURL("/set-cookie?" + kCookies)));
      ASSERT_TRUE(context_request_performer.response_body());
      EXPECT_EQ(*context_request_performer.response_body(), kCookies);
    }
    {
      ContextRequestPerformer context_request_performer(iter.second);
      ASSERT_NO_FATAL_FAILURE(context_request_performer.PerformRequest(
          embedded_test_server()->GetURL("/echoheader?cookie")));
      ASSERT_TRUE(context_request_performer.response_body());
      EXPECT_EQ(*context_request_performer.response_body(), kCookies);
    }

    // Verify that wilco network context neither sends nor saves any cookies.
    {
      ServiceRequestPerformer request_performer(web_request_service());
      ASSERT_NO_FATAL_FAILURE(request_performer.PerformRequest(
          embedded_test_server()->GetURL("/echoheader?cookie")));
      EXPECT_EQ(request_performer.response_body_release(), "None");
    }
    {
      ServiceRequestPerformer request_performer(web_request_service());
      ASSERT_NO_FATAL_FAILURE(request_performer.PerformRequest(
          embedded_test_server()->GetURL("/set-cookie?" + kCookies)));
      EXPECT_EQ(request_performer.response_body_release(), kCookies);
    }
    {
      ServiceRequestPerformer request_performer(web_request_service());
      ASSERT_NO_FATAL_FAILURE(request_performer.PerformRequest(
          embedded_test_server()->GetURL("/echoheader?cookie")));
      EXPECT_EQ(request_performer.response_body_release(), "None");
    }

    // Verify that we still have same cookies for non wilco network context.
    {
      ContextRequestPerformer context_request_performer(iter.second);
      ASSERT_NO_FATAL_FAILURE(context_request_performer.PerformRequest(
          embedded_test_server()->GetURL("/echoheader?cookie")));
      ASSERT_TRUE(context_request_performer.response_body());
      EXPECT_EQ(*context_request_performer.response_body(), kCookies);
    }
  }
}

}  // namespace chromeos
