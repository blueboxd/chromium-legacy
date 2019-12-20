// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/origin_policy/origin_policy_parser.h"
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

TEST(OriginPolicyParser, ValidButEmpty) {
  auto policy_contents = OriginPolicyParser::Parse("{}");
  AssertEmptyPolicy(policy_contents);
}

TEST(OriginPolicyParser, SimpleCSP) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      { "content-security-policy": [{
          "policy": "script-src 'self' 'unsafe-inline'"
      }] }
  )");
  ASSERT_EQ(policy_contents->content_security_policies.size(), 1U);
  ASSERT_EQ(policy_contents->content_security_policies[0],
            "script-src 'self' 'unsafe-inline'");
}

TEST(OriginPolicyParser, DoubleCSP) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      { "content-security-policy": [{
          "policy": "script-src 'self' 'unsafe-inline'",
          "report-only": false
        },{
          "policy": "script-src 'self' 'https://example.com/'",
          "report-only": true
      }] }
  )");
  ASSERT_EQ(policy_contents->content_security_policies.size(), 1U);
  ASSERT_EQ(policy_contents->content_security_policies_report_only.size(), 1U);

  ASSERT_EQ(policy_contents->content_security_policies[0],
            "script-src 'self' 'unsafe-inline'");
  ASSERT_EQ(policy_contents->content_security_policies_report_only[0],
            "script-src 'self' 'https://example.com/'");
}

TEST(OriginPolicyParser, HalfDoubleCSP) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      { "content-security-policy": [{
          "policy": "script-src 'self' 'unsafe-inline'",
        },{
          "policies": "script-src 'self' 'https://example.com/'",
      }] }
  )");
  AssertEmptyPolicy(policy_contents);
}

TEST(OriginPolicyParser, CSPWithoutCSP) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      { "content-security-policy": [{
          "police": "script-src 'self' 'unsafe-inline'",
          "report-only": false
        }] }
  )");
  AssertEmptyPolicy(policy_contents);
}

TEST(OriginPolicyParser, ExtraFieldsDontBreakParsing) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      { "potatoes": "are better than kale",
        "content-security-policy": [{
          "report-only": false,
          "potatoes": "are best",
          "policy": "script-src 'self' 'unsafe-inline'"
        }],
        "other": {
          "name": "Sieglinde",
          "value": "best of potatoes"
      }}
  )");
  ASSERT_EQ(policy_contents->content_security_policies.size(), 1U);
  ASSERT_EQ(policy_contents->content_security_policies[0],
            "script-src 'self' 'unsafe-inline'");
}

TEST(OriginPolicyParser, CSPDispositionEnforce) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      { "content-security-policy": [{
          "policy": "script-src 'self'",
          "report-only": false
        }] }
  )");
  ASSERT_EQ(policy_contents->content_security_policies.size(), 1U);
  ASSERT_EQ(policy_contents->content_security_policies[0], "script-src 'self'");
}

TEST(OriginPolicyParser, CSPDispositionReport) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      { "content-security-policy": [{
          "policy": "script-src 'self'",
          "report-only": true
        }] }
  )");
  ASSERT_EQ(policy_contents->content_security_policies_report_only.size(), 1U);
  ASSERT_EQ(policy_contents->content_security_policies_report_only[0],
            "script-src 'self'");
}

TEST(OriginPolicyParser, CSPDispositionInvalid) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      { "content-security-policy": [{
          "policy": "script-src 'self'",
          "report-only": "potato"
        }] }
  )");
  ASSERT_EQ(policy_contents->content_security_policies.size(), 1U);
  ASSERT_EQ(policy_contents->content_security_policies[0], "script-src 'self'");
}

TEST(OriginPolicyParser, CSPDispositionAbsent) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      { "content-security-policy": [{
          "policy": "script-src 'self'"
        }] }
  )");
  ASSERT_EQ(policy_contents->content_security_policies.size(), 1U);
  ASSERT_EQ(policy_contents->content_security_policies[0], "script-src 'self'");
}

TEST(OriginPolicyParser, FeatureOne) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      { "features": { "policy":
        "geolocation 'self' http://maps.google.com"
      } } )");
  ASSERT_EQ("geolocation 'self' http://maps.google.com",
            policy_contents->feature_policy);
}

TEST(OriginPolicyParser, FeatureTwo) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      { "features": { "policy":
        "geolocation 'self' http://maps.google.com; camera https://example.com"
      } } )");
  ASSERT_EQ(
      "geolocation 'self' http://maps.google.com; camera https://example.com",
      policy_contents->feature_policy);
}

TEST(OriginPolicyParser, FeatureTwoFeatures) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      { "features": { "policy": "geolocation 'self' http://maps.google.com" },
        "features": { "policy": "camera https://example.com"}
      } )");

  ASSERT_EQ("camera https://example.com", policy_contents->feature_policy);
}

TEST(OriginPolicyParser, FeatureTwoPolicy) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      { "features": { "policy": "geolocation 'self' http://maps.google.com",
                      "policy": "camera https://example.com"
      } } )");

  ASSERT_EQ("camera https://example.com", policy_contents->feature_policy);
}

// At this level we don't validate the syntax, so commas get passed through.
// Integration tests will show that comma-containing policies get discarded,
// though.
TEST(OriginPolicyParser, FeatureComma) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      { "features": { "policy":
        "geolocation 'self' http://maps.google.com, camera https://example.com"
      } } )");

  ASSERT_EQ(
      "geolocation 'self' http://maps.google.com, camera https://example.com",
      policy_contents->feature_policy);
}

// Similarly, complete garbage will be passed through; this is expected.
TEST(OriginPolicyParser, FeatureGarbage) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      { "features": { "policy":
        "Lorem ipsum! dolor sit amet"
      } } )");

  ASSERT_EQ("Lorem ipsum! dolor sit amet", policy_contents->feature_policy);
}

TEST(OriginPolicyParser, FeatureNonDict) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      { "features": "geolocation 'self' http://maps.google.com"
      } )");
  ASSERT_FALSE(policy_contents->feature_policy.has_value());
}

TEST(OriginPolicyParser, FeatureNonString) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      { "features": { "policy": ["geolocation 'self' http://maps.google.com"]
      } )");
  ASSERT_FALSE(policy_contents->feature_policy.has_value());
}

}  // namespace network
