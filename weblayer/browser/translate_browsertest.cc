// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/browser/translate_waiter.h"
#include "components/translate/core/browser/language_state.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/browser/translate_client_impl.h"
#include "weblayer/public/tab.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

namespace {

TranslateClientImpl* GetTranslateClient(Shell* shell) {
  return TranslateClientImpl::FromWebContents(
      static_cast<TabImpl*>(shell->tab())->web_contents());
}

std::unique_ptr<translate::TranslateWaiter> CreateTranslateWaiter(
    Shell* shell,
    translate::TranslateWaiter::WaitEvent wait_event) {
  return std::make_unique<translate::TranslateWaiter>(
      GetTranslateClient(shell)->translate_driver(), wait_event);
}

void WaitUntilLanguageDetermined(Shell* shell) {
  CreateTranslateWaiter(
      shell, translate::TranslateWaiter::WaitEvent::kLanguageDetermined)
      ->Wait();
}

}  // namespace

using TranslateBrowserTest = WebLayerBrowserTest;

// Tests that the CLD (Compact Language Detection) works properly.
IN_PROC_BROWSER_TEST_F(TranslateBrowserTest, PageLanguageDetection) {
  EXPECT_TRUE(embedded_test_server()->Start());

  TranslateClientImpl* translate_client = GetTranslateClient(shell());

  NavigateAndWaitForCompletion(GURL("about:blank"), shell());
  WaitUntilLanguageDetermined(shell());
  EXPECT_EQ("und", translate_client->GetLanguageState().original_language());

  // Go to a page in English.
  NavigateAndWaitForCompletion(
      GURL(embedded_test_server()->GetURL("/english_page.html")), shell());
  WaitUntilLanguageDetermined(shell());
  EXPECT_EQ("en", translate_client->GetLanguageState().original_language());

  // Now navigate to a page in French.
  NavigateAndWaitForCompletion(
      GURL(embedded_test_server()->GetURL("/french_page.html")), shell());
  WaitUntilLanguageDetermined(shell());
  EXPECT_EQ("fr", translate_client->GetLanguageState().original_language());
}

}  // namespace weblayer
