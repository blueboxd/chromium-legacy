// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_window_tree_host_x11.h"

#include <algorithm>
#include <list>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/null_window_targeter.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/buildflags.h"
#include "ui/base/class_property.h"
#include "ui/base/dragdrop/os_exchange_data_provider_aurax11.h"
#include "ui/base/hit_test.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/layout.h"
#include "ui/base/x/x11_pointer_grab.h"
#include "ui/base/x/x11_util.h"
#include "ui/base/x/x11_util_internal.h"
#include "ui/display/screen.h"
#include "ui/events/devices/x11/device_list_cache_x11.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/x/events_x_utils.h"
#include "ui/events/x/x11_window_event_manager.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/path_x11.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/views/linux_ui/linux_ui.h"
#include "ui/views/views_switches.h"
#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_aurax11.h"
#include "ui/views/widget/desktop_aura/desktop_native_cursor_manager.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_observer_x11.h"
#include "ui/views/widget/desktop_aura/x11_desktop_handler.h"
#include "ui/views/widget/desktop_aura/x11_desktop_window_move_client.h"
#include "ui/views/window/native_frame_view.h"
#include "ui/wm/core/compound_event_filter.h"
#include "ui/wm/core/window_util.h"

#if BUILDFLAG(USE_ATK)
#include "ui/accessibility/platform/atk_util_auralinux.h"
#endif

DEFINE_UI_CLASS_PROPERTY_TYPE(views::DesktopWindowTreeHostX11*)

namespace views {

DesktopWindowTreeHostX11* DesktopWindowTreeHostX11::g_current_capture = nullptr;
std::list<XID>* DesktopWindowTreeHostX11::open_windows_ = nullptr;

DEFINE_UI_CLASS_PROPERTY_KEY(aura::Window*, kViewsWindowForRootWindow, NULL)

DEFINE_UI_CLASS_PROPERTY_KEY(DesktopWindowTreeHostX11*,
                             kHostForRootWindow,
                             NULL)

namespace {

// Returns the whole path from |window| to the root.
std::vector<::Window> GetParentsList(XDisplay* xdisplay, ::Window window) {
  ::Window parent_win, root_win;
  Window* child_windows;
  unsigned int num_child_windows;
  std::vector<::Window> result;

  while (window) {
    result.push_back(window);
    if (!XQueryTree(xdisplay, window, &root_win, &parent_win, &child_windows,
                    &num_child_windows))
      break;
    if (child_windows)
      XFree(child_windows);
    window = parent_win;
  }
  return result;
}

class SwapWithNewSizeObserverHelper : public ui::CompositorObserver {
 public:
  using HelperCallback = base::RepeatingCallback<void(const gfx::Size&)>;
  SwapWithNewSizeObserverHelper(ui::Compositor* compositor,
                                const HelperCallback& callback)
      : compositor_(compositor), callback_(callback) {
    compositor_->AddObserver(this);
  }
  ~SwapWithNewSizeObserverHelper() override {
    if (compositor_)
      compositor_->RemoveObserver(this);
  }

 private:
  // ui::CompositorObserver:
  void OnCompositingCompleteSwapWithNewSize(ui::Compositor* compositor,
                                            const gfx::Size& size) override {
    DCHECK_EQ(compositor, compositor_);
    callback_.Run(size);
  }
  void OnCompositingShuttingDown(ui::Compositor* compositor) override {
    DCHECK_EQ(compositor, compositor_);
    compositor_->RemoveObserver(this);
    compositor_ = nullptr;
  }

  ui::Compositor* compositor_;
  const HelperCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(SwapWithNewSizeObserverHelper);
};

bool ShouldDiscardKeyEvent(XEvent* xev) {
#if BUILDFLAG(USE_ATK)
  return ui::AtkUtilAuraLinux::HandleKeyEvent(xev) ==
         ui::DiscardAtkKeyEvent::Discard;
#else
  return false;
#endif
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostX11, public:
DesktopWindowTreeHostX11::DesktopWindowTreeHostX11(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura)
    : DesktopWindowTreeHostLinux(native_widget_delegate,
                                 desktop_native_widget_aura) {}

DesktopWindowTreeHostX11::~DesktopWindowTreeHostX11() {
  window()->ClearProperty(kHostForRootWindow);
  wm::SetWindowMoveClient(window(), nullptr);

  // ~DWTHPlatform notifies the DestkopNativeWidgetAura about destruction and
  // also destroyes the dispatcher.
}

// static
aura::Window* DesktopWindowTreeHostX11::GetContentWindowForXID(XID xid) {
  aura::WindowTreeHost* host =
      aura::WindowTreeHost::GetForAcceleratedWidget(xid);
  return host ? host->window()->GetProperty(kViewsWindowForRootWindow) : NULL;
}

// static
DesktopWindowTreeHostX11* DesktopWindowTreeHostX11::GetHostForXID(XID xid) {
  aura::WindowTreeHost* host =
      aura::WindowTreeHost::GetForAcceleratedWidget(xid);
  return host ? host->window()->GetProperty(kHostForRootWindow) : nullptr;
}

// static
std::vector<aura::Window*> DesktopWindowTreeHostX11::GetAllOpenWindows() {
  std::vector<aura::Window*> windows(open_windows().size());
  std::transform(open_windows().begin(), open_windows().end(), windows.begin(),
                 GetContentWindowForXID);
  return windows;
}

gfx::Rect DesktopWindowTreeHostX11::GetX11RootWindowBounds() const {
  return GetBoundsInPixels();
}

gfx::Rect DesktopWindowTreeHostX11::GetX11RootWindowOuterBounds() const {
  return GetXWindow()->GetOutterBounds();
}

::Region DesktopWindowTreeHostX11::GetWindowShape() const {
  return GetXWindow()->shape();
}

void DesktopWindowTreeHostX11::AddObserver(
    DesktopWindowTreeHostObserverX11* observer) {
  observer_list_.AddObserver(observer);
}

void DesktopWindowTreeHostX11::RemoveObserver(
    DesktopWindowTreeHostObserverX11* observer) {
  observer_list_.RemoveObserver(observer);
}

void DesktopWindowTreeHostX11::CleanUpWindowList(
    void (*func)(aura::Window* window)) {
  if (!open_windows_)
    return;
  while (!open_windows_->empty()) {
    gfx::AcceleratedWidget widget = open_windows_->front();
    func(GetContentWindowForXID(widget));
    if (!open_windows_->empty() && open_windows_->front() == widget)
      open_windows_->erase(open_windows_->begin());
  }

  delete open_windows_;
  open_windows_ = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostX11, DesktopWindowTreeHost implementation:

void DesktopWindowTreeHostX11::Init(const Widget::InitParams& params) {
  DesktopWindowTreeHostPlatform::Init(params);

  // Set XEventDelegate to receive selection, drag&drop and raw key events.
  //
  // TODO(https://crbug.com/990756): There are two cases of this delegate:
  // XEvents for DragAndDrop client and raw key events. DragAndDrop could be
  // unified so that DragAndrDropClientOzone is used and XEvent are handled on
  // platform level.
  static_cast<ui::X11Window*>(platform_window())->SetXEventDelegate(this);

  // Can it be unified and will Ozone benefit from this? Check comment above
  // where this class is defined and declared.
  if (ui::IsSyncExtensionAvailable()) {
    compositor_observer_ = std::make_unique<SwapWithNewSizeObserverHelper>(
        compositor(), base::BindRepeating(
                          &DesktopWindowTreeHostX11::OnCompleteSwapWithNewSize,
                          base::Unretained(this)));
  }
}

void DesktopWindowTreeHostX11::OnNativeWidgetCreated(
    const Widget::InitParams& params) {
  window()->SetProperty(kViewsWindowForRootWindow, content_window());
  window()->SetProperty(kHostForRootWindow, this);

  // Ensure that the X11DesktopHandler exists so that it tracks create/destroy
  // notify events.
  X11DesktopHandler::get();

  x11_window_move_client_ = std::make_unique<X11DesktopWindowMoveClient>();
  wm::SetWindowMoveClient(window(), x11_window_move_client_.get());

  SetWindowTransparency();

  DesktopWindowTreeHostLinux::OnNativeWidgetCreated(params);
}

std::unique_ptr<aura::client::DragDropClient>
DesktopWindowTreeHostX11::CreateDragDropClient(
    DesktopNativeCursorManager* cursor_manager) {
  drag_drop_client_ = new DesktopDragDropClientAuraX11(window(), cursor_manager,
                                                       GetXWindow()->display(),
                                                       GetXWindow()->window());
  drag_drop_client_->Init();
  return base::WrapUnique(drag_drop_client_);
}

bool DesktopWindowTreeHostX11::IsVisible() const {
  return platform_window() ? GetXWindow()->IsVisible() : false;
}

void DesktopWindowTreeHostX11::StackAbove(aura::Window* window) {
  XDisplay* display = GetXWindow()->display();
  ::Window xwindow = GetXWindow()->window();

  if (window && window->GetRootWindow()) {
    ::Window window_below = window->GetHost()->GetAcceleratedWidget();
    // Find all parent windows up to the root.
    std::vector<::Window> window_below_parents =
        GetParentsList(display, window_below);
    std::vector<::Window> window_above_parents =
        GetParentsList(display, xwindow);

    // Find their common ancestor.
    auto it_below_window = window_below_parents.rbegin();
    auto it_above_window = window_above_parents.rbegin();
    for (; it_below_window != window_below_parents.rend() &&
           it_above_window != window_above_parents.rend() &&
           *it_below_window == *it_above_window;
         ++it_below_window, ++it_above_window) {
    }

    if (it_below_window != window_below_parents.rend() &&
        it_above_window != window_above_parents.rend()) {
      // First stack |xwindow| below so Z-order of |window| stays the same.
      ::Window windows[] = {*it_below_window, *it_above_window};
      if (XRestackWindows(display, windows, 2) == 0) {
        // Now stack them properly.
        std::swap(windows[0], windows[1]);
        XRestackWindows(display, windows, 2);
      }
    }
  }
}

void DesktopWindowTreeHostX11::StackAtTop() {
  GetXWindow()->StackAtTop();
}

std::string DesktopWindowTreeHostX11::GetWorkspace() const {
  base::Optional<int> workspace = GetXWindow()->workspace();
  return workspace ? base::NumberToString(workspace.value()) : std::string();
}

void DesktopWindowTreeHostX11::SetShape(
    std::unique_ptr<Widget::ShapeRects> native_shape) {
  _XRegion* xregion = nullptr;
  if (native_shape) {
    SkRegion native_region;
    for (const gfx::Rect& rect : *native_shape)
      native_region.op(gfx::RectToSkIRect(rect), SkRegion::kUnion_Op);
    gfx::Transform transform = GetRootTransform();
    if (!transform.IsIdentity() && !native_region.isEmpty()) {
      SkPath path_in_dip;
      if (native_region.getBoundaryPath(&path_in_dip)) {
        SkPath path_in_pixels;
        path_in_dip.transform(transform.matrix(), &path_in_pixels);
        xregion = gfx::CreateRegionFromSkPath(path_in_pixels);
      } else {
        xregion = XCreateRegion();
      }
    } else {
      xregion = gfx::CreateRegionFromSkRegion(native_region);
    }
  }
  GetXWindow()->SetShape(xregion);
  ResetWindowRegion();
}

bool DesktopWindowTreeHostX11::IsActive() const {
  return GetXWindow()->IsActive();
}

bool DesktopWindowTreeHostX11::HasCapture() const {
  return g_current_capture == this;
}

void DesktopWindowTreeHostX11::SetVisibleOnAllWorkspaces(bool always_visible) {
  GetXWindow()->SetVisibleOnAllWorkspaces(always_visible);
}

bool DesktopWindowTreeHostX11::IsVisibleOnAllWorkspaces() const {
  return GetXWindow()->IsVisibleOnAllWorkspaces();
}

Widget::MoveLoopResult DesktopWindowTreeHostX11::RunMoveLoop(
    const gfx::Vector2d& drag_offset,
    Widget::MoveLoopSource source,
    Widget::MoveLoopEscapeBehavior escape_behavior) {
  wm::WindowMoveSource window_move_source =
      source == Widget::MOVE_LOOP_SOURCE_MOUSE ? wm::WINDOW_MOVE_SOURCE_MOUSE
                                               : wm::WINDOW_MOVE_SOURCE_TOUCH;
  if (x11_window_move_client_->RunMoveLoop(content_window(), drag_offset,
                                           window_move_source) ==
      wm::MOVE_SUCCESSFUL)
    return Widget::MOVE_LOOP_SUCCESSFUL;

  return Widget::MOVE_LOOP_CANCELED;
}

void DesktopWindowTreeHostX11::EndMoveLoop() {
  x11_window_move_client_->EndMoveLoop();
}

void DesktopWindowTreeHostX11::SetVisibilityChangedAnimationsEnabled(
    bool value) {
  // Much like the previous NativeWidgetGtk, we don't have anything to do here.
}

bool DesktopWindowTreeHostX11::ShouldUseNativeFrame() const {
  return GetXWindow()->use_native_frame();
}

bool DesktopWindowTreeHostX11::ShouldWindowContentsBeTransparent() const {
  return GetXWindow()->has_alpha();
}

void DesktopWindowTreeHostX11::FrameTypeChanged() {
  Widget::FrameType new_type =
      native_widget_delegate()->AsWidget()->frame_type();
  if (new_type == Widget::FrameType::kDefault) {
    // The default is determined by Widget::InitParams::remove_standard_frame
    // and does not change.
    return;
  }
  // Avoid mutating |View::children_| while possibly iterating over them.
  // See View::PropagateNativeThemeChanged().
  // TODO(varkha, sadrul): Investigate removing this (and instead expecting the
  // NonClientView::UpdateFrame() to update the frame-view when theme changes,
  // like all other views).
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&DesktopWindowTreeHostX11::DelayedChangeFrameType,
                     weak_factory_.GetWeakPtr(), new_type));
}

void DesktopWindowTreeHostX11::SetOpacity(float opacity) {
  GetXWindow()->SetOpacity(opacity);
}

void DesktopWindowTreeHostX11::SetAspectRatio(const gfx::SizeF& aspect_ratio) {
  GetXWindow()->SetAspectRatio(aspect_ratio);
}

void DesktopWindowTreeHostX11::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                              const gfx::ImageSkia& app_icon) {
  GetXWindow()->SetWindowIcons(window_icon, app_icon);
}

void DesktopWindowTreeHostX11::InitModalType(ui::ModalType modal_type) {
  switch (modal_type) {
    case ui::MODAL_TYPE_NONE:
      break;
    default:
      // TODO(erg): Figure out under what situations |modal_type| isn't
      // none. The comment in desktop_native_widget_aura.cc suggests that this
      // is rare.
      NOTIMPLEMENTED();
  }
}

void DesktopWindowTreeHostX11::FlashFrame(bool flash_frame) {
  GetXWindow()->FlashFrame(flash_frame);
}

bool DesktopWindowTreeHostX11::IsAnimatingClosed() const {
  return false;
}

bool DesktopWindowTreeHostX11::IsTranslucentWindowOpacitySupported() const {
  // This function may be called before InitX11Window() (which
  // initializes |visual_has_alpha_|), so we cannot simply return
  // |visual_has_alpha_|.
  return ui::XVisualManager::GetInstance()->ArgbVisualAvailable();
}

void DesktopWindowTreeHostX11::SizeConstraintsChanged() {
  GetXWindow()->UpdateMinAndMaxSize();
}

bool DesktopWindowTreeHostX11::ShouldUpdateWindowTransparency() const {
  return true;
}

bool DesktopWindowTreeHostX11::ShouldUseDesktopNativeCursorManager() const {
  return true;
}

bool DesktopWindowTreeHostX11::ShouldCreateVisibilityController() const {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostX11, aura::WindowTreeHost implementatio

void DesktopWindowTreeHostX11::SetCapture() {
  if (HasCapture())
    return;

  // Grabbing the mouse is asynchronous. However, we synchronously start
  // forwarding all mouse events received by Chrome to the
  // aura::WindowEventDispatcher which has capture. This makes capture
  // synchronous for all intents and purposes if either:
  // - |g_current_capture|'s X window has capture.
  // OR
  // - The topmost window underneath the mouse is managed by Chrome.
  DesktopWindowTreeHostX11* old_capturer = g_current_capture;

  // Update |g_current_capture| prior to calling OnHostLostWindowCapture() to
  // avoid releasing pointer grab.
  g_current_capture = this;
  if (old_capturer)
    old_capturer->OnHostLostWindowCapture();

  GetXWindow()->GrabPointer();
}

void DesktopWindowTreeHostX11::ReleaseCapture() {
  if (g_current_capture == this) {
    // Release mouse grab asynchronously. A window managed by Chrome is likely
    // the topmost window underneath the mouse so the capture release being
    // asynchronous is likely inconsequential.
    g_current_capture = nullptr;
    GetXWindow()->ReleasePointerGrab();

    OnHostLostWindowCapture();
  }
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostX11, private:

void DesktopWindowTreeHostX11::SetUseNativeFrame(bool use_native_frame) {
  GetXWindow()->SetUseNativeFrame(use_native_frame);
  ResetWindowRegion();
}

void DesktopWindowTreeHostX11::DispatchMouseEvent(ui::MouseEvent* event) {
  // In Windows, the native events sent to chrome are separated into client
  // and non-client versions of events, which we record on our LocatedEvent
  // structures. On X11, we emulate the concept of non-client. Before we pass
  // this event to the cross platform event handling framework, we need to
  // make sure it is appropriately marked as non-client if it's in the non
  // client area, or otherwise, we can get into a state where the a window is
  // set as the |mouse_pressed_handler_| in window_event_dispatcher.cc
  // despite the mouse button being released.
  //
  // We can't do this later in the dispatch process because we share that
  // with ash, and ash gets confused about event IS_NON_CLIENT-ness on
  // events, since ash doesn't expect this bit to be set, because it's never
  // been set before. (This works on ash on Windows because none of the mouse
  // events on the ash desktop are clicking in what Windows considers to be a
  // non client area.) Likewise, we won't want to do the following in any
  // WindowTreeHost that hosts ash.
  if (content_window() && content_window()->delegate()) {
    int flags = event->flags();
    gfx::Point location_in_dip = event->location();
    GetRootTransform().TransformPointReverse(&location_in_dip);
    int hit_test_code =
        content_window()->delegate()->GetNonClientComponent(location_in_dip);
    if (hit_test_code != HTCLIENT && hit_test_code != HTNOWHERE)
      flags |= ui::EF_IS_NON_CLIENT;
    event->set_flags(flags);
  }

  // While we unset the urgency hint when we gain focus, we also must remove it
  // on mouse clicks because we can call FlashFrame() on an active window.
  if (event->IsAnyButton() || event->IsMouseWheelEvent())
    FlashFrame(false);

  if (!g_current_capture || g_current_capture == this) {
    SendEventToSink(event);
  } else {
    // Another DesktopWindowTreeHostX11 has installed itself as
    // capture. Translate the event's location and dispatch to the other.
    DCHECK_EQ(ui::GetScaleFactorForNativeView(window()),
              ui::GetScaleFactorForNativeView(g_current_capture->window()));
    ConvertEventLocationToTargetWindowLocation(
        g_current_capture->GetLocationOnScreenInPixels(),
        GetLocationOnScreenInPixels(), event->AsLocatedEvent());
    g_current_capture->SendEventToSink(event);
  }
}

void DesktopWindowTreeHostX11::DispatchTouchEvent(ui::TouchEvent* event) {
  if (g_current_capture && g_current_capture != this &&
      event->type() == ui::ET_TOUCH_PRESSED) {
    DCHECK_EQ(ui::GetScaleFactorForNativeView(window()),
              ui::GetScaleFactorForNativeView(g_current_capture->window()));
    ConvertEventLocationToTargetWindowLocation(
        g_current_capture->GetLocationOnScreenInPixels(),
        GetLocationOnScreenInPixels(), event->AsLocatedEvent());
    g_current_capture->SendEventToSink(event);
  } else {
    SendEventToSink(event);
  }
}

void DesktopWindowTreeHostX11::DispatchKeyEvent(ui::KeyEvent* event) {
  if (native_widget_delegate()->AsWidget()->IsActive())
    SendEventToSink(event);
}

void DesktopWindowTreeHostX11::ResetWindowRegion() {
  _XRegion* xregion = nullptr;
  if (!GetXWindow()->use_custom_shape() && !IsMaximized() && !IsFullscreen()) {
    SkPath window_mask;
    Widget* widget = native_widget_delegate()->AsWidget();
    if (widget->non_client_view()) {
      // Some frame views define a custom (non-rectangular) window mask. If
      // so, use it to define the window shape. If not, fall through.
      widget->non_client_view()->GetWindowMask(GetXWindow()->bounds().size(),
                                               &window_mask);
      if (window_mask.countPoints() > 0) {
        xregion = gfx::CreateRegionFromSkPath(window_mask);
      }
    }
  }
  GetXWindow()->UpdateWindowRegion(xregion);
}

std::list<gfx::AcceleratedWidget>& DesktopWindowTreeHostX11::open_windows() {
  if (!open_windows_)
    open_windows_ = new std::list<gfx::AcceleratedWidget>();
  return *open_windows_;
}

void DesktopWindowTreeHostX11::SetWindowTransparency() {
  bool has_alpha = GetXWindow()->has_alpha();
  compositor()->SetBackgroundColor(has_alpha ? SK_ColorTRANSPARENT
                                             : SK_ColorWHITE);
  window()->SetTransparent(has_alpha);
  content_window()->SetTransparent(has_alpha);
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostX11 implementation:

void DesktopWindowTreeHostX11::DelayedChangeFrameType(Widget::FrameType type) {
  SetUseNativeFrame(type == Widget::FrameType::kForceNative);
  // Replace the frame and layout the contents. Even though we don't have a
  // swappable glass frame like on Windows, we still replace the frame because
  // the button assets don't update otherwise.
  native_widget_delegate()->AsWidget()->non_client_view()->UpdateFrame();
}

base::OnceClosure DesktopWindowTreeHostX11::DisableEventListening() {
  // Allows to open multiple file-pickers. See https://crbug.com/678982
  modal_dialog_counter_++;
  if (modal_dialog_counter_ == 1) {
    // ScopedWindowTargeter is used to temporarily replace the event-targeter
    // with NullWindowEventTargeter to make |dialog| modal.
    targeter_for_modal_ = std::make_unique<aura::ScopedWindowTargeter>(
        window(), std::make_unique<aura::NullWindowTargeter>());
  }

  return base::BindOnce(&DesktopWindowTreeHostX11::EnableEventListening,
                        weak_factory_.GetWeakPtr());
}

void DesktopWindowTreeHostX11::EnableEventListening() {
  DCHECK_GT(modal_dialog_counter_, 0UL);
  if (!--modal_dialog_counter_)
    targeter_for_modal_.reset();
}

void DesktopWindowTreeHostX11::OnCompleteSwapWithNewSize(
    const gfx::Size& size) {
  GetXWindow()->NotifySwapAfterResize();
}

base::flat_map<std::string, std::string>
DesktopWindowTreeHostX11::GetKeyboardLayoutMap() {
  if (views::LinuxUI::instance())
    return views::LinuxUI::instance()->GetKeyboardLayoutMap();
  return {};
}

void DesktopWindowTreeHostX11::SetVisualId(VisualID visual_id) {
  DCHECK_EQ(GetXWindow()->window(), x11::None);
  GetXWindow()->set_visual_id(visual_id);
}

void DesktopWindowTreeHostX11::OnBoundsChanged(const gfx::Rect& new_bounds) {
  ResetWindowRegion();
  WindowTreeHostPlatform::OnBoundsChanged(new_bounds);
}

void DesktopWindowTreeHostX11::DispatchEvent(ui::Event* event) {
  if (event->IsKeyEvent())
    DispatchKeyEvent(event->AsKeyEvent());
  else if (event->IsMouseEvent())
    DispatchMouseEvent(event->AsMouseEvent());
  else if (event->IsTouchEvent())
    DispatchTouchEvent(event->AsTouchEvent());
  else
    SendEventToSink(event);
}

void DesktopWindowTreeHostX11::OnClosed() {
  open_windows().remove(GetAcceleratedWidget());
  DesktopWindowTreeHostLinux::OnClosed();
}

void DesktopWindowTreeHostX11::OnWindowStateChanged(
    ui::PlatformWindowState new_state) {
  DesktopWindowTreeHostPlatform::OnWindowStateChanged(new_state);
  ResetWindowRegion();
}

void DesktopWindowTreeHostX11::OnAcceleratedWidgetAvailable(
    gfx::AcceleratedWidget widget) {
  open_windows().push_front(widget);
  WindowTreeHostPlatform::OnAcceleratedWidgetAvailable(widget);
}

void DesktopWindowTreeHostX11::OnAcceleratedWidgetDestroyed() {}

void DesktopWindowTreeHostX11::OnActivationChanged(bool active) {
  if (active) {
    // TODO(thomasanderson): Remove this window shuffling and use XWindowCache
    // instead.
    auto widget = GetAcceleratedWidget();
    open_windows().remove(widget);
    open_windows().insert(open_windows().begin(), widget);
  }
  desktop_native_widget_aura()->HandleActivationChanged(active);
  native_widget_delegate()->AsWidget()->GetRootView()->SchedulePaint();
}

void DesktopWindowTreeHostX11::OnXWindowMapped() {
  for (DesktopWindowTreeHostObserverX11& observer : observer_list_)
    observer.OnWindowMapped(GetXWindow()->window());
}

void DesktopWindowTreeHostX11::OnXWindowUnmapped() {
  for (DesktopWindowTreeHostObserverX11& observer : observer_list_)
    observer.OnWindowUnmapped(GetXWindow()->window());
}

void DesktopWindowTreeHostX11::OnLostMouseGrab() {
  dispatcher()->OnHostLostMouseGrab();
}

void DesktopWindowTreeHostX11::OnWorkspaceChanged() {
  OnHostWorkspaceChanged();
}

void DesktopWindowTreeHostX11::OnXWindowSelectionEvent(XEvent* xev) {
  DCHECK(xev);
  DCHECK(drag_drop_client_);
  drag_drop_client_->OnSelectionNotify(xev->xselection);
}

void DesktopWindowTreeHostX11::OnXWindowDragDropEvent(XEvent* xev) {
  DCHECK(xev);
  DCHECK(drag_drop_client_);

  ::Atom message_type = xev->xclient.message_type;
  if (message_type == gfx::GetAtom("XdndEnter")) {
    drag_drop_client_->OnXdndEnter(xev->xclient);
  } else if (message_type == gfx::GetAtom("XdndLeave")) {
    drag_drop_client_->OnXdndLeave(xev->xclient);
  } else if (message_type == gfx::GetAtom("XdndPosition")) {
    drag_drop_client_->OnXdndPosition(xev->xclient);
  } else if (message_type == gfx::GetAtom("XdndStatus")) {
    drag_drop_client_->OnXdndStatus(xev->xclient);
  } else if (message_type == gfx::GetAtom("XdndFinished")) {
    drag_drop_client_->OnXdndFinished(xev->xclient);
  } else if (message_type == gfx::GetAtom("XdndDrop")) {
    drag_drop_client_->OnXdndDrop(xev->xclient);
  }
}

void DesktopWindowTreeHostX11::OnXWindowRawKeyEvent(XEvent* xev) {
  switch (xev->type) {
    case KeyPress:
      if (!ShouldDiscardKeyEvent(xev)) {
        ui::KeyEvent keydown_event(xev);
        DispatchKeyEvent(&keydown_event);
      }
      break;
    case KeyRelease:

      // There is no way to deactivate a window in X11 so ignore input if
      // window is supposed to be 'inactive'.
      if (!IsActive() && !HasCapture())
        break;

      if (!ShouldDiscardKeyEvent(xev)) {
        ui::KeyEvent keyup_event(xev);
        DispatchKeyEvent(&keyup_event);
      }
      break;
    default:
      NOTREACHED() << xev->type;
      break;
  }
}

ui::XWindow* DesktopWindowTreeHostX11::GetXWindow() {
  DCHECK(platform_window());
  // ui::X11Window inherits both PlatformWindow and ui::XWindow.
  return static_cast<ui::XWindow*>(
      static_cast<ui::X11Window*>(platform_window()));
}

const ui::XWindow* DesktopWindowTreeHostX11::GetXWindow() const {
  DCHECK(platform_window());
  // ui::X11Window inherits both PlatformWindow and ui::XWindow.
  return static_cast<const ui::XWindow*>(
      static_cast<const ui::X11Window*>(platform_window()));
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHost, public:

// static
DesktopWindowTreeHost* DesktopWindowTreeHost::Create(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura) {
  return new DesktopWindowTreeHostX11(native_widget_delegate,
                                      desktop_native_widget_aura);
}

}  // namespace views
