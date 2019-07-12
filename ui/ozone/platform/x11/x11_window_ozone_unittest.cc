// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_window_ozone.h"

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/events/event.h"
#include "ui/events/platform/x11/x11_event_source_libevent.h"
#include "ui/events/test/events_test_utils_x11.h"
#include "ui/ozone/platform/x11/x11_window_manager_ozone.h"
#include "ui/ozone/test/mock_platform_window_delegate.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {

namespace {

using ::testing::_;

constexpr int kPointerDeviceId = 1;

ACTION_P(StoreWidget, widget_ptr) {
  *widget_ptr = arg0;
}

ACTION_P(CloneEvent, event_ptr) {
  *event_ptr = Event::Clone(*arg0);
}

}  // namespace

class X11WindowOzoneTest : public testing::Test {
 public:
  X11WindowOzoneTest()
      : task_env_(std::make_unique<base::test::ScopedTaskEnvironment>(
            base::test::ScopedTaskEnvironment::MainThreadType::UI)) {}

  ~X11WindowOzoneTest() override = default;

  void SetUp() override {
    XDisplay* display = gfx::GetXDisplay();
    event_source_ = std::make_unique<X11EventSourceLibevent>(display);
    window_manager_ = std::make_unique<X11WindowManagerOzone>();

    TouchFactory::GetInstance()->SetPointerDeviceForTest({kPointerDeviceId});
  }

 protected:
  std::unique_ptr<PlatformWindow> CreatePlatformWindow(
      MockPlatformWindowDelegate* delegate,
      const gfx::Rect& bounds,
      gfx::AcceleratedWidget* widget) {
    EXPECT_CALL(*delegate, OnAcceleratedWidgetAvailable(_))
        .WillOnce(StoreWidget(widget));
    PlatformWindowInitProperties init_params(bounds);
    auto window = std::make_unique<X11WindowOzone>(delegate, init_params,
                                                   window_manager_.get());
    return std::move(window);
  }

  void DispatchXEvent(XEvent* event, gfx::AcceleratedWidget widget) {
    DCHECK_EQ(GenericEvent, event->type);
    XIDeviceEvent* device_event =
        static_cast<XIDeviceEvent*>(event->xcookie.data);
    device_event->event = widget;
    event_source_->ProcessXEvent(event);
  }

  X11WindowManagerOzone* window_manager() const {
    return window_manager_.get();
  }

 private:
  std::unique_ptr<base::test::ScopedTaskEnvironment> task_env_;
  std::unique_ptr<X11WindowManagerOzone> window_manager_;
  std::unique_ptr<X11EventSourceLibevent> event_source_;

  DISALLOW_COPY_AND_ASSIGN(X11WindowOzoneTest);
};

// This test ensures that events are handled by a right target(widget).
TEST_F(X11WindowOzoneTest, SendPlatformEventToRightTarget) {
  MockPlatformWindowDelegate delegate;
  gfx::AcceleratedWidget widget;
  constexpr gfx::Rect bounds(30, 80, 800, 600);
  auto window = CreatePlatformWindow(&delegate, bounds, &widget);

  ScopedXI2Event xi_event;
  xi_event.InitGenericButtonEvent(kPointerDeviceId, ET_MOUSE_PRESSED,
                                  gfx::Point(218, 290), EF_NONE);

  // First check events can be received by a target window.
  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate, DispatchEvent(_)).WillOnce(CloneEvent(&event));

  DispatchXEvent(xi_event, widget);
  EXPECT_EQ(ET_MOUSE_PRESSED, event->type());
  testing::Mock::VerifyAndClearExpectations(&delegate);

  MockPlatformWindowDelegate delegate_2;
  gfx::AcceleratedWidget widget_2;
  gfx::Rect bounds_2(525, 155, 296, 407);

  auto window_2 = CreatePlatformWindow(&delegate_2, bounds_2, &widget_2);

  // Check event goes to right target without capture being set.
  event.reset();
  EXPECT_CALL(delegate, DispatchEvent(_)).Times(0);
  EXPECT_CALL(delegate_2, DispatchEvent(_)).WillOnce(CloneEvent(&event));

  DispatchXEvent(xi_event, widget_2);
  EXPECT_EQ(ET_MOUSE_PRESSED, event->type());

  EXPECT_CALL(delegate, OnClosed()).Times(1);
  EXPECT_CALL(delegate_2, OnClosed()).Times(1);
}

// This test case ensures that events are consumed by a window with explicit
// capture, even though the event is sent to other window.
TEST_F(X11WindowOzoneTest, SendPlatformEventToCapturedWindow) {
  MockPlatformWindowDelegate delegate;
  gfx::AcceleratedWidget widget;
  constexpr gfx::Rect bounds(30, 80, 800, 600);
  EXPECT_CALL(delegate, OnClosed()).Times(1);
  auto window = CreatePlatformWindow(&delegate, bounds, &widget);

  MockPlatformWindowDelegate delegate_2;
  gfx::AcceleratedWidget widget_2;
  gfx::Rect bounds_2(525, 155, 296, 407);
  EXPECT_CALL(delegate_2, OnClosed()).Times(1);
  auto window_2 = CreatePlatformWindow(&delegate_2, bounds_2, &widget_2);

  ScopedXI2Event xi_event;
  xi_event.InitGenericButtonEvent(kPointerDeviceId, ET_MOUSE_PRESSED,
                                  gfx::Point(218, 290), EF_NONE);

  // Set capture to the second window, but send an event to another window
  // target. The event must have its location converted and received by the
  // captured window instead.
  window_2->SetCapture();
  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate, DispatchEvent(_)).Times(0);
  EXPECT_CALL(delegate_2, DispatchEvent(_)).WillOnce(CloneEvent(&event));

  DispatchXEvent(xi_event, widget);
  EXPECT_EQ(ET_MOUSE_PRESSED, event->type());
  EXPECT_EQ(gfx::Point(-277, 215), event->AsLocatedEvent()->location());
}

// This test case ensures window_manager properly provides X11WindowOzone
// instances as they are created/destroyed.
TEST_F(X11WindowOzoneTest, GetWindowFromAcceleratedWigets) {
  MockPlatformWindowDelegate delegate;
  gfx::Rect bounds(0, 0, 100, 100);
  gfx::AcceleratedWidget widget_1;
  auto window_1 = CreatePlatformWindow(&delegate, bounds, &widget_1);
  EXPECT_EQ(window_1.get(), window_manager()->GetWindow(widget_1));

  gfx::AcceleratedWidget widget_2;
  auto window_2 = CreatePlatformWindow(&delegate, bounds, &widget_2);
  EXPECT_EQ(window_2.get(), window_manager()->GetWindow(widget_2));
  EXPECT_EQ(window_1.get(), window_manager()->GetWindow(widget_1));

  window_1->Close();
  window_1.reset();
  EXPECT_EQ(nullptr, window_manager()->GetWindow(widget_1));
  EXPECT_EQ(window_2.get(), window_manager()->GetWindow(widget_2));

  window_2.reset();
  EXPECT_EQ(nullptr, window_manager()->GetWindow(widget_1));
  EXPECT_EQ(nullptr, window_manager()->GetWindow(widget_2));
}

}  // namespace ui
