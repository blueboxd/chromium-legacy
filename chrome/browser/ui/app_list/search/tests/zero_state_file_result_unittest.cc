// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/zero_state_file_result.h"

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {

class ZeroStateFileResultTest : public testing::Test {
 public:
  ZeroStateFileResultTest() {
    TestingProfile::Builder profile_builder;
    profile_ = profile_builder.Build();
  }

  ~ZeroStateFileResultTest() override = default;

  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<Profile> profile_;
};

TEST_F(ZeroStateFileResultTest, CheckMetadata) {
  ZeroStateFileResult result(base::FilePath("/my/test/MIXED_case_FILE.Pdf"),
                             0.2f, profile_.get());
  EXPECT_EQ(base::UTF16ToUTF8(result.title()),
            std::string("MIXED_case_FILE.Pdf"));
  EXPECT_EQ(result.id(), "zerostatefile:///my/test/MIXED_case_FILE.Pdf");
  EXPECT_EQ(result.relevance(), 0.2f);
}

}  // namespace app_list
