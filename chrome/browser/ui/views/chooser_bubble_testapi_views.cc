// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind_test_util.h"
#include "chrome/browser/ui/chooser_bubble_testapi.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

namespace test {
namespace {

const char* kViewClassName = "ChooserBubbleUiViewDelegate";

class ChooserBubbleUiWaiterViews : public ChooserBubbleUiWaiter {
 public:
  ChooserBubbleUiWaiterViews()
      : observer_(views::test::AnyWidgetTestPasskey{}) {
    observer_.set_shown_callback(
        base::BindLambdaForTesting([&](views::Widget* widget) {
          if (widget->GetName() == kViewClassName)
            has_shown_ = true;
        }));
    observer_.set_closing_callback(
        base::BindLambdaForTesting([&](views::Widget* widget) {
          if (widget->GetName() == kViewClassName) {
            has_closed_ = true;
            run_loop_.Quit();
          }
        }));
  }

  void WaitForClose() override { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
  views::AnyWidgetObserver observer_;
};

}  // namespace

// static
std::unique_ptr<ChooserBubbleUiWaiter> ChooserBubbleUiWaiter::Create() {
  return std::make_unique<ChooserBubbleUiWaiterViews>();
}

}  // namespace test
