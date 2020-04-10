// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/feature_policy/document_policy_parser.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/feature_policy/document_policy.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy_feature.mojom-blink.h"

namespace blink {
namespace {

constexpr const mojom::blink::DocumentPolicyFeature kBoolFeature =
    static_cast<mojom::blink::DocumentPolicyFeature>(1);
constexpr const mojom::blink::DocumentPolicyFeature kDoubleFeature =
    static_cast<mojom::blink::DocumentPolicyFeature>(2);

class DocumentPolicyParserTest : public ::testing::Test {
 protected:
  DocumentPolicyParserTest()
      : name_feature_map(DocumentPolicyNameFeatureMap{
            {"f-bool", kBoolFeature},
            {"f-double", kDoubleFeature},
        }),
        feature_info_map(DocumentPolicyFeatureInfoMap{
            {kBoolFeature, {"f-bool", "", PolicyValue(true)}},
            {kDoubleFeature, {"f-double", "value", PolicyValue(1.0)}},
        }) {
    available_features.insert(kBoolFeature);
    available_features.insert(kDoubleFeature);
  }

  ~DocumentPolicyParserTest() override = default;

  base::Optional<DocumentPolicy::ParsedDocumentPolicy> Parse(
      const String& policy_string) {
    return DocumentPolicyParser::ParseInternal(
        policy_string, name_feature_map, feature_info_map, available_features);
  }

  base::Optional<std::string> Serialize(
      const DocumentPolicy::FeatureState& policy) {
    return DocumentPolicy::SerializeInternal(policy, feature_info_map);
  }

 private:
  const DocumentPolicyNameFeatureMap name_feature_map;
  const DocumentPolicyFeatureInfoMap feature_info_map;
  DocumentPolicyFeatureSet available_features;
};

const char* const kValidPolicies[] = {
    "",   // An empty policy.
    " ",  // An empty policy.
    "f-bool",
    "no-f-bool",
    "f-double;value=1.0",
    "f-double;value=2",
    "f-double;value=2.0,no-f-bool",
    "no-f-bool,f-double;value=2.0",
    "no-f-bool;report-to=default,f-double;value=2.0",
    "no-f-bool;report-to=default,f-double;value=2.0;report-to=default",
    "no-f-bool;report-to=default,f-double;report-to=default;value=2.0",
    "no-f-bool;report-to=default,f-double;report-to=endpoint;value=2.0",
};

const char* const kInvalidPolicies[] = {
    "bad-feature-name", "no-bad-feature-name",
    "f-bool;value=true",             // unnecessary param
    "f-double;value=?0",             // wrong type of param
    "f-double;ppb=2",                // wrong param key
    "\"f-bool\"",                    // policy member should be token instead of
                                     // string
    "();value=2",                    // empty feature token
    "(f-bool f-double);value=2",     // too many
                                     // feature
                                     // tokens
    "f-double;report-to=default",    // missing param
    "f-bool;report-to=\"default\"",  // report-to member should
                                     // be token instead of
                                     // string
};

const std::pair<DocumentPolicy::FeatureState, std::string>
    kPolicySerializationTestCases[] = {
        {{{kBoolFeature, PolicyValue(false)},
          {kDoubleFeature, PolicyValue(1.0)}},
         "no-f-bool, f-double;value=1.0"},
        // Changing ordering of FeatureState element should not affect
        // serialization result.
        {{{kDoubleFeature, PolicyValue(1.0)},
          {kBoolFeature, PolicyValue(false)}},
         "no-f-bool, f-double;value=1.0"},
        // Flipping boolean-valued policy from false to true should not affect
        // result ordering of feature.
        {{{kBoolFeature, PolicyValue(true)},
          {kDoubleFeature, PolicyValue(1.0)}},
         "f-bool, f-double;value=1.0"}};

const std::pair<const char*, DocumentPolicy::ParsedDocumentPolicy>
    kPolicyParseTestCases[] = {
        {"no-f-bool,f-double;value=1",
         {{{kBoolFeature, PolicyValue(false)},
           {kDoubleFeature, PolicyValue(1.0)}},
          {} /* endpoint_map */}},
        // White-space is allowed in some positions in structured-header.
        {"no-f-bool,   f-double;value=1",
         {{{kBoolFeature, PolicyValue(false)},
           {kDoubleFeature, PolicyValue(1.0)}},
          {} /* endpoint_map */}},
        {"no-f-bool,f-double;value=1;report-to=default",
         {{{kBoolFeature, PolicyValue(false)},
           {kDoubleFeature, PolicyValue(1.0)}},
          {{kDoubleFeature, "default"}}}},
        {"no-f-bool;report-to=default,f-double;value=1",
         {{{kBoolFeature, PolicyValue(false)},
           {kDoubleFeature, PolicyValue(1.0)}},
          {{kBoolFeature, "default"}}}}};

const DocumentPolicy::FeatureState kParsedPolicies[] = {
    {},  // An empty policy
    {{kBoolFeature, PolicyValue(false)}},
    {{kBoolFeature, PolicyValue(true)}},
    {{kDoubleFeature, PolicyValue(1.0)}},
    {{kBoolFeature, PolicyValue(true)}, {kDoubleFeature, PolicyValue(1.0)}}};

// Serialize and then Parse the result of serialization should cancel each
// other out, i.e. d == Parse(Serialize(d)).
// The other way s == Serialize(Parse(s)) is not always true because structured
// header allows some optional white spaces in its parsing targets and floating
// point numbers will be rounded, e.g. value=1 will be parsed to
// PolicyValue(1.0) and get serialized to value=1.0.
TEST_F(DocumentPolicyParserTest, SerializeAndParse) {
  for (const auto& policy : kParsedPolicies) {
    const base::Optional<std::string> policy_string = Serialize(policy);
    ASSERT_TRUE(policy_string.has_value());
    const base::Optional<DocumentPolicy::ParsedDocumentPolicy> reparsed_policy =
        Parse(policy_string.value().c_str());

    ASSERT_TRUE(reparsed_policy.has_value());
    EXPECT_EQ(reparsed_policy.value().feature_state, policy);
  }
}

TEST_F(DocumentPolicyParserTest, ParseValidPolicy) {
  for (const char* policy : kValidPolicies) {
    EXPECT_NE(Parse(policy), base::nullopt) << "Should parse " << policy;
  }
}

TEST_F(DocumentPolicyParserTest, ParseInvalidPolicy) {
  for (const char* policy : kInvalidPolicies) {
    EXPECT_EQ(Parse(policy),
              base::make_optional(DocumentPolicy::ParsedDocumentPolicy{}))
        << "Should fail to parse " << policy;
  }
}

TEST_F(DocumentPolicyParserTest, SerializeResultShouldMatch) {
  for (const auto& test_case : kPolicySerializationTestCases) {
    const DocumentPolicy::FeatureState& policy = test_case.first;
    const std::string& expected = test_case.second;
    const auto result = Serialize(policy);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), expected);
  }
}

TEST_F(DocumentPolicyParserTest, ParseResultShouldMatch) {
  for (const auto& test_case : kPolicyParseTestCases) {
    const char* input = test_case.first;
    const DocumentPolicy::ParsedDocumentPolicy& expected = test_case.second;
    const auto result = Parse(input);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), expected);
  }
}

}  // namespace
}  // namespace blink
