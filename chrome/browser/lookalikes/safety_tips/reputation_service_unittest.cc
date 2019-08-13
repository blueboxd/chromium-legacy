// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/safety_tips/reputation_service.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/lookalikes/safety_tips/safety_tips.pb.h"
#include "chrome/browser/lookalikes/safety_tips/safety_tips_config.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "testing/gtest/include/gtest/gtest.h"

using chrome_browser_safety_tips::FlaggedPage;
using chrome_browser_safety_tips::SafetyTipsConfig;
using FlagType = FlaggedPage::FlagType;
using safety_tips::GetUrlBlockType;
using safety_tips::SafetyTipType;

namespace {

void BlockPatterns(std::vector<std::pair<std::string, FlagType>> patterns) {
  auto config_proto = std::make_unique<SafetyTipsConfig>();
  config_proto->set_version_id(2);

  std::sort(patterns.begin(), patterns.end());
  for (auto pair : patterns) {
    FlaggedPage* page = config_proto->add_flagged_page();
    page->set_pattern(pair.first);
    page->set_type(pair.second);
  }

  safety_tips::SetRemoteConfigProto(std::move(config_proto));
}

}  // namespace

class ReputationServiceTest : public ChromeRenderViewHostTestHarness {
 protected:
  ReputationServiceTest() {}
  ~ReputationServiceTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ReputationServiceTest);
};

// Test that the blocklist blocks patterns as expected.
TEST_F(ReputationServiceTest, BlocklistTest) {
  auto config_proto = std::make_unique<SafetyTipsConfig>();
  config_proto->set_version_id(2);

  BlockPatterns({{"domain.test/", FlaggedPage::BAD_REP},
                 {"directory.test/foo/", FlaggedPage::BAD_REP},
                 {"path.test/foo/bar.html", FlaggedPage::BAD_REP},
                 {"query.test/foo/bar.html?baz=test", FlaggedPage::BAD_REP},
                 {"sub.subdomain.test/", FlaggedPage::BAD_REP}});

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
