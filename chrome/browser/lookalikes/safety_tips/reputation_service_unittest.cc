// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/safety_tips/reputation_service.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/lookalikes/safety_tips/safety_tip_test_utils.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "testing/gtest/include/gtest/gtest.h"

using safety_tips::GetUrlBlockType;
using safety_tips::SafetyTipType;

class ReputationServiceTest : public ChromeRenderViewHostTestHarness {
 protected:
  ReputationServiceTest() {}
  ~ReputationServiceTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ReputationServiceTest);
};

// Test that the blocklist blocks patterns as expected.
TEST_F(ReputationServiceTest, BlocklistTest) {
  SetSafetyTipBadRepPatterns(
      {"domain.test/", "directory.test/foo/", "path.test/foo/bar.html",
       "query.test/foo/bar.html?baz=test", "sub.subdomain.test/"});

  const std::vector<std::pair<std::string, safety_tips::SafetyTipType>> kTests =
      {
          {"http://unblocked.test", SafetyTipType::kNone},
          {"http://unblocked.test/foo", SafetyTipType::kNone},
          {"http://unblocked.test/foo.html?bar=baz", SafetyTipType::kNone},

          {"http://sub.domain.test", SafetyTipType::kBadReputation},
          {"http://domain.test", SafetyTipType::kBadReputation},
          {"http://domain.test/foo", SafetyTipType::kBadReputation},
          {"http://domain.test/foo/bar", SafetyTipType::kBadReputation},
          {"http://domain.test/foo.html?bar=baz",
           SafetyTipType::kBadReputation},

          {"http://directory.test", SafetyTipType::kNone},
          {"http://directory.test/bar", SafetyTipType::kNone},
          {"http://directory.test/bar/foo.html", SafetyTipType::kNone},
          {"http://directory.test/foo", SafetyTipType::kNone},
          {"http://directory.test/foo/bar/", SafetyTipType::kBadReputation},
          {"http://directory.test/foo/bar.html?bar=baz",
           SafetyTipType::kBadReputation},

          {"http://path.test", SafetyTipType::kNone},
          {"http://path.test/foo", SafetyTipType::kNone},
          {"http://path.test/foo/bar/", SafetyTipType::kNone},
          {"http://path.test/foo/bar.htm", SafetyTipType::kNone},
          {"http://path.test/foo/bar.html", SafetyTipType::kBadReputation},
          {"http://path.test/foo/bar.html?bar=baz",
           SafetyTipType::kBadReputation},
          {"http://path.test/bar/foo.html", SafetyTipType::kNone},

          {"http://query.test", SafetyTipType::kNone},
          {"http://query.test/foo", SafetyTipType::kNone},
          {"http://query.test/foo/bar/", SafetyTipType::kNone},
          {"http://query.test/foo/bar.html", SafetyTipType::kNone},
          {"http://query.test/foo/bar.html?baz=test",
           SafetyTipType::kBadReputation},
          {"http://query.test/foo/bar.html?baz=test&a=1", SafetyTipType::kNone},
          {"http://query.test/foo/bar.html?baz=no", SafetyTipType::kNone},

          {"http://subdomain.test", SafetyTipType::kNone},
          {"http://sub.subdomain.test", SafetyTipType::kBadReputation},
          {"http://sub.subdomain.test/foo/bar", SafetyTipType::kBadReputation},
          {"http://sub.subdomain.test/foo.html?bar=baz",
           SafetyTipType::kBadReputation},
      };

  for (auto test : kTests) {
    EXPECT_EQ(GetUrlBlockType(GURL(test.first)), test.second);
  }
}
