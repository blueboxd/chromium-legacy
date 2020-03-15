// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/feature_policy/document_policy_parser.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/feature_policy/document_policy.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy_feature.mojom-blink.h"

namespace blink {
namespace {

using DocumentPolicyParserTest = ::testing::Test;

const char* const kValidPolicies[] = {
    "",   // An empty policy.
    " ",  // An empty policy.
    "font-display-late-swap",
    "no-font-display-late-swap",
    "unoptimized-lossless-images;bpp=1.0",
    "unoptimized-lossless-images;bpp=2",
    "unoptimized-lossless-images;bpp=2.0,no-font-display-late-swap",
    "no-font-display-late-swap,unoptimized-lossless-images;bpp=2.0",
    "no-font-display-late-swap;report-to=default,unoptimized-lossless-images;"
    "bpp=2.0",
    "no-font-display-late-swap;report-to=default,unoptimized-lossless-images;"
    "bpp=2.0;report-to=default",
    "no-font-display-late-swap;report-to=default,unoptimized-lossless-images;"
    "report-to=default;bpp=2.0",
    "no-font-display-late-swap;report-to=default,unoptimized-lossless-images;"
    "report-to=endpoint;bpp=2.0",
};

const char* const kInvalidPolicies[] = {
    "bad-feature-name", "no-bad-feature-name",
    "font-display-late-swap;value=true",   // unnecessary param
    "unoptimized-lossless-images;bpp=?0",  // wrong type of param
    "unoptimized-lossless-images;ppb=2",   // wrong param key
    "\"font-display-late-swap\"",  // policy member should be token instead of
                                   // string
    "();bpp=2",                    // empty feature token
    "(font-display-late-swap unoptimized-lossless-images);bpp=2",  // too many
                                                                   // feature
                                                                   // tokens
    "unoptimized-lossless-images;report-to=default",  // missing param
    "font-display-late-swap;report-to=\"default\"",   // report-to member should
                                                      // be token instead of
                                                      // string
};

// TODO(chenleihu): find a DocumentPolicyFeature name start with 'f' < c < 'n'
// to further strengthen the test on proving "no-" prefix is not counted as part
// of feature name for ordering.
const std::pair<DocumentPolicy::FeatureState, std::string>
    kPolicySerializationTestCases[] = {
        {{{blink::mojom::DocumentPolicyFeature::kFontDisplay,
           PolicyValue(false)},
          {blink::mojom::DocumentPolicyFeature::kUnoptimizedLosslessImages,
           PolicyValue(1.0)}},
         "no-font-display-late-swap, unoptimized-lossless-images;bpp=1.0"},
        // Changing ordering of FeatureState element should not affect
        // serialization result.
        {{{blink::mojom::DocumentPolicyFeature::kUnoptimizedLosslessImages,
           PolicyValue(1.0)},
          {blink::mojom::DocumentPolicyFeature::kFontDisplay,
           PolicyValue(false)}},
         "no-font-display-late-swap, unoptimized-lossless-images;bpp=1.0"},
        // Flipping boolean-valued policy from false to true should not affect
        // result ordering of feature.
        {{{blink::mojom::DocumentPolicyFeature::kFontDisplay,
           PolicyValue(true)},
          {blink::mojom::DocumentPolicyFeature::kUnoptimizedLosslessImages,
           PolicyValue(1.0)}},
         "font-display-late-swap, unoptimized-lossless-images;bpp=1.0"}};

const std::pair<const char*, DocumentPolicy::ParsedDocumentPolicy>
    kPolicyParseTestCases[] = {
        {"no-font-display-late-swap,unoptimized-lossless-images;bpp=1",
         {{{blink::mojom::DocumentPolicyFeature::kFontDisplay,
            PolicyValue(false)},
           {blink::mojom::DocumentPolicyFeature::kUnoptimizedLosslessImages,
            PolicyValue(1.0)}},
          {} /* endpoint_map */}},
        // White-space is allowed in some positions in structured-header.
        {"no-font-display-late-swap,   unoptimized-lossless-images;bpp=1",
         {{{blink::mojom::DocumentPolicyFeature::kFontDisplay,
            PolicyValue(false)},
           {blink::mojom::DocumentPolicyFeature::kUnoptimizedLosslessImages,
            PolicyValue(1.0)}},
          {} /* endpoint_map */}},
        {"no-font-display-late-swap,unoptimized-lossless-images;bpp=1;report-"
         "to=default",
         {{{blink::mojom::DocumentPolicyFeature::kFontDisplay,
            PolicyValue(false)},
           {blink::mojom::DocumentPolicyFeature::kUnoptimizedLosslessImages,
            PolicyValue(1.0)}},
          {{blink::mojom::DocumentPolicyFeature::kUnoptimizedLosslessImages,
            "default"}}}},
        {"no-font-display-late-swap;report-to=default,unoptimized-lossless-"
         "images;bpp=1",
         {{{blink::mojom::DocumentPolicyFeature::kFontDisplay,
            PolicyValue(false)},
           {blink::mojom::DocumentPolicyFeature::kUnoptimizedLosslessImages,
            PolicyValue(1.0)}},
          {{blink::mojom::DocumentPolicyFeature::kFontDisplay, "default"}}}}};

const DocumentPolicy::FeatureState kParsedPolicies[] = {
    {},  // An empty policy
    {{mojom::DocumentPolicyFeature::kFontDisplay, PolicyValue(false)}},
    {{mojom::DocumentPolicyFeature::kFontDisplay, PolicyValue(true)}},
    {{mojom::DocumentPolicyFeature::kUnoptimizedLosslessImages,
      PolicyValue(1.0)}},
    {{mojom::DocumentPolicyFeature::kFontDisplay, PolicyValue(true)},
     {mojom::DocumentPolicyFeature::kUnoptimizedLosslessImages,
      PolicyValue(1.0)}}};

// Serialize and then Parse the result of serialization should cancel each
// other out, i.e. d == Parse(Serialize(d)).
// The other way s == Serialize(Parse(s)) is not always true because structured
// header allows some optional white spaces in its parsing targets and floating
// point numbers will be rounded, e.g. bpp=1 will be parsed to PolicyValue(1.0)
// and get serialized to bpp=1.0.
TEST_F(DocumentPolicyParserTest, SerializeAndParse) {
  for (const auto& policy : kParsedPolicies) {
    const base::Optional<std::string> policy_string =
        DocumentPolicy::Serialize(policy);
    ASSERT_TRUE(policy_string.has_value());
    const base::Optional<DocumentPolicy::ParsedDocumentPolicy> reparsed_policy =
        DocumentPolicyParser::Parse(policy_string.value().c_str());

    ASSERT_TRUE(reparsed_policy.has_value());
    EXPECT_EQ(reparsed_policy.value().feature_state, policy);
  }
}

TEST_F(DocumentPolicyParserTest, ParseValidPolicy) {
  for (const char* policy : kValidPolicies) {
    EXPECT_NE(DocumentPolicyParser::Parse(policy), base::nullopt)
        << "Should parse " << policy;
  }
}

TEST_F(DocumentPolicyParserTest, ParseInvalidPolicy) {
  for (const char* policy : kInvalidPolicies) {
    EXPECT_EQ(DocumentPolicyParser::Parse(policy),
              base::make_optional(DocumentPolicy::ParsedDocumentPolicy{}))
        << "Should fail to parse " << policy;
  }
}

TEST_F(DocumentPolicyParserTest, SerializeResultShouldMatch) {
  for (const auto& test_case : kPolicySerializationTestCases) {
    const DocumentPolicy::FeatureState& policy = test_case.first;
    const std::string& expected = test_case.second;
    const auto result = DocumentPolicy::Serialize(policy);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), expected);
  }
}

TEST_F(DocumentPolicyParserTest, ParseResultShouldMatch) {
  for (const auto& test_case : kPolicyParseTestCases) {
    const char* input = test_case.first;
    const DocumentPolicy::ParsedDocumentPolicy& expected = test_case.second;
    const auto result = DocumentPolicyParser::Parse(input);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), expected);
  }
}

}  // namespace
}  // namespace blink
