// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_fallback_win.h"

#include <tuple>

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_task_environment.h"
#include "base/win/windows_version.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/uscript.h"
#include "third_party/icu/source/common/unicode/utf16.h"
#include "third_party/skia/include/core/SkTypeface.h"

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

// A font test case for the parameterized unittests.
struct FallbackFontTestCase {
  UScriptCode script;
  base::string16 text;
  std::vector<std::string> fallback_fonts;
  bool is_win10 = false;
};

using FallbackFontTestParamInfo =
    std::tuple<FallbackFontTestCase, FallbackFontTestOption>;

class GetFallbackFontTest
    : public ::testing::TestWithParam<FallbackFontTestParamInfo> {
 public:
  GetFallbackFontTest() = default;

  static std::string ParamInfoToString(
      ::testing::TestParamInfo<FallbackFontTestParamInfo> param_info) {
    const FallbackFontTestCase& test_case = std::get<0>(param_info.param);

    return uscript_getName(test_case.script);
  }

  void SetUp() override { std::tie(test_case_, test_option_) = GetParam(); }

 protected:
  bool GetFallbackFont(const Font& font, Font* result) {
    return gfx::GetFallbackFont(font, kDefaultApplicationLocale,
                                test_case_.text, result);
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

        ADD_FAILURE() << "CodePoint '" << code_point
                      << "' is not part of the script '" << script_name
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
  bool result = GetFallbackFont(base_font, &fallback_font);
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
                    << fallback_font.GetFontName();
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

// A list of script and the fallback font on a default windows installation.
// This list may need to be updated if fonts or operating systems are
// upgraded.
constexpr bool kWin10Only = true;
FallbackFontTestCase kGetFontFallbackTests[] = {
    {USCRIPT_ARABIC, L"\u062A\u062D", {"Segoe UI", "Tahoma"}},
    {USCRIPT_ARMENIAN, L"\u0540\u0541", {"Segoe UI", "Tahoma", "Sylfaen"}},
    {USCRIPT_BENGALI, L"\u09B8\u09AE", {"Nirmala UI", "Vrinda"}},
    {USCRIPT_BRAILLE, L"\u2870\u2871", {"Segoe UI Symbol"}},
    {USCRIPT_BUGINESE, L"\u1A00\u1A01", {"Leelawadee UI"}, kWin10Only},
    {USCRIPT_CANADIAN_ABORIGINAL, L"\u1410\u1411", {"Gadugi", "Euphemia"}},

    {USCRIPT_CARIAN,
     L"\U000102A0\U000102A1",
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_CHEROKEE,
     L"\u13A1\u13A2",
     {"Gadugi", "Plantagenet Cheroke"},
     kWin10Only},

    {USCRIPT_COPTIC, L"\u2C81\u2C82", {"Segoe UI Historic"}, kWin10Only},

    {USCRIPT_CUNEIFORM,
     L"\U00012000\U0001200C",
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_CYPRIOT,
     L"\U00010800\U00010801",
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_CYRILLIC, L"\u0410\u0411\u0412", {"Times New Roman"}},

    {USCRIPT_DESERET, L"\U00010400\U00010401", {"Segoe UI Symbol"}, kWin10Only},

    {USCRIPT_DEVANAGARI, L"\u0905\u0906", {"Mangal", "Nirmala UI"}},
    {USCRIPT_ETHIOPIC, L"\u1201\u1202", {"Ebrima", "Nyala"}},
    {USCRIPT_GEORGIAN, L"\u10A0\u10A1", {"Sylfaen", "Segoe UI"}, kWin10Only},
    {USCRIPT_GREEK, L"\u0391\u0392", {"Times New Roman"}},
    {USCRIPT_GURMUKHI, L"\u0A21\u0A22", {"Raavi", "Nirmala UI"}},
    {USCRIPT_HANGUL, L"\u1100\u1101", {"Malgun Gothic", "Gulim"}, kWin10Only},
    {USCRIPT_HEBREW, L"\u05D1\u05D2", {"Segoe UI", "Tahoma"}},
    {USCRIPT_KHMER,
     L"\u1780\u1781",
     {"Leelawadee UI", "Khmer UI", "Khmer OS", "MoolBoran", "DaunPenh"}},

    {USCRIPT_IMPERIAL_ARAMAIC,
     L"\U00010841\U00010842",
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_INSCRIPTIONAL_PAHLAVI,
     L"\U00010B61\U00010B62",
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_INSCRIPTIONAL_PARTHIAN,
     L"\U00010B41\U00010B42",
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_JAVANESE, L"\uA991\uA992", {"Javanese Text"}, kWin10Only},
    {USCRIPT_KANNADA, L"\u0CA1\u0CA2", {"Nirmala UI", "Tunga"}},

    {USCRIPT_KHAROSHTHI,
     L"\U00010A10\U00010A11",
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_LAO, L"\u0ED0\u0ED1", {"Lao UI", "Leelawadee UI"}},
    {USCRIPT_LISU, L"\uA4D0\uA4D1", {"Segoe UI"}, kWin10Only},

    {USCRIPT_LYCIAN,
     L"\U00010281\U00010282",
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_LYDIAN,
     L"\U00010921\U00010922",
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_MALAYALAM, L"\u0D21\u0D22", {"Kartika", "Nirmala UI"}},

    {USCRIPT_MEROITIC_CURSIVE,
     L"\U000109A1\U000109A2",
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_MYANMAR, L"\u1000\u1001", {"Myanmar Text"}, kWin10Only},
    {USCRIPT_NEW_TAI_LUE, L"\u1981\u1982", {"Microsoft New Tai Lue"}},
    {USCRIPT_NKO, L"\u07C1\u07C2", {"Ebrima"}},

    {USCRIPT_OGHAM, L"\u1680\u1681", {"Segoe UI Symbol", "Segoe UI Historic"}},

    {USCRIPT_OL_CHIKI, L"\u1C51\u1C52", {"Nirmala UI"}, kWin10Only},

    {USCRIPT_OLD_ITALIC,
     L"\U00010301\U00010302",
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_OLD_PERSIAN,
     L"\U000103A1\U000103A2",
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_OLD_SOUTH_ARABIAN,
     L"\U00010A61\U00010A62",
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_ORIYA, L"\u0B21\u0B22", {"Kalinga", "Nirmala UI"}},
    {USCRIPT_PHAGS_PA, L"\uA841\uA842", {"Microsoft PhagsPa"}},

    {USCRIPT_RUNIC, L"\u16A0\u16A1", {"Segoe UI Symbol", "Segoe UI Historic"}},

    {USCRIPT_SHAVIAN,
     L"\U00010451\U00010452",
     {"Segoe UI", "Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_SINHALA, L"\u0D91\u0D92", {"Iskoola Pota", "Nirmala UI"}},

    {USCRIPT_SORA_SOMPENG, L"\U000110D1\U000110D2", {"Nirmala UI"}, kWin10Only},

    {USCRIPT_SYRIAC,
     L"\u0711\u0712",
     {"Estrangelo Edessa", "Segoe UI Historic"}},

    {USCRIPT_TAI_LE, L"\u1951\u1952", {"Microsoft Tai Le"}},
    {USCRIPT_TAMIL, L"\u0BB1\u0BB2", {"Latha", "Nirmala UI"}},
    {USCRIPT_TELUGU, L"\u0C21\u0C22", {"Gautami", "Nirmala UI"}},
    {USCRIPT_THAANA, L"\u0781\u0782", {"Mv Boli", "MV Boli"}},
    {USCRIPT_THAI,
     L"\u0e01\u0e02",
     {"Tahoma", "Leelawadee UI", "Leelawadee"},
     kWin10Only},
    {USCRIPT_TIBETAN, L"\u0F01\u0F02", {"Microsoft Himalaya"}},
    {USCRIPT_TIFINAGH, L"\u2D31\u2D32", {"Ebrima"}},
    {USCRIPT_VAI, L"\uA501\uA502", {"Ebrima"}},
    {USCRIPT_YI, L"\uA000\uA001", {"Microsoft Yi Baiti"}},
};

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

    FallbackFontTestCase test_case{script, text};
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
