// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/slow_download_http_response.h"

#include "base/strings/stringprintf.h"

namespace content {

// static
const char SlowDownloadHttpResponse::kUnknownSizeUrl[] =
    "/download-unknown-size";
const char SlowDownloadHttpResponse::kKnownSizeUrl[] = "/download-known-size";

// static
std::unique_ptr<net::test_server::HttpResponse>
SlowDownloadHttpResponse::HandleSlowDownloadRequest(
    const net::test_server::HttpRequest& request) {
  auto response =
      std::make_unique<SlowDownloadHttpResponse>(request.relative_url);
  if (!response->IsHandledUrl()) {
    return nullptr;
  }
  return std::move(response);
}

SlowDownloadHttpResponse::SlowDownloadHttpResponse(const std::string& url)
    : SlowHttpResponse(url) {}

SlowDownloadHttpResponse::~SlowDownloadHttpResponse() = default;

bool SlowDownloadHttpResponse::IsHandledUrl() {
  return (url() == SlowDownloadHttpResponse::kUnknownSizeUrl ||
          url() == SlowDownloadHttpResponse::kKnownSizeUrl ||
          SlowHttpResponse::IsHandledUrl());
}

void SlowDownloadHttpResponse::AddResponseHeaders(std::string* response) {
  response->append("Content-type: application/octet-stream\r\n");
  response->append("Cache-Control: max-age=0\r\n");

  if (base::LowerCaseEqualsASCII(kKnownSizeUrl, url())) {
    response->append(
        base::StringPrintf("Content-Length: %d\r\n",
                           kFirstResponsePartSize + kSecondResponsePartSize));
  }
}

}  // namespace content
