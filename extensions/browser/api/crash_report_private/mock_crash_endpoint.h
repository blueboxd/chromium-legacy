// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_CRASH_REPORT_PRIVATE_MOCK_CRASH_ENDPOINT_H_
#define EXTENSIONS_BROWSER_API_CRASH_REPORT_PRIVATE_MOCK_CRASH_ENDPOINT_H_

#include <memory>
#include <string>

#include "base/callback.h"

namespace net {
namespace test_server {
class EmbeddedTestServer;
class HttpResponse;
struct HttpRequest;
}  // namespace test_server
}  // namespace net

// Installs a mock crash server endpoint into chrome.crashReportPrivate.
class MockCrashEndpoint {
 public:
  struct Report {
    std::string query;
    std::string content;
  };

  explicit MockCrashEndpoint(net::test_server::EmbeddedTestServer* test_server);
  MockCrashEndpoint(const MockCrashEndpoint&) = delete;
  MockCrashEndpoint& operator=(const MockCrashEndpoint&) = delete;
  ~MockCrashEndpoint();

  // Returns the last received report, or waits for a query and returns it.
  const Report WaitForReport();

  const Report& last_report() const { return last_report_; }

  // Configures whether the mock crash reporter client has user-consent for
  // submitting crash reports.
  void set_consented(bool consented) { consented_ = consented; }

 private:
  class Client;

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request);

  net::test_server::EmbeddedTestServer* test_server_;
  std::unique_ptr<Client> client_;
  Report last_report_;
  bool consented_ = true;
  base::RepeatingClosure on_report_;
};

#endif  // EXTENSIONS_BROWSER_API_CRASH_REPORT_PRIVATE_MOCK_CRASH_ENDPOINT_H_
