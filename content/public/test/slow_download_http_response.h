// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_SLOW_DOWNLOAD_HTTP_RESPONSE_H_
#define CONTENT_PUBLIC_TEST_SLOW_DOWNLOAD_HTTP_RESPONSE_H_

#include "content/public/test/slow_http_response.h"

namespace content {

// A subclass of SlowHttpResponse that serves a download.
class SlowDownloadHttpResponse : public SlowHttpResponse {
 public:
  // Test URLs.
  static const char kUnknownSizeUrl[];
  static const char kKnownSizeUrl[];

  static std::unique_ptr<net::test_server::HttpResponse>
  HandleSlowDownloadRequest(const net::test_server::HttpRequest& request);

  SlowDownloadHttpResponse(const std::string& url);
  ~SlowDownloadHttpResponse() override;

  SlowDownloadHttpResponse(const SlowDownloadHttpResponse&) = delete;
  SlowDownloadHttpResponse& operator=(const SlowDownloadHttpResponse&) = delete;

  // SlowHttpResponse:
  bool IsHandledUrl() override;
  void AddResponseHeaders(std::string* response) override;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_SLOW_DOWNLOAD_HTTP_RESPONSE_H_
