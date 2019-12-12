// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cross_origin_opener_policy_parser.h"

#include <string>
#include <vector>

#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

using CrossOriginOpenerPolicy = mojom::CrossOriginOpenerPolicy;

TEST(CrossOriginOpenerPolicyParserTest, Parse) {
  const struct {
    const char* raw_coop_string;
    CrossOriginOpenerPolicy policy;
  } kTestCases[] = {
      {"same-origin", CrossOriginOpenerPolicy::kSameOrigin},
      {"same-origin-allow-popups",
       CrossOriginOpenerPolicy::kSameOriginAllowPopups},
      {"unsafe-none", CrossOriginOpenerPolicy::kUnsafeNone},
      // Leading whitespaces.
      {"   same-origin", CrossOriginOpenerPolicy::kSameOrigin},
      // Leading character tabulation.
      {"\tsame-origin", CrossOriginOpenerPolicy::kSameOrigin},
      // Trailing whitespaces.
      {"same-origin-allow-popups   ",
       CrossOriginOpenerPolicy::kSameOriginAllowPopups},
      // Empty string.
      {"", CrossOriginOpenerPolicy::kUnsafeNone},
      // Only whitespaces.
      {"   ", CrossOriginOpenerPolicy::kUnsafeNone},
      // invalid same-site value
      {"same-site", CrossOriginOpenerPolicy::kUnsafeNone},
      // Misspelling.
      {"some-origin", CrossOriginOpenerPolicy::kUnsafeNone},
      // Trailing line-tab.
      {"same-origin\x0B", CrossOriginOpenerPolicy::kUnsafeNone},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message() << std::endl
                                    << "raw_coop_string = "
                                    << test_case.raw_coop_string << std::endl);
    EXPECT_EQ(test_case.policy,
              ParseCrossOriginOpenerPolicyHeader(test_case.raw_coop_string));
  }
}

}  // namespace network
