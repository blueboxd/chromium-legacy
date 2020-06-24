// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_aurax11.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/x/x11_move_loop.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/gfx/x/xproto.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/desktop_aura/desktop_native_cursor_manager.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/widget.h"

// TODO(crbug.com/990756): Transfer all tests from this file to better places
// when DDDClientAuraX11 goes away.

namespace views {

namespace {

class TestDragDropClient;

// Collects messages which would otherwise be sent to |window_| via
// SendXClientEvent().
class ClientMessageEventCollector {
 public:
  ClientMessageEventCollector(x11::Window window, TestDragDropClient* client);
  virtual ~ClientMessageEventCollector();

  // Returns true if |events_| is non-empty.
  bool HasEvents() const { return !events_.empty(); }

  // Pops all of |events_| and returns the popped events in the order that they
  // were on the stack
  std::vector<x11::ClientMessageEvent> PopAllEvents();

  // Adds |event| to the stack.
  void RecordEvent(const x11::ClientMessageEvent& event);

 private:
  x11::Window window_;

  // Not owned.
  TestDragDropClient* client_;

  std::vector<x11::ClientMessageEvent> events_;

  DISALLOW_COPY_AND_ASSIGN(ClientMessageEventCollector);
};

// An implementation of ui::X11MoveLoop where RunMoveLoop() always starts the
// move loop.
class TestMoveLoop : public ui::X11MoveLoop {
 public:
  explicit TestMoveLoop(ui::X11MoveLoopDelegate* delegate);
  ~TestMoveLoop() override;

  // Returns true if the move loop is running.
  bool IsRunning() const;

  // ui::X11MoveLoop:
  bool RunMoveLoop(bool can_grab_pointer,
                   ::Cursor old_cursor,
                   ::Cursor new_cursor) override;
  void UpdateCursor(::Cursor cursor) override;
  void EndMoveLoop() override;

 private:
  // Not owned.
  ui::X11MoveLoopDelegate* delegate_;

  // Ends the move loop.
  base::OnceClosure quit_closure_;

  bool is_running_ = false;
};

// Implementation of DesktopDragDropClientAuraX11 which short circuits
// FindWindowFor().
class SimpleTestDragDropClient : public DesktopDragDropClientAuraX11 {
 public:
  SimpleTestDragDropClient(aura::Window*,
                           DesktopNativeCursorManager* cursor_manager);
  ~SimpleTestDragDropClient() override;

  // Sets |window| as the topmost window for all mouse positions.
  void SetTopmostXWindow(x11::Window window);

  // Returns true if the move loop is running.
  bool IsMoveLoopRunning();

  Widget* drag_widget() { return DesktopDragDropClientAuraX11::drag_widget(); }

 private:
  // DesktopDragDropClientAuraX11:
  std::unique_ptr<ui::X11MoveLoop> CreateMoveLoop(
      ui::X11MoveLoopDelegate* delegate) override;
  x11::Window FindWindowFor(const gfx::Point& screen_point) override;

  // The x11::Window of the window which is simulated to be the topmost window.
  x11::Window target_window_ = x11::Window::None;

  // The move loop. Not owned.
  TestMoveLoop* loop_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SimpleTestDragDropClient);
};

// Implementation of DesktopDragDropClientAuraX11 which works with a fake
// |DesktopDragDropClientAuraX11::source_current_window_|.
class TestDragDropClient : public SimpleTestDragDropClient {
 public:
  // The location in screen coordinates used for the synthetic mouse moves
  // generated in SetTopmostXWindowAndMoveMouse().
  static constexpr int kMouseMoveX = 100;
  static constexpr int kMouseMoveY = 200;

  TestDragDropClient(aura::Window* window,
                     DesktopNativeCursorManager* cursor_manager);
  ~TestDragDropClient() override;

  // Returns the x11::Window of the window which initiated the drag.
  x11::Window source_xwindow() { return source_window_; }

  // Returns the Atom with |name|.
  x11::Atom GetAtom(const char* name);

  // Returns true if the event's message has |type|.
  bool MessageHasType(const x11::ClientMessageEvent& event, const char* type);

  // Sets |collector| to collect x11::ClientMessageEvents which would otherwise
  // have been sent to the drop target window.
  void SetEventCollectorFor(x11::Window window,
                            ClientMessageEventCollector* collector);

  // Builds an XdndStatus message and sends it to
  // DesktopDragDropClientAuraX11.
  void OnStatus(x11::Window target_window,
                bool will_accept_drop,
                x11::Atom accepted_action);

  // Builds an XdndFinished message and sends it to
  // DesktopDragDropClientAuraX11.
  void OnFinished(x11::Window target_window,
                  bool accepted_drop,
                  x11::Atom performed_action);

  // Sets |window| as the topmost window at the current mouse position and
  // generates a synthetic mouse move.
  void SetTopmostXWindowAndMoveMouse(x11::Window window);

 private:
  // DesktopDragDropClientAuraX11:
  void SendXClientEvent(x11::Window window,
                        const x11::ClientMessageEvent& event) override;

  // The x11::Window of the window which initiated the drag.
  x11::Window source_window_;

  // Map of x11::Windows to the collector which intercepts
  // x11::ClientMessageEvents for that window.
  std::map<x11::Window, ClientMessageEventCollector*> collectors_;

  DISALLOW_COPY_AND_ASSIGN(TestDragDropClient);
};

///////////////////////////////////////////////////////////////////////////////
// ClientMessageEventCollector

ClientMessageEventCollector::ClientMessageEventCollector(
    x11::Window window,
    TestDragDropClient* client)
    : window_(window), client_(client) {
  client->SetEventCollectorFor(window, this);
}

ClientMessageEventCollector::~ClientMessageEventCollector() {
  client_->SetEventCollectorFor(window_, nullptr);
}

std::vector<x11::ClientMessageEvent>
ClientMessageEventCollector::PopAllEvents() {
  std::vector<x11::ClientMessageEvent> to_return;
  to_return.swap(events_);
  return to_return;
}

void ClientMessageEventCollector::RecordEvent(
    const x11::ClientMessageEvent& event) {
  events_.push_back(event);
}

///////////////////////////////////////////////////////////////////////////////
// TestMoveLoop

TestMoveLoop::TestMoveLoop(ui::X11MoveLoopDelegate* delegate)
    : delegate_(delegate) {}

TestMoveLoop::~TestMoveLoop() = default;

bool TestMoveLoop::IsRunning() const {
  return is_running_;
}

bool TestMoveLoop::RunMoveLoop(bool can_grab_pointer,
                               ::Cursor old_cursor,
                               ::Cursor new_cursor) {
  is_running_ = true;
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
  return true;
}

void TestMoveLoop::UpdateCursor(::Cursor cursor) {}

void TestMoveLoop::EndMoveLoop() {
  if (is_running_) {
    delegate_->OnMoveLoopEnded();
    is_running_ = false;
    std::move(quit_closure_).Run();
  }
}

///////////////////////////////////////////////////////////////////////////////
// SimpleTestDragDropClient

SimpleTestDragDropClient::SimpleTestDragDropClient(
    aura::Window* window,
    DesktopNativeCursorManager* cursor_manager)
    : DesktopDragDropClientAuraX11(window,
                                   cursor_manager,
                                   window->GetHost()->GetAcceleratedWidget()) {}

SimpleTestDragDropClient::~SimpleTestDragDropClient() = default;

void SimpleTestDragDropClient::SetTopmostXWindow(x11::Window window) {
  target_window_ = window;
}

bool SimpleTestDragDropClient::IsMoveLoopRunning() {
  return loop_->IsRunning();
}

std::unique_ptr<ui::X11MoveLoop> SimpleTestDragDropClient::CreateMoveLoop(
    ui::X11MoveLoopDelegate* delegate) {
  loop_ = new TestMoveLoop(delegate);
  return base::WrapUnique(loop_);
}

x11::Window SimpleTestDragDropClient::FindWindowFor(
    const gfx::Point& screen_point) {
  return target_window_;
}

///////////////////////////////////////////////////////////////////////////////
// TestDragDropClient

TestDragDropClient::TestDragDropClient(
    aura::Window* window,
    DesktopNativeCursorManager* cursor_manager)
    : SimpleTestDragDropClient(window, cursor_manager),
      source_window_(window->GetHost()->GetAcceleratedWidget()) {}

TestDragDropClient::~TestDragDropClient() = default;

x11::Atom TestDragDropClient::GetAtom(const char* name) {
  return gfx::GetAtom(name);
}

bool TestDragDropClient::MessageHasType(const x11::ClientMessageEvent& event,
                                        const char* type) {
  return event.type == GetAtom(type);
}

void TestDragDropClient::SetEventCollectorFor(
    x11::Window window,
    ClientMessageEventCollector* collector) {
  if (collector)
    collectors_[window] = collector;
  else
    collectors_.erase(window);
}

void TestDragDropClient::OnStatus(x11::Window target_window,
                                  bool will_accept_drop,
                                  x11::Atom accepted_action) {
  x11::ClientMessageEvent event;
  event.type = GetAtom("XdndStatus");
  event.format = 32;
  event.window = source_window_;
  event.data.data32[0] = static_cast<uint32_t>(target_window);
  event.data.data32[1] = will_accept_drop ? 1 : 0;
  event.data.data32[2] = 0;
  event.data.data32[3] = 0;
  event.data.data32[4] = static_cast<uint32_t>(accepted_action);
  HandleXdndEvent(event);
}

void TestDragDropClient::OnFinished(x11::Window target_window,
                                    bool accepted_drop,
                                    x11::Atom performed_action) {
  x11::ClientMessageEvent event;
  event.type = GetAtom("XdndFinished");
  event.format = 32;
  event.window = source_window_;
  event.data.data32[0] = static_cast<uint32_t>(target_window);
  event.data.data32[1] = accepted_drop ? 1 : 0;
  event.data.data32[2] = static_cast<uint32_t>(performed_action);
  event.data.data32[3] = 0;
  event.data.data32[4] = 0;
  HandleXdndEvent(event);
}

void TestDragDropClient::SetTopmostXWindowAndMoveMouse(x11::Window window) {
  SetTopmostXWindow(window);
  OnMouseMovement(gfx::Point(kMouseMoveX, kMouseMoveY), ui::EF_NONE,
                  ui::EventTimeForNow());
}

void TestDragDropClient::SendXClientEvent(
    x11::Window window,
    const x11::ClientMessageEvent& event) {
  auto it = collectors_.find(window);
  if (it != collectors_.end())
    it->second->RecordEvent(event);
}

}  // namespace

class DesktopDragDropClientAuraX11Test : public ViewsTestBase {
 public:
  DesktopDragDropClientAuraX11Test() = default;
  ~DesktopDragDropClientAuraX11Test() override = default;

  int StartDragAndDrop() {
    auto data(std::make_unique<ui::OSExchangeData>());
    data->SetString(base::ASCIIToUTF16("Test"));
    SkBitmap drag_bitmap;
    drag_bitmap.allocN32Pixels(10, 10);
    drag_bitmap.eraseARGB(0xFF, 0, 0, 0);
    gfx::ImageSkia drag_image(gfx::ImageSkia::CreateFrom1xBitmap(drag_bitmap));
    data->provider().SetDragImage(drag_image, gfx::Vector2d());

    return client_->StartDragAndDrop(
        std::move(data), widget_->GetNativeWindow()->GetRootWindow(),
        widget_->GetNativeWindow(), gfx::Point(), ui::DragDropTypes::DRAG_COPY,
        ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE);
  }

  // ViewsTestBase:
  void SetUp() override {
    set_native_widget_type(NativeWidgetType::kDesktop);

    ViewsTestBase::SetUp();

    // Create widget to initiate the drags.
    widget_ = std::make_unique<Widget>();
    Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(100, 100);
    widget_->Init(std::move(params));
    widget_->Show();

    cursor_manager_ = std::make_unique<DesktopNativeCursorManager>();

    client_ = std::make_unique<TestDragDropClient>(widget_->GetNativeWindow(),
                                                   cursor_manager_.get());
    client_->Init();
  }

  void TearDown() override {
    client_.reset();
    cursor_manager_.reset();
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  TestDragDropClient* client() { return client_.get(); }

 private:
  std::unique_ptr<TestDragDropClient> client_;
  std::unique_ptr<DesktopNativeCursorManager> cursor_manager_;

  // The widget used to initiate drags.
  std::unique_ptr<Widget> widget_;

  DISALLOW_COPY_AND_ASSIGN(DesktopDragDropClientAuraX11Test);
};

void HighDPIStep(TestDragDropClient* client) {
  float scale =
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor();
  // Start dragging at 100, 100 in native coordinates.
  gfx::Point mouse_position_in_screen_pixel(100, 100);
  client->OnMouseMovement(mouse_position_in_screen_pixel, 0,
                          ui::EventTimeForNow());

  EXPECT_EQ(gfx::ScaleToFlooredPoint(gfx::Point(100, 100), 1.f / scale),
            client->drag_widget()->GetWindowBoundsInScreen().origin());

  // Drag the mouse down 200 pixels.
  mouse_position_in_screen_pixel.Offset(200, 0);
  client->OnMouseMovement(mouse_position_in_screen_pixel, 0,
                          ui::EventTimeForNow());
  EXPECT_EQ(gfx::ScaleToFlooredPoint(gfx::Point(300, 100), 1.f / scale),
            client->drag_widget()->GetWindowBoundsInScreen().origin());

  client->OnMouseReleased();
}

// TODO(crbug.com/990756): Turn this into tests of DesktopDragDropClientOzone
// or its equivalent.
TEST_F(DesktopDragDropClientAuraX11Test, HighDPI200) {
  aura::TestScreen* screen =
      static_cast<aura::TestScreen*>(display::Screen::GetScreen());
  screen->SetDeviceScaleFactor(2.0f);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&HighDPIStep, client()));
  int result = StartDragAndDrop();
  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE, result);
}

// TODO(crbug.com/990756): Turn this into tests of DesktopDragDropClientOzone
// or its equivalent.
TEST_F(DesktopDragDropClientAuraX11Test, HighDPI150) {
  aura::TestScreen* screen =
      static_cast<aura::TestScreen*>(display::Screen::GetScreen());
  screen->SetDeviceScaleFactor(1.5f);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&HighDPIStep, client()));
  int result = StartDragAndDrop();
  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE, result);
}

namespace {

// DragDropDelegate which counts the number of each type of drag-drop event and
// keeps track of the most recent drag-drop event.
class TestDragDropDelegate : public aura::client::DragDropDelegate {
 public:
  TestDragDropDelegate() = default;
  ~TestDragDropDelegate() override = default;

  int num_enters() const { return num_enters_; }
  int num_updates() const { return num_updates_; }
  int num_exits() const { return num_exits_; }
  int num_drops() const { return num_drops_; }
  gfx::Point last_event_mouse_position() const {
    return last_event_mouse_position_;
  }
  int last_event_flags() const { return last_event_flags_; }

 private:
  // aura::client::DragDropDelegate:
  void OnDragEntered(const ui::DropTargetEvent& event) override {
    ++num_enters_;
    last_event_mouse_position_ = event.location();
    last_event_flags_ = event.flags();
  }

  int OnDragUpdated(const ui::DropTargetEvent& event) override {
    ++num_updates_;
    last_event_mouse_position_ = event.location();
    last_event_flags_ = event.flags();
    return ui::DragDropTypes::DRAG_COPY;
  }

  void OnDragExited() override { ++num_exits_; }

  int OnPerformDrop(const ui::DropTargetEvent& event,
                    std::unique_ptr<OSExchangeData> data) override {
    ++num_drops_;
    last_event_mouse_position_ = event.location();
    last_event_flags_ = event.flags();
    return ui::DragDropTypes::DRAG_COPY;
  }

  int num_enters_ = 0;
  int num_updates_ = 0;
  int num_exits_ = 0;
  int num_drops_ = 0;

  gfx::Point last_event_mouse_position_;
  int last_event_flags_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestDragDropDelegate);
};

}  // namespace

// Test harness for tests where the drag and drop source and target are both
// Chrome windows.
class DesktopDragDropClientAuraX11ChromeSourceTargetTest
    : public ViewsTestBase {
 public:
  DesktopDragDropClientAuraX11ChromeSourceTargetTest() = default;

  ~DesktopDragDropClientAuraX11ChromeSourceTargetTest() override = default;

  int StartDragAndDrop() {
    auto data(std::make_unique<ui::OSExchangeData>());
    data->SetString(base::ASCIIToUTF16("Test"));

    return client_->StartDragAndDrop(
        std::move(data), widget_->GetNativeWindow()->GetRootWindow(),
        widget_->GetNativeWindow(), gfx::Point(), ui::DragDropTypes::DRAG_COPY,
        ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE);
  }

  // ViewsTestBase:
  void SetUp() override {
    ViewsTestBase::SetUp();

    // Create widget to initiate the drags.
    widget_ = std::make_unique<Widget>();
    Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.native_widget = new DesktopNativeWidgetAura(widget_.get());
    params.bounds = gfx::Rect(100, 100);
    widget_->Init(std::move(params));
    widget_->Show();

    cursor_manager_ = std::make_unique<DesktopNativeCursorManager>();

    client_ = std::make_unique<SimpleTestDragDropClient>(
        widget_->GetNativeWindow(), cursor_manager_.get());
    client_->Init();
  }

  void TearDown() override {
    client_.reset();
    cursor_manager_.reset();
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  SimpleTestDragDropClient* client() { return client_.get(); }

 private:
  std::unique_ptr<SimpleTestDragDropClient> client_;
  std::unique_ptr<DesktopNativeCursorManager> cursor_manager_;

  // The widget used to initiate drags.
  std::unique_ptr<Widget> widget_;

  DISALLOW_COPY_AND_ASSIGN(DesktopDragDropClientAuraX11ChromeSourceTargetTest);
};

namespace {

void ChromeSourceTargetStep2(SimpleTestDragDropClient* client,
                             int modifier_flags) {
  EXPECT_TRUE(client->IsMoveLoopRunning());

  std::unique_ptr<Widget> target_widget(new Widget);
  Widget::InitParams target_params(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  target_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  target_params.native_widget =
      new DesktopNativeWidgetAura(target_widget.get());
  target_params.bounds = gfx::Rect(100, 100);
  target_widget->Init(std::move(target_params));
  target_widget->Show();

  std::unique_ptr<TestDragDropDelegate> delegate(new TestDragDropDelegate);
  aura::client::SetDragDropDelegate(target_widget->GetNativeWindow(),
                                    delegate.get());

  client->SetTopmostXWindow(
      target_widget->GetNativeView()->GetHost()->GetAcceleratedWidget());

  gfx::Rect target_widget_bounds_in_screen =
      target_widget->GetWindowBoundsInScreen();
  gfx::Point point1_in_screen = target_widget_bounds_in_screen.CenterPoint();
  gfx::Point point1_in_target_widget(
      target_widget_bounds_in_screen.width() / 2,
      target_widget_bounds_in_screen.height() / 2);
  gfx::Point point2_in_screen = point1_in_screen + gfx::Vector2d(1, 0);
  gfx::Point point2_in_target_widget =
      point1_in_target_widget + gfx::Vector2d(1, 0);

  client->OnMouseMovement(point1_in_screen, modifier_flags,
                          ui::EventTimeForNow());
  EXPECT_EQ(1, delegate->num_enters());
  EXPECT_EQ(1, delegate->num_updates());
  EXPECT_EQ(0, delegate->num_exits());
  EXPECT_EQ(0, delegate->num_drops());
  EXPECT_EQ(point1_in_target_widget.ToString(),
            delegate->last_event_mouse_position().ToString());
  EXPECT_EQ(modifier_flags, delegate->last_event_flags());

  client->OnMouseMovement(point2_in_screen, modifier_flags,
                          ui::EventTimeForNow());
  EXPECT_EQ(1, delegate->num_enters());
  EXPECT_EQ(2, delegate->num_updates());
  EXPECT_EQ(0, delegate->num_exits());
  EXPECT_EQ(0, delegate->num_drops());
  EXPECT_EQ(point2_in_target_widget.ToString(),
            delegate->last_event_mouse_position().ToString());
  EXPECT_EQ(modifier_flags, delegate->last_event_flags());

  client->OnMouseReleased();
  EXPECT_EQ(1, delegate->num_enters());
  EXPECT_EQ(2, delegate->num_updates());
  EXPECT_EQ(0, delegate->num_exits());
  EXPECT_EQ(1, delegate->num_drops());
  EXPECT_EQ(point2_in_target_widget.ToString(),
            delegate->last_event_mouse_position().ToString());
  EXPECT_EQ(modifier_flags, delegate->last_event_flags());

  EXPECT_FALSE(client->IsMoveLoopRunning());
}

}  // namespace

// TODO(crbug.com/990756): Turn this into tests of DesktopDragDropClientOzone
// or its equivalent.
TEST_F(DesktopDragDropClientAuraX11ChromeSourceTargetTest, Basic) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ChromeSourceTargetStep2, client(), ui::EF_NONE));
  int result = StartDragAndDrop();
  EXPECT_EQ(ui::DragDropTypes::DRAG_COPY, result);
}

// TODO(crbug.com/990756): Turn this into tests of DesktopDragDropClientOzone
// or its equivalent.
// Test that if 'Ctrl' is pressed during a drag and drop operation, that
// the aura::client::DragDropDelegate is properly notified.
TEST_F(DesktopDragDropClientAuraX11ChromeSourceTargetTest, CtrlPressed) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ChromeSourceTargetStep2, client(), ui::EF_CONTROL_DOWN));
  int result = StartDragAndDrop();
  EXPECT_EQ(ui::DragDropTypes::DRAG_COPY, result);
}

}  // namespace views
