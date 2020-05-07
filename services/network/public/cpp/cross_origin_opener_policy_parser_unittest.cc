// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cross_origin_opener_policy_parser.h"

#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

TEST(CrossOriginOpenerPolicyTest, Parse) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kCrossOriginOpenerPolicy);

  using mojom::CrossOriginOpenerPolicyValue;
  constexpr auto kSameOrigin = CrossOriginOpenerPolicyValue::kSameOrigin;
  constexpr auto kSameOriginAllowPopups =
      CrossOriginOpenerPolicyValue::kSameOriginAllowPopups;
  constexpr auto kUnsafeNone = CrossOriginOpenerPolicyValue::kUnsafeNone;

  const auto kNoHeader = base::Optional<std::string>();
  const auto kNoEndpoint = kNoHeader;

  const struct {
    base::Optional<std::string> raw_coop_string;
    base::Optional<std::string> raw_coop_report_only_string;
    base::Optional<std::string> expected_endpoint;
    CrossOriginOpenerPolicyValue expected_policy;
    base::Optional<std::string> expected_endpoit_report_only;
    CrossOriginOpenerPolicyValue expected_policy_report_only;
  } kTestCases[] = {
      {"same-origin", kNoHeader, kNoEndpoint, kSameOrigin, kNoEndpoint,
       kUnsafeNone},
      {"same-origin-allow-popups", kNoHeader, kNoEndpoint,
       kSameOriginAllowPopups, kNoEndpoint, kUnsafeNone},
      {"unsafe-none", kNoHeader, kNoEndpoint, kUnsafeNone, kNoEndpoint,
       kUnsafeNone},
      // Leading whitespaces.
      {"   same-origin", kNoHeader, kNoEndpoint, kSameOrigin, kNoEndpoint,
       kUnsafeNone},
      // Leading character tabulation.
      {"\tsame-origin", kNoHeader, kNoEndpoint, kSameOrigin, kNoEndpoint,
       kUnsafeNone},
      // Trailing whitespaces.
      {"same-origin-allow-popups   ", kNoHeader, kNoEndpoint,
       kSameOriginAllowPopups, kNoEndpoint, kUnsafeNone},
      // Empty string.
      {"", kNoHeader, kNoEndpoint, kUnsafeNone, kNoEndpoint, kUnsafeNone},
      // Only whitespaces.
      {"   ", kNoHeader, kNoEndpoint, kUnsafeNone, kNoEndpoint, kUnsafeNone},
      // Invalid same-site value
      {"same-site", kNoHeader, kNoEndpoint, kUnsafeNone, kNoEndpoint,
       kUnsafeNone},
      // Misspelling.
      {"some-origin", kNoHeader, kNoEndpoint, kUnsafeNone, kNoEndpoint,
       kUnsafeNone},
      // Trailing line-tab.
      {"same-origin\x0B", kNoHeader, kNoEndpoint, kUnsafeNone, kNoEndpoint,
       kUnsafeNone},
      // Adding report endpoint.
      {"same-origin; report-to=\"endpoint\"", kNoHeader, "endpoint",
       kSameOrigin, kNoEndpoint, kUnsafeNone},
      // Extraneous parameter, ignored.
      {"same-origin; report-to=\"endpoint\"; foo=bar", kNoHeader, "endpoint",
       kSameOrigin, kNoEndpoint, kUnsafeNone},
      // Multiple endpoints
      {"same-origin; report-to=\"endpoint\"; report-to=\"foo\"", kNoHeader,
       "foo", kSameOrigin, kNoEndpoint, kUnsafeNone},
      // Leading spaces in the reporting endpoint.
      {"same-origin-allow-popups;    report-to=\"endpoint\"", kNoHeader,
       "endpoint", kSameOriginAllowPopups, kNoEndpoint, kUnsafeNone},
      // Unsafe-none with endpoint.
      {"unsafe-none; report-to=\"endpoint\"", kNoHeader, "endpoint",
       kUnsafeNone, kNoEndpoint, kUnsafeNone},
      // Unknown parameters should just be ignored.
      {"same-origin; invalidparameter=\"unknown\"", kNoHeader, kNoEndpoint,
       kSameOrigin, kNoEndpoint, kUnsafeNone},
      // Non-string report-to value.
      {"same-origin; report-to=other-endpoint", kNoHeader, kNoEndpoint,
       kSameOrigin, kNoEndpoint, kUnsafeNone},
      // Malformated parameter value.
      {"same-origin-allow-popups;   foo", kNoHeader, kNoEndpoint,
       kSameOriginAllowPopups, kNoEndpoint, kUnsafeNone},
      // Report to empty string endpoint.
      {"same-origin; report-to=\"\"", kNoHeader, "", kSameOrigin, kNoEndpoint,
       kUnsafeNone},
      // Empty parameter value, parsing fails.
      {"same-origin; report-to=", kNoHeader, kNoEndpoint, kUnsafeNone,
       kNoEndpoint, kUnsafeNone},
      // Empty parameter key, parsing fails.
      {"same-origin; =\"\"", kNoHeader, kNoEndpoint, kUnsafeNone, kNoEndpoint,
       kUnsafeNone},
      // Report only same origin header.
      {kNoHeader, "same-origin", kNoEndpoint, kUnsafeNone, kNoEndpoint,
       kSameOrigin},
      // Report only header.
      {kNoHeader, "same-origin-allow-popups", kNoEndpoint, kUnsafeNone,
       kNoEndpoint, kSameOriginAllowPopups},
      // Report only header.
      {kNoHeader, "unsafe-none", kNoEndpoint, kUnsafeNone, kNoEndpoint,
       kUnsafeNone},
      // Report only same origin header with endpoint.
      {kNoHeader, "same-origin; report-to=\"endpoint\"", kNoEndpoint,
       kUnsafeNone, "endpoint", kSameOrigin},
      // Report only allow popups with endpoint.
      {kNoHeader, "same-origin-allow-popups; report-to=\"endpoint\"",
       kNoEndpoint, kUnsafeNone, "endpoint", kSameOriginAllowPopups},
      // Report only unsafe none with endpoint.
      {kNoHeader, "unsafe-none; report-to=\"endpoint\" ", kNoEndpoint,
       kUnsafeNone, "endpoint", kUnsafeNone},
      // Invalid COOP header with valid COOP report only.
      {"INVALID HEADER", "same-origin; report-to=\"endpoint\"", kNoEndpoint,
       kUnsafeNone, "endpoint", kSameOrigin},
      // Unsafe none COOP and allow popups COOP report only.
      {"unsafe-none", "same-origin-allow-popups; report-to=\"endpoint\"",
       kNoEndpoint, kUnsafeNone, "endpoint", kSameOriginAllowPopups},
      // Same-origin-allow-popups coop + same-origin report-only.
      {"same-origin-allow-popups", "same-origin; report-to=\"endpoint\" ",
       kNoEndpoint, kSameOriginAllowPopups, "endpoint", kSameOrigin},
      // Same-origin-allow-popups coop + same-origin report-only, with reporting
      // on both.
      {"same-origin-allow-popups; report-to=\"endpointA\"",
       "same-origin; report-to=\"endpointB\" ", "endpointA",
       kSameOriginAllowPopups, "endpointB", kSameOrigin},
      // Same-origin-allow-popups coop + same-origin report-only, with reporting
      // on both, same endpoint.
      {"same-origin-allow-popups; report-to=\"endpoint\"",
       "same-origin; report-to=\"endpoint\" ", "endpoint",
       kSameOriginAllowPopups, "endpoint", kSameOrigin},
      // Unsafe coop + same-origin report-only, with reporting on both.
      {"unsafe-none; report-to=\"endpoint\"",
       "same-origin; report-to=\"endpoint\" ", "endpoint", kUnsafeNone,
       "endpoint", kSameOrigin},
      // Same-origin with reporting COOP, invalid COOP report-only.
      {"same-origin; report-to=\"endpoint\"", "INVALID HEADER", "endpoint",
       kSameOrigin, kNoEndpoint, kUnsafeNone},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << std::endl
                 << "raw_coop_string = "
                 << (test_case.raw_coop_string ? *test_case.raw_coop_string
                                               : "No header")
                 << std::endl
                 << "raw_coop_report_only_string = "
                 << (test_case.raw_coop_report_only_string
                         ? *test_case.raw_coop_report_only_string
                         : "No header")
                 << std::endl);
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    if (test_case.raw_coop_string) {
      headers->AddHeader("cross-origin-Opener-policy",
                         *test_case.raw_coop_string);
    }
    if (test_case.raw_coop_report_only_string) {
      headers->AddHeader("cross-origin-opener-policy-report-only",
                         *test_case.raw_coop_report_only_string);
    }
    auto parsed_policy = ParseCrossOriginOpenerPolicy(*headers);
    EXPECT_EQ(test_case.expected_endpoint, parsed_policy.reporting_endpoint);
    EXPECT_EQ(test_case.expected_policy, parsed_policy.value);
    EXPECT_EQ(test_case.expected_endpoit_report_only,
              parsed_policy.report_only_reporting_endpoint);
    EXPECT_EQ(test_case.expected_policy_report_only,
              parsed_policy.report_only_value);
  }
}

}  // namespace network
