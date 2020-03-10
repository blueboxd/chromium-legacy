// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_time/network_time_test_utils.h"

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "net/http/http_response_headers.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network_time {

// Update as follows:
//
// curl -i http://clients2.google.com/time/1/current?cup2key=4:123123123
//
// where 4 is the key version and 123123123 is the nonce.  Copy the response
// and the x-cup-server-proof header into |kGoodTimeResponseBody| and
// |kGoodTimeResponseServerProofHeader| respectively, and the
// 'current_time_millis' value of the response into
// |kGoodTimeResponseHandlerJsTime|.  Do this twice, so that the two requests
// appear in order below.
const char* kGoodTimeResponseBody[] = {
    ")]}'\n{\"current_time_millis\":1583777597124,"
    "\"server_nonce\":-1.2173462458909911E256}",
    ")]}'\n{\"current_time_millis\":1583777940456,"
    "\"server_nonce\":1.0627453636135456E-51}"};
const char* kGoodTimeResponseServerProofHeader[] = {
    "3045022100f486431d9e9c8d4b7bef1eb9505eefef326bda6e903615543ed934b7741f4609"
    "022019d3edc1b14f7dd404cbe42e70c813d2351468b4fbefdbde101494d4f02d1b9d:"
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "3045022100c98cb7288bcd56d4bcb83aefc3c137dfc77bbbd9b793155b28af02df51fb2074"
    "022022f1436e92febeccefab5dfadbe61ca2dc92447b63426a573fcf6419cfbfdba7:"
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"};
const double kGoodTimeResponseHandlerJsTime[] = {1583777597124, 1583777940456};

std::unique_ptr<net::test_server::HttpResponse> GoodTimeResponseHandler(
    const net::test_server::HttpRequest& request) {
  net::test_server::BasicHttpResponse* response =
      new net::test_server::BasicHttpResponse();
  response->set_code(net::HTTP_OK);
  response->set_content(kGoodTimeResponseBody[0]);
  response->AddCustomHeader("x-cup-server-proof",
                            kGoodTimeResponseServerProofHeader[0]);
  return std::unique_ptr<net::test_server::HttpResponse>(response);
}

FieldTrialTest::FieldTrialTest() {}

FieldTrialTest::~FieldTrialTest() {}

void FieldTrialTest::SetNetworkQueriesWithVariationsService(
    bool enable,
    float query_probability,
    NetworkTimeTracker::FetchBehavior fetch_behavior) {
  scoped_feature_list_.Reset();
  if (!enable) {
    scoped_feature_list_.InitAndDisableFeature(kNetworkTimeServiceQuerying);
    return;
  }

  base::FieldTrialParams params;
  params["RandomQueryProbability"] = base::NumberToString(query_probability);
  params["CheckTimeIntervalSeconds"] = base::NumberToString(360);
  std::string fetch_behavior_param;
  switch (fetch_behavior) {
    case NetworkTimeTracker::FETCH_BEHAVIOR_UNKNOWN:
      NOTREACHED();
      fetch_behavior_param = "unknown";
      break;
    case NetworkTimeTracker::FETCHES_IN_BACKGROUND_ONLY:
      fetch_behavior_param = "background-only";
      break;
    case NetworkTimeTracker::FETCHES_ON_DEMAND_ONLY:
      fetch_behavior_param = "on-demand-only";
      break;
    case NetworkTimeTracker::FETCHES_IN_BACKGROUND_AND_ON_DEMAND:
      fetch_behavior_param = "background-and-on-demand";
      break;
  }
  params["FetchBehavior"] = fetch_behavior_param;
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      kNetworkTimeServiceQuerying, params);
}

}  // namespace network_time
