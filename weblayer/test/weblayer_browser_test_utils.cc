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
  enum class NavigationEventToObserve { Completion, Failure };

  TestNavigationObserver(base::OnceClosure closure,
                         const GURL& url,
                         NavigationEventToObserve event,
                         Shell* shell)
      : closure_(std::move(closure)),
        url_(url),
        event_(event),
        browser_(shell->browser_controller()) {
    browser_->GetNavigationController()->AddObserver(this);
  }

  ~TestNavigationObserver() override {
    browser_->GetNavigationController()->RemoveObserver(this);
  }

 private:
  // NavigationObserver implementation:
  void NavigationCompleted(Navigation* navigation) override {
    if (navigation->GetURL() == url_ &&
        event_ == NavigationEventToObserve::Completion)
      std::move(closure_).Run();
  }

  void NavigationFailed(Navigation* navigation) override {
    if (navigation->GetURL() == url_ &&
        event_ == NavigationEventToObserve::Failure) {
      std::move(closure_).Run();
    }
  }

  base::OnceClosure closure_;
  const GURL url_;
  NavigationEventToObserve event_;
  BrowserController* browser_;
};

// Navigates to |url| in |shell| and waits for |event| to occur.
void NavigateAndWaitForEvent(
    const GURL& url,
    Shell* shell,
    TestNavigationObserver::NavigationEventToObserve event) {
  base::RunLoop run_loop;
  TestNavigationObserver test_observer(run_loop.QuitClosure(), url, event,
                                       shell);

  shell->browser_controller()->GetNavigationController()->Navigate(url);
  run_loop.Run();
}

}  // namespace

void NavigateAndWaitForCompletion(const GURL& url, Shell* shell) {
  NavigateAndWaitForEvent(
      url, shell, TestNavigationObserver::NavigationEventToObserve::Completion);
}

void NavigateAndWaitForFailure(const GURL& url, Shell* shell) {
  NavigateAndWaitForEvent(
      url, shell, TestNavigationObserver::NavigationEventToObserve::Failure);
}

}  // namespace weblayer
