// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/crash_report_private/mock_crash_endpoint.h"

#include "base/run_loop.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/crash/core/app/crash_reporter_client.h"
#include "extensions/browser/api/crash_report_private/crash_report_private_api.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {
constexpr const char* kTestCrashEndpoint = "/crash";
}

class MockCrashEndpoint::Client : public crash_reporter::CrashReporterClient {
 public:
  explicit Client(MockCrashEndpoint* owner) : owner_(owner) {}

  bool GetCollectStatsConsent() override {
    // In production, GetCollectStatsConsent may be blocking due to file reads.
    // Simulate this in our tests as well.
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    return owner_->consented_;
  }
  void GetProductNameAndVersion(std::string* product_name,
                                std::string* version,
                                std::string* channel) override {
    *product_name = "Chrome (Chrome OS)";
    *version = "1.2.3.4";
    *channel = "Stable";
  }

 private:
  MockCrashEndpoint* owner_;
};

MockCrashEndpoint::MockCrashEndpoint(
    net::test_server::EmbeddedTestServer* test_server)
    : test_server_(test_server) {
  test_server->RegisterRequestHandler(
      base::Bind(&MockCrashEndpoint::HandleRequest, base::Unretained(this)));
  EXPECT_TRUE(test_server->Start());

  extensions::api::SetCrashEndpointForTesting(
      test_server->GetURL(kTestCrashEndpoint).spec());
  client_ = std::make_unique<Client>(this);
  crash_reporter::SetCrashReporterClient(client_.get());
}

MockCrashEndpoint::~MockCrashEndpoint() {
  crash_reporter::SetCrashReporterClient(nullptr);
}

const MockCrashEndpoint::Report MockCrashEndpoint::WaitForReport() {
  if (!last_report_.query.empty()) {
    return std::move(last_report_);
  }
  base::RunLoop run_loop;
  on_report_ = run_loop.QuitClosure();
  run_loop.Run();
  on_report_.Reset();
  return std::move(last_report_);
}

std::unique_ptr<net::test_server::HttpResponse>
MockCrashEndpoint::HandleRequest(const net::test_server::HttpRequest& request) {
  GURL absolute_url = test_server_->GetURL(request.relative_url);
  if (absolute_url.path() != kTestCrashEndpoint) {
    return nullptr;
  }

  last_report_ = {absolute_url.query(), request.content};
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content("123");
  http_response->set_content_type("text/plain");
  if (on_report_) {
    on_report_.Run();
  }
  return http_response;
}
