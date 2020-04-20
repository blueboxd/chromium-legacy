// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/discover/discover_browser_test.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/chromeos/login/discover/discover_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/discover/discover_window_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/discover/discover_window_manager_observer.h"
#include "content/public/test/browser_test_utils.h"

namespace chromeos {
namespace {

// Waits for Discover Browser to be created.
class DiscoverBrowserWaiter : public DiscoverWindowManagerObserver {
 public:
  DiscoverBrowserWaiter() {
    DiscoverWindowManager::GetInstance()->AddObserver(this);
  }

  ~DiscoverBrowserWaiter() override {
    DiscoverWindowManager::GetInstance()->RemoveObserver(this);
  }

  // Blocks until Discover App browser is created. Returns created browser.
  Browser* Wait() {
    if (!browser_)
      run_loop_.Run();

    DCHECK(browser_);
    return browser_;
  }

 private:
  // DiscoverWindowManagerObserver
  void OnNewDiscoverWindow(Browser* browser) override {
    ASSERT_FALSE(browser_);
    browser_ = browser;
    run_loop_.Quit();
  }

  Browser* browser_ = nullptr;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(DiscoverBrowserWaiter);
};

// Waits for Discover APP JS to report "initialized".
class DiscoverUIInitializedWaiter : public DiscoverUI::Observer {
 public:
  explicit DiscoverUIInitializedWaiter(DiscoverUI* ui) : ui_(ui) {
    ui->AddObserver(this);
  }

  ~DiscoverUIInitializedWaiter() override { ui_->RemoveObserver(this); }

  // Blocks until Discover App JS is initialized.
  void Wait() { run_loop_.Run(); }

 private:
  // DiscoverUI::Observer
  void OnInitialized() override { run_loop_.Quit(); }

  DiscoverUI* ui_ = nullptr;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(DiscoverUIInitializedWaiter);
};

}  // anonymous namespace

void DiscoverBrowserTest::LoadAndInitializeDiscoverApp(Profile* profile) {
  LoadDiscoverApp(profile);
  InitializeDiscoverApp();
}

content::WebContents* DiscoverBrowserTest::GetWebContents() const {
  DCHECK(discover_browser_);
  return discover_browser_->tab_strip_model()->GetWebContentsAt(0);
}

DiscoverUI* DiscoverBrowserTest::GetDiscoverUI() const {
  const content::WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return nullptr;

  return DiscoverUI::GetDiscoverUI(web_contents);
}

test::JSChecker DiscoverBrowserTest::DiscoverJS() const {
  return test::JSChecker(GetWebContents());
}

void DiscoverBrowserTest::ClickOnCard(const std::string& card_selector) const {
  DiscoverJS().Evaluate(base::StringPrintf(R"(
        $('discoverUI').root.querySelector('%s').root.
            querySelector('discover-card').click();
        )",
        card_selector.c_str()));
}

void DiscoverBrowserTest::LoadDiscoverApp(Profile* profile) {
  DiscoverBrowserWaiter waiter;

  DiscoverWindowManager::GetInstance()->ShowChromeDiscoverPageForProfile(
      profile);
  discover_browser_ = waiter.Wait();

  ASSERT_TRUE(GetWebContents());
}

void DiscoverBrowserTest::InitializeDiscoverApp() const {
  DiscoverUI* ui = GetDiscoverUI();
  ASSERT_TRUE(ui);
  DiscoverUIInitializedWaiter(ui).Wait();
}

}  // namespace chromeos
