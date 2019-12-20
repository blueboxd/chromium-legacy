// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spell_check_host_chrome_impl.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/win/windows_version.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/spellcheck/browser/spellcheck_platform.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/mock_render_process_host.h"
#include "mojo/public/cpp/bindings/remote.h"

class SpellCheckHostChromeImplWinBrowserTest : public InProcessBrowserTest {
 public:
  SpellCheckHostChromeImplWinBrowserTest() {
#if BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{spellcheck::kWinUseBrowserSpellChecker,
                              spellcheck::kWinUseHybridSpellChecker},
        /*disabled_features=*/{});
#else
    feature_list_.InitAndEnableFeature(spellcheck::kWinUseBrowserSpellChecker);
#endif  // BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)
  }

  void SetUpOnMainThread() override {
    content::BrowserContext* context = browser()->profile();
    renderer_.reset(new content::MockRenderProcessHost(context));

    SpellCheckHostChromeImpl::Create(
        renderer_->GetID(), spell_check_host_.BindNewPipeAndPassReceiver());
  }

  void TearDownOnMainThread() override { renderer_.reset(); }

  void OnSpellcheckResult(const std::vector<SpellCheckResult>& result) {
    received_result_ = true;
    result_ = result;
    if (quit_)
      std::move(quit_).Run();
  }

  void OnSuggestionResult(
      const std::vector<std::vector<::base::string16>>& suggestions) {
    received_result_ = true;
    suggestion_result_ = suggestions;
    if (quit_)
      std::move(quit_).Run();
  }

  void SetLanguageCompletionCallback(bool result) {
    received_result_ = true;
    if (quit_)
      std::move(quit_).Run();
  }

  void RunUntilResultReceived() {
    if (received_result_)
      return;
    base::RunLoop run_loop;
    quit_ = run_loop.QuitClosure();
    run_loop.Run();

    // reset status
    received_result_ = false;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<content::MockRenderProcessHost> renderer_;
  mojo::Remote<spellcheck::mojom::SpellCheckHost> spell_check_host_;

  bool received_result_ = false;
  std::vector<SpellCheckResult> result_;
  std::vector<std::vector<::base::string16>> suggestion_result_;
  base::OnceClosure quit_;
};

// Uses browsertest to setup chrome threads.
IN_PROC_BROWSER_TEST_F(SpellCheckHostChromeImplWinBrowserTest,
                       SpellCheckReturnMessage) {
  if (!spellcheck::WindowsVersionSupportsSpellchecker()) {
    return;
  }

  spellcheck_platform::SetLanguage(
      "en-US", base::BindOnce(&SpellCheckHostChromeImplWinBrowserTest::
                                  SetLanguageCompletionCallback,
                              base::Unretained(this)));
  RunUntilResultReceived();

  spell_check_host_->RequestTextCheck(
      base::UTF8ToUTF16("zz."),
      /*route_id=*/123,
      base::BindOnce(
          &SpellCheckHostChromeImplWinBrowserTest::OnSpellcheckResult,
          base::Unretained(this)));
  RunUntilResultReceived();

  ASSERT_EQ(1U, result_.size());
  EXPECT_EQ(result_[0].location, 0);
  EXPECT_EQ(result_[0].length, 2);
  EXPECT_EQ(result_[0].decoration, SpellCheckResult::SPELLING);
}

#if BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)
IN_PROC_BROWSER_TEST_F(SpellCheckHostChromeImplWinBrowserTest,
                       WithPartialResults) {
  if (!spellcheck::WindowsVersionSupportsSpellchecker()) {
    return;
  }

  spellcheck_platform::SetLanguage(
      "en-US", base::BindOnce(&SpellCheckHostChromeImplWinBrowserTest::
                                  SetLanguageCompletionCallback,
                              base::Unretained(this)));
  RunUntilResultReceived();

  // Fake renderer results: "tihs" is misspelled but pretend "wrod" isn't
  std::vector<SpellCheckResult> renderer_results;
  renderer_results.push_back(
      SpellCheckResult(SpellCheckResult::SPELLING, 0, 4));
  spell_check_host_->RequestPartialTextCheck(
      base::UTF8ToUTF16("tihs is a word wrod."),
      /*route_id=*/123, renderer_results,
      /*fill_suggestions=*/false,
      base::BindOnce(
          &SpellCheckHostChromeImplWinBrowserTest::OnSpellcheckResult,
          base::Unretained(this)));
  RunUntilResultReceived();

  // Only "tihs" should be found, since "wrod" was deemed correct by the
  // renderer
  ASSERT_EQ(1U, result_.size());
  EXPECT_EQ(result_[0].location, 0);
  EXPECT_EQ(result_[0].length, 4);
  EXPECT_EQ(result_[0].decoration, SpellCheckResult::SPELLING);
}

IN_PROC_BROWSER_TEST_F(SpellCheckHostChromeImplWinBrowserTest,
                       WithoutPartialResults) {
  if (!spellcheck::WindowsVersionSupportsSpellchecker()) {
    return;
  }

  spellcheck_platform::SetLanguage(
      "en-US", base::BindOnce(&SpellCheckHostChromeImplWinBrowserTest::
                                  SetLanguageCompletionCallback,
                              base::Unretained(this)));
  RunUntilResultReceived();

  // Empty renderer results
  std::vector<SpellCheckResult> renderer_results;
  spell_check_host_->RequestPartialTextCheck(
      base::UTF8ToUTF16("tihs is a wrod."),
      /*route_id=*/123, renderer_results,
      /*fill_suggestions=*/true,
      base::BindOnce(
          &SpellCheckHostChromeImplWinBrowserTest::OnSpellcheckResult,
          base::Unretained(this)));
  RunUntilResultReceived();

  // Both "tihs" and "wrod" should be detected, and should have replacement
  // suggestions
  ASSERT_EQ(2U, result_.size());

  EXPECT_EQ(result_[0].location, 0);
  EXPECT_EQ(result_[0].length, 4);
  EXPECT_EQ(result_[0].decoration, SpellCheckResult::SPELLING);
  EXPECT_GT(result_[0].replacements.size(), 0U);

  EXPECT_EQ(result_[1].location, 10);
  EXPECT_EQ(result_[1].length, 4);
  EXPECT_EQ(result_[1].decoration, SpellCheckResult::SPELLING);
  EXPECT_GT(result_[1].replacements.size(), 0U);
}

IN_PROC_BROWSER_TEST_F(SpellCheckHostChromeImplWinBrowserTest,
                       PerLanguageSuggestions) {
  if (!spellcheck::WindowsVersionSupportsSpellchecker()) {
    return;
  }

  spellcheck_platform::SetLanguage(
      "en-US", base::BindOnce(&SpellCheckHostChromeImplWinBrowserTest::
                                  SetLanguageCompletionCallback,
                              base::Unretained(this)));
  RunUntilResultReceived();

  spell_check_host_->GetPerLanguageSuggestions(
      base::UTF8ToUTF16("tihs"),
      base::BindOnce(
          &SpellCheckHostChromeImplWinBrowserTest::OnSuggestionResult,
          base::Unretained(this)));
  RunUntilResultReceived();

  // Should have 1 vector of results, which contains at least 1 suggestion
  ASSERT_EQ(1U, suggestion_result_.size());
  EXPECT_GT(suggestion_result_[0].size(), 0U);
}
#endif  // BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)
