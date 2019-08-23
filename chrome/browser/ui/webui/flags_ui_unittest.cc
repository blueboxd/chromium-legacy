// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/flags_ui.h"

#include "base/values.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui_data_source.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class FlagsUITest : public testing::Test {
 public:
  FlagsUITest() = default;

 private:
  content::TestBrowserThreadBundle bundle_;
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

TEST_F(FlagsUITest, FlagsAndEnterpriseSources) {
  std::unique_ptr<content::TestWebUIDataSource> flags_strings =
      content::TestWebUIDataSource::Create("A");
  std::unique_ptr<content::TestWebUIDataSource> enterprise_strings =
      content::TestWebUIDataSource::Create("B");
  FlagsUI::AddFlagsStrings(flags_strings->GetWebUIDataSource());
  FlagsEnterpriseUI::AddEnterpriseStrings(
      enterprise_strings->GetWebUIDataSource());
  EXPECT_EQ(flags_strings->GetLocalizedStrings()->size(),
            enterprise_strings->GetLocalizedStrings()->size());
}
