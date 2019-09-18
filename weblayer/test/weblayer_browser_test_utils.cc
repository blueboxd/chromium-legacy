// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/weblayer_browser_test_utils.h"

#include "base/run_loop.h"
#include "url/gurl.h"
#include "weblayer/public/browser_controller.h"
#include "weblayer/public/navigation.h"
#include "weblayer/public/navigation_controller.h"
#include "weblayer/public/navigation_observer.h"
#include "weblayer/shell/browser/shell.h"

namespace weblayer {

namespace {

// Runs |closure| once |url| is successfully navigated to.
class TestNavigationObserver : public NavigationObserver {
 public:
  TestNavigationObserver(base::OnceClosure closure,
                         const GURL& url,
                         Shell* shell)
      : closure_(std::move(closure)),
        url_(url),
        browser_(shell->browser_controller()) {
    browser_->GetNavigationController()->AddObserver(this);
  }

  ~TestNavigationObserver() override {
    browser_->GetNavigationController()->RemoveObserver(this);
  }

 private:
  // NavigationObserver implementation:
  void NavigationCompleted(Navigation* navigation) override {
    if (navigation->GetURL() == url_)
      std::move(closure_).Run();
  }

  base::OnceClosure closure_;
  const GURL url_;
  BrowserController* browser_;
};

}  // namespace

void NavigateAndWait(const GURL& url, Shell* shell) {
  base::RunLoop run_loop;
  TestNavigationObserver test_observer(run_loop.QuitClosure(), url, shell);

  shell->browser_controller()->GetNavigationController()->Navigate(url);
  run_loop.Run();
}

}  // namespace weblayer
