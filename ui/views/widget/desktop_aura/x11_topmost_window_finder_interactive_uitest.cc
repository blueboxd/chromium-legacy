// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/x11/x11_topmost_window_finder.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "base/stl_util.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/x/test/x11_property_change_waiter.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/events/x/x11_window_event_manager.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/shape.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/x11_path.h"
#include "ui/platform_window/x11/x11_window.h"
#include "ui/platform_window/x11/x11_window_manager.h"

namespace views {

namespace {

// Waits until the |x11_window| is mapped and becomes viewable.
class X11VisibilityWaiter : public ui::XEventDispatcher {
 public:
  X11VisibilityWaiter() = default;
  X11VisibilityWaiter(const X11VisibilityWaiter&) = delete;
  X11VisibilityWaiter& operator=(const X11VisibilityWaiter&) = delete;
  ~X11VisibilityWaiter() override = default;

  void WaitUntilWindowIsVisible(x11::Window x11_window) {
    if (ui::IsWindowVisible(x11_window))
      return;

    auto events = std::make_unique<ui::XScopedEventSelector>(
        x11_window, StructureNotifyMask | SubstructureNotifyMask);
    x11_window_ = x11_window;

    dispatcher_ =
        ui::X11EventSource::GetInstance()->OverrideXEventDispatcher(this);

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  // XEventDispatcher:
  bool DispatchXEvent(x11::Event* event) override {
    auto* map = event->As<x11::MapNotifyEvent>();
    if (map && map->window == x11_window_) {
      if (!quit_closure_.is_null())
        std::move(quit_closure_).Run();
      dispatcher_.reset();
      return true;
    }
    return false;
  }

  x11::Window x11_window_;
  std::unique_ptr<ui::ScopedXEventDispatcher> dispatcher_;
  // Ends the run loop.
  base::OnceClosure quit_closure_;
};

class TestPlatformWindowDelegate : public ui::PlatformWindowDelegate {
 public:
  TestPlatformWindowDelegate() = default;
  TestPlatformWindowDelegate(const TestPlatformWindowDelegate&) = delete;
  TestPlatformWindowDelegate& operator=(const TestPlatformWindowDelegate&) =
      delete;
  ~TestPlatformWindowDelegate() override = default;

  ui::PlatformWindowState state() { return state_; }

  // PlatformWindowDelegate:
  void OnBoundsChanged(const gfx::Rect& new_bounds) override {}
  void OnDamageRect(const gfx::Rect& damaged_region) override {}
  void DispatchEvent(ui::Event* event) override {}
  void OnCloseRequest() override {}
  void OnClosed() override {}
  void OnWindowStateChanged(ui::PlatformWindowState new_state) override {
    state_ = new_state;
  }
  void OnLostCapture() override {}
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) override {
    widget_ = widget;
  }
  void OnWillDestroyAcceleratedWidget() override {}
  void OnAcceleratedWidgetDestroyed() override {
    widget_ = gfx::kNullAcceleratedWidget;
  }
  void OnActivationChanged(bool active) override {}
  void OnMouseEnter() override {}

 private:
  gfx::AcceleratedWidget widget_ = gfx::kNullAcceleratedWidget;
  ui::PlatformWindowState state_ = ui::PlatformWindowState::kUnknown;
};

// Waits till |window| is minimized.
class MinimizeWaiter : public ui::X11PropertyChangeWaiter {
 public:
  explicit MinimizeWaiter(x11::Window window)
      : ui::X11PropertyChangeWaiter(window, "_NET_WM_STATE") {}

  ~MinimizeWaiter() override = default;

 private:
  // ui::X11PropertyChangeWaiter:
  bool ShouldKeepOnWaiting(x11::Event* event) override {
    std::vector<x11::Atom> wm_states;
    if (ui::GetAtomArrayProperty(xwindow(), "_NET_WM_STATE", &wm_states)) {
      return !base::Contains(wm_states, gfx::GetAtom("_NET_WM_STATE_HIDDEN"));
    }
    return true;
  }

  DISALLOW_COPY_AND_ASSIGN(MinimizeWaiter);
};

// Waits till |_NET_CLIENT_LIST_STACKING| is updated to include
// |expected_windows|.
class StackingClientListWaiter : public ui::X11PropertyChangeWaiter {
 public:
  StackingClientListWaiter(x11::Window* expected_windows, size_t count)
      : ui::X11PropertyChangeWaiter(ui::GetX11RootWindow(),
                                    "_NET_CLIENT_LIST_STACKING"),
        expected_windows_(expected_windows, expected_windows + count) {}

  ~StackingClientListWaiter() override = default;

  // X11PropertyChangeWaiter:
  void Wait() override {
    // StackingClientListWaiter may be created after
    // _NET_CLIENT_LIST_STACKING already contains |expected_windows|.
    if (!ShouldKeepOnWaiting(nullptr))
      return;

    ui::X11PropertyChangeWaiter::Wait();
  }

 private:
  // ui::X11PropertyChangeWaiter:
  bool ShouldKeepOnWaiting(x11::Event* event) override {
    std::vector<x11::Window> stack;
    ui::GetXWindowStack(ui::GetX11RootWindow(), &stack);
    return !std::all_of(
        expected_windows_.cbegin(), expected_windows_.cend(),
        [&stack](x11::Window window) { return base::Contains(stack, window); });
  }

  std::vector<x11::Window> expected_windows_;

  DISALLOW_COPY_AND_ASSIGN(StackingClientListWaiter);
};

}  // namespace

class X11TopmostWindowFinderTest : public testing::Test {
 public:
  X11TopmostWindowFinderTest()
      : task_env_(base::test::TaskEnvironment::MainThreadType::UI) {}
  ~X11TopmostWindowFinderTest() override = default;

  // DesktopWidgetTestInteractive
  void SetUp() override {
    auto* connection = x11::Connection::Get();
    event_source_ = std::make_unique<ui::X11EventSource>(connection);

    // Make X11 synchronous for our display connection. This does not force the
    // window manager to behave synchronously.
    XSynchronize(xdisplay(), x11::True);
  }

  void TearDown() override {
    XSynchronize(xdisplay(), x11::False);
  }

  // Creates and shows an X11Window with |bounds|. The caller takes ownership of
  // the returned window.
  std::unique_ptr<ui::X11Window> CreateAndShowX11Window(
      ui::PlatformWindowDelegate* delegate,
      const gfx::Rect& bounds) {
    ui::PlatformWindowInitProperties init_params(bounds);
    init_params.remove_standard_frame = true;
    auto window = std::make_unique<ui::X11Window>(delegate);
    window->Initialize(std::move(init_params));
    window->Show(false);

    // Wait until the window becomes visible so that window finder doesn't skip
    // these windows (it's required to wait because mapping and searching for
    // toplevel window is a subject to races).
    X11VisibilityWaiter waiter;
    waiter.WaitUntilWindowIsVisible(
        static_cast<x11::Window>(window->GetWidget()));
    return window;
  }

  // Creates and shows an X window with |bounds|.
  x11::Window CreateAndShowXWindow(const gfx::Rect& bounds) {
    x11::Window root = ui::GetX11RootWindow();
    x11::Window window = static_cast<x11::Window>(
        XCreateSimpleWindow(xdisplay(), static_cast<uint32_t>(root), 0, 0, 1, 1,
                            0,    // border_width
                            0,    // border
                            0));  // background

    ui::SetUseOSWindowFrame(window, false);
    ShowAndSetXWindowBounds(window, bounds);
    // Wait until the window becomes visible so that window finder doesn't skip
    // these windows (it's required to wait because mapping and searching for
    // toplevel window is a subject to races).
    X11VisibilityWaiter waiter;
    waiter.WaitUntilWindowIsVisible(static_cast<x11::Window>(window));
    return window;
  }

  // Shows |window| and sets its bounds.
  void ShowAndSetXWindowBounds(x11::Window window, const gfx::Rect& bounds) {
    XMapWindow(xdisplay(), static_cast<uint32_t>(window));

    XWindowChanges changes = {0};
    changes.x = bounds.x();
    changes.y = bounds.y();
    changes.width = bounds.width();
    changes.height = bounds.height();
    XConfigureWindow(xdisplay(), static_cast<uint32_t>(window),
                     CWX | CWY | CWWidth | CWHeight, &changes);
  }

  Display* xdisplay() { return gfx::GetXDisplay(); }

  // Returns the topmost X window at the passed in screen position.
  x11::Window FindTopmostXWindowAt(int screen_x, int screen_y) {
    ui::X11TopmostWindowFinder finder;
    return finder.FindWindowAt(gfx::Point(screen_x, screen_y));
  }

  // Returns the topmost ui::X11Window at the passed in screen position. Returns
  // nullptr if the topmost window does not have an associated ui::X11Window.
  ui::X11Window* FindTopmostLocalProcessWindowAt(int screen_x, int screen_y) {
    ui::X11TopmostWindowFinder finder;
    auto x11_window =
        finder.FindLocalProcessWindowAt(gfx::Point(screen_x, screen_y), {});
    return x11_window == x11::Window::None
               ? nullptr
               : ui::X11WindowManager::GetInstance()->GetWindow(
                     static_cast<gfx::AcceleratedWidget>(x11_window));
  }

  // Returns the topmost ui::X11Window at the passed in screen position ignoring
  // |ignore_window|. Returns nullptr if the topmost window does not have an
  // associated ui::X11Window.
  ui::X11Window* FindTopmostLocalProcessWindowWithIgnore(
      int screen_x,
      int screen_y,
      x11::Window ignore_window) {
    std::set<gfx::AcceleratedWidget> ignore;
    ignore.insert(static_cast<gfx::AcceleratedWidget>(ignore_window));
    ui::X11TopmostWindowFinder finder;
    auto x11_window =
        finder.FindLocalProcessWindowAt(gfx::Point(screen_x, screen_y), ignore);
    return x11_window == x11::Window::None
               ? nullptr
               : ui::X11WindowManager::GetInstance()->GetWindow(
                     static_cast<gfx::AcceleratedWidget>(x11_window));
  }

 private:
  base::test::TaskEnvironment task_env_;
  std::unique_ptr<ui::X11EventSource> event_source_;

  DISALLOW_COPY_AND_ASSIGN(X11TopmostWindowFinderTest);
};

TEST_F(X11TopmostWindowFinderTest, Basic) {
  // Avoid positioning test windows at 0x0 because window managers often have a
  // panel/launcher along one of the screen edges and do not allow windows to
  // position themselves to overlap the panel/launcher.
  TestPlatformWindowDelegate delegate;
  auto window1 = CreateAndShowX11Window(&delegate, {100, 100, 200, 100});
  auto x11_window1 = static_cast<x11::Window>(window1->GetWidget());

  x11::Window x11_window2 = CreateAndShowXWindow(gfx::Rect(200, 100, 100, 200));

  TestPlatformWindowDelegate delegate2;
  auto window3 = CreateAndShowX11Window(&delegate2, {100, 190, 200, 110});
  window3->Show(false);
  auto x11_window3 = static_cast<x11::Window>(window3->GetWidget());

  x11::Window windows[] = {x11_window1, x11_window2, x11_window3};
  StackingClientListWaiter waiter(windows, base::size(windows));
  waiter.Wait();
  ui::X11EventSource::GetInstance()->DispatchXEvents();

  EXPECT_EQ(x11_window1, FindTopmostXWindowAt(150, 150));
  EXPECT_EQ(window1.get(), FindTopmostLocalProcessWindowAt(150, 150));

  EXPECT_EQ(x11_window2, FindTopmostXWindowAt(250, 150));
  EXPECT_FALSE(FindTopmostLocalProcessWindowAt(250, 150));

  EXPECT_EQ(x11_window3, FindTopmostXWindowAt(250, 250));
  EXPECT_EQ(window3.get(), FindTopmostLocalProcessWindowAt(250, 250));

  EXPECT_EQ(x11_window3, FindTopmostXWindowAt(150, 250));
  EXPECT_EQ(window3.get(), FindTopmostLocalProcessWindowAt(150, 250));

  EXPECT_EQ(x11_window3, FindTopmostXWindowAt(150, 195));
  EXPECT_EQ(window3.get(), FindTopmostLocalProcessWindowAt(150, 195));

  EXPECT_NE(x11_window1, FindTopmostXWindowAt(1000, 1000));
  EXPECT_NE(x11_window2, FindTopmostXWindowAt(1000, 1000));
  EXPECT_NE(x11_window3, FindTopmostXWindowAt(1000, 1000));
  EXPECT_FALSE(FindTopmostLocalProcessWindowAt(1000, 1000));

  EXPECT_EQ(window1.get(),
            FindTopmostLocalProcessWindowWithIgnore(150, 150, x11_window3));
  EXPECT_FALSE(FindTopmostLocalProcessWindowWithIgnore(250, 250, x11_window3));
  EXPECT_FALSE(FindTopmostLocalProcessWindowWithIgnore(150, 250, x11_window3));
  EXPECT_EQ(window1.get(),
            FindTopmostLocalProcessWindowWithIgnore(150, 195, x11_window3));

  XDestroyWindow(xdisplay(), static_cast<uint32_t>(x11_window2));
}

// Test that the minimized state is properly handled.
TEST_F(X11TopmostWindowFinderTest, Minimized) {
  TestPlatformWindowDelegate delegate;
  auto window1 = CreateAndShowX11Window(&delegate, {100, 100, 100, 100});
  auto x11_window1 = static_cast<x11::Window>(window1->GetWidget());

  x11::Window x11_window2 = CreateAndShowXWindow(gfx::Rect(300, 100, 100, 100));

  x11::Window windows[] = {x11_window1, x11_window2};
  StackingClientListWaiter stack_waiter(windows, base::size(windows));
  stack_waiter.Wait();
  ui::X11EventSource::GetInstance()->DispatchXEvents();

  EXPECT_EQ(x11_window1, FindTopmostXWindowAt(150, 150));
  {
    MinimizeWaiter minimize_waiter(x11_window1);
    XIconifyWindow(xdisplay(), static_cast<uint32_t>(x11_window1), 0);
    minimize_waiter.Wait();
  }
  EXPECT_NE(x11_window1, FindTopmostXWindowAt(150, 150));
  EXPECT_NE(x11_window2, FindTopmostXWindowAt(150, 150));

  // Repeat test for an X window which does not belong to a views::Widget
  // because the code path is different.
  EXPECT_EQ(x11_window2, FindTopmostXWindowAt(350, 150));
  {
    MinimizeWaiter minimize_waiter(x11_window2);
    XIconifyWindow(xdisplay(), static_cast<uint32_t>(x11_window2), 0);
    minimize_waiter.Wait();
  }
  EXPECT_NE(x11_window1, FindTopmostXWindowAt(350, 150));
  EXPECT_NE(x11_window2, FindTopmostXWindowAt(350, 150));

  XDestroyWindow(xdisplay(), static_cast<uint32_t>(x11_window2));
}

// Test that non-rectangular windows are properly handled.
TEST_F(X11TopmostWindowFinderTest, NonRectangular) {
  if (!ui::IsShapeExtensionAvailable())
    return;

  TestPlatformWindowDelegate delegate;
  auto x11_window1 = CreateAndShowX11Window(&delegate, {100, 100, 100, 100});
  auto window1 = static_cast<x11::Window>(x11_window1->GetWidget());

  auto shape1 = std::make_unique<std::vector<gfx::Rect>>();
  shape1->emplace_back(0, 10, 10, 90);
  shape1->emplace_back(10, 0, 90, 100);
  gfx::Transform transform;
  transform.Scale(1.0f, 1.0f);
  x11_window1->SetShape(std::move(shape1), transform);

  SkRegion skregion2;
  skregion2.op(SkIRect::MakeXYWH(0, 10, 10, 90), SkRegion::kUnion_Op);
  skregion2.op(SkIRect::MakeXYWH(10, 0, 90, 100), SkRegion::kUnion_Op);
  x11::Window window2 = CreateAndShowXWindow(gfx::Rect(300, 100, 100, 100));
  auto region2 = gfx::CreateRegionFromSkRegion(skregion2);
  x11::Connection::Get()->shape().Rectangles({
      .operation = x11::Shape::So::Set,
      .destination_kind = x11::Shape::Sk::Bounding,
      .ordering = x11::ClipOrdering::YXBanded,
      .destination_window = window2,
      .rectangles = *region2,
  });
  x11::Window windows[] = {window1, window2};
  StackingClientListWaiter stack_waiter(windows, base::size(windows));
  stack_waiter.Wait();
  ui::X11EventSource::GetInstance()->DispatchXEvents();

  EXPECT_EQ(window1, FindTopmostXWindowAt(105, 120));
  EXPECT_NE(window1, FindTopmostXWindowAt(105, 105));
  EXPECT_NE(window2, FindTopmostXWindowAt(105, 105));

  // Repeat test for an X window which does not belong to a views::Widget
  // because the code path is different.
  EXPECT_EQ(window2, FindTopmostXWindowAt(305, 120));
  EXPECT_NE(window1, FindTopmostXWindowAt(305, 105));
  EXPECT_NE(window2, FindTopmostXWindowAt(305, 105));

  XDestroyWindow(xdisplay(), static_cast<uint32_t>(window2));
}

// Test that a window with an empty shape are properly handled.
TEST_F(X11TopmostWindowFinderTest, NonRectangularEmptyShape) {
  if (!ui::IsShapeExtensionAvailable())
    return;

  TestPlatformWindowDelegate delegate;
  auto x11_window1 = CreateAndShowX11Window(&delegate, {100, 100, 100, 100});
  auto window1 = static_cast<x11::Window>(x11_window1->GetWidget());

  auto shape1 = std::make_unique<std::vector<gfx::Rect>>();
  shape1->emplace_back();
  gfx::Transform transform;
  transform.Scale(1.0f, 1.0f);
  x11_window1->SetShape(std::move(shape1), transform);

  x11::Window windows[] = {window1};
  StackingClientListWaiter stack_waiter(windows, base::size(windows));
  stack_waiter.Wait();
  ui::X11EventSource::GetInstance()->DispatchXEvents();

  EXPECT_NE(window1, FindTopmostXWindowAt(105, 105));
}

// Test that setting a Null shape removes the shape.
TEST_F(X11TopmostWindowFinderTest, NonRectangularNullShape) {
  if (!ui::IsShapeExtensionAvailable())
    return;

  TestPlatformWindowDelegate delegate;
  auto x11_window1 = CreateAndShowX11Window(&delegate, {100, 100, 100, 100});
  auto window1 = static_cast<x11::Window>(x11_window1->GetWidget());

  auto shape1 = std::make_unique<std::vector<gfx::Rect>>();
  shape1->emplace_back();
  gfx::Transform transform;
  transform.Scale(1.0f, 1.0f);
  x11_window1->SetShape(std::move(shape1), transform);

  // Remove the shape - this is now just a normal window.
  x11_window1->SetShape(nullptr, transform);

  x11::Window windows[] = {window1};
  StackingClientListWaiter stack_waiter(windows, base::size(windows));
  stack_waiter.Wait();
  ui::X11EventSource::GetInstance()->DispatchXEvents();

  EXPECT_EQ(window1, FindTopmostXWindowAt(105, 105));
}

// Test that the TopmostWindowFinder finds windows which belong to menus
// (which may or may not belong to Chrome).
//
// Flakes (https://crbug.com/955316)
TEST_F(X11TopmostWindowFinderTest, DISABLED_Menu) {
  x11::Window window = CreateAndShowXWindow(gfx::Rect(100, 100, 100, 100));

  x11::Window root = ui::GetX11RootWindow();
  XSetWindowAttributes swa;
  swa.override_redirect = x11::True;
  x11::Window menu_window = static_cast<x11::Window>(XCreateWindow(
      xdisplay(), static_cast<uint32_t>(root), 0, 0, 1, 1,
      0,                                                   // border width
      static_cast<int>(x11::WindowClass::CopyFromParent),  // depth
      static_cast<int>(x11::WindowClass::InputOutput),
      nullptr,  // visual
      CWOverrideRedirect, &swa));
  {
    ui::SetAtomProperty(menu_window, "_NET_WM_WINDOW_TYPE", "ATOM",
                        gfx::GetAtom("_NET_WM_WINDOW_TYPE_MENU"));
  }
  ui::SetUseOSWindowFrame(menu_window, false);
  ShowAndSetXWindowBounds(menu_window, gfx::Rect(140, 110, 100, 100));
  ui::X11EventSource::GetInstance()->DispatchXEvents();

  // |menu_window| is never added to _NET_CLIENT_LIST_STACKING.
  x11::Window windows[] = {window};
  StackingClientListWaiter stack_waiter(windows, base::size(windows));
  stack_waiter.Wait();

  EXPECT_EQ(window, FindTopmostXWindowAt(110, 110));
  EXPECT_EQ(menu_window, FindTopmostXWindowAt(150, 120));
  EXPECT_EQ(menu_window, FindTopmostXWindowAt(210, 120));

  XDestroyWindow(xdisplay(), static_cast<uint32_t>(window));
  XDestroyWindow(xdisplay(), static_cast<uint32_t>(menu_window));
}

}  // namespace views
