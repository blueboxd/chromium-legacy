// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/trust_token_attribute_parsing.h"

#include "services/network/public/mojom/trust_tokens.mojom-blink.h"
#include "services/network/trust_tokens/test/trust_token_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/json/json_parser.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace internal {
namespace {

network::mojom::blink::TrustTokenParamsPtr NetworkParamsToBlinkParams(
    network::mojom::TrustTokenParamsPtr params) {
  auto ret = network::mojom::blink::TrustTokenParams::New();
  ret->type = params->type;
  ret->refresh_policy = params->refresh_policy;
  ret->sign_request_data = params->sign_request_data;
  ret->include_timestamp_header = params->include_timestamp_header;
  if (params->issuer)
    ret->issuer = SecurityOrigin::CreateFromUrlOrigin(*params->issuer);
  for (const std::string& header : params->additional_signed_headers) {
    ret->additional_signed_headers.push_back(String::FromUTF8(header));
  }
  return ret;
}

}  // namespace

using TrustTokenAttributeParsingSuccess =
    ::testing::TestWithParam<network::TrustTokenTestParameters>;

INSTANTIATE_TEST_SUITE_P(
    WithIssuanceParams,
    TrustTokenAttributeParsingSuccess,
    ::testing::ValuesIn(network::kIssuanceTrustTokenTestParameters));
INSTANTIATE_TEST_SUITE_P(
    WithRedemptionParams,
    TrustTokenAttributeParsingSuccess,
    ::testing::ValuesIn(network::kRedemptionTrustTokenTestParameters));
INSTANTIATE_TEST_SUITE_P(
    WithSigningParams,
    TrustTokenAttributeParsingSuccess,
    ::testing::ValuesIn(network::kSigningTrustTokenTestParameters));

// Test roundtrip serializations-then-deserializations for a collection of test
// cases covering all possible values of all enum attributes, and all
// possibilities (e.g. optional members present vs. not present) for all other
// attributes.
TEST_P(TrustTokenAttributeParsingSuccess, Roundtrip) {
  network::mojom::TrustTokenParams network_expectation;
  std::string input;

  network::TrustTokenParametersAndSerialization
      expected_params_and_serialization =
          network::SerializeTrustTokenParametersAndConstructExpectation(
              GetParam());

  network::mojom::blink::TrustTokenParamsPtr expectation =
      NetworkParamsToBlinkParams(
          std::move(expected_params_and_serialization.params));

  std::unique_ptr<JSONValue> json_value = ParseJSON(
      String::FromUTF8(expected_params_and_serialization.serialized_params));
  ASSERT_TRUE(json_value);
  auto result = TrustTokenParamsFromJson(std::move(json_value));
  ASSERT_TRUE(result);

  // We can't use mojo's generated Equals method here because it doesn't play
  // well with the "issuer" field's type of
  // scoped_refptr<blink::SecurityOrigin>: in particular, the method does an
  // address-to-address comparison of the pointers.
  EXPECT_EQ(result->type, expectation->type);
  EXPECT_EQ(result->refresh_policy, expectation->refresh_policy);
  EXPECT_EQ(result->sign_request_data, expectation->sign_request_data);
  EXPECT_EQ(result->include_timestamp_header,
            expectation->include_timestamp_header);

  EXPECT_EQ(!!result->issuer, !!expectation->issuer);
  if (result->issuer)
    EXPECT_EQ(result->issuer->ToString(), expectation->issuer->ToString());

  EXPECT_EQ(result->additional_signed_headers,
            expectation->additional_signed_headers);
}

TEST(TrustTokenAttributeParsing, NotADictionary) {
  auto json = ParseJSON(R"(
    3
  )");
  ASSERT_TRUE(json);
  ASSERT_FALSE(TrustTokenParamsFromJson(std::move(json)));
}

TEST(TrustTokenAttributeParsing, MissingType) {
  auto json = ParseJSON(R"(
    { }
  )");
  ASSERT_TRUE(json);
  ASSERT_FALSE(TrustTokenParamsFromJson(std::move(json)));
}

TEST(TrustTokenAttributeParsing, TypeUnsafeType) {
  auto json = ParseJSON(R"(
    { "type": 3 }
  )");
  ASSERT_TRUE(json);
  ASSERT_FALSE(TrustTokenParamsFromJson(std::move(json)));
}

TEST(TrustTokenAttributeParsing, InvalidType) {
  auto json = ParseJSON(R"(
    { "type": "not a valid type" }
  )");
  ASSERT_TRUE(json);
  ASSERT_FALSE(TrustTokenParamsFromJson(std::move(json)));
}

TEST(TrustTokenAttributeParsing, TypeUnsafeRefreshPolicy) {
  auto json = ParseJSON(R"(
    { "type": "token-request",
      "refreshPolicy": 3 }
  )");
  ASSERT_TRUE(json);
  ASSERT_FALSE(TrustTokenParamsFromJson(std::move(json)));
}

TEST(TrustTokenAttributeParsing, InvalidRefreshPolicy) {
  auto json = ParseJSON(R"(
    { "type": "token-request",
      "refreshPolicy": "not a valid refresh policy" }
  )");
  ASSERT_TRUE(json);
  ASSERT_FALSE(TrustTokenParamsFromJson(std::move(json)));
}

TEST(TrustTokenAttributeParsing, TypeUnsafeSignRequestData) {
  auto json = ParseJSON(R"(
    { "type": "token-request",
      "signRequestData": 3 }
  )");
  ASSERT_TRUE(json);
  ASSERT_FALSE(TrustTokenParamsFromJson(std::move(json)));
}

TEST(TrustTokenAttributeParsing, InvalidSignRequestData) {
  auto json = ParseJSON(R"(
    { "type": "token-request",
      "signRequestData": "not a member of the signRequestData enum" }
  )");
  ASSERT_TRUE(json);
  ASSERT_FALSE(TrustTokenParamsFromJson(std::move(json)));
}

TEST(TrustTokenAttributeParsing, TypeUnsafeIncludeTimestampHeader) {
  auto json = ParseJSON(R"(
    { "type": "token-request",
      "includeTimestampHeader": 3 }
  )");
  ASSERT_TRUE(json);
  ASSERT_FALSE(TrustTokenParamsFromJson(std::move(json)));
}

TEST(TrustTokenAttributeParsing, TypeUnsafeIssuer) {
  auto json = ParseJSON(R"(
    { "type": "token-request",
      "issuer": 3 }
  )");
  ASSERT_TRUE(json);
  ASSERT_FALSE(TrustTokenParamsFromJson(std::move(json)));
}

// Test that the parser requires that |issuer| be a valid origin.
TEST(TrustTokenAttributeParsing, NonUrlIssuer) {
  JSONParseError err;
  auto json = ParseJSON(R"(
    { "type": "token-request",
      "issuer": "not a URL" }
  )",
                        &err);
  ASSERT_TRUE(json);
  ASSERT_FALSE(TrustTokenParamsFromJson(std::move(json)));
}

// Test that the parser requires that |issuer| be a potentially trustworthy
// origin.
TEST(TrustTokenAttributeParsing, InsecureIssuer) {
  auto json = ParseJSON(R"(
    { "type": "token-request",
      "issuer": "http://not-potentially-trustworthy.example" }
  )");
  ASSERT_TRUE(json);
  ASSERT_FALSE(TrustTokenParamsFromJson(std::move(json)));
}

// Test that the parser requires that |issuer| be a HTTP or HTTPS origin.
TEST(TrustTokenAttributeParsing, NonHttpNonHttpsIssuer) {
  auto json = ParseJSON(R"(
    { "type": "token-request",
      "issuer": "file:///" }
  )");
  ASSERT_TRUE(json);
  ASSERT_FALSE(TrustTokenParamsFromJson(std::move(json)));
}

TEST(TrustTokenAttributeParsing, TypeUnsafeAdditionalSignedHeaders) {
  auto json = ParseJSON(R"(
    { "type": "token-request",
      "additionalSignedHeaders": 3}
  )");
  ASSERT_TRUE(json);
  ASSERT_FALSE(TrustTokenParamsFromJson(std::move(json)));
}

// Test that the parser requires that all members of the additionalSignedHeaders
// be strings.
TEST(TrustTokenAttributeParsing, TypeUnsafeAdditionalSignedHeader) {
  auto json = ParseJSON(R"(
    { "type": "token-request",
      "additionalSignedHeaders": ["plausible header", 17] }
  )");
  ASSERT_TRUE(json);
  ASSERT_FALSE(TrustTokenParamsFromJson(std::move(json)));
}

}  // namespace internal
}  // namespace blink
