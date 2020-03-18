// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_test_util.h"
#include "base/test/bind_test_util.h"

namespace network {

TrustTokenRequestHelperTest::TrustTokenRequestHelperTest(
    base::test::TaskEnvironment::TimeSource time_source)
    : env_(time_source) {}
TrustTokenRequestHelperTest::~TrustTokenRequestHelperTest() = default;

std::unique_ptr<net::URLRequest> TrustTokenRequestHelperTest::MakeURLRequest(
    std::string spec) {
  return context_.CreateRequest(GURL(spec),
                                net::RequestPriority::DEFAULT_PRIORITY,
                                &delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);
}

mojom::TrustTokenOperationStatus
TrustTokenRequestHelperTest::ExecuteBeginOperationAndWaitForResult(
    TrustTokenRequestHelper* helper,
    net::URLRequest* request) {
  base::RunLoop run_loop;
  mojom::TrustTokenOperationStatus status;
  helper->Begin(request,
                base::BindLambdaForTesting(
                    [&](mojom::TrustTokenOperationStatus returned_status) {
                      status = returned_status;
                      run_loop.Quit();
                    }));
  run_loop.Run();
  return status;
}

}  // namespace network
