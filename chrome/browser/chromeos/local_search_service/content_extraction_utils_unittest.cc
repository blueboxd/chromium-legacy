// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/local_search_service/content_extraction_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace local_search_service {

TEST(ContentExtractionUtilsTest, ConsolidateTokenTest) {
  {
    const base::string16 text(base::UTF8ToUTF16(
        "Check duplicate. Duplicate is #@$%^&@#$%#@$^@#$ bad"));
    const auto tokens =
        ConsolidateToken(ExtractContent("3rd test", text, "en"));
    EXPECT_EQ(tokens.size(), 3u);

    bool found = false;
    for (const auto& token : tokens) {
      if (token.content == base::UTF8ToUTF16("duplicate")) {
        found = true;
        EXPECT_EQ(token.positions[0].content_id, "3rd test");
        EXPECT_EQ(token.positions[0].start, 6u);
        EXPECT_EQ(token.positions[0].length, 9u);
        EXPECT_EQ(token.positions[1].start, 17u);
        EXPECT_EQ(token.positions[1].length, 9u);
      }
    }
    EXPECT_TRUE(found);
  }
  {
    std::vector<Token> sources = {
        Token(base::UTF8ToUTF16("A"),
              {TokenPosition("ID1", 1u, 1u), TokenPosition("ID1", 3u, 1u)}),
        Token(base::UTF8ToUTF16("B"), {TokenPosition("ID1", 5, 1)}),
        Token(base::UTF8ToUTF16("A"), {TokenPosition("ID2", 10, 1)})};
    const auto tokens = ConsolidateToken(sources);
    EXPECT_EQ(tokens.size(), 2u);

    bool found = false;
    for (const auto& token : tokens) {
      if (token.content == base::UTF8ToUTF16("A")) {
        found = true;
        EXPECT_EQ(token.positions[0].content_id, "ID1");
        EXPECT_EQ(token.positions[0].start, 1u);
        EXPECT_EQ(token.positions[0].length, 1u);
        EXPECT_EQ(token.positions[1].content_id, "ID1");
        EXPECT_EQ(token.positions[1].start, 3u);
        EXPECT_EQ(token.positions[1].length, 1u);
        EXPECT_EQ(token.positions[2].content_id, "ID2");
        EXPECT_EQ(token.positions[2].start, 10u);
        EXPECT_EQ(token.positions[2].length, 1u);
      }
    }
    EXPECT_TRUE(found);
  }
}

TEST(ContentExtractionUtilsTest, ExtractContentTest) {
  {
    const base::string16 text(base::UTF8ToUTF16(
        "Normal... English!!! paragraph: email@gmail.com. Here is a link: "
        "https://google.com, ip=8.8.8.8"));
    const auto tokens = ExtractContent("first test", text, "en");
    EXPECT_EQ(tokens.size(), 7u);

    EXPECT_EQ(tokens[1].content, base::UTF8ToUTF16("english"));
    EXPECT_EQ(tokens[1].positions[0].content_id, "first test");
    EXPECT_EQ(tokens[1].positions[0].start, 10u);
    EXPECT_EQ(tokens[1].positions[0].length, 7u);
  }
  {
    const base::string16 text(base::UTF8ToUTF16("@#$%@^你好!!!"));
    const auto tokens = ExtractContent("2nd test", text, "zh");
    EXPECT_EQ(tokens.size(), 1u);

    EXPECT_EQ(tokens[0].content, base::UTF8ToUTF16("你好"));
    EXPECT_EQ(tokens[0].positions[0].content_id, "2nd test");
    EXPECT_EQ(tokens[0].positions[0].start, 6u);
    EXPECT_EQ(tokens[0].positions[0].length, 2u);
  }
}

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
