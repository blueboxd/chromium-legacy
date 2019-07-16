// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_no_sinks_view.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "chrome/test/views/chrome_test_views_delegate.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_base.h"

namespace media_router {

class CastDialogNoSinksViewTest : public views::ViewsTestBase {
 public:
  CastDialogNoSinksViewTest() = default;
  ~CastDialogNoSinksViewTest() override = default;

  void SetUp() override {
    auto thread_bundle = std::make_unique<content::TestBrowserThreadBundle>(
        base::test::ScopedTaskEnvironment::MainThreadType::UI,
        base::test::ScopedTaskEnvironment::TimeSource::MOCK_TIME);
    thread_bundle_ = thread_bundle.get();
    set_scoped_task_environment(std::move(thread_bundle));
    set_views_delegate(std::make_unique<ChromeTestViewsDelegate>());
    views::ViewsTestBase::SetUp();
    no_sinks_view_ = std::make_unique<CastDialogNoSinksView>(nullptr);
  }

 protected:
  views::View* looking_for_sinks_view() {
    return no_sinks_view_->looking_for_sinks_view_for_test();
  }
  views::View* help_icon_view() {
    return no_sinks_view_->help_icon_view_for_test();
  }

  void AdvanceTime(base::TimeDelta delta) {
    thread_bundle_->FastForwardBy(delta);
  }

 private:
  content::TestBrowserThreadBundle* thread_bundle_ = nullptr;
  std::unique_ptr<CastDialogNoSinksView> no_sinks_view_;

  DISALLOW_COPY_AND_ASSIGN(CastDialogNoSinksViewTest);
};

TEST_F(CastDialogNoSinksViewTest, SwitchViews) {
  // Initially, only the throbber view should be shown.
  EXPECT_TRUE(looking_for_sinks_view()->GetVisible());
  EXPECT_FALSE(help_icon_view());

  AdvanceTime(base::TimeDelta::FromSeconds(3));
  // After three seconds, only the help icon view should be shown.
  EXPECT_FALSE(looking_for_sinks_view());
  EXPECT_TRUE(help_icon_view()->GetVisible());
}

}  // namespace media_router
