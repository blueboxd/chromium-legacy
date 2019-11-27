// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/simple_test_clock.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/known_interception_disclosure_infobar_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "ui/base/window_open_disposition.h"

namespace {

size_t GetInfobarCount(content::WebContents* contents) {
  InfoBarService* infobar_service = InfoBarService::FromWebContents(contents);
  if (!infobar_service)
    return 0;
  return infobar_service->infobar_count();
}

infobars::InfoBar* GetInfobar(content::WebContents* contents) {
  InfoBarService* infobar_service = InfoBarService::FromWebContents(contents);
  DCHECK(infobar_service);
  return infobar_service->infobar_at(0);
}

// Follows same logic as clicking the "Continue" button would.
void CloseInfobar(content::WebContents* contents) {
  infobars::InfoBar* infobar = GetInfobar(contents);
  if (!infobar)
    return;

  ConfirmInfoBarDelegate* delegate =
      static_cast<ConfirmInfoBarDelegate*>(infobar->delegate());
  delegate->Accept();
  infobar->RemoveSelf();
}

}  // namespace

using KnownInterceptionDisclosureInfobarTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(KnownInterceptionDisclosureInfobarTest,
                       OnlyShowDisclosureOncePerSession) {
  const GURL kTestUrl("https://badssl.com/test/monitoring-disclosure/");
  const GURL kOtherUrl("https://example.com");

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  content::WebContents* tab1 = tab_strip_model->GetActiveWebContents();

  auto* clock = new base::SimpleTestClock();
  clock->SetNow(base::Time::Now());
  KnownInterceptionDisclosureCooldown::GetInstance()->SetClockForTesting(
      std::unique_ptr<base::Clock>(clock));

  // Trigger the disclosure infobar.
  MaybeShowKnownInterceptionDisclosureDialog(tab1, kTestUrl);
  EXPECT_EQ(1u, GetInfobarCount(tab1));

  // Test that the infobar is shown on new tabs after it has been triggered
  // once.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), kOtherUrl, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  content::WebContents* tab2 = tab_strip_model->GetActiveWebContents();
  EXPECT_EQ(1u, GetInfobarCount(tab2));

  // Close the tab.
  tab_strip_model->CloseWebContentsAt(tab_strip_model->active_index(),
                                      TabStripModel::CLOSE_USER_GESTURE);

  // Dismiss the infobar.
  CloseInfobar(tab1);
  EXPECT_EQ(0u, GetInfobarCount(tab1));

  // Try to trigger again -- infobar should not show.
  MaybeShowKnownInterceptionDisclosureDialog(tab1, kTestUrl);
  EXPECT_EQ(0u, GetInfobarCount(tab1));

  // Move clock ahead 8 days.
  clock->Advance(base::TimeDelta::FromDays(8));

  // Trigger the infobar again -- infobar should show again.
  MaybeShowKnownInterceptionDisclosureDialog(tab1, kTestUrl);
  EXPECT_EQ(1u, GetInfobarCount(tab1));
}

IN_PROC_BROWSER_TEST_F(KnownInterceptionDisclosureInfobarTest,
                       PRE_CooldownResetsOnBrowserRestartDesktop) {
  const GURL kTestUrl("https://badssl.com/test/monitoring-disclosure/");

  // Trigger the disclosure infobar.
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  MaybeShowKnownInterceptionDisclosureDialog(tab, kTestUrl);
  EXPECT_EQ(1u, GetInfobarCount(tab));

  // Dismiss the infobar.
  CloseInfobar(tab);
  EXPECT_EQ(0u, GetInfobarCount(tab));
}

IN_PROC_BROWSER_TEST_F(KnownInterceptionDisclosureInfobarTest,
                       CooldownResetsOnBrowserRestartDesktop) {
  const GURL kTestUrl("https://badssl.com/test/monitoring-disclosure/");

  // On restart, no infobar should be shown initially.
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(0u, GetInfobarCount(tab));

  // Triggering the disclosure infobar again after browser restart should show
  // the infobar (the cooldown period should no longer apply on Desktop).
  MaybeShowKnownInterceptionDisclosureDialog(tab, kTestUrl);
  EXPECT_EQ(1u, GetInfobarCount(tab));
}
