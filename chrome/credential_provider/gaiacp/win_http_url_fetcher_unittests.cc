// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "chrome/credential_provider/test/gls_runner_test_base.h"

namespace credential_provider {

namespace testing {

// Test the WinHttpUrlFetcher::BuildRequestAndFetchResultFromHttpService method
// used to make various HTTP requests.
// Parameters are:
// bool  true:  HTTP call succeeds.
//       false: Fails due to invalid response from server.
class GcpWinHttpUrlFetcherTest : public GlsRunnerTestBase,
                                 public ::testing::WithParamInterface<bool> {};

TEST_P(GcpWinHttpUrlFetcherTest,
       BuildRequestAndFetchResultFromHttpServiceTest) {
  bool invalid_response = GetParam();

  const int timeout_in_millis = 12000;
  const std::string header1 = "test-header-1";
  const std::string header1_value = "test-value-1";
  const GURL test_url =
      GURL("https://test-service.googleapis.com/v1/testEndpoint");
  const std::string access_token = "test-access-token";

  base::Value request(base::Value::Type::DICTIONARY);
  request.SetStringKey("request-str-key", "request-str-value");
  request.SetIntKey("request-int-key", 1234);
  base::TimeDelta request_timeout =
      base::TimeDelta::FromMilliseconds(timeout_in_millis);
  base::Optional<base::Value> request_result;

  base::Value expected_result(base::Value::Type::DICTIONARY);
  expected_result.SetStringKey("response-str-key", "response-str-value");
  expected_result.SetIntKey("response-int-key", 4321);
  std::string expected_response;
  base::JSONWriter::Write(expected_result, &expected_response);

  fake_http_url_fetcher_factory()->SetFakeResponse(
      test_url, FakeWinHttpUrlFetcher::Headers(),
      invalid_response ? "Invalid json response" : expected_response);
  fake_http_url_fetcher_factory()->SetCollectRequestData(true);

  HRESULT hr = WinHttpUrlFetcher::BuildRequestAndFetchResultFromHttpService(
      test_url, access_token, {{header1, header1_value}}, request,
      request_timeout, &request_result);

  if (invalid_response) {
    ASSERT_TRUE(FAILED(hr));
  } else {
    ASSERT_EQ(S_OK, hr);
    ASSERT_EQ(expected_result, request_result.value());
  }

  ASSERT_TRUE(fake_http_url_fetcher_factory()->requests_created() > 0);
  for (size_t idx = 0;
       idx < fake_http_url_fetcher_factory()->requests_created(); ++idx) {
    FakeWinHttpUrlFetcherFactory::RequestData request_data =
        fake_http_url_fetcher_factory()->GetRequestData(idx);

    ASSERT_EQ(timeout_in_millis, request_data.timeout_in_millis);
    ASSERT_EQ(1u, request_data.headers.count("Authorization"));
    ASSERT_NE(std::string::npos,
              request_data.headers.at("Authorization").find(access_token));
    ASSERT_EQ(1u, request_data.headers.count(header1));
    ASSERT_EQ(header1_value, request_data.headers.at(header1));
    base::Optional<base::Value> body_value =
        base::JSONReader::Read(request_data.body);
    ASSERT_EQ(request, body_value.value());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         GcpWinHttpUrlFetcherTest,
                         ::testing::Values(true, false));

}  // namespace testing
}  // namespace credential_provider
