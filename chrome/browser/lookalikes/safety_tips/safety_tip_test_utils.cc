// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/safety_tips/safety_tip_test_utils.h"

#include "chrome/browser/lookalikes/safety_tips/safety_tips_config.h"

void SetSafetyTipPatternsWithFlagType(
    std::vector<std::string> patterns,
    chrome_browser_safety_tips::FlaggedPage::FlagType type) {
  std::unique_ptr<chrome_browser_safety_tips::SafetyTipsConfig> config_proto =
      std::make_unique<chrome_browser_safety_tips::SafetyTipsConfig>();
  // Any version ID will do.
  config_proto->set_version_id(4);
  std::sort(patterns.begin(), patterns.end());
  for (const auto& pattern : patterns) {
    chrome_browser_safety_tips::FlaggedPage* page =
        config_proto->add_flagged_page();
    page->set_pattern(pattern);
    page->set_type(type);
  }

  safety_tips::SetRemoteConfigProto(std::move(config_proto));
}

void SetSafetyTipBadRepPatterns(std::vector<std::string> patterns) {
  SetSafetyTipPatternsWithFlagType(
      patterns, chrome_browser_safety_tips::FlaggedPage::BAD_REP);
}
