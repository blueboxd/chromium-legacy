// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/multipart_uploader.h"

#include <memory>

#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "content/public/test/browser_task_environment.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

using ::testing::Invoke;

class MultipartUploadRequestTest : public testing::Test {
 public:
  MultipartUploadRequestTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  content::BrowserTaskEnvironment task_environment_;
};

class MockMultipartUploadRequest : public MultipartUploadRequest {
 public:
  MockMultipartUploadRequest()
      : MultipartUploadRequest(nullptr,
                               GURL(),
                               "",
                               "",
                               TRAFFIC_ANNOTATION_FOR_TESTS,
                               base::DoNothing()) {}

  MOCK_METHOD0(SendRequest, void());
};

TEST_F(MultipartUploadRequestTest, GeneratesCorrectBody) {
  std::unique_ptr<MultipartUploadRequest> request =
      MultipartUploadRequest::Create(nullptr, GURL(), "metadata", "data",
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

  request->set_boundary("boundary");
  EXPECT_EQ(request->GenerateRequestBody("metadata", "file data"),
            expected_body);
}

// TODO(crbug.com/1015974): Re-enable this test.
TEST_F(MultipartUploadRequestTest, DISABLED_RetriesCorrectly) {
  MockMultipartUploadRequest mock_request;

  EXPECT_CALL(mock_request, SendRequest())
      .Times(3)
      .WillRepeatedly(Invoke([&mock_request]() {
        mock_request.RetryOrFinish(net::OK, net::HTTP_BAD_REQUEST,
                                   std::make_unique<std::string>("response"));
      }));
  mock_request.Start();
  task_environment_.FastForwardUntilNoTasksRemain();
}

}  // namespace safe_browsing
