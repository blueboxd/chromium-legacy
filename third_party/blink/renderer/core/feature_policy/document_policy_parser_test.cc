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
      const String& policy_string,
      PolicyParserMessageBuffer& logger) {
    return DocumentPolicyParser::ParseInternal(policy_string, name_feature_map,
                                               feature_info_map,
                                               available_features, logger);
  }

  base::Optional<std::string> Serialize(
      const DocumentPolicy::FeatureState& policy) {
    return DocumentPolicy::SerializeInternal(policy, feature_info_map);
  }

 private:
  const DocumentPolicyNameFeatureMap name_feature_map;
  const DocumentPolicyFeatureInfoMap feature_info_map;
  DocumentPolicyFeatureSet available_features;

 protected:
  struct ParseTestCase {
    const char* test_name;
    const char* input_string;
    DocumentPolicy::ParsedDocumentPolicy parsed_policy;
    Vector<PolicyParserMessageBuffer::Message> messages;
  };

  // |kPolicyParseTestCases| is made as a member of
  // |DocumentPolicyParserTest| because |PolicyParserMessageBuffer::Message| has
  // a member of type WTF::String which cannot be statically allocated.
  const std::vector<ParseTestCase> kPolicyParseTestCases = {
      //
      // Parse valid policy strings.
      //
      {
          "Parse an empty policy string.",
          "",
          /* parsed_policy */
          {
              /* feature_state */ {},
              /* endpoint_map */ {},
          },
          /* messages */ {},
      },
      {
          "Parse an empty policy string.",
          " ",
          /* parsed_policy */
          {
              /* feature_state */ {},
              /* endpoint_map */ {},
          },
          /* messages */ {},
      },
      {
          "Parse bool feature with value true.",
          "f-bool",
          /* parsed_policy */
          {
              /* feature_state */ {{kBoolFeature, PolicyValue(true)}},
              /* endpoint_map */ {},
          },
          /* messages */ {},
      },
      {
          "Parse bool feature with value false.",
          "no-f-bool",
          /* parsed_policy */
          {
              /* feature_state */ {{kBoolFeature, PolicyValue(false)}},
              /* endpoint_map */ {},
          },
          /* messages */ {},
      },
      {
          "Parse double feature with value 1.0.",
          "f-double;value=1.0",
          /* parsed_policy */
          {
              /* feature_state */ {{kDoubleFeature, PolicyValue(1.0)}},
              /* endpoint_map */ {},
          },
          /* messages */ {},
      },
      {
          "Parse double feature with value literal 2.",
          "f-double;value=2",
          /* parsed_policy */
          {
              /* feature_state */ {{kDoubleFeature, PolicyValue(2.0)}},
              /* endpoint_map */ {},
          },
          /* messages */ {},
      },
      {
          "Parse double feature and bool feature.",
          "f-double;value=1,no-f-bool",
          /* parsed_policy */
          {
              /* feature_state */ {{kBoolFeature, PolicyValue(false)},
                                   {kDoubleFeature, PolicyValue(1.0)}},
              /* endpoint_map */ {},
          },
          /* messages */ {},
      },
      {
          "Parse bool feature and double feature.",
          "no-f-bool,f-double;value=1",
          /* parsed_policy */
          {
              /* feature_state */ {{kBoolFeature, PolicyValue(false)},
                                   {kDoubleFeature, PolicyValue(1.0)}},
              /* endpoint_map */ {},
          },
          /* messages */ {},
      },
      {
          "White-space is allowed in some positions in structured-header.",
          "no-f-bool,   f-double;value=1",
          /* parsed_policy */
          {/* feature_state */ {{kBoolFeature, PolicyValue(false)},
                                {kDoubleFeature, PolicyValue(1.0)}},
           /* endpoint_map */ {}},
          /* messages */ {},
      },
      {
          "Unrecognized parameters are ignored, but the feature entry should "
          "remain valid.",
          "no-f-bool,f-double;value=1;unknown_param=xxx",
          /* parsed_policy */
          {/* feature_state */ {{kBoolFeature, PolicyValue(false)},
                                {kDoubleFeature, PolicyValue(1.0)}},
           /* endpoint_map */ {}},
          /* messages */
          {{mojom::blink::ConsoleMessageLevel::kWarning,
            "Unrecognized parameter name unknown_param for feature f-double."}},
      },
      {
          "Parse policy with report endpoint specified.",
          "no-f-bool,f-double;value=1;report-to=default",
          /* parsed_policy */
          {/* feature_state */ {{kBoolFeature, PolicyValue(false)},
                                {kDoubleFeature, PolicyValue(1.0)}},
           /* endpoint_map */ {{kDoubleFeature, "default"}}},
          /* messages */ {},
      },
      {
          "Parse policy with report endpoint specified.",
          "no-f-bool;report-to=default,f-double;value=1",
          /* parsed_policy */
          {/* feature_state */ {{kBoolFeature, PolicyValue(false)},
                                {kDoubleFeature, PolicyValue(1.0)}},
           /* endpoint_map */ {{kBoolFeature, "default"}}},
          /* messages */ {},
      },
      {
          "Parse policy with default report endpoint specified. 'none' "
          "keyword should overwrite default value assignment.",
          "no-f-bool;report-to=none, f-double;value=2.0, *;report-to=default",
          /* parsed_policy */
          {/* feature_state */ {{kBoolFeature, PolicyValue(false)},
                                {kDoubleFeature, PolicyValue(2.0)}},
           /* endpoint_map */ {{kDoubleFeature, "default"}}},
          /* messages */ {},
      },
      {
          "Parse policy with default report endpoint specified.",
          "no-f-bool;report-to=not_none, f-double;value=2.0, "
          "*;report-to=default",
          /* parsed_policy */
          {/* feature_state */ {{kBoolFeature, PolicyValue(false)},
                                {kDoubleFeature, PolicyValue(2.0)}},
           /* endpoint_map */ {{kBoolFeature, "not_none"},
                               {kDoubleFeature, "default"}}},
          /* messages */ {},
      },
      {
          "Parse policy with default report endpoint 'none'.",
          "no-f-bool;report-to=not_none, f-double;value=2.0, *;report-to=none",
          /* parsed_policy */
          {/* feature_state */ {{kBoolFeature, PolicyValue(false)},
                                {kDoubleFeature, PolicyValue(2.0)}},
           /* endpoint_map */ {{kBoolFeature, "not_none"}}},
          /* messages */ {},
      },
      {
          "Default endpoint can be specified anywhere in the header, not "
          "necessary at the end.",
          "no-f-bool;report-to=not_none, *;report-to=default, "
          "f-double;value=2.0",
          /* parsed_policy */
          {/* feature_state */ {{kBoolFeature, PolicyValue(false)},
                                {kDoubleFeature, PolicyValue(2.0)}},
           /* endpoint_map */ {{kBoolFeature, "not_none"},
                               {kDoubleFeature, "default"}}},
          /* messages */ {},
      },
      {
          "Default endpoint can be specified multiple times in the header. "
          "According to SH rules, last value wins.",
          "no-f-bool;report-to=not_none, f-double;value=2.0, "
          "*;report-to=default, *;report-to=none",
          /* parsed_policy */
          {/* feature_state */ {{kBoolFeature, PolicyValue(false)},
                                {kDoubleFeature, PolicyValue(2.0)}},
           /* endpoint_map */ {{kBoolFeature, "not_none"}}},
          /* messages */ {},
      },
      {
          "Even if default endpoint is not specified, none still should be "
          "treated as a reserved keyword for endpoint names.",
          "no-f-bool;report-to=none",
          /* parsed_policy */
          {/* feature_state */ {{kBoolFeature, PolicyValue(false)}},
           /* endpoint_map */ {}},
          /* messages */ {},
      },

      //
      // Parse invalid policies.
      //
      {
          "Parse policy with unrecognized feature name.",
          "bad-feature-name",
          /* parsed_policy */
          {
              /* feature_state */ {},
              /* endpoint_map */ {},
          },
          /* messages */
          {{mojom::blink::ConsoleMessageLevel::kWarning,
            "Unrecognized document policy feature name "
            "bad-feature-name."}},
      },
      {
          "Parse policy with unrecognized feature name.",
          "no-bad-feature-name",
          /* parsed_policy */
          {
              /* feature_state */ {},
              /* endpoint_map */ {},
          },
          /* messages */
          {{mojom::blink::ConsoleMessageLevel::kWarning,
            "Unrecognized document policy feature name "
            "no-bad-feature-name."}},
      },
      {
          "Parse policy with wrong type of param. Expected double type but get "
          "boolean type.",
          "f-double;value=?0",
          /* parsed_policy */
          {
              /* feature_state */ {},
              /* endpoint_map */ {},
          },
          /* messages */
          {{mojom::blink::ConsoleMessageLevel::kWarning,
            "Parameter value in feature f-double should be Double, but get "
            "Boolean."}},
      },
      {
          "Policy member should be token instead of string.",
          "\"f-bool\"",
          /* parsed_policy */
          {
              /* feature_state */ {},
              /* endpoint_map */ {},
          },
          /* messages */
          {{mojom::blink::ConsoleMessageLevel::kWarning,
            "The item in directive should be token type."}},
      },
      {
          "Feature token should not be empty.",
          "();value=2",
          /* parsed_policy */
          {
              /* feature_state */ {},
              /* endpoint_map */ {},
          },
          /* messages */
          {{mojom::blink::ConsoleMessageLevel::kWarning,
            "Directives must not be inner lists."}},
      },
      {
          "Too many feature tokens.",
          "(f-bool f-double);value=2",
          /* parsed_policy */
          {
              /* feature_state */ {},
              /* endpoint_map */ {},
          },
          /* messages */
          {{mojom::blink::ConsoleMessageLevel::kWarning,
            "Directives must not be inner lists."}},
      },
      {
          "Missing mandatory parameter.",
          "f-double;report-to=default",
          /* parsed_policy */
          {
              /* feature_state */ {},
              /* endpoint_map */ {},
          },
          /* messages */
          {{mojom::blink::ConsoleMessageLevel::kWarning,
            "Policy value parameter missing for feature f-double. Expected "
            "something like \"f-double;value=...\"."}},
      },
      {
          "\"report-to\" parameter value type should be token instead of "
          "string.",
          "f-bool;report-to=\"default\"",
          /* parsed_policy */
          {
              /* feature_state */ {},
              /* endpoint_map */ {},
          },
          /* messages */
          {{mojom::blink::ConsoleMessageLevel::kWarning,
            "\"report-to\" parameter should be a token in feature f-bool."}},
      },
  };
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
    PolicyParserMessageBuffer logger;
    const base::Optional<DocumentPolicy::ParsedDocumentPolicy> reparsed_policy =
        Parse(policy_string.value().c_str(), logger);

    ASSERT_TRUE(reparsed_policy.has_value());
    EXPECT_EQ(reparsed_policy.value().feature_state, policy);
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
    const String& test_name = test_case.test_name;

    PolicyParserMessageBuffer logger;
    const auto result = Parse(test_case.input_string, logger);

    // All tesecases should not return base::nullopt because they all comply to
    // structured header syntax.
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->endpoint_map, test_case.parsed_policy.endpoint_map)
        << test_name << "\n endpoint map should match";
    EXPECT_EQ(result->feature_state, test_case.parsed_policy.feature_state)
        << test_name << "\n feature state should match";
    EXPECT_EQ(logger.GetMessages().size(), test_case.messages.size())
        << test_name << "\n messages length should match";
    for (auto *it_actual = logger.GetMessages().begin(),
              *it_expected = test_case.messages.begin();
         it_actual != logger.GetMessages().end() &&
         it_expected != test_case.messages.end();
         it_actual++, it_expected++) {
      EXPECT_EQ(it_actual->level, it_expected->level)
          << test_name << "\n message level should match";
      EXPECT_EQ(it_actual->content, it_expected->content)
          << test_name << "\n message content should match";
    }
  }
}

}  // namespace
}  // namespace blink
