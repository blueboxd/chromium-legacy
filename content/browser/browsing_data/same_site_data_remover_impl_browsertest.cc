// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_data/same_site_data_remover_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/task/post_task.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/browser/browsing_data/browsing_data_test_utils.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/same_site_data_remover.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/browser/system_connector.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/network_service_util.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::IsEmpty;

namespace content {

namespace {

class ServiceWorkerActivationObserver
    : public ServiceWorkerContextCoreObserver {
 public:
  static void SignalActivation(ServiceWorkerContextWrapper* context,
                               const base::Closure& callback) {
    new ServiceWorkerActivationObserver(context, callback);
  }

 private:
  ServiceWorkerActivationObserver(ServiceWorkerContextWrapper* context,
                                  const base::Closure& callback)
      : context_(context), scoped_observer_(this), callback_(callback) {
    scoped_observer_.Add(context);
  }

  ~ServiceWorkerActivationObserver() override {}

  // ServiceWorkerContextCoreObserver overrides.
  void OnVersionStateChanged(int64_t version_id,
                             const GURL& scope,
                             ServiceWorkerVersion::Status) override {
    if (context_->GetLiveVersion(version_id)->status() ==
        ServiceWorkerVersion::ACTIVATED) {
      callback_.Run();
      delete this;
    }
  }

  ServiceWorkerContextWrapper* context_;
  ScopedObserver<ServiceWorkerContextWrapper, ServiceWorkerContextCoreObserver>
      scoped_observer_;
  base::Closure callback_;
};

}  // namespace

class SameSiteDataRemoverBrowserTest : public ContentBrowserTest {
 public:
  SameSiteDataRemoverBrowserTest() {}

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    // Set up HTTP and HTTPS test servers that handle all hosts.
    host_resolver()->AddRule("*", "127.0.0.1");

    if (IsOutOfProcessNetworkService())
      SetUpMockCertVerifier(net::OK);

    https_server_.reset(new net::EmbeddedTestServer(
        net::test_server::EmbeddedTestServer::TYPE_HTTPS));
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    https_server_->RegisterRequestHandler(
        base::BindRepeating(&SameSiteDataRemoverBrowserTest::HandleRequest,
                            base::Unretained(this)));
    ASSERT_TRUE(https_server_->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);

    if (IsOutOfProcessNetworkService()) {
      // |MockCertVerifier| only seems to work when Network Service was enabled.
      command_line->AppendSwitch(switches::kUseMockCertVerifierForTesting);
    } else {
      // We're redirecting all hosts to localhost even on HTTPS, so we'll get
      // certificate errors.
      command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
    }
  }

  BrowserContext* GetBrowserContext() {
    return shell()->web_contents()->GetBrowserContext();
  }

  net::EmbeddedTestServer* GetHttpsServer() { return https_server_.get(); }

  void SetUpMockCertVerifier(int32_t default_result) {
    network::mojom::NetworkServiceTestPtr network_service_test;
    GetSystemConnector()->BindInterface(mojom::kNetworkServiceName,
                                        &network_service_test);

    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    network_service_test->MockCertVerifierSetDefaultResult(
        default_result, run_loop.QuitClosure());
    run_loop.Run();
  }

  void AddServiceWorker(const std::string& origin) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    StoragePartition* partition =
        BrowserContext::GetDefaultStoragePartition(GetBrowserContext());
    ServiceWorkerContextWrapper* service_worker_context =
        static_cast<ServiceWorkerContextWrapper*>(
            partition->GetServiceWorkerContext());

    GURL scope_url = GetHttpsServer()->GetURL(origin, "/");
    GURL js_url = GetHttpsServer()->GetURL(origin, "/?file=worker.js");

    // Register the worker.
    blink::mojom::ServiceWorkerRegistrationOptions options(
        scope_url, blink::mojom::ScriptType::kClassic,
        blink::mojom::ServiceWorkerUpdateViaCache::kImports);
    RunOrPostTaskOnThread(
        FROM_HERE, ServiceWorkerContextWrapper::GetCoreThreadId(),
        base::BindOnce(
            &ServiceWorkerContextWrapper::RegisterServiceWorker,
            base::Unretained(service_worker_context), js_url, options,
            base::Bind(
                &SameSiteDataRemoverBrowserTest::AddServiceWorkerCallback,
                base::Unretained(this))));

    // Wait for its activation.
    base::RunLoop run_loop;
    RunOrPostTaskOnThread(
        FROM_HERE, ServiceWorkerContextWrapper::GetCoreThreadId(),
        base::BindOnce(&ServiceWorkerActivationObserver::SignalActivation,
                       base::Unretained(service_worker_context),
                       run_loop.QuitClosure()));
    run_loop.Run();
  }

  std::vector<StorageUsageInfo> GetServiceWorkers() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    StoragePartition* partition =
        BrowserContext::GetDefaultStoragePartition(GetBrowserContext());
    ServiceWorkerContextWrapper* service_worker_context =
        static_cast<ServiceWorkerContextWrapper*>(
            partition->GetServiceWorkerContext());

    std::vector<StorageUsageInfo> service_workers;
    base::RunLoop run_loop;
    RunOrPostTaskOnThread(
        FROM_HERE, ServiceWorkerContextWrapper::GetCoreThreadId(),
        base::BindOnce(
            &ServiceWorkerContextWrapper::GetAllOriginsInfo,
            base::Unretained(service_worker_context),
            base::Bind(
                &SameSiteDataRemoverBrowserTest::GetServiceWorkersCallback,
                base::Unretained(this), run_loop.QuitClosure(),
                base::Unretained(&service_workers))));
    run_loop.Run();

    return service_workers;
  }

  void ClearData(bool clear_storage) {
    base::RunLoop run_loop;
    ClearSameSiteNoneData(run_loop.QuitClosure(), GetBrowserContext(),
                          clear_storage);
    run_loop.Run();
  }

 private:
  void AddServiceWorkerCallback(bool success) { ASSERT_TRUE(success); }

  void GetServiceWorkersCallback(
      base::OnceClosure callback,
      std::vector<StorageUsageInfo>* out_service_workers,
      const std::vector<StorageUsageInfo>& service_workers) {
    *out_service_workers = service_workers;
    std::move(callback).Run();
  }

  // Handles all requests.
  //
  // Supports the following <key>=<value> query parameters in the url:
  // <key>="file"         responds with the content of file <value>
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    auto response(std::make_unique<net::test_server::BasicHttpResponse>());

    std::string value;
    if (net::GetValueForKeyInQuery(request.GetURL(), "file", &value)) {
      base::FilePath path(GetTestFilePath("browsing_data", value.c_str()));
      base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
      EXPECT_TRUE(file.IsValid());
      int64_t length = file.GetLength();
      EXPECT_GE(length, 0);
      std::unique_ptr<char[]> buffer(new char[length + 1]);
      file.Read(0, buffer.get(), length);
      buffer[length] = '\0';

      if (path.Extension() == FILE_PATH_LITERAL(".js"))
        response->set_content_type("application/javascript");
      else
        NOTREACHED();

      response->set_content(buffer.get());
    }

    return std::move(response);
  }

  std::unique_ptr<net::EmbeddedTestServer> https_server_;

  DISALLOW_COPY_AND_ASSIGN(SameSiteDataRemoverBrowserTest);
};

IN_PROC_BROWSER_TEST_F(SameSiteDataRemoverBrowserTest,
                       TestClearDataWithStorageRemoval) {
  CreateCookieForTest("TestCookie", "www.google.com",
                      net::CookieSameSite::NO_RESTRICTION,
                      net::CookieOptions::SameSiteCookieContext::CROSS_SITE,
                      GetBrowserContext());
  AddServiceWorker("www.google.com");

  ClearData(/* clear_storage= */ true);

  // Check that cookies were deleted.
  const std::vector<net::CanonicalCookie>& cookies =
      GetAllCookies(GetBrowserContext());
  EXPECT_THAT(cookies, IsEmpty());

  // Check that the service worker for the cookie domain was removed.
  std::vector<StorageUsageInfo> service_workers = GetServiceWorkers();
  EXPECT_THAT(service_workers, IsEmpty());
}

IN_PROC_BROWSER_TEST_F(SameSiteDataRemoverBrowserTest,
                       TestClearDataWithoutStorageRemoval) {
  CreateCookieForTest("TestCookie", "www.google.com",
                      net::CookieSameSite::NO_RESTRICTION,
                      net::CookieOptions::SameSiteCookieContext::CROSS_SITE,
                      GetBrowserContext());
  AddServiceWorker("www.google.com");

  ClearData(/* clear_storage= */ false);

  // Check that cookies were deleted.
  const std::vector<net::CanonicalCookie>& cookies =
      GetAllCookies(GetBrowserContext());
  EXPECT_THAT(cookies, IsEmpty());

  // Storage partition data should NOT have been cleared.
  std::vector<StorageUsageInfo> service_workers = GetServiceWorkers();
  ASSERT_EQ(1u, service_workers.size());
  EXPECT_EQ(service_workers[0].origin.GetURL(),
            GetHttpsServer()->GetURL("www.google.com", "/"));
}

}  // namespace content
