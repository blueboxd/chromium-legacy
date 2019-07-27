// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_VIEWS_TEST_BASE_H_
#define UI_VIEWS_TEST_VIEWS_TEST_BASE_H_

#include <memory>

#include "base/macros.h"
#include "base/test/scoped_task_environment.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/test/scoped_views_test_helper.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "ui/base/win/scoped_ole_initializer.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/test/aura_test_helper.h"
#endif

namespace views {

// A base class for views unit test. It creates a message loop necessary
// to drive UI events and takes care of OLE initialization for windows.
class ViewsTestBase : public PlatformTest {
 public:
  using ScopedTaskEnvironment = base::test::ScopedTaskEnvironment;

  enum class NativeWidgetType {
    // On Aura, corresponds to NativeWidgetAura.
    kDefault,
    // On chromeos, corresponds to NativeWidgetAura (DesktopNativeWidgetAura
    // is not used on ChromeOS).
    kDesktop,
  };

  ViewsTestBase();
  ~ViewsTestBase() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  // This copies some of the setup done in ViewsTestSuite, so it's only
  // necessary for a ViewsTestBase that runs out of that test suite, such as in
  // interactive ui tests.
  void SetUpForInteractiveTests();

  void RunPendingMessages();

  // Creates a widget of |type| with any platform specific data for use in
  // cross-platform tests.
  virtual Widget::InitParams CreateParams(Widget::InitParams::Type type);

  bool HasCompositingManager() const;

  // Simulate an OS-level destruction of the native window held by |widget|.
  void SimulateNativeDestroy(Widget* widget);

 protected:
  TestViewsDelegate* test_views_delegate() const {
    return test_helper_->test_views_delegate();
  }

  void set_native_widget_type(NativeWidgetType native_widget_type) {
    DCHECK(!setup_called_);
    native_widget_type_ = native_widget_type;
  }

  void set_scoped_task_environment(
      std::unique_ptr<ScopedTaskEnvironment> scoped_task_environment) {
    DCHECK(!setup_called_);
    scoped_task_environment_ = std::move(scoped_task_environment);
  }

  ScopedTaskEnvironment* scoped_task_environment() {
    return scoped_task_environment_.get();
  }

  void set_views_delegate(std::unique_ptr<TestViewsDelegate> views_delegate) {
    DCHECK(!setup_called_);
    views_delegate_for_setup_.swap(views_delegate);
  }

#if defined(USE_AURA)
  aura::Window* root_window() {
    return aura::test::AuraTestHelper::GetInstance()->root_window();
  }

  ui::EventSink* event_sink() {
    return aura::test::AuraTestHelper::GetInstance()->event_sink();
  }

  aura::WindowTreeHost* host() {
    return aura::test::AuraTestHelper::GetInstance()->host();
  }
#endif

  // Returns a context view. In aura builds, this will be the
  // RootWindow. Everywhere else, NULL.
  gfx::NativeWindow GetContext();

  // Factory for creating the native widget when |native_widget_type_| is set to
  // kDesktop.
  NativeWidget* CreateNativeWidgetForTest(
      const Widget::InitParams& init_params,
      internal::NativeWidgetDelegate* delegate);

 private:
  // Controls what type of widget will be created by default for a test (i.e.
  // when creating a Widget and leaving InitParams::native_widget unspecified).
  // kDefault will allow the system default to be used (typically
  // NativeWidgetAura on Aura). kDesktop forces DesktopNativeWidgetAura on Aura.
  // There are exceptions, such as for modal dialog widgets, for which this
  // value is ignored.
  NativeWidgetType native_widget_type_ = NativeWidgetType::kDefault;

  std::unique_ptr<ScopedTaskEnvironment> scoped_task_environment_;
  std::unique_ptr<TestViewsDelegate> views_delegate_for_setup_;
  std::unique_ptr<ScopedViewsTestHelper> test_helper_;
  bool interactive_setup_called_ = false;
  bool setup_called_ = false;
  bool teardown_called_ = false;
  bool has_compositing_manager_ = false;

#if defined(OS_WIN)
  ui::ScopedOleInitializer ole_initializer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ViewsTestBase);
};

class ViewsTestBaseWithNativeWidgetType
    : public ViewsTestBase,
      public testing::WithParamInterface<ViewsTestBase::NativeWidgetType> {
 public:
  ViewsTestBaseWithNativeWidgetType() = default;
  ~ViewsTestBaseWithNativeWidgetType() override = default;

  // ViewsTestBase:
  void SetUp() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ViewsTestBaseWithNativeWidgetType);
};

// A helper that makes it easier to declare basic views tests that want to test
// desktop native widgets. See |ViewsTestBase::native_wiget_type_| and
// |ViewsTestBase::CreateNativeWidgetForTest|. In short, for Aura, this will
// result in most Widgets automatically being backed by a
// DesktopNativeWidgetAura. For Mac, it has no impact as a NativeWidgetMac is
// used either way.
class ViewsTestWithDesktopNativeWidget : public ViewsTestBase {
 public:
  ViewsTestWithDesktopNativeWidget() = default;
  ~ViewsTestWithDesktopNativeWidget() override = default;

  // ViewsTestBase:
  void SetUp() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ViewsTestWithDesktopNativeWidget);
};

}  // namespace views

#endif  // UI_VIEWS_TEST_VIEWS_TEST_BASE_H_
