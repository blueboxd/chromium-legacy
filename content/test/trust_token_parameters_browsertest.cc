// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_string_value_serializer.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Field;
using ::testing::Optional;
using ::testing::Property;

// These integration tests verify that calling the Fetch API with Trust Tokens
// parameters results in the parameters' counterparts appearing downstream in
// network::ResourceRequest.
//
// They use URLLoaderInterceptor, as opposed to an embedded test server, in
// order to directly inspect network::ResourceRequest instances.
//
// Separately, Blink layout tests check that the API correctly rejects invalid
// input.

namespace content {

namespace {
const char kTestHeaders[] = "HTTP/1.1 200 OK\nContent-type: text/html\n\n";

std::string TrustTokenEnumToString(
    network::mojom::TrustTokenOperationType type) {
  switch (type) {
    case network::mojom::TrustTokenOperationType::kIssuance:
      return "token-request";
    case network::mojom::TrustTokenOperationType::kRedemption:
      return "srr-token-redemption";
    case network::mojom::TrustTokenOperationType::kSigning:
      return "send-srr";
  }
}

std::string TrustTokenEnumToString(
    network::mojom::TrustTokenRefreshPolicy policy) {
  switch (policy) {
    case network::mojom::TrustTokenRefreshPolicy::kUseCached:
      return "none";
    case network::mojom::TrustTokenRefreshPolicy::kRefresh:
      return "refresh";
  }
}

std::string TrustTokenEnumToString(
    network::mojom::TrustTokenSignRequestData sign_request_data) {
  switch (sign_request_data) {
    case network::mojom::TrustTokenSignRequestData::kOmit:
      return "omit";
    case network::mojom::TrustTokenSignRequestData::kHeadersOnly:
      return "headers-only";
    case network::mojom::TrustTokenSignRequestData::kInclude:
      return "include";
  }
}

// The values instantiations of this struct will be serialized and passed to a
// `fetch` call in executed JS.
struct Input {
  // Input (nullopt in an optional field will be omitted from the parameter's
  // value):
  network::mojom::TrustTokenOperationType type;
  base::Optional<network::mojom::TrustTokenRefreshPolicy> refresh_policy;
  base::Optional<network::mojom::TrustTokenSignRequestData> sign_request_data;
  base::Optional<bool> include_timestamp_header;
  // Because static initialization of GURLs/Origins isn't allowed in tests, use
  // the string representation of the issuer origin and convert it to an Origin
  // in the test.
  base::Optional<std::string> issuer_spec;
  base::Optional<std::vector<std::string>> additional_signed_headers;
};

// For a given test case, creates and returns:
// 1. a serialized JSON dictionary suitable for passing as the value of
//    `fetch`'s `trustToken` parameter.
// 2. a network::mojom::TrustTokenParams object that should equal the value
//    eventually passed to network::ResourceRequest when a fetch is executed
//    with the returned trustToken parameter value.
std::pair<std::string, network::mojom::TrustTokenParams>
SerializeParametersAndConstructExpectation(const Input& input) {
  network::mojom::TrustTokenParams expectation;

  base::Value parameters(base::Value::Type::DICTIONARY);
  parameters.SetStringKey("type", TrustTokenEnumToString(input.type));
  expectation.type = input.type;

  if (input.refresh_policy.has_value()) {
    parameters.SetStringKey("refreshPolicy",
                            TrustTokenEnumToString(*input.refresh_policy));
    expectation.refresh_policy = *input.refresh_policy;
  }

  if (input.sign_request_data.has_value()) {
    parameters.SetStringKey("signRequestData",
                            TrustTokenEnumToString(*input.sign_request_data));
    expectation.sign_request_data = *input.sign_request_data;
  }

  if (input.include_timestamp_header.has_value()) {
    parameters.SetBoolKey("includeTimestampHeader",
                          *input.include_timestamp_header);
    expectation.include_timestamp_header = *input.include_timestamp_header;
  }

  if (input.issuer_spec.has_value()) {
    parameters.SetStringKey("issuer", *input.issuer_spec);
    expectation.issuer = url::Origin::Create(GURL(*input.issuer_spec));
  }

  if (input.additional_signed_headers.has_value()) {
    base::Value headers(base::Value::Type::LIST);
    for (const std::string& header : *input.additional_signed_headers)
      headers.Append(header);
    parameters.SetKey("additionalSignedHeaders", std::move(headers));
    expectation.additional_signed_headers = *input.additional_signed_headers;
  }

  std::string serialized_parameters;
  JSONStringValueSerializer serializer(&serialized_parameters);
  CHECK(serializer.Serialize(parameters));
  return std::make_pair(serialized_parameters, expectation);
}

const Input kIssuanceInputs[]{
    // For issuance, there are no additional parameters to specify.
    {.type = network::mojom::TrustTokenOperationType::kIssuance}};

const Input kRedemptionInputs[]{
    // The only free parameter for redemption is refreshPolicy.
    {.type = network::mojom::TrustTokenOperationType::kRedemption,
     .refresh_policy = network::mojom::TrustTokenRefreshPolicy::kRefresh},
    {.type = network::mojom::TrustTokenOperationType::kRedemption,
     .refresh_policy = network::mojom::TrustTokenRefreshPolicy::kUseCached},
    {.type = network::mojom::TrustTokenOperationType::kRedemption}};

const Input kSigningInputs[]{
    // Signing's inputs are issuer, signRequestData, additionalSignedHeaders,
    // and includeTimestampHeader; "issuer" has no default and must always be
    // a secure origin.
    {.type = network::mojom::TrustTokenOperationType::kSigning,
     .sign_request_data = network::mojom::TrustTokenSignRequestData::kOmit,
     .include_timestamp_header = true,
     .issuer_spec = "https://issuer.example",
     .additional_signed_headers =
         std::vector<std::string>{"one header's name",
                                  "another header's name"}},
    {.type = network::mojom::TrustTokenOperationType::kSigning,
     .sign_request_data =
         network::mojom::TrustTokenSignRequestData::kHeadersOnly,
     .include_timestamp_header = false,
     .issuer_spec = "https://issuer.example"},
    {.type = network::mojom::TrustTokenOperationType::kSigning,
     .sign_request_data = network::mojom::TrustTokenSignRequestData::kInclude,
     .issuer_spec = "https://issuer.example"},
};

}  // namespace

class TrustTokenParametersBrowsertest
    : public ::testing::WithParamInterface<Input>,
      public ContentBrowserTest {
 public:
  TrustTokenParametersBrowsertest() {
    features_.InitAndEnableFeature(network::features::kTrustTokens);
  }

 protected:
  base::test::ScopedFeatureList features_;
};

INSTANTIATE_TEST_SUITE_P(WithIssuanceParameters,
                         TrustTokenParametersBrowsertest,
                         testing::ValuesIn(kIssuanceInputs));
INSTANTIATE_TEST_SUITE_P(WithRedemptionParameters,
                         TrustTokenParametersBrowsertest,
                         testing::ValuesIn(kRedemptionInputs));
INSTANTIATE_TEST_SUITE_P(WithSigningParameters,
                         TrustTokenParametersBrowsertest,
                         testing::ValuesIn(kSigningInputs));

IN_PROC_BROWSER_TEST_P(TrustTokenParametersBrowsertest,
                       PopulatesResourceRequest) {
  bool attempted_to_load_image = false;

  network::mojom::TrustTokenParams expectation;
  std::string fetch_trust_token_parameter;
  std::tie(fetch_trust_token_parameter, expectation) =
      SerializeParametersAndConstructExpectation(GetParam());

  URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&attempted_to_load_image, &fetch_trust_token_parameter,
       &expectation](URLLoaderInterceptor::RequestParams* params) -> bool {
        std::string spec = params->url_request.url.spec();

        // On the first request, to "main.com", load a landing page from which
        // to make the Trust Tokens request.
        if (spec.find("main") != std::string::npos) {
          URLLoaderInterceptor::WriteResponse(
              kTestHeaders,
              "<html><script>fetch('https://www.image.com/image.png', "
              "{trustToken: " +
                  fetch_trust_token_parameter + "});</script></html>",
              params->client.get());
          return true;
        }

        // On the second request, to a path containing "image", verify that the
        // network::ResourceRequest has the correct Trust Tokens parameters.
        if (spec.find("image")) {
          // Can't ASSERT_TRUE in a non-void-returning method, but we don't want
          // to continue to the next line if there's no |trust_token_params|
          // field present, because we'd trigger a check failure anyways, when
          // dereferencing nullopt.
          CHECK(params->url_request.trust_token_params);

          EXPECT_TRUE(
              params->url_request.trust_token_params->Equals(expectation));

          attempted_to_load_image = true;
        }

        return false;
      }));

  EXPECT_TRUE(NavigateToURL(shell(), GURL("https://main.com/")));

  // As a sanity check, make sure the test did actually try to load the
  // subresource.
  ASSERT_TRUE(attempted_to_load_image);
}

}  // namespace content
