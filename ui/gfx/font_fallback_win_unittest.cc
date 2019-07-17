// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_fallback_win.h"

#include <ios>
#include <tuple>

#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_task_environment.h"
#include "base/win/windows_version.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/uscript.h"
#include "third_party/icu/source/common/unicode/utf16.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/gfx/test/font_fallback_test_data.h"

namespace gfx {

namespace {

const char kDefaultApplicationLocale[] = "us-en";

class FontFallbackWinTest : public testing::Test {
 public:
  FontFallbackWinTest() = default;

 private:
  // Needed to bypass DCHECK in GetFallbackFont.
  base::test::ScopedTaskEnvironment scoped_task_environment_{
      base::test::ScopedTaskEnvironment::MainThreadType::UI};

  DISALLOW_COPY_AND_ASSIGN(FontFallbackWinTest);
};

// Options to parameterized unittests.
struct FallbackFontTestOption {
  bool ignore_get_fallback_failure = false;
  bool skip_code_point_validation = false;
  bool skip_fallback_fonts_validation = false;
};

const FallbackFontTestOption default_fallback_option = {false, false, false};
// Options for tests that does not validate the GetFallbackFont(...) parameters.
const FallbackFontTestOption untested_fallback_option = {true, true, true};

using FallbackFontTestParamInfo =
    std::tuple<FallbackFontTestCase, FallbackFontTestOption>;

class GetFallbackFontTest
    : public ::testing::TestWithParam<FallbackFontTestParamInfo> {
 public:
  GetFallbackFontTest() = default;

  static std::string ParamInfoToString(
      ::testing::TestParamInfo<FallbackFontTestParamInfo> param_info) {
    const FallbackFontTestCase& test_case = std::get<0>(param_info.param);

    std::string language_tag = test_case.language_tag;
    base::RemoveChars(language_tag, "-", &language_tag);
    return std::string("S") + uscript_getName(test_case.script) + "L" +
           language_tag;
  }

  void SetUp() override { std::tie(test_case_, test_option_) = GetParam(); }

 protected:
  bool GetFallbackFont(const Font& font,
                       const std::string& language_tag,
                       Font* result) {
    return gfx::GetFallbackFont(font, language_tag, test_case_.text, result);
  }

  bool EnsuresScriptSupportCodePoints(const base::string16& text,
                                      UScriptCode script,
                                      const std::string& script_name) {
    size_t i = 0;
    while (i < text.length()) {
      UChar32 code_point;
      U16_NEXT(text.c_str(), i, text.size(), code_point);
      if (!uscript_hasScript(code_point, script)) {
        // Retrieve the appropriate script
        UErrorCode script_error;
        UScriptCode codepoint_script =
            uscript_getScript(code_point, &script_error);

        ADD_FAILURE() << "CodePoint U+" << std::hex << code_point
                      << " is not part of the script '" << script_name
                      << "'. Script '" << uscript_getName(codepoint_script)
                      << "' detected.";
        return false;
      }
    }
    return true;
  }

  bool DoesFontSupportCodePoints(Font font, const base::string16& text) {
    sk_sp<SkTypeface> skia_face =
        SkTypeface::MakeFromName(font.GetFontName().c_str(), SkFontStyle());

    size_t i = 0;
    const SkGlyphID kUnsupportedGlyph = 0;
    while (i < text.length()) {
      UChar32 code_point;
      U16_NEXT(text.c_str(), i, text.size(), code_point);
      SkGlyphID glyph_id = skia_face->unicharToGlyph(code_point);
      if (glyph_id == kUnsupportedGlyph)
        return false;
    }
    return true;
  }

  FallbackFontTestCase test_case_;
  FallbackFontTestOption test_option_;
  std::string script_name_;

 private:
  // Needed to bypass DCHECK in GetFallbackFont.
  base::test::ScopedTaskEnvironment scoped_task_environment_{
      base::test::ScopedTaskEnvironment::MainThreadType::UI};

  DISALLOW_COPY_AND_ASSIGN(GetFallbackFontTest);
};

}  // namespace

TEST_F(FontFallbackWinTest, ParseFontLinkEntry) {
  std::string file;
  std::string font;

  internal::ParseFontLinkEntry("TAHOMA.TTF", &file, &font);
  EXPECT_EQ("TAHOMA.TTF", file);
  EXPECT_EQ("", font);

  internal::ParseFontLinkEntry("MSGOTHIC.TTC,MS UI Gothic", &file, &font);
  EXPECT_EQ("MSGOTHIC.TTC", file);
  EXPECT_EQ("MS UI Gothic", font);

  internal::ParseFontLinkEntry("MALGUN.TTF,128,96", &file, &font);
  EXPECT_EQ("MALGUN.TTF", file);
  EXPECT_EQ("", font);

  internal::ParseFontLinkEntry("MEIRYO.TTC,Meiryo,128,85", &file, &font);
  EXPECT_EQ("MEIRYO.TTC", file);
  EXPECT_EQ("Meiryo", font);
}

TEST_F(FontFallbackWinTest, ParseFontFamilyString) {
  std::vector<std::string> font_names;

  internal::ParseFontFamilyString("Times New Roman (TrueType)", &font_names);
  ASSERT_EQ(1U, font_names.size());
  EXPECT_EQ("Times New Roman", font_names[0]);
  font_names.clear();

  internal::ParseFontFamilyString("Cambria & Cambria Math (TrueType)",
                                  &font_names);
  ASSERT_EQ(2U, font_names.size());
  EXPECT_EQ("Cambria", font_names[0]);
  EXPECT_EQ("Cambria Math", font_names[1]);
  font_names.clear();

  internal::ParseFontFamilyString(
      "Meiryo & Meiryo Italic & Meiryo UI & Meiryo UI Italic (TrueType)",
      &font_names);
  ASSERT_EQ(4U, font_names.size());
  EXPECT_EQ("Meiryo", font_names[0]);
  EXPECT_EQ("Meiryo Italic", font_names[1]);
  EXPECT_EQ("Meiryo UI", font_names[2]);
  EXPECT_EQ("Meiryo UI Italic", font_names[3]);
}

TEST_F(FontFallbackWinTest, EmptyStringFallback) {
  Font base_font;
  Font fallback_font;
  bool result = GetFallbackFont(base_font, kDefaultApplicationLocale,
                                base::StringPiece16(), &fallback_font);
  EXPECT_FALSE(result);
}

TEST_F(FontFallbackWinTest, NulTerminatedStringPiece) {
  Font base_font;
  Font fallback_font;
  // Multiple ending NUL characters.
  const wchar_t kTest1[] = {0x0540, 0x0541, 0, 0, 0};
  EXPECT_FALSE(GetFallbackFont(base_font, kDefaultApplicationLocale,
                               base::StringPiece16(kTest1, ARRAYSIZE(kTest1)),
                               &fallback_font));
  // No ending NUL character.
  const wchar_t kTest2[] = {0x0540, 0x0541};
  EXPECT_TRUE(GetFallbackFont(base_font, kDefaultApplicationLocale,
                              base::StringPiece16(kTest2, ARRAYSIZE(kTest2)),
                              &fallback_font));

  // NUL only characters.
  const wchar_t kTest3[] = {0, 0, 0};
  EXPECT_FALSE(GetFallbackFont(base_font, kDefaultApplicationLocale,
                               base::StringPiece16(kTest3, ARRAYSIZE(kTest3)),
                               &fallback_font));
}

TEST_F(FontFallbackWinTest, CJKLocaleFallback) {
  // The uniscribe fallback used by win7 does not support locale.
  if (base::win::GetVersion() < base::win::Version::WIN10)
    return;

  // Han unification is an effort to map multiple character sets of the CJK
  // languages into a single set of unified characters. Han characters are a
  // common feature of written Chinese (hanzi), Japanese (kanji), and Korean
  // (hanja). The same text will be rendered using a different font based on
  // locale.
  const wchar_t kCJKTest[] = L"\u8AA4\u904E\u9AA8";
  Font base_font;
  Font fallback_font;

  EXPECT_TRUE(GetFallbackFont(base_font, "zh-CN", kCJKTest, &fallback_font));
  EXPECT_EQ(fallback_font.GetFontName(), "Microsoft YaHei UI");

  EXPECT_TRUE(GetFallbackFont(base_font, "zh-TW", kCJKTest, &fallback_font));
  EXPECT_EQ(fallback_font.GetFontName(), "Microsoft JhengHei UI");

  EXPECT_TRUE(GetFallbackFont(base_font, "zh-HK", kCJKTest, &fallback_font));
  EXPECT_EQ(fallback_font.GetFontName(), "Microsoft JhengHei UI");

  EXPECT_TRUE(GetFallbackFont(base_font, "ja", kCJKTest, &fallback_font));
  EXPECT_EQ(fallback_font.GetFontName(), "Yu Gothic UI");

  EXPECT_TRUE(GetFallbackFont(base_font, "ja-JP", kCJKTest, &fallback_font));
  EXPECT_EQ(fallback_font.GetFontName(), "Yu Gothic UI");

  EXPECT_TRUE(GetFallbackFont(base_font, "ko", kCJKTest, &fallback_font));
  EXPECT_EQ(fallback_font.GetFontName(), "Malgun Gothic");

  EXPECT_TRUE(GetFallbackFont(base_font, "ko-KR", kCJKTest, &fallback_font));
  EXPECT_EQ(fallback_font.GetFontName(), "Malgun Gothic");
}

// This test ensures the font fallback work correctly. It will ensures that
//   1) The script supports the text
//   2) The input font does not already support the text
//   3) The call to GetFallbackFont() succeed
//   4) The fallback font has a glyph for every character of the text
//
// The previous checks can be activated or deactivated through the class
// FallbackFontTestOption (e.g. test_option_).
TEST_P(GetFallbackFontTest, GetFallbackFont) {
  // Default system font on Windows.
  const Font base_font("Segoe UI", 14);

  // Skip testing this call to GetFallbackFont on older windows versions. Some
  // fonts only got introduced on windows 10 and the test will fail on previous
  // versions.
  const bool is_win10 = base::win::GetVersion() >= base::win::Version::WIN10;
  if (test_case_.is_win10 && !is_win10)
    return;

  // Retrieve the name of the current script.
  script_name_ = uscript_getName(test_case_.script);

  // Validate that tested characters are part of the script.
  if (!test_option_.skip_code_point_validation &&
      !EnsuresScriptSupportCodePoints(test_case_.text, test_case_.script,
                                      script_name_)) {
    return;
  }

  // The default font already support it, do not try to find a fallback font.
  if (DoesFontSupportCodePoints(base_font, test_case_.text))
    return;

  // Retrieve the fallback font.
  Font fallback_font;
  bool result =
      GetFallbackFont(base_font, test_case_.language_tag, &fallback_font);
  if (!result) {
    if (!test_option_.ignore_get_fallback_failure)
      ADD_FAILURE() << "GetFallbackFont failed for '" << script_name_ << "'";
    return;
  }

  // Ensure the fallback font is a part of the validation fallback fonts list.
  if (!test_option_.skip_fallback_fonts_validation) {
    bool valid = std::find(test_case_.fallback_fonts.begin(),
                           test_case_.fallback_fonts.end(),
                           fallback_font.GetFontName()) !=
                 test_case_.fallback_fonts.end();
    if (!valid) {
      ADD_FAILURE() << "GetFallbackFont failed for '" << script_name_
                    << "' invalid fallback font: "
                    << fallback_font.GetFontName()
                    << " not among valid options: "
                    << base::JoinString(test_case_.fallback_fonts, ", ");
      return;
    }
  }

  // Ensure that glyphs exists in the fallback font.
  if (!DoesFontSupportCodePoints(fallback_font, test_case_.text)) {
    ADD_FAILURE() << "Font '" << fallback_font.GetFontName()
                  << "' does not matched every CodePoints.";
    return;
  }
}

// Produces a font test case for every script.
std::vector<FallbackFontTestCase> GetSampleFontTestCases() {
  std::vector<FallbackFontTestCase> result;

  const unsigned int script_max = u_getIntPropertyMaxValue(UCHAR_SCRIPT) + 1;
  for (unsigned int i = 0; i < script_max; i++) {
    const UScriptCode script = static_cast<UScriptCode>(i);

    // Make a sample text to test the script.
    UChar text[8];
    UErrorCode errorCode = U_ZERO_ERROR;
    int text_length =
        uscript_getSampleString(script, text, ARRAYSIZE(text), &errorCode);
    if (text_length <= 0 || errorCode != U_ZERO_ERROR)
      continue;

    FallbackFontTestCase test_case(script, "", text, {});
    result.push_back(test_case);
  }
  return result;
}

// Ensures that the default fallback font gives known results. The test
// is validating that a known fallback font is given for a given text and font.
INSTANTIATE_TEST_SUITE_P(
    KnownExpectedFonts,
    GetFallbackFontTest,
    testing::Combine(testing::ValuesIn(kGetFontFallbackTests),
                     testing::Values(default_fallback_option)),
    GetFallbackFontTest::ParamInfoToString);

// Ensures that font fallback functions are working properly for any string
// (strings from any script). The test doesn't enforce the functions to
// give a fallback font. The accepted behaviors are:
//    1) The fallback function failed and doesn't provide a fallback.
//    2) The fallback function succeeded and the font supports every glyphs.
INSTANTIATE_TEST_SUITE_P(
    Glyphs,
    GetFallbackFontTest,
    testing::Combine(testing::ValuesIn(GetSampleFontTestCases()),
                     testing::Values(untested_fallback_option)),
    GetFallbackFontTest::ParamInfoToString);
}  // namespace gfx
