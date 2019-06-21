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

class SpellCheckHostChromeImplWinBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    content::BrowserContext* context = browser()->profile();
    renderer_.reset(new content::MockRenderProcessHost(context));

    service_manager::BindSourceInfo source_info;
    source_info.identity = renderer_->GetChildIdentity();

    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(spellcheck::kWinUseBrowserSpellChecker);

    SpellCheckHostChromeImpl::Create(mojo::MakeRequest(&spell_check_host_),
                                     source_info);
  }

  void TearDownOnMainThread() override { renderer_.reset(); }

  void LogResult(const std::vector<SpellCheckResult>& result) {
    received_result_ = true;
    result_ = result;
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
  std::unique_ptr<content::MockRenderProcessHost> renderer_;
  spellcheck::mojom::SpellCheckHostPtr spell_check_host_;

  bool received_result_ = false;
  std::vector<SpellCheckResult> result_;
  base::OnceClosure quit_;
};

// Uses browsertest to setup chrome threads.
IN_PROC_BROWSER_TEST_F(SpellCheckHostChromeImplWinBrowserTest,
                       SpellCheckReturnMessage) {
  if (base::win::GetVersion() < base::win::Version::WIN8)
    return;

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(spellcheck::kWinUseBrowserSpellChecker);

  spellcheck_platform::SetLanguage(
      "en-US", base::BindOnce(&SpellCheckHostChromeImplWinBrowserTest::
                                  SetLanguageCompletionCallback,
                              base::Unretained(this)));

  RunUntilResultReceived();

  spell_check_host_->RequestTextCheck(
      base::UTF8ToUTF16("zz."), 123,
      base::BindOnce(&SpellCheckHostChromeImplWinBrowserTest::LogResult,
                     base::Unretained(this)));
  RunUntilResultReceived();

  ASSERT_EQ(1U, result_.size());
  EXPECT_EQ(result_[0].location, 0);
  EXPECT_EQ(result_[0].length, 2);
  EXPECT_EQ(result_[0].decoration, SpellCheckResult::SPELLING);
}
