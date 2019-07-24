// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/journey/journey_info_fetcher.h"

#include <utility>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/http/http_util.h"
#include "services/data_decoder/public/cpp/testing_json_parser.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace journey {
const char kEmail[] = "foo_email";
const char kJourneyServer[] =
    "https://chrome-memex-dev.appspot.com/api/journey_from_pageload";
const char kEmptyErrorString[] = "";
const std::vector<int64_t> kTimestamps = {1532563271195406};

using MockFetchResponseAvailableCallback =
    base::MockCallback<FetchResponseAvailableCallback>;

class JourneyInfoFetcherTest : public testing::Test {
 protected:
  JourneyInfoFetcherTest() {}

  ~JourneyInfoFetcherTest() override {}

  void SetUp() override {
    scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    journey_info_fetcher_ = std::make_unique<JourneyInfoFetcher>(
        identity_test_env_.identity_manager(), test_shared_loader_factory);

    SignIn();
  }

  void TearDown() override {}

  void SignIn() {
    identity_test_env_.MakePrimaryAccountAvailable(kEmail);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
  }

  void SignOut() { identity_test_env_.ClearPrimaryAccount(); }

  std::string GenerateJsonResponse(const std::vector<int64_t>& timestamps) {
    int timestamps_size = timestamps.size();
    std::string response_string = R"(
      [
        {
          "status": "STATUS_OK",
          "default_autotabs": {
            "pageloads": [
            {
              "title": {
                "weight": 1.0,
                "title": "foo"
              },
              "url": "https://foo.com/",
              "image": {
                "snippet": "PRS_REPRESENTATIVE_IMAGE",
                "confidence": 1.0,
                "thumbnail_url": "https://foo-png"
              },
              "timestamp_us": "1532563271195406",
              "is_pruned": false
            }
            ],
            "selection_type": "SELECTION_TYPE_LEAVES_AND_TAB_AND_TASK"
          },
          "journey_id": "3021296114337328326",
          "source_page_timestamp_usec": [
    )";

    for (int i = 0; i < timestamps_size; i++) {
      response_string += "\"" + base::NumberToString(timestamps[i]) + "\"";
      if (i < timestamps_size - 1)
        response_string += ", ";
    }

    response_string += R"(
          ]
        }
      ]
    )";

    return response_string;
  }

  void SetFakeResponse(const GURL& request_url,
                       const std::string& response_data,
                       net::HttpStatusCode response_code,
                       net::Error error) {
    network::ResourceResponseHead head;
    std::string headers(base::StringPrintf(
        "HTTP/1.1 %d %s\nContent-type: application/json\n\n",
        static_cast<int>(response_code), GetHttpReasonPhrase(response_code)));
    head.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(headers));
    head.mime_type = "application/json";
    network::URLLoaderCompletionStatus status(error);
    status.decoded_body_length = response_data.size();
    test_url_loader_factory_.AddResponse(request_url, head, response_data,
                                         status);
  }

  void SendAndAwaitResponse(std::vector<int64_t> timestamps) {
    journey_info_fetcher()->FetchJourneyInfo(
        timestamps, journey_info_available_callback().Get());
    base::RunLoop().RunUntilIdle();
  }

  MockFetchResponseAvailableCallback& journey_info_available_callback() {
    return mock_callback_;
  }

  JourneyInfoFetcher* journey_info_fetcher() {
    return journey_info_fetcher_.get();
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

  signin::IdentityTestEnvironment& identity_test_env() {
    return identity_test_env_;
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  data_decoder::TestingJsonParser::ScopedFactoryOverride factory_override_;
  signin::IdentityTestEnvironment identity_test_env_;
  MockFetchResponseAvailableCallback mock_callback_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<JourneyInfoFetcher> journey_info_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(JourneyInfoFetcherTest);
};

TEST_F(JourneyInfoFetcherTest, FetchJourneyInfo) {
  std::string json_response_string = GenerateJsonResponse(kTimestamps);
  SetFakeResponse(GURL(kJourneyServer), json_response_string, net::HTTP_OK,
                  net::OK);
  EXPECT_CALL(journey_info_available_callback(),
              Run(testing::Ne(base::nullopt), kEmptyErrorString));

  SendAndAwaitResponse(kTimestamps);

  EXPECT_THAT(journey_info_fetcher()->GetLastJsonForDebugging(),
              testing::Eq(json_response_string));
}

TEST_F(JourneyInfoFetcherTest, FetchJourneyInfoOAuthError) {
  identity_test_env().SetAutomaticIssueOfAccessTokens(false);

  EXPECT_CALL(journey_info_available_callback(),
              Run(testing::Eq(base::nullopt), testing::Ne(kEmptyErrorString)));

  journey_info_fetcher()->FetchJourneyInfo(
      kTimestamps, journey_info_available_callback().Get());

  identity_test_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));

  base::RunLoop().RunUntilIdle();
}

#if !defined(OS_CHROMEOS)
TEST_F(JourneyInfoFetcherTest, FetchJourneyInfoUserNotSignedIn) {
  SignOut();

  EXPECT_CALL(journey_info_available_callback(),
              Run(testing::Eq(base::nullopt), testing::Ne(kEmptyErrorString)));

  SendAndAwaitResponse(kTimestamps);

  EXPECT_EQ(journey_info_fetcher()->GetLastJsonForDebugging(), "");
}
#endif

TEST_F(JourneyInfoFetcherTest, FetchJourneyInfoWithNonParsableResponse) {
  std::string json_response_string = "[";
  SetFakeResponse(GURL(kJourneyServer), json_response_string, net::HTTP_OK,
                  net::OK);
  EXPECT_CALL(journey_info_available_callback(),
              Run(testing::Eq(base::nullopt), testing::Ne(kEmptyErrorString)));

  SendAndAwaitResponse(kTimestamps);

  EXPECT_THAT(journey_info_fetcher()->GetLastJsonForDebugging(),
              testing::Eq(json_response_string));
}

TEST_F(JourneyInfoFetcherTest, FetchJourneyInfoWithBadJSONResponse) {
  std::string json_response_string = "[]";
  SetFakeResponse(GURL(kJourneyServer), json_response_string, net::HTTP_OK,
                  net::OK);
  EXPECT_CALL(journey_info_available_callback(),
              Run(testing::Ne(base::nullopt), kEmptyErrorString));

  SendAndAwaitResponse(kTimestamps);

  EXPECT_THAT(journey_info_fetcher()->GetLastJsonForDebugging(),
              testing::Eq(json_response_string));
}

TEST_F(JourneyInfoFetcherTest, FetchJourneyInfoNetworkError) {
  std::string json_response_string = "[]";
  SetFakeResponse(GURL(kJourneyServer), json_response_string, net::HTTP_OK,
                  net::ERR_FAILED);
  EXPECT_CALL(journey_info_available_callback(),
              Run(testing::Eq(base::nullopt), testing::Ne(kEmptyErrorString)));

  SendAndAwaitResponse(kTimestamps);

  EXPECT_EQ(journey_info_fetcher()->GetLastJsonForDebugging(), "");
}

TEST_F(JourneyInfoFetcherTest, FetchJourneyInfoHttpError) {
  std::string json_response_string = "[]";
  SetFakeResponse(GURL(kJourneyServer), json_response_string,
                  net::HTTP_BAD_REQUEST, net::OK);
  EXPECT_CALL(journey_info_available_callback(),
              Run(testing::Eq(base::nullopt), testing::Ne(kEmptyErrorString)));

  SendAndAwaitResponse(kTimestamps);

  EXPECT_EQ(journey_info_fetcher()->GetLastJsonForDebugging(), "");
}

}  // namespace journey
