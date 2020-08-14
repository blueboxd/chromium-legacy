// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/image_fetcher.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "components/feed/core/v2/test/callback_receiver.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "url/gurl.h"

using ::testing::HasSubstr;

namespace feed {
namespace {

class ImageFetcherTest : public testing::Test {
 public:
  ImageFetcherTest() = default;
  ImageFetcherTest(ImageFetcherTest&) = delete;
  ImageFetcherTest& operator=(const ImageFetcherTest&) = delete;
  ~ImageFetcherTest() override = default;

  void SetUp() override {
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_factory_);
    image_fetcher_ = std::make_unique<ImageFetcher>(shared_url_loader_factory_);
  }

  ImageFetcher* image_fetcher() { return image_fetcher_.get(); }

  void Respond(const GURL& url,
               const std::string& response_string,
               net::HttpStatusCode code = net::HTTP_OK,
               network::URLLoaderCompletionStatus status =
                   network::URLLoaderCompletionStatus()) {
    auto head = network::mojom::URLResponseHead::New();
    if (code >= 0) {
      head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
          "HTTP/1.1 " + base::NumberToString(code));
      status.decoded_body_length = response_string.length();
    }

    test_factory_.AddResponse(url, std::move(head), response_string, status);
  }

  network::ResourceRequest RespondToRequest(const std::string& response_string,
                                            net::HttpStatusCode code) {
    task_environment_.RunUntilIdle();
    network::TestURLLoaderFactory::PendingRequest* pending_request =
        test_factory_.GetPendingRequest(0);
    CHECK(pending_request);
    network::ResourceRequest resource_request = pending_request->request;
    Respond(pending_request->request.url, response_string, code);
    task_environment_.FastForwardUntilNoTasksRemain();
    return resource_request;
  }

 private:
  std::unique_ptr<ImageFetcher> image_fetcher_;
  network::TestURLLoaderFactory test_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(ImageFetcherTest, SendRequestSendsValidRequest) {
  CallbackReceiver<std::unique_ptr<std::string>> receiver;
  image_fetcher()->Fetch(GURL("https://example.com"), receiver.Bind());
  network::ResourceRequest resource_request =
      RespondToRequest("", net::HTTP_OK);

  EXPECT_EQ(GURL("https://example.com"), resource_request.url);
  EXPECT_EQ("GET", resource_request.method);
}

TEST_F(ImageFetcherTest, SendRequestValidResponse) {
  CallbackReceiver<std::unique_ptr<std::string>> receiver;
  image_fetcher()->Fetch(GURL("https://example.com"), receiver.Bind());
  RespondToRequest("example_response", net::HTTP_OK);

  ASSERT_TRUE(receiver.GetResult());
  EXPECT_THAT(**receiver.GetResult(), HasSubstr("example_response"));
}

TEST_F(ImageFetcherTest, SendSequentialRequestsValidResponses) {
  CallbackReceiver<std::unique_ptr<std::string>> receiver1;
  image_fetcher()->Fetch(GURL("https://example1.com"), receiver1.Bind());
  RespondToRequest("example1_response", net::HTTP_OK);

  CallbackReceiver<std::unique_ptr<std::string>> receiver2;
  image_fetcher()->Fetch(GURL("https://example2.com"), receiver2.Bind());
  RespondToRequest("example2_response", net::HTTP_OK);

  ASSERT_TRUE(receiver1.GetResult());
  EXPECT_THAT(**receiver1.GetResult(), HasSubstr("example1_response"));
  ASSERT_TRUE(receiver2.GetResult());
  EXPECT_THAT(**receiver2.GetResult(), HasSubstr("example2_response"));
}

TEST_F(ImageFetcherTest, SendParallelRequestsValidResponses) {
  CallbackReceiver<std::unique_ptr<std::string>> receiver1;
  image_fetcher()->Fetch(GURL("https://example1.com"), receiver1.Bind());
  CallbackReceiver<std::unique_ptr<std::string>> receiver2;
  image_fetcher()->Fetch(GURL("https://example2.com"), receiver2.Bind());

  RespondToRequest("example1_response", net::HTTP_OK);
  RespondToRequest("example2_response", net::HTTP_OK);

  ASSERT_TRUE(receiver1.GetResult());
  EXPECT_THAT(**receiver1.GetResult(), HasSubstr("example1_response"));
  ASSERT_TRUE(receiver2.GetResult());
  EXPECT_THAT(**receiver2.GetResult(), HasSubstr("example2_response"));
}

}  // namespace
}  // namespace feed
