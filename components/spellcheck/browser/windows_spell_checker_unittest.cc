// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/browser/spellcheck_platform.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "components/spellcheck/browser/windows_spell_checker.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class WindowsSpellCheckerTest : public testing::Test {
 public:
  WindowsSpellCheckerTest() {
    if (spellcheck::WindowsVersionSupportsSpellchecker()) {
      feature_list_.InitAndEnableFeature(
          spellcheck::kWinUseBrowserSpellChecker);

      win_spell_checker_ = std::make_unique<WindowsSpellChecker>(
          base::ThreadPool::CreateCOMSTATaskRunner({base::MayBlock()}));

      win_spell_checker_->CreateSpellChecker(
          "en-US", base::BindOnce(
                       &WindowsSpellCheckerTest::SetLanguageCompletionCallback,
                       base::Unretained(this)));

      RunUntilResultReceived();
    }
  }

  void RunUntilResultReceived() {
    if (callback_finished_)
      return;
    base::RunLoop run_loop;
    quit_ = run_loop.QuitClosure();
    run_loop.Run();

    // reset status
    callback_finished_ = false;
  }

  void SetLanguageCompletionCallback(bool result) {
    set_language_result_ = result;
    callback_finished_ = true;
    if (quit_)
      std::move(quit_).Run();
  }

  void TextCheckCompletionCallback(
      const std::vector<SpellCheckResult>& results) {
    callback_finished_ = true;
    spell_check_results_ = results;
    if (quit_)
      std::move(quit_).Run();
  }

#if BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)
  void PerLanguageSuggestionsCompletionCallback(
      const spellcheck::PerLanguageSuggestions& suggestions) {
    callback_finished_ = true;
    per_language_suggestions_ = suggestions;
    if (quit_)
      std::move(quit_).Run();
  }
#endif  // BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)

 protected:
  std::unique_ptr<WindowsSpellChecker> win_spell_checker_;

  bool callback_finished_ = false;
  base::OnceClosure quit_;
  base::test::ScopedFeatureList feature_list_;

  bool set_language_result_;
  std::vector<SpellCheckResult> spell_check_results_;
#if BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)
  spellcheck::PerLanguageSuggestions per_language_suggestions_;
#endif  // BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
};

TEST_F(WindowsSpellCheckerTest, RequestTextCheck) {
  if (!spellcheck::WindowsVersionSupportsSpellchecker()) {
    return;
  }

  ASSERT_TRUE(set_language_result_);

  static const struct {
    const char* text_to_check;
    const char* expected_suggestion;
  } kTestCases[] = {
      {"absense", "absence"},    {"becomeing", "becoming"},
      {"cieling", "ceiling"},    {"definate", "definite"},
      {"eigth", "eight"},        {"exellent", "excellent"},
      {"finaly", "finally"},     {"garantee", "guarantee"},
      {"humerous", "humorous"},  {"imediately", "immediately"},
      {"jellous", "jealous"},    {"knowlege", "knowledge"},
      {"lenght", "length"},      {"manuever", "maneuver"},
      {"naturaly", "naturally"}, {"ommision", "omission"},
  };

  for (size_t i = 0; i < base::size(kTestCases); ++i) {
    const auto& test_case = kTestCases[i];
    const base::string16 word(base::ASCIIToUTF16(test_case.text_to_check));

    // Check if the suggested words occur.
    win_spell_checker_->RequestTextCheck(
        1, word,
        base::BindOnce(&WindowsSpellCheckerTest::TextCheckCompletionCallback,
                       base::Unretained(this)));
    RunUntilResultReceived();

    ASSERT_EQ(1u, spell_check_results_.size())
        << "RequestTextCheckTests case " << i << ": Wrong number of results";

    const std::vector<base::string16>& suggestions =
        spell_check_results_.front().replacements;
    const base::string16 suggested_word(
        base::ASCIIToUTF16(test_case.expected_suggestion));
    auto position =
        std::find_if(suggestions.begin(), suggestions.end(),
                     [&](const base::string16& suggestion) {
                       return suggestion.compare(suggested_word) == 0;
                     });

    ASSERT_NE(suggestions.end(), position) << "RequestTextCheckTests case " << i
                                           << ": Expected suggestion not found";
  }
}

#if BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)
TEST_F(WindowsSpellCheckerTest, GetPerLanguageSuggestions) {
  if (!spellcheck::WindowsVersionSupportsSpellchecker()) {
    return;
  }

  ASSERT_TRUE(set_language_result_);

  win_spell_checker_->GetPerLanguageSuggestions(
      base::ASCIIToUTF16("tihs"),
      base::BindOnce(
          &WindowsSpellCheckerTest::PerLanguageSuggestionsCompletionCallback,
          base::Unretained(this)));
  RunUntilResultReceived();

  ASSERT_EQ(per_language_suggestions_.size(), 1u);
  ASSERT_GT(per_language_suggestions_[0].size(), 0u);
}
#endif  // BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)

}  // namespace
