// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/multipart_uploader.h"

#include <memory>

#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

using ::testing::Invoke;

class MultipartUploadRequestTest : public testing::Test {
 public:
  MultipartUploadRequestTest()
      : thread_bundle_(
            base::test::ScopedTaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
};

class MockMultipartUploadRequest : public MultipartUploadRequest {
 public:
  MockMultipartUploadRequest(const std::string& metadata,
                             const std::string& data,
                             Callback callback)
      : MultipartUploadRequest(nullptr,
                               GURL(),
                               metadata,
                               data,
                               TRAFFIC_ANNOTATION_FOR_TESTS,
                               std::move(callback)) {}

  MOCK_METHOD(void, SendRequest, (), (override));
};

TEST_F(MultipartUploadRequestTest, GeneratesCorrectBody) {
  MultipartUploadRequest request(nullptr, GURL(), "metadata", "data",
                                 TRAFFIC_ANNOTATION_FOR_TESTS,
                                 base::DoNothing());

  std::string expected_body =
      "--boundary\r\n"
      "Content-Type: application/octet-stream\r\n"
      "\r\n"
      "metadata\r\n"
      "--boundary\r\n"
      "Content-Type: application/octet-stream\r\n"
      "\r\n"
      "file data\r\n"
      "--boundary--\r\n";

  request.set_boundary("boundary");
  EXPECT_EQ(request.GenerateRequestBody("metadata", "file data"),
            expected_body);
}

TEST_F(MultipartUploadRequestTest, RetriesCorrectly) {
  MockMultipartUploadRequest mock_request("metadata", "data",
                                          base::DoNothing());
  EXPECT_CALL(mock_request, SendRequest())
      .Times(3)
      .WillRepeatedly(Invoke([&mock_request]() {
        mock_request.RetryOrFinish(net::OK, net::HTTP_BAD_REQUEST,
                                   std::make_unique<std::string>("response"));
      }));
  mock_request.Start();
  thread_bundle_.FastForwardUntilNoTasksRemain();
}

}  // namespace safe_browsing
