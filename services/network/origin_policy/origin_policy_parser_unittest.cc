// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/origin_policy/origin_policy_parser.h"
#include "base/strings/stringprintf.h"
#include "services/network/public/cpp/isolation_opt_in_hints.h"
#include "services/network/public/mojom/origin_policy_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

// Unit tests for OriginPolicyParser.
//
// These are fairly simple "smoke tests". The majority of test coverage is
// expected from wpt/origin-policy/ end-to-end tests.

namespace {

void AssertEmptyPolicy(
    const network::OriginPolicyContentsPtr& policy_contents) {
  ASSERT_FALSE(policy_contents->feature_policy.has_value());
  ASSERT_FALSE(policy_contents->isolation_optin_hints.has_value());
  ASSERT_EQ(0u, policy_contents->ids.size());
  ASSERT_EQ(0u, policy_contents->content_security_policies.size());
  ASSERT_EQ(0u, policy_contents->content_security_policies_report_only.size());
}

}  // namespace

namespace network {

TEST(OriginPolicyParser, Empty) {
  auto policy_contents = OriginPolicyParser::Parse("");
  AssertEmptyPolicy(policy_contents);
}

TEST(OriginPolicyParser, Invalid) {
  auto policy_contents = OriginPolicyParser::Parse("potato potato potato");
  AssertEmptyPolicy(policy_contents);
}

TEST(OriginPolicyParser, InvalidString) {
  auto policy_contents = OriginPolicyParser::Parse("\"potato potato potato\"");
  AssertEmptyPolicy(policy_contents);
}

TEST(OriginPolicyParser, InvalidArray) {
  auto policy_contents =
      OriginPolicyParser::Parse("[\"potato potato potato\"]");
  AssertEmptyPolicy(policy_contents);
}

TEST(OriginPolicyParser, EmptyObject) {
  auto policy_contents = OriginPolicyParser::Parse("{}");
  AssertEmptyPolicy(policy_contents);
}

TEST(OriginPolicyParser, ValidOtherFieldsButNoIDs) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      {
        "content_security": {
          "policies": ["script-src 'self' 'unsafe-inline'"],
          "policies_report_only": ["script-src 'self' 'https://example.com/'"]
        },
        "features": {
          "policy": "geolocation 'self' http://maps.google.com"
        },
        "isolation": true
      }
  )");

  AssertEmptyPolicy(policy_contents);
}

TEST(OriginPolicyParser, ValidOtherFieldsButEmptyIDs) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      {
        "ids": [],
        "content_security": {
          "policies": ["script-src 'self' 'unsafe-inline'"],
          "policies_report_only": ["script-src 'self' 'https://example.com/'"]
        },
        "features": {
          "policy": "geolocation 'self' http://maps.google.com"
        },
        "isolation": true
      }
  )");

  AssertEmptyPolicy(policy_contents);
}

TEST(OriginPolicyParser, ValidOtherFieldsButEmptyIDsAfterNonemptyIDs) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      {
        "ids": ["my-policy"],
        "ids": [],
        "content_security": {
          "policies": ["script-src 'self' 'unsafe-inline'"],
          "policies_report_only": ["script-src 'self' 'https://example.com/'"]
        },
        "features": {
          "policy": "geolocation 'self' http://maps.google.com"
        },
        "isolation": true
      }
  )");

  AssertEmptyPolicy(policy_contents);
}

TEST(OriginPolicyParser, ValidOtherFieldsButNonArrayID) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      {
        "ids": "banana",
        "content_security": {
          "policies": ["script-src 'self' 'unsafe-inline'"],
          "policies_report_only": ["script-src 'self' 'https://example.com/'"]
        },
        "features": {
          "policy": "geolocation 'self' http://maps.google.com"
        },
        "isolation": true
      }
  )");

  AssertEmptyPolicy(policy_contents);
}

TEST(OriginPolicyParser, MultipleIDs) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
    {
      "ids": ["my-policy-1", "my policy 2"]
    }
  )");

  ASSERT_EQ(policy_contents->ids.size(), 2U);
  ASSERT_EQ(policy_contents->ids[0], "my-policy-1");
  ASSERT_EQ(policy_contents->ids[1], "my policy 2");
}

TEST(OriginPolicyParser, SkipNonStringIDs) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
    {
      "ids": [
        "my-policy-1",
        ["my-policy-array"],
        5,
        null,
        { "id": "my-policy-object" },
        "my-policy-2",
        true
      ]
    }
  )");

  ASSERT_EQ(policy_contents->ids.size(), 2U);
  ASSERT_EQ(policy_contents->ids[0], "my-policy-1");
  ASSERT_EQ(policy_contents->ids[1], "my-policy-2");
}

TEST(OriginPolicyParser, IDsWithVariousCharacters) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
    {
      "ids": [
        "~",
        " ",
        "\u0000",
        "\t",
        "my\tpolicy",
        "!\"#$%&'()*+,-./:;<=>?@{|}~",
        "my\u007Fpolicy",
        "azAZ",
        "my\u0080policy",
        "my~policy",
        "my\u1234policy"
      ]
    }
  )");

  ASSERT_EQ(policy_contents->ids.size(), 5U);
  ASSERT_EQ(policy_contents->ids[0], "~");
  ASSERT_EQ(policy_contents->ids[1], " ");
  ASSERT_EQ(policy_contents->ids[2], "!\"#$%&'()*+,-./:;<=>?@{|}~");
  ASSERT_EQ(policy_contents->ids[3], "azAZ");
  ASSERT_EQ(policy_contents->ids[4], "my~policy");
}

TEST(OriginPolicyParser, SecondIDsWins) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
    {
      "ids": ["1", "2"],
      "ids": ["3", "4", "5"]
    }
  )");

  ASSERT_EQ(policy_contents->ids.size(), 3U);
  ASSERT_EQ(policy_contents->ids[0], "3");
  ASSERT_EQ(policy_contents->ids[1], "4");
  ASSERT_EQ(policy_contents->ids[2], "5");
}

TEST(OriginPolicyParser, SimpleCSP) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      {
        "ids": ["my-policy"],
        "content_security": {
          "policies": ["script-src 'self' 'unsafe-inline'"]
        }
      }
  )");

  ASSERT_EQ(policy_contents->content_security_policies.size(), 1U);
  ASSERT_EQ(policy_contents->content_security_policies[0],
            "script-src 'self' 'unsafe-inline'");
}

TEST(OriginPolicyParser, CSPIncludingReportOnly) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      {
        "ids": ["my-policy"],
        "content_security": {
          "policies": ["script-src 'self' 'unsafe-inline'"],
          "policies_report_only": ["script-src 'self' 'https://example.com/'"]
        }
      }
  )");

  ASSERT_EQ(policy_contents->content_security_policies.size(), 1U);
  ASSERT_EQ(policy_contents->content_security_policies_report_only.size(), 1U);

  ASSERT_EQ(policy_contents->content_security_policies[0],
            "script-src 'self' 'unsafe-inline'");
  ASSERT_EQ(policy_contents->content_security_policies_report_only[0],
            "script-src 'self' 'https://example.com/'");
}

TEST(OriginPolicyParser, CSPMultiItemArrays) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      {
        "ids": ["my-policy"],
        "content_security": {
          "policies": [
            "script-src 'self' 'unsafe-inline'",
            "frame-ancestors 'none'",
            "object-src 'none'"
          ],
          "policies_report_only": [
            "script-src 'self' 'https://example.com/'",
            "object-src 'none'"
          ]
        }
      }
  )");

  ASSERT_EQ(policy_contents->content_security_policies.size(), 3U);
  ASSERT_EQ(policy_contents->content_security_policies_report_only.size(), 2U);

  ASSERT_EQ(policy_contents->content_security_policies[0],
            "script-src 'self' 'unsafe-inline'");
  ASSERT_EQ(policy_contents->content_security_policies[1],
            "frame-ancestors 'none'");
  ASSERT_EQ(policy_contents->content_security_policies[2], "object-src 'none'");

  ASSERT_EQ(policy_contents->content_security_policies_report_only[0],
            "script-src 'self' 'https://example.com/'");
  ASSERT_EQ(policy_contents->content_security_policies_report_only[1],
            "object-src 'none'");
}

TEST(OriginPolicyParser, CSPTwoContentSecurity) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      {
        "ids": ["my-policy"],
        "content_security": {
          "policies": ["frame-ancestors 'none'", "object-src 'none'"],
          "policies_report_only": ["script-src 'self' https://cdn.example.com/js/"]
        }, "content_security": {
          "policies": ["script-src 'self' 'unsafe-inline'"],
          "policies_report_only": ["script-src 'self' 'https://example.com/'"]
        }
      }
  )");

  ASSERT_EQ(policy_contents->content_security_policies.size(), 1U);
  ASSERT_EQ(policy_contents->content_security_policies_report_only.size(), 1U);

  ASSERT_EQ(policy_contents->content_security_policies[0],
            "script-src 'self' 'unsafe-inline'");
  ASSERT_EQ(policy_contents->content_security_policies_report_only[0],
            "script-src 'self' 'https://example.com/'");
}

TEST(OriginPolicyParser, CSPTwoContentSecurityNoReportOnly) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      {
        "ids": ["my-policy"],
        "content_security": {
          "policies": ["script-src 'self' 'unsafe-inline'"]
        },
        "content_security": {
          "policies": ["img-src 'none'"]
        }
      }
  )");

  ASSERT_EQ(policy_contents->content_security_policies.size(), 1U);
  ASSERT_EQ(policy_contents->content_security_policies_report_only.size(), 0U);

  ASSERT_EQ(policy_contents->content_security_policies[0], "img-src 'none'");
}

TEST(OriginPolicyParser, CSPTwoPolicies) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      {
        "ids": ["my-policy"],
        "content_security": {
          "policies": ["frame-ancestors 'none'", "object-src 'none'"],
          "policies": ["script-src 'self' 'unsafe-inline'"],
          "policies_report_only": ["script-src 'self' https://cdn.example.com/js/"],
          "policies_report_only": ["script-src 'self' 'https://example.com/'"]
        }
      }
  )");

  ASSERT_EQ(policy_contents->content_security_policies.size(), 1U);
  ASSERT_EQ(policy_contents->content_security_policies_report_only.size(), 1U);

  ASSERT_EQ(policy_contents->content_security_policies[0],
            "script-src 'self' 'unsafe-inline'");
  ASSERT_EQ(policy_contents->content_security_policies_report_only[0],
            "script-src 'self' 'https://example.com/'");
}

TEST(OriginPolicyParser, CSPWithoutCSP) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      {
        "ids": ["my-policy"],
        "content_security": {
          "police": ["frame-ancestors 'none'", "object-src 'none'"]
        }
      }
  )");

  ASSERT_EQ(policy_contents->content_security_policies.size(), 0U);
}

TEST(OriginPolicyParser, ExtraFieldsDontBreakParsing) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      {
        "ids": ["my-policy"],
        "content_security": {
          "policies": ["script-src 'self' 'unsafe-inline'"],
          "policies_report_only": ["script-src 'self' 'https://example.com/'"],
          "potatoes": "are best"
        }
      }
  )");

  ASSERT_EQ(policy_contents->content_security_policies.size(), 1U);
  ASSERT_EQ(policy_contents->content_security_policies_report_only.size(), 1U);

  ASSERT_EQ(policy_contents->content_security_policies[0],
            "script-src 'self' 'unsafe-inline'");
  ASSERT_EQ(policy_contents->content_security_policies_report_only[0],
            "script-src 'self' 'https://example.com/'");
}

// At this level we don't validate the syntax, so commas get passed through.
// Integration tests will show that comma-containing policies get discarded,
// though.
TEST(OriginPolicyParser, CSPComma) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      {
        "ids": ["my-policy"],
        "content_security": {
          "policies": ["script-src 'self' 'unsafe-inline', script-src 'self' 'https://example.com/'"],
          "policies_report_only": ["script-src 'self' 'https://example.com/', frame-ancestors 'none', object-src 'none'"]
        }
      }
  )");

  ASSERT_EQ(policy_contents->content_security_policies.size(), 1U);
  ASSERT_EQ(policy_contents->content_security_policies_report_only.size(), 1U);

  ASSERT_EQ(policy_contents->content_security_policies[0],
            "script-src 'self' 'unsafe-inline', script-src 'self' "
            "'https://example.com/'");
  ASSERT_EQ(policy_contents->content_security_policies_report_only[0],
            "script-src 'self' 'https://example.com/', frame-ancestors 'none', "
            "object-src 'none'");
}

// Similarly, complete garbage will be passed through; this is expected.
TEST(OriginPolicyParser, CSPGarbage) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      {
        "ids": ["my-policy"],
        "content_security": {
          "policies": ["potato potato potato"],
          "policies_report_only": ["tomato tomato tomato"]
        }
      }
  )");

  ASSERT_EQ(policy_contents->content_security_policies.size(), 1U);
  ASSERT_EQ(policy_contents->content_security_policies_report_only.size(), 1U);

  ASSERT_EQ(policy_contents->content_security_policies[0],
            "potato potato potato");
  ASSERT_EQ(policy_contents->content_security_policies_report_only[0],
            "tomato tomato tomato");
}

TEST(OriginPolicyParser, CSPNonDict) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      {
        "ids": ["my-policy"],
        "content_security": "script-src 'self' 'unsafe-inline'"
      } )");

  ASSERT_EQ(policy_contents->content_security_policies.size(), 0U);
}

TEST(OriginPolicyParser, CSPNonArray) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      {
        "ids": ["my-policy"],
        "content_security": { "policies": "script-src 'self' 'unsafe-inline'" }
      } )");

  ASSERT_EQ(policy_contents->content_security_policies.size(), 0U);
}

TEST(OriginPolicyParser, CSPNonString) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      {
        "ids": ["my-policy"],
        "content_security": {
          "policies": [["script-src 'self' 'unsafe-inline'"]]
        }
      } )");

  ASSERT_EQ(policy_contents->content_security_policies.size(), 0U);
}

TEST(OriginPolicyParser, FeatureOne) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      {
        "ids": ["my-policy"],
        "features": {
          "policy": "geolocation 'self' http://maps.google.com"
        }
      } )");

  ASSERT_EQ("geolocation 'self' http://maps.google.com",
            policy_contents->feature_policy);
}

TEST(OriginPolicyParser, FeatureTwo) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      {
        "ids": ["my-policy"],
        "features": {
          "policy": "geolocation 'self' http://maps.google.com; camera https://example.com"
        }
      } )");

  ASSERT_EQ(
      "geolocation 'self' http://maps.google.com; camera https://example.com",
      policy_contents->feature_policy);
}

TEST(OriginPolicyParser, FeatureTwoFeatures) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      {
        "ids": ["my-policy"],
        "features": { "policy": "geolocation 'self' http://maps.google.com" },
        "features": { "policy": "camera https://example.com" }
      } )");

  ASSERT_EQ("camera https://example.com", policy_contents->feature_policy);
}

TEST(OriginPolicyParser, FeatureTwoPolicy) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      {
        "ids": ["my-policy"],
        "features": {
          "policy": "geolocation 'self' http://maps.google.com",
          "policy": "camera https://example.com"
        }
      } )");

  ASSERT_EQ("camera https://example.com", policy_contents->feature_policy);
}

// At this level we don't validate the syntax, so commas get passed through.
// Integration tests will show that comma-containing policies get discarded,
// though.
TEST(OriginPolicyParser, FeatureComma) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      {
        "ids": ["my-policy"],
        "features": {
          "policy": "geolocation 'self' http://maps.google.com, camera https://example.com"
        }
      } )");

  ASSERT_EQ(
      "geolocation 'self' http://maps.google.com, camera https://example.com",
      policy_contents->feature_policy);
}

// Similarly, complete garbage will be passed through; this is expected.
TEST(OriginPolicyParser, FeatureGarbage) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      {
        "ids": ["my-policy"],
        "features": {
          "policy": "Lorem ipsum! dolor sit amet"
        }
      } )");

  ASSERT_EQ("Lorem ipsum! dolor sit amet", policy_contents->feature_policy);
}

TEST(OriginPolicyParser, FeatureNonDict) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      {
        "ids": ["my-policy"],
        "features": "geolocation 'self' http://maps.google.com"
      } )");

  ASSERT_FALSE(policy_contents->feature_policy.has_value());
}

TEST(OriginPolicyParser, FeatureNonString) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      {
        "ids": ["my-policy"],
        "features": {
          "policy": ["geolocation 'self' http://maps.google.com"]
        }
      } )");

  ASSERT_FALSE(policy_contents->feature_policy.has_value());
}

namespace {

void TestHintsHelper(const std::vector<std::string>& target_hints) {
  std::string hints_substr;
  for (auto hint_str : target_hints) {
    hints_substr += base::StringPrintf("%s\"%s\": true",
                                       (!hints_substr.empty() ? ", " : ""),
                                       hint_str.c_str());
  }
  std::string manifest_string =
      base::StringPrintf("{ \"ids\": [\"my-policy\"], \"isolation\": { %s }}",
                         hints_substr.c_str());
  auto policy_contents = OriginPolicyParser::Parse(manifest_string);

  ASSERT_TRUE(policy_contents->isolation_optin_hints.has_value());
  for (auto target_hint_str : target_hints) {
    IsolationOptInHints target_hint =
        GetIsolationOptInHintFromString(target_hint_str);
    EXPECT_EQ(target_hint,
              target_hint & policy_contents->isolation_optin_hints.value());
  }
}

}  // namespace

TEST(OriginPolicyParser, IsolationOptInNoIsolationKey) {
  auto policy_contents =
      OriginPolicyParser::Parse(R"({ "ids": ["my-policy"] })");
  ASSERT_FALSE(policy_contents->isolation_optin_hints.has_value());
}

TEST(OriginPolicyParser, IsolationOptInNoDictTrue) {
  auto policy_contents = OriginPolicyParser::Parse(R"({
    "ids": ["my-policy"],
    "isolation": true
  })");
  ASSERT_TRUE(policy_contents->isolation_optin_hints.has_value());
  EXPECT_EQ(IsolationOptInHints::NO_HINTS,
            policy_contents->isolation_optin_hints.value());
}

TEST(OriginPolicyParser, IsolationOptInNoDictFalse) {
  auto policy_contents = OriginPolicyParser::Parse(R"({
    "ids": ["my-policy"],
    "isolation": false
  })");

  ASSERT_FALSE(OriginPolicyParser::Parse(R"({ "isolation": false })")
                   ->isolation_optin_hints.has_value());
}

TEST(OriginPolicyParser, IsolationOptInEmptyDict) {
  TestHintsHelper({});
}

TEST(OriginPolicyParser, IsolationOptInTestOneHint) {
  TestHintsHelper({"prefer_isolated_event_loop"});
  TestHintsHelper({"prefer_isolated_memory"});
  TestHintsHelper({"for_side_channel_protection"});
  TestHintsHelper({"for_memory_measurement"});
}

TEST(OriginPolicyParser, IsolationOptInTestTwoHints) {
  TestHintsHelper({"prefer_isolated_event_loop", "prefer_isolated_memory"});
  TestHintsHelper(
      {"prefer_isolated_event_loop", "for_side_channel_protection"});
  TestHintsHelper({"prefer_isolated_event_loop", "for_memory_measurement"});
  TestHintsHelper({"prefer_isolated_memory", "for_side_channel_protection"});
  TestHintsHelper({"prefer_isolated_memory", "for_memory_measurement"});
  TestHintsHelper({"for_side_channel_protection", "for_memory_measurement"});
}

TEST(OriginPolicyParser, IsolationOptInTestThreeHints) {
  TestHintsHelper({"prefer_isolated_event_loop", "prefer_isolated_memory",
                   "for_side_channel_protection"});
}

TEST(OriginPolicyParser, IsolationOptInIgnoreUnrecognisedKeys) {
  std::string manifest_string = R"( {
    "ids": ["my-policy"],
    "isolation": {
      "prefer_isolated_event_loop": true,
      "foo": true
    }
  } )";
  auto policy_contents = OriginPolicyParser::Parse(manifest_string);
  ASSERT_TRUE(policy_contents->isolation_optin_hints.has_value());
  EXPECT_EQ(IsolationOptInHints::PREFER_ISOLATED_EVENT_LOOP,
            policy_contents->isolation_optin_hints.value());
}

TEST(OriginPolicyParser, IsolationOptInIgnoreFalseValues) {
  std::string manifest_string = R"( {
    "ids": ["my-policy"],
    "isolation": {
      "prefer_isolated_event_loop": false
    }
  } )";
  auto policy_contents = OriginPolicyParser::Parse(manifest_string);
  ASSERT_TRUE(policy_contents->isolation_optin_hints.has_value());
  EXPECT_EQ(IsolationOptInHints::NO_HINTS,
            policy_contents->isolation_optin_hints.value());
}

}  // namespace network
