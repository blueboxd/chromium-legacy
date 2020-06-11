// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/local_search_service/content_extraction_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace local_search_service {

TEST(ContentExtractionUtilsTest, StopwordTest) {
  // Non English.
  EXPECT_FALSE(IsStopword(base::UTF8ToUTF16("was"), "vn"));

  // English.
  EXPECT_TRUE(IsStopword(base::UTF8ToUTF16("i"), "en-US"));
  EXPECT_TRUE(IsStopword(base::UTF8ToUTF16("my"), "en"));
  EXPECT_FALSE(IsStopword(base::UTF8ToUTF16("stopword"), "en"));
}

TEST(ContentExtractionUtilsTest, NormalizerTest) {
  // Test diacritic removed.
  EXPECT_EQ(
      Normalizer(base::UTF8ToUTF16("các dấu câu đã được loại bỏ thành công")),
      base::UTF8ToUTF16("cac dau cau da duoc loai bo thanh cong"));

  // Test hyphens removed.
  EXPECT_EQ(Normalizer(base::UTF8ToUTF16(u8"wi\u2015fi----"), true),
            base::UTF8ToUTF16("wifi"));

  // Keep hyphen.
  EXPECT_EQ(Normalizer(base::UTF8ToUTF16("wi-fi"), false),
            base::UTF8ToUTF16("wi-fi"));

  // Case folding test.
  EXPECT_EQ(Normalizer(base::UTF8ToUTF16("This Is sOmE WEIRD LooKing text")),
            base::UTF8ToUTF16("this is some weird looking text"));

  // Combine test.
  EXPECT_EQ(
      Normalizer(base::UTF8ToUTF16(
                     "Đây là MỘT trình duyệt tuyệt vời và mượt\u2014\u058Amà"),
                 true),
      base::UTF8ToUTF16("day la mot trinh duyet tuyet voi va muotma"));
}

}  // namespace local_search_service
