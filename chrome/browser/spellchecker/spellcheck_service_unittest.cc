// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_service.h"

#include <ostream>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/supports_user_data.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/spellcheck/browser/spellcheck_platform.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

struct TestCase {
  TestCase(
      const std::string& accept_languages,
      const std::vector<std::string>& spellcheck_dictionaries,
      const std::vector<std::string>& expected_languages,
      const std::vector<std::string>& expected_languages_used_for_spellcheck)
      : accept_languages(accept_languages),
        spellcheck_dictionaries(spellcheck_dictionaries) {
    SpellcheckService::Dictionary dictionary;
    for (const auto& language : expected_languages) {
      if (!language.empty()) {
        dictionary.language = language;
        dictionary.used_for_spellcheck =
            base::Contains(expected_languages_used_for_spellcheck, language);
        expected_dictionaries.push_back(dictionary);
      }
    }
  }

  ~TestCase() {}

  std::string accept_languages;
  std::vector<std::string> spellcheck_dictionaries;
  std::vector<SpellcheckService::Dictionary> expected_dictionaries;
};

bool operator==(const SpellcheckService::Dictionary& lhs,
                const SpellcheckService::Dictionary& rhs) {
  return lhs.language == rhs.language &&
         lhs.used_for_spellcheck == rhs.used_for_spellcheck;
}

std::ostream& operator<<(std::ostream& out,
                         const SpellcheckService::Dictionary& dictionary) {
  out << "{\"" << dictionary.language << "\", used_for_spellcheck="
      << (dictionary.used_for_spellcheck ? "true " : "false") << "}";
  return out;
}

std::ostream& operator<<(std::ostream& out, const TestCase& test_case) {
  out << "language::prefs::kAcceptLanguages=[" << test_case.accept_languages
      << "], prefs::kSpellCheckDictionaries=["
      << base::JoinString(test_case.spellcheck_dictionaries, ",")
      << "], expected=[";
  for (const auto& dictionary : test_case.expected_dictionaries) {
    out << dictionary << ",";
  }
  out << "]";
  return out;
}

static std::unique_ptr<KeyedService> BuildSpellcheckService(
    content::BrowserContext* profile) {
  return std::make_unique<SpellcheckService>(static_cast<Profile*>(profile));
}

class SpellcheckServiceUnitTestBase : public testing::Test {
 public:
  SpellcheckServiceUnitTestBase() = default;
  ~SpellcheckServiceUnitTestBase() override = default;

  content::BrowserContext* browser_context() { return &profile_; }
  PrefService* prefs() { return profile_.GetPrefs(); }

 protected:
  void SetUp() override {
#if defined(OS_WIN)
    // Tests were designed assuming Hunspell dictionary used and may fail when
    // Windows spellcheck is enabled by default.
    feature_list_.InitAndDisableFeature(spellcheck::kWinUseBrowserSpellChecker);
#endif  // defined(OS_WIN)

    // Use SetTestingFactoryAndUse to force creation and initialization.
    SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        &profile_, base::BindRepeating(&BuildSpellcheckService));
  }

  content::BrowserTaskEnvironment task_environment_;

#if defined(OS_WIN)
  // feature_list_ needs to be destroyed after profile_.
  base::test::ScopedFeatureList feature_list_;
#endif  // defined(OS_WIN)
  TestingProfile profile_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SpellcheckServiceUnitTestBase);
};

class SpellcheckServiceUnitTest : public SpellcheckServiceUnitTestBase,
                                  public testing::WithParamInterface<TestCase> {
};

INSTANTIATE_TEST_SUITE_P(
    TestCases,
    SpellcheckServiceUnitTest,
    testing::Values(
        TestCase("en,aa", {"aa"}, {""}, {""}),
        TestCase("en,en-JP,fr,aa", {"fr"}, {"fr"}, {"fr"}),
        TestCase("en,en-JP,fr,zz,en-US", {"fr"}, {"fr", "en-US"}, {"fr"}),
        TestCase("en,en-US,en-GB", {"en-GB"}, {"en-US", "en-GB"}, {"en-GB"}),
        TestCase("en,en-US,en-AU", {"en-AU"}, {"en-US", "en-AU"}, {"en-AU"}),
        TestCase("en,en-US,en-AU", {"en-US"}, {"en-US", "en-AU"}, {"en-US"}),
        TestCase("en,en-US", {"en-US"}, {"en-US"}, {"en-US"}),
        TestCase("en,en-US,fr", {"en-US"}, {"en-US", "fr"}, {"en-US"}),
        TestCase("en,fr,en-US,en-AU",
                 {"en-US", "fr"},
                 {"fr", "en-US", "en-AU"},
                 {"fr", "en-US"}),
        TestCase("en-US,en", {"en-US"}, {"en-US"}, {"en-US"}),
#if BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)
        // Scenario where user disabled the Windows spellcheck feature with some
        // non-Hunspell languages set in preferences.
        TestCase("fr,eu,en-US,ar",
                 {"fr", "eu", "en-US", "ar"},
                 {"fr", "en-US"},
                 {"fr", "en-US"}),
#endif  // BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)
        TestCase("hu-HU,hr-HR", {"hr"}, {"hu", "hr"}, {"hr"})));

TEST_P(SpellcheckServiceUnitTest, GetDictionaries) {
  prefs()->SetString(language::prefs::kAcceptLanguages,
                     GetParam().accept_languages);
  base::ListValue spellcheck_dictionaries;
  spellcheck_dictionaries.AppendStrings(GetParam().spellcheck_dictionaries);
  prefs()->Set(spellcheck::prefs::kSpellCheckDictionaries,
               spellcheck_dictionaries);

  std::vector<SpellcheckService::Dictionary> dictionaries;
  SpellcheckService::GetDictionaries(browser_context(), &dictionaries);

  EXPECT_EQ(GetParam().expected_dictionaries, dictionaries);
}

#if BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)
class SpellcheckServiceHybridUnitTestBase
    : public SpellcheckServiceUnitTestBase {
 public:
  SpellcheckServiceHybridUnitTestBase() = default;

 protected:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(spellcheck::kWinUseBrowserSpellChecker);

    // Add command line switch that forces first run state, since code path
    // through SpellcheckService::InitWindowsDictionaryLanguages depends on
    // whether this is first run.
    first_run::ResetCachedSentinelDataForTesting();
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kForceFirstRun);

    // Use SetTestingFactoryAndUse to force creation and initialization.
    SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        &profile_, base::BindRepeating(&BuildSpellcheckService));
  }

  // Used for faking the presence of Windows spellcheck dictionaries.
  static const std::vector<std::string>
      windows_spellcheck_languages_for_testing_;
};

// static
const std::vector<std::string> SpellcheckServiceHybridUnitTestBase::
    windows_spellcheck_languages_for_testing_ = {
        "fr-FR",   // Has both Windows and Hunspell support.
        "es-MX",   // Has both Windows and Hunspell support, but for Hunspell
                   // maps to es-ES.
        "gl-ES",   // (Galician) Has only Windows support, no Hunspell
                   // dictionary.
        "fi-FI",   // (Finnish) Has only Windows support, no Hunspell
                   // dictionary.
        "haw-US",  // (Hawaiian) No Hunspell dictionary. Note that first two
                   // letters of language code are "ha," the same as Hausa.
        "ast",     // (Asturian) Has only Windows support, no Hunspell
                   // dictionary. Note that last two letters of language
                   // code are "st," the same as Sesotho.
        "kok-Deva-IN",       // Konkani (Devanagari, India)--note presence of
                             // script subtag.
        "sr-Cyrl-ME",        // Serbian (Cyrillic, Montenegro)--note presence of
                             // script subtag.
        "sr-Latn-ME",        // Serbian (Latin, Montenegro)--note presence of
                             // script subtag.
        "ja-Latn-JP-x-ext",  // Japanese with Latin script--note presence of
                             // private use subtag. Ignore private use
                             // dictionaries.
};

class SpellcheckServiceHybridUnitTest
    : public SpellcheckServiceHybridUnitTestBase,
      public testing::WithParamInterface<TestCase> {};

INSTANTIATE_TEST_SUITE_P(
    TestCases,
    SpellcheckServiceHybridUnitTest,
    testing::Values(
        // Galician (gl) has only Windows support, no Hunspell dictionary.
        // Croatian (hr) has only Hunspell support, no local Windows dictionary.
        // First language is supported by windows and should be spellchecked
        TestCase("gl", {""}, {"gl"}, {"gl"}),
        TestCase("gl", {"gl"}, {"gl"}, {"gl"}),
        TestCase("gl,hr", {""}, {"gl", "hr"}, {"gl"}),
        TestCase("gl,hr", {"gl"}, {"gl", "hr"}, {"gl"}),
        TestCase("gl,hr", {"hr"}, {"gl", "hr"}, {"gl", "hr"}),
        TestCase("gl,hr", {"gl", "hr"}, {"gl", "hr"}, {"gl", "hr"}),
        // First language is not supported by windows so nothing is changed
        TestCase("hr", {""}, {"hr"}, {""}),
        TestCase("hr", {"hr"}, {"hr"}, {"hr"}),
        TestCase("hr,gl", {"hr"}, {"hr", "gl"}, {"hr"}),
        // Finnish has only "fi" in hard-coded list of accept languages.
        TestCase("fi-FI,fi,en-US,en",
                 {"en-US"},
                 {"fi", "en-US"},
                 {"fi", "en-US"}),
        // First language is supported by Windows but private use dictionaries
        // are ignored.
        TestCase("ja,gl", {"gl"}, {"gl"}, {"gl"}),
        // (Basque) No Hunspell support, has Windows support but
        // language pack not present.
        TestCase("eu", {"eu"}, {""}, {""}),
        TestCase("es-419,es-MX",
                 {"es-419", "es-MX"},
                 {"es-419", "es-MX"},
                 {"es-419", "es-MX"}),
        TestCase("fr-FR,es-MX,gl,pt-BR,hr,it",
                 {"fr-FR", "gl", "pt-BR", "it"},
                 {"fr-FR", "es-MX", "gl", "pt-BR", "hr", "it"},
                 {"fr-FR", "gl", "pt-BR", "it"}),
        // Hausa with Hawaiian language pack (ha/haw string in string).
        TestCase("ha", {"ha"}, {""}, {""}),
        // Sesotho with Asturian language pack (st/ast string in string).
        TestCase("st", {"st"}, {""}, {""}),
        // User chose generic Serbian in languages preferences (which uses
        // Cyrillic script).
        TestCase("sr,sr-Latn-RS", {"sr", "sr-Latn-RS"}, {"sr"}, {"sr"})));

TEST_P(SpellcheckServiceHybridUnitTest, GetDictionaries) {
  if (!spellcheck::WindowsVersionSupportsSpellchecker())
    return;

  prefs()->SetString(language::prefs::kAcceptLanguages,
                     GetParam().accept_languages);
  base::ListValue spellcheck_dictionaries;
  spellcheck_dictionaries.AppendStrings(GetParam().spellcheck_dictionaries);
  prefs()->Set(spellcheck::prefs::kSpellCheckDictionaries,
               spellcheck_dictionaries);

  // Fake the presence of Windows spellcheck dictionaries.
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetInstance()->GetForContext(browser_context());

  spellcheck_service->InitWindowsDictionaryLanguages(
      windows_spellcheck_languages_for_testing_);

  std::vector<SpellcheckService::Dictionary> dictionaries;
  SpellcheckService::GetDictionaries(browser_context(), &dictionaries);

  EXPECT_EQ(GetParam().expected_dictionaries, dictionaries);
}

struct DictionaryMappingTestCase {
  std::string full_tag;
  std::string expected_accept_language;
  std::string expected_tag_passed_to_spellcheck;
};

std::ostream& operator<<(std::ostream& out,
                         const DictionaryMappingTestCase& test_case) {
  out << "full_tag=" << test_case.full_tag
      << ", expected_accept_language=" << test_case.expected_accept_language
      << ", expected_tag_passed_to_spellcheck="
      << test_case.expected_tag_passed_to_spellcheck;

  return out;
}

class SpellcheckServiceWindowsDictionaryMappingUnitTest
    : public SpellcheckServiceHybridUnitTestBase,
      public testing::WithParamInterface<DictionaryMappingTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    TestCases,
    SpellcheckServiceWindowsDictionaryMappingUnitTest,
    testing::Values(DictionaryMappingTestCase({"en-CA", "en-CA", "en-CA"}),
                    DictionaryMappingTestCase({"en-PH", "en", "en"}),
                    DictionaryMappingTestCase({"es-MX", "es-MX", "es-MX"}),
                    DictionaryMappingTestCase({"ar-SA", "ar", "ar"}),
                    // Konkani not supported in Chromium.
                    DictionaryMappingTestCase({"kok-Deva-IN", "", "kok-Deva"}),
                    DictionaryMappingTestCase({"sr-Cyrl-RS", "sr", "sr-Cyrl"}),
                    DictionaryMappingTestCase({"sr-Cyrl-ME", "sr", "sr-Cyrl"}),
                    // Only sr with Cyrillic implied supported in Chromium.
                    DictionaryMappingTestCase({"sr-Latn-RS", "", "sr-Latn"}),
                    DictionaryMappingTestCase({"sr-Latn-ME", "", "sr-Latn"}),
                    DictionaryMappingTestCase({"ca-ES", "ca", "ca"}),
                    DictionaryMappingTestCase({"ca-ES-valencia", "ca", "ca"})));

TEST_P(SpellcheckServiceWindowsDictionaryMappingUnitTest, CheckMappings) {
  if (!spellcheck::WindowsVersionSupportsSpellchecker())
    return;

  // Fake the presence of the Windows spellcheck dictionary.
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetInstance()->GetForContext(browser_context());
  if (!GetParam().full_tag.empty()) {
    spellcheck_service->InitWindowsDictionaryLanguages({GetParam().full_tag});
  }

  std::string supported_accept_language =
      SpellcheckService::GetSupportedAcceptLanguageCode(GetParam().full_tag);

  EXPECT_EQ(GetParam().expected_accept_language, supported_accept_language);

  if (!supported_accept_language.empty()) {
    EXPECT_EQ(GetParam().full_tag,
              spellcheck_service->GetSupportedWindowsDictionaryLanguage(
                  GetParam().expected_accept_language));
  } else {
    // Unsupported language--should not be in map.
    ASSERT_TRUE(spellcheck_service->windows_spellcheck_dictionary_map_.empty());
  }

  EXPECT_EQ(GetParam().expected_tag_passed_to_spellcheck,
            SpellcheckService::GetTagToPassToWindowsSpellchecker(
                GetParam().expected_accept_language, GetParam().full_tag));

  // Special case for Serbian. The "sr" accept language is interpreted as using
  // Cyrillic script. There should be an extra entry in the windows dictionary
  // map if Cyrillic windows dictionary is installed.
  if (base::EqualsCaseInsensitiveASCII(
          "sr-Cyrl", SpellcheckService::GetLanguageAndScriptTag(
                         GetParam().full_tag,
                         /* include_script_tag */ true))) {
    EXPECT_EQ(GetParam().full_tag,
              spellcheck_service->GetSupportedWindowsDictionaryLanguage("sr"));
  } else {
    EXPECT_TRUE(spellcheck_service->GetSupportedWindowsDictionaryLanguage("sr")
                    .empty());
  }
}
#endif  // BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)
