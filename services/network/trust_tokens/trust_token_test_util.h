// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_TEST_UTIL_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_TEST_UTIL_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/test/task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "services/network/trust_tokens/trust_token_request_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
class URLRequest;
}  // namespace net

namespace network {

// TrustTokenRequestHelperTest is a fixture common to tests for Trust Tokens
// issuance, redemption, and signing. It factors out the boilerplate of
// constructing net::URLRequests.
class TrustTokenRequestHelperTest : public ::testing::Test {
 public:
  explicit TrustTokenRequestHelperTest(
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::DEFAULT);
  ~TrustTokenRequestHelperTest() override;

  TrustTokenRequestHelperTest(const TrustTokenRequestHelperTest&) = delete;
  TrustTokenRequestHelperTest& operator=(const TrustTokenRequestHelperTest&) =
      delete;

 protected:
  // Constructs and returns a URLRequest with destination |spec|.
  std::unique_ptr<net::URLRequest> MakeURLRequest(std::string spec);

  // Executes a request helper's Begin operation synchronously, removing some
  // boilerplate from waiting for the results of the (actually asynchronous)
  // operation's result.
  mojom::TrustTokenOperationStatus ExecuteBeginOperationAndWaitForResult(
      TrustTokenRequestHelper* helper,
      net::URLRequest* request);

  base::test::TaskEnvironment env_;
  net::TestDelegate delegate_;
  net::TestURLRequestContext context_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_TEST_UTIL_H_
