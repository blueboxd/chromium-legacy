#include "third_party/blink/renderer/platform/wtf/text/case_map.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace WTF {

struct CaseFoldingTestData {
  const char* source_description;
  const char* source;
  const char** locale_list;
  size_t locale_list_length;
  const char* expected;
};

// \xC4\xB0 = U+0130 (capital dotted I)
// \xC4\xB1 = U+0131 (lowercase dotless I)
const char* g_turkic_input = "Isi\xC4\xB0 \xC4\xB0s\xC4\xB1I";
const char* g_greek_input =
    "\xCE\x9F\xCE\x94\xCE\x8C\xCE\xA3 \xCE\x9F\xCE\xB4\xCF\x8C\xCF\x82 "
    "\xCE\xA3\xCE\xBF \xCE\xA3\xCE\x9F o\xCE\xA3 \xCE\x9F\xCE\xA3 \xCF\x83 "
    "\xE1\xBC\x95\xCE\xBE";
const char* g_lithuanian_input =
    "I \xC3\x8F J J\xCC\x88 \xC4\xAE \xC4\xAE\xCC\x88 \xC3\x8C \xC3\x8D "
    "\xC4\xA8 xi\xCC\x87\xCC\x88 xj\xCC\x87\xCC\x88 x\xC4\xAF\xCC\x87\xCC\x88 "
    "xi\xCC\x87\xCC\x80 xi\xCC\x87\xCC\x81 xi\xCC\x87\xCC\x83 XI X\xC3\x8F XJ "
    "XJ\xCC\x88 X\xC4\xAE X\xC4\xAE\xCC\x88";

const char* g_turkic_locales[] = {
    "tr", "tr-TR", "tr_TR", "tr@foo=bar", "tr-US", "TR", "tr-tr", "tR",
    "az", "az-AZ", "az_AZ", "az@foo=bar", "az-US", "Az", "AZ-AZ",
};
const char* g_non_turkic_locales[] = {
    "en", "en-US", "en_US", "en@foo=bar", "EN", "En",
    "ja", "el",    "fil",   "fi",         "lt",
};
const char* g_greek_locales[] = {
    "el", "el-GR", "el_GR", "el@foo=bar", "el-US", "EL", "el-gr", "eL",
};
const char* g_non_greek_locales[] = {
    "en", "en-US", "en_US", "en@foo=bar", "EN", "En",
    "ja", "tr",    "az",    "fil",        "fi", "lt",
};
const char* g_lithuanian_locales[] = {
    "lt", "lt-LT", "lt_LT", "lt@foo=bar", "lt-US", "LT", "lt-lt", "lT",
};
// Should not have "tr" or "az" because "lt" and 'tr/az' rules conflict with
// each other.
const char* g_non_lithuanian_locales[] = {
    "en", "en-US", "en_US", "en@foo=bar", "EN", "En", "ja", "fil", "fi", "el",
};

TEST(CaseMapTest, ToUpperLocale) {
  CaseFoldingTestData test_data_list[] = {
      {
          "Turkic input",
          g_turkic_input,
          g_turkic_locales,
          sizeof(g_turkic_locales) / sizeof(const char*),
          "IS\xC4\xB0\xC4\xB0 \xC4\xB0SII",
      },
      {
          "Turkic input",
          g_turkic_input,
          g_non_turkic_locales,
          sizeof(g_non_turkic_locales) / sizeof(const char*),
          "ISI\xC4\xB0 \xC4\xB0SII",
      },
      {
          "Greek input",
          g_greek_input,
          g_greek_locales,
          sizeof(g_greek_locales) / sizeof(const char*),
          "\xCE\x9F\xCE\x94\xCE\x9F\xCE\xA3 \xCE\x9F\xCE\x94\xCE\x9F\xCE\xA3 "
          "\xCE\xA3\xCE\x9F \xCE\xA3\xCE\x9F \x4F\xCE\xA3 \xCE\x9F\xCE\xA3 "
          "\xCE\xA3 \xCE\x95\xCE\x9E",
      },
      {
          "Greek input",
          g_greek_input,
          g_non_greek_locales,
          sizeof(g_non_greek_locales) / sizeof(const char*),
          "\xCE\x9F\xCE\x94\xCE\x8C\xCE\xA3 \xCE\x9F\xCE\x94\xCE\x8C\xCE\xA3 "
          "\xCE\xA3\xCE\x9F \xCE\xA3\xCE\x9F \x4F\xCE\xA3 \xCE\x9F\xCE\xA3 "
          "\xCE\xA3 \xE1\xBC\x9D\xCE\x9E",
      },
      {
          "Lithuanian input",
          g_lithuanian_input,
          g_lithuanian_locales,
          sizeof(g_lithuanian_locales) / sizeof(const char*),
          "I \xC3\x8F J J\xCC\x88 \xC4\xAE \xC4\xAE\xCC\x88 \xC3\x8C \xC3\x8D "
          "\xC4\xA8 XI\xCC\x88 XJ\xCC\x88 X\xC4\xAE\xCC\x88 XI\xCC\x80 "
          "XI\xCC\x81 XI\xCC\x83 XI X\xC3\x8F XJ XJ\xCC\x88 X\xC4\xAE "
          "X\xC4\xAE\xCC\x88",
      },
      {
          "Lithuanian input",
          g_lithuanian_input,
          g_non_lithuanian_locales,
          sizeof(g_non_lithuanian_locales) / sizeof(const char*),
          "I \xC3\x8F J J\xCC\x88 \xC4\xAE \xC4\xAE\xCC\x88 \xC3\x8C \xC3\x8D "
          "\xC4\xA8 XI\xCC\x87\xCC\x88 XJ\xCC\x87\xCC\x88 "
          "X\xC4\xAE\xCC\x87\xCC\x88 XI\xCC\x87\xCC\x80 XI\xCC\x87\xCC\x81 "
          "XI\xCC\x87\xCC\x83 XI X\xC3\x8F XJ XJ\xCC\x88 X\xC4\xAE "
          "X\xC4\xAE\xCC\x88",
      },
  };

  for (size_t i = 0; i < sizeof(test_data_list) / sizeof(test_data_list[0]);
       ++i) {
    const char* expected = test_data_list[i].expected;
    String source = String::FromUTF8(test_data_list[i].source);
    for (size_t j = 0; j < test_data_list[i].locale_list_length; ++j) {
      const char* locale = test_data_list[i].locale_list[j];
      CaseMap case_map(locale);
      EXPECT_EQ(expected, case_map.ToUpper(source).Utf8())
          << test_data_list[i].source_description << "; locale=" << locale;
    }
  }
}

TEST(CaseMapTest, ToLowerLocale) {
  CaseFoldingTestData test_data_list[] = {
      {
          "Turkic input",
          g_turkic_input,
          g_turkic_locales,
          sizeof(g_turkic_locales) / sizeof(const char*),
          "\xC4\xB1sii is\xC4\xB1\xC4\xB1",
      },
      {
          "Turkic input",
          g_turkic_input,
          g_non_turkic_locales,
          sizeof(g_non_turkic_locales) / sizeof(const char*),
          // U+0130 is lowercased to U+0069 followed by U+0307
          "isii\xCC\x87 i\xCC\x87s\xC4\xB1i",
      },
      {
          "Greek input",
          g_greek_input,
          g_greek_locales,
          sizeof(g_greek_locales) / sizeof(const char*),
          "\xCE\xBF\xCE\xB4\xCF\x8C\xCF\x82 \xCE\xBF\xCE\xB4\xCF\x8C\xCF\x82 "
          "\xCF\x83\xCE\xBF \xCF\x83\xCE\xBF \x6F\xCF\x82 \xCE\xBF\xCF\x82 "
          "\xCF\x83 \xE1\xBC\x95\xCE\xBE",
      },
      {
          "Greek input",
          g_greek_input,
          g_non_greek_locales,
          sizeof(g_greek_locales) / sizeof(const char*),
          "\xCE\xBF\xCE\xB4\xCF\x8C\xCF\x82 \xCE\xBF\xCE\xB4\xCF\x8C\xCF\x82 "
          "\xCF\x83\xCE\xBF \xCF\x83\xCE\xBF \x6F\xCF\x82 \xCE\xBF\xCF\x82 "
          "\xCF\x83 \xE1\xBC\x95\xCE\xBE",
      },
      {
          "Lithuanian input",
          g_lithuanian_input,
          g_lithuanian_locales,
          sizeof(g_lithuanian_locales) / sizeof(const char*),
          "i \xC3\xAF j j\xCC\x87\xCC\x88 \xC4\xAF \xC4\xAF\xCC\x87\xCC\x88 "
          "i\xCC\x87\xCC\x80 i\xCC\x87\xCC\x81 i\xCC\x87\xCC\x83 "
          "xi\xCC\x87\xCC\x88 xj\xCC\x87\xCC\x88 x\xC4\xAF\xCC\x87\xCC\x88 "
          "xi\xCC\x87\xCC\x80 xi\xCC\x87\xCC\x81 xi\xCC\x87\xCC\x83 xi "
          "x\xC3\xAF xj xj\xCC\x87\xCC\x88 x\xC4\xAF x\xC4\xAF\xCC\x87\xCC\x88",
      },
      {
          "Lithuanian input",
          g_lithuanian_input,
          g_non_lithuanian_locales,
          sizeof(g_non_lithuanian_locales) / sizeof(const char*),
          "\x69 \xC3\xAF \x6A \x6A\xCC\x88 \xC4\xAF \xC4\xAF\xCC\x88 \xC3\xAC "
          "\xC3\xAD \xC4\xA9 \x78\x69\xCC\x87\xCC\x88 \x78\x6A\xCC\x87\xCC\x88 "
          "\x78\xC4\xAF\xCC\x87\xCC\x88 \x78\x69\xCC\x87\xCC\x80 "
          "\x78\x69\xCC\x87\xCC\x81 \x78\x69\xCC\x87\xCC\x83 \x78\x69 "
          "\x78\xC3\xAF \x78\x6A \x78\x6A\xCC\x88 \x78\xC4\xAF "
          "\x78\xC4\xAF\xCC\x88",
      },
  };

  for (size_t i = 0; i < sizeof(test_data_list) / sizeof(test_data_list[0]);
       ++i) {
    const char* expected = test_data_list[i].expected;
    String source = String::FromUTF8(test_data_list[i].source);
    for (size_t j = 0; j < test_data_list[i].locale_list_length; ++j) {
      const char* locale = test_data_list[i].locale_list[j];
      CaseMap case_map(locale);
      EXPECT_EQ(expected, case_map.ToLower(source).Utf8())
          << test_data_list[i].source_description << "; locale=" << locale;
    }
  }
}

}  // namespace WTF
