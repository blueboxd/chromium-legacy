// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/weblayer_browser_test_utils.h"

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "url/gurl.h"
#include "weblayer/public/navigation.h"
#include "weblayer/public/navigation_controller.h"
#include "weblayer/public/navigation_observer.h"
#include "weblayer/public/tab.h"
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
        tab_(shell->tab()) {
    tab_->GetNavigationController()->AddObserver(this);
  }

  ~TestNavigationObserver() override {
    tab_->GetNavigationController()->RemoveObserver(this);
  }

 private:
  // NavigationObserver implementation:
  void NavigationCompleted(Navigation* navigation) override {
    if (navigation->GetURL() == url_ &&
        event_ == NavigationEventToObserve::Completion) {
      navigation_complete_ = true;
      CheckComplete();
    }
  }

  void NavigationFailed(Navigation* navigation) override {
    if (navigation->GetURL() == url_ &&
        event_ == NavigationEventToObserve::Failure) {
      std::move(closure_).Run();
    }
  }

  void LoadStateChanged(bool is_loading, bool to_different_document) override {
    done_loading_ = !is_loading;
    CheckComplete();
  }

  void CheckComplete() {
    if (done_loading_ && navigation_complete_)
      std::move(closure_).Run();
  }

  base::OnceClosure closure_;
  const GURL url_;
  NavigationEventToObserve event_;
  Tab* tab_;
  bool done_loading_ = false;
  bool navigation_complete_ = false;
};

// Navigates to |url| in |shell| and waits for |event| to occur.
void NavigateAndWaitForEvent(
    const GURL& url,
    Shell* shell,
    TestNavigationObserver::NavigationEventToObserve event) {
  base::RunLoop run_loop;
  TestNavigationObserver test_observer(run_loop.QuitClosure(), url, event,
                                       shell);

  shell->tab()->GetNavigationController()->Navigate(url);
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

base::Value ExecuteScript(Shell* shell,
                          const std::string& script,
                          bool use_separate_isolate) {
  base::Value final_result;
  base::RunLoop run_loop;
  shell->tab()->ExecuteScript(
      base::ASCIIToUTF16(script), use_separate_isolate,
      base::BindLambdaForTesting(
          [&run_loop, &final_result](base::Value result) {
            final_result = std::move(result);
            run_loop.Quit();
          }));
  run_loop.Run();
  return final_result;
}

}  // namespace weblayer
