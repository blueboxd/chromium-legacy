// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_window_tree_host_platform.h"
#include "base/run_loop.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/widget_observer.h"

namespace views {

namespace {

class WidgetDestroyingObserver : public WidgetObserver {
 public:
  explicit WidgetDestroyingObserver(Widget* widget) : widget_(widget) {
    widget_->AddObserver(this);
  }
  ~WidgetDestroyingObserver() override {
    if (widget_)
      widget_->RemoveObserver(this);
  }

  // Returns immediately when |widget_| becomes NULL, otherwise a RunLoop is
  // used until widget closing event is received.
  void Wait() {
    if (!on_widget_destroying_)
      run_loop_.Run();
  }

  bool widget_destroying() const { return on_widget_destroying_; }

 private:
  // views::WidgetObserver override:
  void OnWidgetDestroying(Widget* widget) override {
    DCHECK_EQ(widget_, widget);
    widget_->RemoveObserver(this);
    widget_ = nullptr;
    on_widget_destroying_ = true;
    if (run_loop_.running())
      run_loop_.Quit();
  }

  Widget* widget_;
  base::RunLoop run_loop_;
  bool on_widget_destroying_ = false;

  DISALLOW_COPY_AND_ASSIGN(WidgetDestroyingObserver);
};

std::unique_ptr<Widget> CreateWidgetWithNativeWidget() {
  std::unique_ptr<Widget> widget(new Widget);
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.delegate = nullptr;
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.remove_standard_frame = true;
  params.native_widget = new DesktopNativeWidgetAura(widget.get());
  params.bounds = gfx::Rect(100, 100, 100, 100);
  widget->Init(std::move(params));
  return widget;
}

}  // namespace

class DesktopWindowTreeHostPlatformTest : public ViewsTestBase {
 public:
  DesktopWindowTreeHostPlatformTest() {}
  ~DesktopWindowTreeHostPlatformTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DesktopWindowTreeHostPlatformTest);
};

TEST_F(DesktopWindowTreeHostPlatformTest, CallOnNativeWidgetDestroying) {
  std::unique_ptr<Widget> widget = CreateWidgetWithNativeWidget();

  WidgetDestroyingObserver observer(
      widget->native_widget_private()->GetWidget());
  widget->CloseNow();

  observer.Wait();
  EXPECT_TRUE(observer.widget_destroying());
}

}  // namespace views
