// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/feature_policy/document_policy.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy_feature.mojom.h"

namespace blink {
namespace {

class DocumentPolicyTest : public ::testing::Test {
 public:
  DocumentPolicyTest() {}

 protected:
  std::unique_ptr<DocumentPolicy> document_policy_;
};

const std::string kValidPolicies[] = {
    "",   // An empty policy.
    " ",  // An empty policy.
    "font-display-late-swap",
    "no-font-display-late-swap",
    "unoptimized-lossless-images;bpp=1.0",
    "unoptimized-lossless-images;bpp=2",
    "unoptimized-lossless-images;bpp=2.0,no-font-display-late-swap",
    "no-font-display-late-swap,unoptimized-lossless-images;bpp=2.0"};

const std::string kInvalidPolicies[] = {
    "bad-feature-name", "no-bad-feature-name",
    "font-display-late-swap;value=true",   // unnecessary param
    "unoptimized-lossless-images;bpp=?0",  // wrong type of param
    "unoptimized-lossless-images;ppb=2",   // wrong param key
    "\"font-display-late-swap\"",  // policy member should be token instead of
                                   // string
    "();bpp=2",                    // empty feature token
    "(font-display-late-swap, unoptimized-lossless-images);bpp=2"  // too many
                                                                   // feature
                                                                   // tokens
};

// TODO(chenleihu): find a FeaturePolicyFeature name start with 'f' < c < 'n'
// to further strengthen the test on proving "no-" prefix is not counted as part
// of feature name for ordering.
const std::pair<DocumentPolicy::FeatureState, std::string>
    kPolicySerializationTestCases[] = {
        {{{blink::mojom::FeaturePolicyFeature::kFontDisplay,
           PolicyValue(false)},
          {blink::mojom::FeaturePolicyFeature::kUnoptimizedLosslessImages,
           PolicyValue(1.0)}},
         "no-font-display-late-swap, unoptimized-lossless-images;bpp=1.0"},
        // Changing ordering of FeatureState element should not affect
        // serialization result.
        {{{blink::mojom::FeaturePolicyFeature::kUnoptimizedLosslessImages,
           PolicyValue(1.0)},
          {blink::mojom::FeaturePolicyFeature::kFontDisplay,
           PolicyValue(false)}},
         "no-font-display-late-swap, unoptimized-lossless-images;bpp=1.0"},
        // Flipping boolean-valued policy from false to true should not affect
        // result ordering of feature.
        {{{blink::mojom::FeaturePolicyFeature::kFontDisplay, PolicyValue(true)},
          {blink::mojom::FeaturePolicyFeature::kUnoptimizedLosslessImages,
           PolicyValue(1.0)}},
         "font-display-late-swap, unoptimized-lossless-images;bpp=1.0"}};

const std::pair<std::string, DocumentPolicy::FeatureState>
    kPolicyParseTestCases[] = {
        {"no-font-display-late-swap,unoptimized-lossless-images;bpp=1",
         {{blink::mojom::FeaturePolicyFeature::kFontDisplay,
           PolicyValue(false)},
          {blink::mojom::FeaturePolicyFeature::kUnoptimizedLosslessImages,
           PolicyValue(1.0)}}},
        // White-space is allowed in some positions in structured-header.
        {"no-font-display-late-swap,   unoptimized-lossless-images;bpp=1",
         {{blink::mojom::FeaturePolicyFeature::kFontDisplay,
           PolicyValue(false)},
          {blink::mojom::FeaturePolicyFeature::kUnoptimizedLosslessImages,
           PolicyValue(1.0)}}}};

const DocumentPolicy::FeatureState kParsedPolicies[] = {
    {},  // An empty policy
    {{mojom::FeaturePolicyFeature::kFontDisplay, PolicyValue(false)}},
    {{mojom::FeaturePolicyFeature::kFontDisplay, PolicyValue(true)}},
    {{mojom::FeaturePolicyFeature::kUnoptimizedLosslessImages,
      PolicyValue(1.0)}},
    {{mojom::FeaturePolicyFeature::kFontDisplay, PolicyValue(true)},
     {mojom::FeaturePolicyFeature::kUnoptimizedLosslessImages,
      PolicyValue(1.0)}}};

// Serialize and then Parse the result of serialization should cancel each
// other out, i.e. d == Parse(Serialize(d)).
// The other way s == Serialize(Parse(s)) is not always true because structured
// header allows some optional white spaces in its parsing targets and floating
// point numbers will be rounded, e.g. bpp=1 will be parsed to PolicyValue(1.0)
// and get serialized to bpp=1.0.
TEST_F(DocumentPolicyTest, SerializeAndParse) {
  for (const auto& policy : kParsedPolicies) {
    const base::Optional<std::string> policy_string =
        DocumentPolicy::Serialize(policy);
    ASSERT_TRUE(policy_string.has_value());
    const base::Optional<DocumentPolicy::FeatureState> reparsed_policy =
        DocumentPolicy::Parse(policy_string.value());

    ASSERT_TRUE(reparsed_policy.has_value());
    EXPECT_EQ(reparsed_policy.value(), policy);
  }
}

TEST_F(DocumentPolicyTest, ParseValidPolicy) {
  for (const std::string& policy : kValidPolicies) {
    EXPECT_NE(DocumentPolicy::Parse(policy), base::nullopt)
        << "Should parse " << policy;
  }
}

TEST_F(DocumentPolicyTest, ParseInvalidPolicy) {
  for (const std::string& policy : kInvalidPolicies) {
    EXPECT_EQ(DocumentPolicy::Parse(policy), base::nullopt)
        << "Should fail to parse " << policy;
  }
}

TEST_F(DocumentPolicyTest, SerializeResultShouldMatch) {
  for (const auto& test_case : kPolicySerializationTestCases) {
    const DocumentPolicy::FeatureState& policy = test_case.first;
    const std::string& expected = test_case.second;
    const auto result = DocumentPolicy::Serialize(policy);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), expected);
  }
}

TEST_F(DocumentPolicyTest, ParseResultShouldMatch) {
  for (const auto& test_case : kPolicyParseTestCases) {
    const std::string& input = test_case.first;
    const DocumentPolicy::FeatureState& expected = test_case.second;
    const auto result = DocumentPolicy::Parse(input);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), expected);
  }
}

// Helper function to convert literal to FeatureState.
template <class T>
DocumentPolicy::FeatureState FeatureState(
    std::vector<std::pair<int32_t, T>> literal) {
  DocumentPolicy::FeatureState result;
  for (const auto& entry : literal) {
    result.insert({static_cast<mojom::FeaturePolicyFeature>(entry.first),
                   PolicyValue(entry.second)});
  }
  return result;
}

TEST_F(DocumentPolicyTest, MergeFeatureState) {
  EXPECT_EQ(DocumentPolicy::MergeFeatureState(
                FeatureState<bool>(
                    {{1, false}, {2, false}, {3, true}, {4, true}, {5, false}}),
                FeatureState<bool>(
                    {{2, true}, {3, true}, {4, false}, {5, false}, {6, true}})),
            FeatureState<bool>({{1, false},
                                {2, false},
                                {3, true},
                                {4, false},
                                {5, false},
                                {6, true}}));
  EXPECT_EQ(
      DocumentPolicy::MergeFeatureState(
          FeatureState<double>({{1, 1.0}, {2, 1.0}, {3, 1.0}, {4, 0.5}}),
          FeatureState<double>({{2, 0.5}, {3, 1.0}, {4, 1.0}, {5, 1.0}})),
      FeatureState<double>({{1, 1.0}, {2, 0.5}, {3, 1.0}, {4, 0.5}, {5, 1.0}}));
}

}  // namespace
}  // namespace blink
