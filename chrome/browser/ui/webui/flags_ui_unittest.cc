// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/flags_ui.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class FlagsUITest : public testing::Test {
 public:
  FlagsUITest() = default;
};

TEST_F(FlagsUITest, IsEnterpriseUrl) {
  const struct {
    std::string url;
    bool is_enterprise;
  } expectations[] = {
      {"chrome://flags", false},
      {"chrome://flags/no/enterprise", false},
      {"chrome://enterprise", false},
      {"chrome://flags/enterprise", true},
      {"chrome://flags/enterprise/", true},
      {"chrome://flags//enterprise/yes?no", false},
  };

  for (const auto& expectation : expectations) {
    EXPECT_EQ(expectation.is_enterprise,
              FlagsEnterpriseUI::IsEnterpriseUrl(GURL(expectation.url)));
  }
}
