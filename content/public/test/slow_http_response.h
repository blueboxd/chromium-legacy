// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_SLOW_HTTP_RESPONSE_H_
#define CONTENT_PUBLIC_TEST_SLOW_HTTP_RESPONSE_H_

#include <string>

#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace content {

// An HTTP response that won't complete until a |kFinishSlowResponseUrl| request
// is received, used to simulate a slow response.
class SlowHttpResponse : public net::test_server::HttpResponse {
 public:
  // Test URLs.
  static const char kSlowResponseHostName[];
  static const char kSlowResponseUrl[];
  static const char kFinishSlowResponseUrl[];
  static const int kFirstResponsePartSize;
  static const int kSecondResponsePartSize;

  explicit SlowHttpResponse(const std::string& url);
  ~SlowHttpResponse() override;

  SlowHttpResponse(const SlowHttpResponse&) = delete;
  SlowHttpResponse& operator=(const SlowHttpResponse&) = delete;

  virtual bool IsHandledUrl();

  // Subclasses can override this method to add custom HTTP response headers.
  // These headers are only applied to the slow response itself, not the
  // response to |kFinishSlowResponseUrl|.
  virtual void AddResponseHeaders(std::string* response);

  // Subclasses can override this method to write a custom status line; the
  // default implementation sets a 200 OK response. This status code is applied
  // only to the slow response itself, not the response to
  // |kFinishSlowResponseUrl|.
  virtual void SetStatusLine(std::string* response);

  // net::test_server::HttpResponse implementations.
  void SendResponse(const net::test_server::SendBytesCallback& send,
                    net::test_server::SendCompleteCallback done) override;

 protected:
  const std::string& url() { return url_; }

 private:
  std::string url_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_SLOW_HTTP_RESPONSE_H_
