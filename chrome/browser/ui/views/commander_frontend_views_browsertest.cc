// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commander_frontend_views.h"

#include "base/macros.h"
#include "chrome/browser/ui/commander/commander_backend.h"
#include "chrome/browser/ui/commander/commander_view_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

class CommanderFrontendViewsTest : public InProcessBrowserTest {
 public:
  // TODO(lgrey): This is copied over from CommanderControllerUnittest, with
  // modifications. If we need it one more time, extract to a common file.
  class TestBackend : public commander::CommanderBackend {
   public:
    void OnTextChanged(const base::string16& text, Browser* browser) override {
      text_changed_invocations_.push_back(text);
    }
    void OnCommandSelected(size_t command_index, int result_set_id) override {
      command_selected_invocations_.push_back(command_index);
    }
    void SetUpdateCallback(commander::CommanderBackend::ViewModelUpdateCallback
                               callback) override {
      callback_ = std::move(callback);
    }
    void Reset() override { reset_invocation_count_++; }

    void CallCallback() {
      commander::CommanderViewModel vm;
      CallCallback(vm);
    }

    void CallCallback(commander::CommanderViewModel vm) { callback_.Run(vm); }

    const std::vector<base::string16> text_changed_invocations() {
      return text_changed_invocations_;
    }
    const std::vector<size_t> command_selected_invocations() {
      return command_selected_invocations_;
    }

    int reset_invocation_count() { return reset_invocation_count_; }

   private:
    commander::CommanderBackend::ViewModelUpdateCallback callback_;
    std::vector<base::string16> text_changed_invocations_;
    std::vector<size_t> command_selected_invocations_;
    int reset_invocation_count_ = 0;
  };

 protected:
  std::unique_ptr<TestBackend> backend_;

 private:
  void SetUpOnMainThread() override {
    backend_ = std::make_unique<TestBackend>();
  }
};

IN_PROC_BROWSER_TEST_F(CommanderFrontendViewsTest, ShowShowsWidget) {
  auto frontend = std::make_unique<CommanderFrontendViews>(backend_.get());

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey(),
                                       "Commander");
  frontend->Show(browser());
  views::Widget* commander_widget = waiter.WaitIfNeededAndGet();
  EXPECT_TRUE(commander_widget);
}

IN_PROC_BROWSER_TEST_F(CommanderFrontendViewsTest, HideHidesWidget) {
  auto frontend = std::make_unique<CommanderFrontendViews>(backend_.get());

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey(),
                                       "Commander");
  frontend->Show(browser());
  views::Widget* commander_widget = waiter.WaitIfNeededAndGet();
  EXPECT_EQ(backend_->reset_invocation_count(), 0);

  views::test::WidgetDestroyedWaiter destroyed_waiter(commander_widget);
  frontend->Hide();
  destroyed_waiter.Wait();
  EXPECT_EQ(backend_->reset_invocation_count(), 1);
}

IN_PROC_BROWSER_TEST_F(CommanderFrontendViewsTest, DismissHidesWidget) {
  auto frontend = std::make_unique<CommanderFrontendViews>(backend_.get());

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey(),
                                       "Commander");
  frontend->Show(browser());
  views::Widget* commander_widget = waiter.WaitIfNeededAndGet();
  EXPECT_EQ(backend_->reset_invocation_count(), 0);

  views::test::WidgetDestroyedWaiter destroyed_waiter(commander_widget);
  frontend->OnDismiss();
  destroyed_waiter.Wait();
  EXPECT_EQ(backend_->reset_invocation_count(), 1);
}

IN_PROC_BROWSER_TEST_F(CommanderFrontendViewsTest, ViewModelCloseHidesWidget) {
  auto frontend = std::make_unique<CommanderFrontendViews>(backend_.get());

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey(),
                                       "Commander");
  frontend->Show(browser());
  views::Widget* commander_widget = waiter.WaitIfNeededAndGet();
  EXPECT_EQ(backend_->reset_invocation_count(), 0);

  views::test::WidgetDestroyedWaiter destroyed_waiter(commander_widget);
  commander::CommanderViewModel vm;
  vm.action = commander::CommanderViewModel::Action::kClose;
  backend_->CallCallback(vm);
  destroyed_waiter.Wait();
  EXPECT_EQ(backend_->reset_invocation_count(), 1);
}

IN_PROC_BROWSER_TEST_F(CommanderFrontendViewsTest, OnHeightChangedSizesWidget) {
  auto frontend = std::make_unique<CommanderFrontendViews>(backend_.get());

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey(),
                                       "Commander");
  frontend->Show(browser());
  views::Widget* commander_widget = waiter.WaitIfNeededAndGet();
  int old_height = commander_widget->GetRootView()->height();
  int new_height = 200;
  // Ensure changing height isn't a no-op.
  EXPECT_NE(old_height, new_height);

  frontend->OnHeightChanged(200);
  EXPECT_EQ(new_height, commander_widget->GetRootView()->height());
}

IN_PROC_BROWSER_TEST_F(CommanderFrontendViewsTest, PassesOnOptionSelected) {
  auto frontend = std::make_unique<CommanderFrontendViews>(backend_.get());

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey(),
                                       "Commander");
  frontend->Show(browser());
  ignore_result(waiter.WaitIfNeededAndGet());

  frontend->OnOptionSelected(8, 13);
  ASSERT_EQ(backend_->command_selected_invocations().size(), 1u);
  EXPECT_EQ(backend_->command_selected_invocations().back(), 8u);
}

IN_PROC_BROWSER_TEST_F(CommanderFrontendViewsTest, PassesOnTextChanged) {
  auto frontend = std::make_unique<CommanderFrontendViews>(backend_.get());
  const base::string16 input = base::ASCIIToUTF16("orange");
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey(),
                                       "Commander");

  frontend->Show(browser());
  ignore_result(waiter.WaitIfNeededAndGet());

  frontend->OnTextChanged(input);
  ASSERT_EQ(backend_->text_changed_invocations().size(), 1u);
  EXPECT_EQ(backend_->text_changed_invocations().back(), input);
}
