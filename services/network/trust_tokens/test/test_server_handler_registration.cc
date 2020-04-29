// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/test/test_server_handler_registration.h"

#include <memory>

#include "base/base64.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/test/bind_test_util.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/trust_tokens/test/trust_token_request_handler.h"

namespace network {

namespace test {

namespace {

const char kIssuanceRelativePath[] = "/issue";
const char kRedemptionRelativePath[] = "/redeem";
const char kSignedRequestVerificationRelativePath[] = "/sign";

std::unique_ptr<net::test_server::HttpResponse>
MakeTrustTokenFailureResponse() {
  // No need to report a failure HTTP code here: returning a vanilla OK should
  // fail the Trust Tokens operation client-side.
  return std::make_unique<net::test_server::BasicHttpResponse>();
}

// Constructs and returns an HTTP response bearing the given base64-encoded
// Trust Tokens issuance or redemption protocol response message.
std::unique_ptr<net::test_server::HttpResponse> MakeTrustTokenResponse(
    base::StringPiece contents) {
  CHECK([&]() {
    std::string temp;
    return base::Base64Decode(contents, &temp);
  }());

  auto ret = std::make_unique<net::test_server::BasicHttpResponse>();
  ret->AddCustomHeader("Sec-Trust-Token", std::string(contents));
  return ret;
}

}  // namespace

void RegisterTrustTokenTestHandlers(net::EmbeddedTestServer* test_server,
                                    TrustTokenRequestHandler* handler) {
  test_server->RegisterRequestHandler(base::BindLambdaForTesting(
      [handler](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        // Decline to handle the request if it isn't destined for this
        // endpoint.
        if (request.relative_url != kIssuanceRelativePath)
          return nullptr;

        if (!base::Contains(request.headers, "Sec-Trust-Token"))
          return MakeTrustTokenFailureResponse();

        base::Optional<std::string> operation_result =
            handler->Issue(request.headers.at("Sec-Trust-Token"));

        if (!operation_result)
          return MakeTrustTokenFailureResponse();

        return MakeTrustTokenResponse(*operation_result);
      }));

  test_server->RegisterRequestHandler(base::BindLambdaForTesting(
      [handler](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url != kRedemptionRelativePath)
          return nullptr;

        if (!base::Contains(request.headers, "Sec-Trust-Token"))
          return MakeTrustTokenFailureResponse();

        base::Optional<std::string> operation_result =
            handler->Redeem(request.headers.at("Sec-Trust-Token"));

        if (!operation_result)
          return MakeTrustTokenFailureResponse();

        return MakeTrustTokenResponse(*operation_result);
      }));

  test_server->RegisterRequestHandler(base::BindLambdaForTesting(
      [handler](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url != kSignedRequestVerificationRelativePath)
          return nullptr;

        std::string error;
        net::HttpRequestHeaders headers;
        for (const auto& name_and_value : request.headers)
          headers.SetHeader(name_and_value.first, name_and_value.second);

        bool success =
            handler->VerifySignedRequest(request.GetURL(), headers, &error);
        LOG_IF(ERROR, !success) << error;

        // Unlike issuance and redemption, there's no special state to return
        // on success for signing.
        return std::make_unique<net::test_server::BasicHttpResponse>();
      }));
}

}  // namespace test

}  // namespace network
