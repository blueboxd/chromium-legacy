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
#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/focus_client.h"
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
#include "ui/events/keyboard_hook.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/x/events_x_utils.h"
#include "ui/events/x/x11_window_event_manager.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/path_x11.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/views/corewm/tooltip_aura.h"
#include "ui/views/linux_ui/linux_ui.h"
#include "ui/views/views_delegate.h"
#include "ui/views/views_switches.h"
#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_aurax11.h"
#include "ui/views/widget/desktop_aura/desktop_native_cursor_manager.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_observer_x11.h"
#include "ui/views/widget/desktop_aura/x11_desktop_handler.h"
#include "ui/views/widget/desktop_aura/x11_desktop_window_move_client.h"
#include "ui/views/widget/desktop_aura/x11_window_event_filter.h"
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

ui::XWindow::Configuration ConvertInitParamsToX11WindowConfig(
    const Widget::InitParams& params) {
  using WindowType = ui::XWindow::WindowType;
  using WindowOpacity = ui::XWindow::WindowOpacity;
  ui::XWindow::Configuration config;

  switch (params.type) {
    case Widget::InitParams::TYPE_WINDOW:
      config.type = WindowType::kWindow;
      break;
    case Widget::InitParams::TYPE_MENU:
      config.type = WindowType::kMenu;
      break;
    case Widget::InitParams::TYPE_TOOLTIP:
      config.type = WindowType::kTooltip;
      break;
    case Widget::InitParams::TYPE_DRAG:
      config.type = WindowType::kDrag;
      break;
    case Widget::InitParams::TYPE_BUBBLE:
      config.type = WindowType::kBubble;
      break;
    default:
      config.type = WindowType::kPopup;
      break;
  }

  switch (params.opacity) {
    case Widget::InitParams::WindowOpacity::INFER_OPACITY:
      config.opacity = WindowOpacity::kInferOpacity;
      break;
    case Widget::InitParams::WindowOpacity::OPAQUE_WINDOW:
      config.opacity = WindowOpacity::kOpaqueWindow;
      break;
    case Widget::InitParams::WindowOpacity::TRANSLUCENT_WINDOW:
      config.opacity = WindowOpacity::kTranslucentWindow;
      break;
  }

  config.activatable =
      params.activatable == Widget::InitParams::ACTIVATABLE_YES;
  config.force_show_in_taskbar = params.force_show_in_taskbar;
  config.keep_on_top =
      params.EffectiveZOrderLevel() != ui::ZOrderLevel::kNormal;
  config.visible_on_all_workspaces = params.visible_on_all_workspaces;
  config.remove_standard_frame = params.remove_standard_frame;

  config.workspace = params.workspace;
  config.wm_class_name = params.wm_class_name;
  config.wm_class_class = params.wm_class_class;
  config.wm_role_name = params.wm_role_name;

  return config;
}

// Returns the whole path from |window| to the root.
std::vector<::Window> GetParentsList(XDisplay* xdisplay, ::Window window) {
  ::Window parent_win, root_win;
  Window* child_windows;
  unsigned int num_child_windows;
  std::vector<::Window> result;

  while (window) {
    result.push_back(window);
    if (!XQueryTree(xdisplay, window,
                    &root_win, &parent_win, &child_windows, &num_child_windows))
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
    : native_widget_delegate_(native_widget_delegate),
      desktop_native_widget_aura_(desktop_native_widget_aura),
      x11_window_(std::make_unique<ui::XWindow>(this)) {}

DesktopWindowTreeHostX11::~DesktopWindowTreeHostX11() {
  window()->ClearProperty(kHostForRootWindow);
  wm::SetWindowMoveClient(window(), nullptr);
  desktop_native_widget_aura_->OnDesktopWindowTreeHostDestroyed(this);
  DestroyDispatcher();
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
  std::transform(open_windows().begin(),
                 open_windows().end(),
                 windows.begin(),
                 GetContentWindowForXID);
  return windows;
}

gfx::Rect DesktopWindowTreeHostX11::GetX11RootWindowBounds() const {
  return x11_window_->bounds();
}

gfx::Rect DesktopWindowTreeHostX11::GetX11RootWindowOuterBounds() const {
  return x11_window_->GetOutterBounds();
}

::Region DesktopWindowTreeHostX11::GetWindowShape() const {
  return x11_window_->shape();
}

void DesktopWindowTreeHostX11::AddObserver(
    DesktopWindowTreeHostObserverX11* observer) {
  observer_list_.AddObserver(observer);
}

void DesktopWindowTreeHostX11::RemoveObserver(
    DesktopWindowTreeHostObserverX11* observer) {
  observer_list_.RemoveObserver(observer);
}

void DesktopWindowTreeHostX11::SwapNonClientEventHandler(
    std::unique_ptr<ui::EventHandler> handler) {
  wm::CompoundEventFilter* compound_event_filter =
      desktop_native_widget_aura_->root_window_event_filter();
  if (x11_non_client_event_filter_)
    compound_event_filter->RemoveHandler(x11_non_client_event_filter_.get());
  compound_event_filter->AddHandler(handler.get());
  x11_non_client_event_filter_ = std::move(handler);
}

void DesktopWindowTreeHostX11::CleanUpWindowList(
    void (*func)(aura::Window* window)) {
  if (!open_windows_)
    return;
  while (!open_windows_->empty()) {
    XID xid = open_windows_->front();
    func(GetContentWindowForXID(xid));
    if (!open_windows_->empty() && open_windows_->front() == xid)
      open_windows_->erase(open_windows_->begin());
  }

  delete open_windows_;
  open_windows_ = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostX11, DesktopWindowTreeHost implementation:

void DesktopWindowTreeHostX11::Init(const Widget::InitParams& params) {
  if (params.type == Widget::InitParams::TYPE_WINDOW)
    content_window()->SetProperty(aura::client::kAnimationsDisabledKey, true);

  InitX11Window(params);
  InitHost();
  window()->Show();
}

void DesktopWindowTreeHostX11::OnNativeWidgetCreated(
    const Widget::InitParams& params) {
  window()->SetProperty(kViewsWindowForRootWindow, content_window());
  window()->SetProperty(kHostForRootWindow, this);

  // Ensure that the X11DesktopHandler exists so that it tracks create/destroy
  // notify events.
  X11DesktopHandler::get();

  // TODO(erg): Unify this code once the other consumer goes away.
  SwapNonClientEventHandler(
      std::unique_ptr<ui::EventHandler>(new X11WindowEventFilter(this)));
  SetUseNativeFrame(params.type == Widget::InitParams::TYPE_WINDOW &&
                    !params.remove_standard_frame);

  x11_window_move_client_ = std::make_unique<X11DesktopWindowMoveClient>();
  wm::SetWindowMoveClient(window(), x11_window_move_client_.get());

  SetWindowTransparency();

  native_widget_delegate_->OnNativeWidgetCreated();
}

void DesktopWindowTreeHostX11::OnWidgetInitDone() {}

void DesktopWindowTreeHostX11::OnActiveWindowChanged(bool active) {}

std::unique_ptr<corewm::Tooltip> DesktopWindowTreeHostX11::CreateTooltip() {
  return base::WrapUnique(new corewm::TooltipAura);
}

std::unique_ptr<aura::client::DragDropClient>
DesktopWindowTreeHostX11::CreateDragDropClient(
    DesktopNativeCursorManager* cursor_manager) {
  drag_drop_client_ = new DesktopDragDropClientAuraX11(
      window(), cursor_manager, x11_window_->display(), x11_window_->window());
  drag_drop_client_->Init();
  return base::WrapUnique(drag_drop_client_);
}

void DesktopWindowTreeHostX11::Close() {
  content_window()->Hide();

  // TODO(erg): Might need to do additional hiding tasks here.
  x11_window_->CancelResize();

  if (!close_widget_factory_.HasWeakPtrs()) {
    // And we delay the close so that if we are called from an ATL callback,
    // we don't destroy the window before the callback returned (as the caller
    // may delete ourselves on destroy and the ATL callback would still
    // dereference us when the callback returns).
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&DesktopWindowTreeHostX11::CloseNow,
                                  close_widget_factory_.GetWeakPtr()));
  }
}

void DesktopWindowTreeHostX11::CloseNow() {
  if (x11_window_->window() == x11::None)
    return;

  ReleaseCapture();
  native_widget_delegate_->OnNativeWidgetDestroying();

  // If we have children, close them. Use a copy for iteration because they'll
  // remove themselves.
  std::set<DesktopWindowTreeHostX11*> window_children_copy = window_children_;
  for (auto* child : window_children_copy)
    child->CloseNow();
  DCHECK(window_children_.empty());

  // If we have a parent, remove ourselves from its children list.
  if (window_parent_) {
    window_parent_->window_children_.erase(this);
    window_parent_ = nullptr;
  }

  // Remove the event listeners we've installed. We need to remove these
  // because otherwise we get assert during ~WindowEventDispatcher().
  desktop_native_widget_aura_->root_window_event_filter()->RemoveHandler(
      x11_non_client_event_filter_.get());
  x11_non_client_event_filter_.reset();

  // Destroy the compositor before destroying the |xwindow_| since shutdown
  // may try to swap, and the swap without a window causes an X error, which
  // causes a crash with in-process renderer.
  DestroyCompositor();

  open_windows().remove(x11_window_->window());
  // Actually free our native resources.
  if (auto* source = ui::PlatformEventSource::GetInstance())
    source->RemovePlatformEventDispatcher(this);

  x11_window_->Close();
  desktop_native_widget_aura_->OnHostClosed();
}

aura::WindowTreeHost* DesktopWindowTreeHostX11::AsWindowTreeHost() {
  return this;
}

void DesktopWindowTreeHostX11::Show(ui::WindowShowState show_state,
                                    const gfx::Rect& restore_bounds) {
  if (compositor())
    SetVisible(true);

  if (!x11_window_->mapped_in_client() || IsMinimized())
    MapWindow(show_state);

  switch (show_state) {
    case ui::SHOW_STATE_MAXIMIZED:
      Maximize();
      if (!restore_bounds.IsEmpty()) {
        // Enforce |restored_bounds_in_pixels_| since calling Maximize() could
        // have reset it.
        restored_bounds_in_pixels_ = ToPixelRect(restore_bounds);
      }
      break;
    case ui::SHOW_STATE_MINIMIZED:
      Minimize();
      break;
    case ui::SHOW_STATE_FULLSCREEN:
      SetFullscreen(true);
      break;
    default:
      break;
  }

  native_widget_delegate_->AsWidget()->SetInitialFocus(show_state);

  content_window()->Show();
}

bool DesktopWindowTreeHostX11::IsVisible() const {
  return x11_window_->IsVisible();
}

void DesktopWindowTreeHostX11::SetSize(const gfx::Size& requested_size) {
  gfx::Size size_in_pixels = ToPixelRect(gfx::Rect(requested_size)).size();
  size_in_pixels = AdjustSize(size_in_pixels);

  bool size_changed = x11_window_->bounds().size() != size_in_pixels;

  x11_window_->SetSize(size_in_pixels);

  if (size_changed) {
    OnHostResizedInPixels(size_in_pixels);
    ResetWindowRegion();
  }
}

void DesktopWindowTreeHostX11::StackAbove(aura::Window* window) {
  XDisplay* display = x11_window_->display();
  ::Window xwindow = x11_window_->window();

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
  x11_window_->StackAtTop();
}

void DesktopWindowTreeHostX11::CenterWindow(const gfx::Size& size) {
  gfx::Size size_in_pixels = ToPixelRect(gfx::Rect(size)).size();
  gfx::Rect parent_bounds_in_pixels = ToPixelRect(GetWorkAreaBoundsInScreen());

  // If |window_|'s transient parent bounds are big enough to contain |size|,
  // use them instead.
  if (wm::GetTransientParent(content_window())) {
    gfx::Rect transient_parent_rect =
        wm::GetTransientParent(content_window())->GetBoundsInScreen();
    if (transient_parent_rect.height() >= size.height() &&
        transient_parent_rect.width() >= size.width()) {
      parent_bounds_in_pixels = ToPixelRect(transient_parent_rect);
    }
  }

  gfx::Rect window_bounds_in_pixels(
      parent_bounds_in_pixels.x() +
          (parent_bounds_in_pixels.width() - size_in_pixels.width()) / 2,
      parent_bounds_in_pixels.y() +
          (parent_bounds_in_pixels.height() - size_in_pixels.height()) / 2,
      size_in_pixels.width(), size_in_pixels.height());
  // Don't size the window bigger than the parent, otherwise the user may not be
  // able to close or move it.
  window_bounds_in_pixels.AdjustToFit(parent_bounds_in_pixels);

  SetBoundsInPixels(window_bounds_in_pixels);
}

void DesktopWindowTreeHostX11::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  *bounds = GetRestoredBounds();

  if (IsFullscreen()) {
    *show_state = ui::SHOW_STATE_FULLSCREEN;
  } else if (IsMinimized()) {
    *show_state = ui::SHOW_STATE_MINIMIZED;
  } else if (IsMaximized()) {
    *show_state = ui::SHOW_STATE_MAXIMIZED;
  } else if (!IsActive()) {
    *show_state = ui::SHOW_STATE_INACTIVE;
  } else {
    *show_state = ui::SHOW_STATE_NORMAL;
  }
}

gfx::Rect DesktopWindowTreeHostX11::GetWindowBoundsInScreen() const {
  gfx::Rect bounds_in_pixels = x11_window_->bounds();
  return ToDIPRect(bounds_in_pixels);
}

gfx::Rect DesktopWindowTreeHostX11::GetClientAreaBoundsInScreen() const {
  // TODO(erg): The NativeWidgetAura version returns |bounds_in_pixels_|,
  // claiming it's needed for View::ConvertPointToScreen() to work correctly.
  // DesktopWindowTreeHostWin::GetClientAreaBoundsInScreen() just asks windows
  // what it thinks the client rect is.
  //
  // Attempts to calculate the rect by asking the NonClientFrameView what it
  // thought its GetBoundsForClientView() were broke combobox drop down
  // placement.
  return GetWindowBoundsInScreen();
}

gfx::Rect DesktopWindowTreeHostX11::GetRestoredBounds() const {
  // We can't reliably track the restored bounds of a window, but we can get
  // the 90% case down. When *chrome* is the process that requests maximizing
  // or restoring bounds, we can record the current bounds before we request
  // maximization, and clear it when we detect a state change.
  if (!restored_bounds_in_pixels_.IsEmpty())
    return ToDIPRect(restored_bounds_in_pixels_);

  return GetWindowBoundsInScreen();
}

std::string DesktopWindowTreeHostX11::GetWorkspace() const {
  base::Optional<int> workspace = x11_window_->workspace();
  return workspace ? base::NumberToString(workspace.value()) : std::string();
}

gfx::Rect DesktopWindowTreeHostX11::GetWorkAreaBoundsInScreen() const {
  return display::Screen::GetScreen()
      ->GetDisplayNearestWindow(const_cast<aura::Window*>(window()))
      .work_area();
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
  x11_window_->SetShape(xregion);
  ResetWindowRegion();
}

void DesktopWindowTreeHostX11::Activate() {
  x11_window_->Activate();
}

void DesktopWindowTreeHostX11::Deactivate() {
  ReleaseCapture();
  x11_window_->Deactivate();
}

bool DesktopWindowTreeHostX11::IsActive() const {
  return x11_window_->IsActive();
}

void DesktopWindowTreeHostX11::Maximize() {
  // TODO(nickdiego): Move into XWindow. For now, it is kept outside
  // it due to |AdjustSize|, which depends on display::Display, which is not
  // accessible at Ozone layer.
  if (x11_window_->IsFullscreen()) {
    // Unfullscreen the window if it is fullscreen.
    x11_window_->SetFullscreen(false);

    // Resize the window so that it does not have the same size as a monitor.
    // (Otherwise, some window managers immediately put the window back in
    // fullscreen mode).
    gfx::Rect bounds = x11_window_->bounds();
    gfx::Rect adjusted_bounds_in_pixels(bounds.origin(),
                                        AdjustSize(bounds.size()));
    if (adjusted_bounds_in_pixels != bounds)
      SetBoundsInPixels(adjusted_bounds_in_pixels);
  }

  // When we are in the process of requesting to maximize a window, we can
  // accurately keep track of our restored bounds instead of relying on the
  // heuristics that are in the PropertyNotify and ConfigureNotify handlers.
  restored_bounds_in_pixels_ = x11_window_->bounds();

  x11_window_->Maximize();
  if (IsMinimized())
    Show(ui::SHOW_STATE_NORMAL, gfx::Rect());
}

void DesktopWindowTreeHostX11::Minimize() {
  ReleaseCapture();
  x11_window_->Minimize();
}

void DesktopWindowTreeHostX11::Restore() {
  x11_window_->Unmaximize();
  Show(ui::SHOW_STATE_NORMAL, gfx::Rect());
  x11_window_->Unhide();
}

bool DesktopWindowTreeHostX11::IsMaximized() const {
  return x11_window_->IsMaximized();
}

bool DesktopWindowTreeHostX11::IsMinimized() const {
  return x11_window_->IsMinimized();
}

bool DesktopWindowTreeHostX11::HasCapture() const {
  return g_current_capture == this;
}

void DesktopWindowTreeHostX11::SetZOrderLevel(ui::ZOrderLevel order) {
  z_order_ = order;

  // Emulate the multiple window levels provided by other platforms by
  // collapsing the z-order enum into kNormal = normal, everything else = always
  // on top.
  x11_window_->SetAlwaysOnTop(order != ui::ZOrderLevel::kNormal);
}

ui::ZOrderLevel DesktopWindowTreeHostX11::GetZOrderLevel() const {
  bool window_always_on_top = x11_window_->is_always_on_top();
  bool level_always_on_top = z_order_ != ui::ZOrderLevel::kNormal;

  if (window_always_on_top == level_always_on_top)
    return z_order_;

  // If something external has forced a window to be always-on-top, map it to
  // kFloatingWindow as a reasonable equivalent.
  return window_always_on_top ? ui::ZOrderLevel::kFloatingWindow
                              : ui::ZOrderLevel::kNormal;
}

void DesktopWindowTreeHostX11::SetVisible(bool visible) {
  if (is_compositor_set_visible_ == visible)
    return;

  is_compositor_set_visible_ = visible;
  if (compositor())
    compositor()->SetVisible(visible);

  native_widget_delegate_->OnNativeWidgetVisibilityChanged(visible);
}

void DesktopWindowTreeHostX11::SetVisibleOnAllWorkspaces(bool always_visible) {
  x11_window_->SetVisibleOnAllWorkspaces(always_visible);
}

bool DesktopWindowTreeHostX11::IsVisibleOnAllWorkspaces() const {
  return x11_window_->IsVisibleOnAllWorkspaces();
}

bool DesktopWindowTreeHostX11::SetWindowTitle(const base::string16& title) {
  return x11_window_->SetTitle(title);
}

void DesktopWindowTreeHostX11::ClearNativeFocus() {
  // This method is weird and misnamed. Instead of clearing the native focus,
  // it sets the focus to our content_window(), which will trigger a cascade
  // of focus changes into views.
  if (content_window() && aura::client::GetFocusClient(content_window()) &&
      content_window()->Contains(
          aura::client::GetFocusClient(content_window())->GetFocusedWindow())) {
    aura::client::GetFocusClient(content_window())
        ->FocusWindow(content_window());
  }
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

NonClientFrameView* DesktopWindowTreeHostX11::CreateNonClientFrameView() {
  return ShouldUseNativeFrame()
             ? new NativeFrameView(native_widget_delegate_->AsWidget())
             : nullptr;
}

bool DesktopWindowTreeHostX11::ShouldUseNativeFrame() const {
  return x11_window_->use_native_frame();
}

bool DesktopWindowTreeHostX11::ShouldWindowContentsBeTransparent() const {
  return x11_window_->has_alpha();
}

void DesktopWindowTreeHostX11::FrameTypeChanged() {
  Widget::FrameType new_type =
      native_widget_delegate_->AsWidget()->frame_type();
  if (new_type == Widget::FRAME_TYPE_DEFAULT) {
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

void DesktopWindowTreeHostX11::SetFullscreen(bool fullscreen) {
  if (is_fullscreen_ == fullscreen)
    return;

  is_fullscreen_ = fullscreen;
  if (is_fullscreen_)
    x11_window_->CancelResize();

  // Work around a bug where if we try to unfullscreen, metacity immediately
  // fullscreens us again. This is a little flickery and not necessary if
  // there's a gnome-panel, but it's not easy to detect whether there's a
  // panel or not.
  bool unmaximize_and_remaximize = !fullscreen && IsMaximized() &&
                                   ui::GuessWindowManager() == ui::WM_METACITY;

  if (unmaximize_and_remaximize)
    Restore();

  x11_window_->SetFullscreen(fullscreen);

  if (unmaximize_and_remaximize)
    Maximize();

  // Try to guess the size we will have after the switch to/from fullscreen:
  // - (may) avoid transient states
  // - works around Flash content which expects to have the size updated
  //   synchronously.
  // See https://crbug.com/361408
  gfx::Rect bounds = x11_window_->bounds();
  if (fullscreen) {
    display::Screen* screen = display::Screen::GetScreen();
    const display::Display display = screen->GetDisplayNearestWindow(window());
    restored_bounds_in_pixels_ = bounds;
    bounds = ToPixelRect(display.bounds());
  } else {
    bounds = restored_bounds_in_pixels_;
  }
  x11_window_->set_bounds(bounds);

  OnHostMovedInPixels(bounds.origin());
  OnHostResizedInPixels(bounds.size());

  if (x11_window_->IsFullscreen() == fullscreen) {
    Relayout();
    ResetWindowRegion();
  }
  // Else: the widget will be relaid out either when the window bounds change or
  // when |xwindow_|'s fullscreen state changes.
}

bool DesktopWindowTreeHostX11::IsFullscreen() const {
  return is_fullscreen_;
}

void DesktopWindowTreeHostX11::SetOpacity(float opacity) {
  x11_window_->SetOpacity(opacity);
}

void DesktopWindowTreeHostX11::SetAspectRatio(const gfx::SizeF& aspect_ratio) {
  x11_window_->SetAspectRatio(aspect_ratio);
}

void DesktopWindowTreeHostX11::SetWindowIcons(
    const gfx::ImageSkia& window_icon, const gfx::ImageSkia& app_icon) {
  x11_window_->SetWindowIcons(window_icon, app_icon);
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
  x11_window_->FlashFrame(flash_frame);
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
  x11_window_->UpdateMinAndMaxSize();
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
// DesktopWindowTreeHostX11, aura::WindowTreeHost implementation:

gfx::Transform DesktopWindowTreeHostX11::GetRootTransform() const {
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  if (IsVisible()) {
    aura::Window* win = const_cast<aura::Window*>(window());
    display = display::Screen::GetScreen()->GetDisplayNearestWindow(win);
  }

  float scale = display.device_scale_factor();
  gfx::Transform transform;
  transform.Scale(scale, scale);
  return transform;
}

ui::EventSource* DesktopWindowTreeHostX11::GetEventSource() {
  return this;
}

gfx::AcceleratedWidget DesktopWindowTreeHostX11::GetAcceleratedWidget() {
  return x11_window_->window();
}

void DesktopWindowTreeHostX11::ShowImpl() {
  Show(ui::SHOW_STATE_NORMAL, gfx::Rect());
}

void DesktopWindowTreeHostX11::HideImpl() {
  if (x11_window_->Hide())
    SetVisible(false);
}

gfx::Rect DesktopWindowTreeHostX11::GetBoundsInPixels() const {
  return x11_window_->bounds();
}

void DesktopWindowTreeHostX11::SetBoundsInPixels(
    const gfx::Rect& requested_bounds_in_pixel) {
  gfx::Rect bounds = x11_window_->bounds();
  gfx::Rect bounds_in_pixels(requested_bounds_in_pixel.origin(),
                             AdjustSize(requested_bounds_in_pixel.size()));

  bool origin_changed = bounds.origin() != bounds_in_pixels.origin();
  bool size_changed = bounds.size() != bounds_in_pixels.size();

  if (size_changed) {
    // Only cancel the delayed resize task if we're already about to call
    // OnHostResized in this function.
    x11_window_->CancelResize();
  }

  x11_window_->SetBounds(bounds_in_pixels);

  if (origin_changed)
    native_widget_delegate_->AsWidget()->OnNativeWidgetMove();

  if (size_changed) {
    OnHostResizedInPixels(bounds_in_pixels.size());
    ResetWindowRegion();
  }
}

gfx::Point DesktopWindowTreeHostX11::GetLocationOnScreenInPixels() const {
  return x11_window_->bounds().origin();
}

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

  x11_window_->GrabPointer();
}

void DesktopWindowTreeHostX11::ReleaseCapture() {
  if (g_current_capture == this) {
    // Release mouse grab asynchronously. A window managed by Chrome is likely
    // the topmost window underneath the mouse so the capture release being
    // asynchronous is likely inconsequential.
    g_current_capture = nullptr;
    x11_window_->ReleasePointerGrab();

    OnHostLostWindowCapture();
  }
}

bool DesktopWindowTreeHostX11::CaptureSystemKeyEventsImpl(
    base::Optional<base::flat_set<ui::DomCode>> dom_codes) {
  // Only one KeyboardHook should be active at a time, otherwise there will be
  // problems with event routing (i.e. which Hook takes precedence) and
  // destruction ordering.
  DCHECK(!keyboard_hook_);
  keyboard_hook_ = ui::KeyboardHook::CreateModifierKeyboardHook(
      std::move(dom_codes), GetAcceleratedWidget(),
      base::BindRepeating(&DesktopWindowTreeHostX11::DispatchKeyEvent,
                          base::Unretained(this)));

  return keyboard_hook_ != nullptr;
}

void DesktopWindowTreeHostX11::ReleaseSystemKeyEventCapture() {
  keyboard_hook_.reset();
}

bool DesktopWindowTreeHostX11::IsKeyLocked(ui::DomCode dom_code) {
  return keyboard_hook_ && keyboard_hook_->IsKeyLocked(dom_code);
}

void DesktopWindowTreeHostX11::SetCursorNative(gfx::NativeCursor cursor) {
  x11_window_->SetCursor(cursor.platform());
}

void DesktopWindowTreeHostX11::MoveCursorToScreenLocationInPixels(
    const gfx::Point& location_in_pixels) {
  x11_window_->MoveCursorTo(location_in_pixels);
}

void DesktopWindowTreeHostX11::OnCursorVisibilityChangedNative(bool show) {
  // TODO(erg): Conditional on us enabling touch on desktop linux builds, do
  // the same tap-to-click disabling here that chromeos does.
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostX11, display::DisplayObserver implementation:

void DesktopWindowTreeHostX11::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  aura::WindowTreeHost::OnDisplayMetricsChanged(display, changed_metrics);

  if ((changed_metrics & DISPLAY_METRIC_DEVICE_SCALE_FACTOR) &&
      display::Screen::GetScreen()->GetDisplayNearestWindow(window()).id() ==
          display.id()) {
    // When the scale factor changes, also pretend that a resize
    // occured so that the window layout will be refreshed and a
    // compositor redraw will be scheduled.  This is weird, but works.
    // TODO(thomasanderson): Figure out a more direct way of doing
    // this.
    x11_window_->DispatchResize();
  }
}

void DesktopWindowTreeHostX11::OnXWindowCreated() {
  if (auto* source = ui::PlatformEventSource::GetInstance())
    source->AddPlatformEventDispatcher(this);

  open_windows().push_front(x11_window_->window());
}

void DesktopWindowTreeHostX11::OnXWindowMapped() {
  for (DesktopWindowTreeHostObserverX11& observer : observer_list_)
    observer.OnWindowMapped(x11_window_->window());
}

void DesktopWindowTreeHostX11::OnXWindowUnmapped() {
  for (DesktopWindowTreeHostObserverX11& observer : observer_list_)
    observer.OnWindowUnmapped(x11_window_->window());
}

void DesktopWindowTreeHostX11::OnXWindowStateChanged() {
  bool was_minimized = x11_window_->was_minimized();
  bool is_minimized = IsMinimized();

  // Propagate the window minimization information to the content window, so
  // the render side can update its visibility properly. OnWMStateUpdated() is
  // called by PropertyNofify event from DispatchEvent() when the browser is
  // minimized or shown from minimized state. On Windows, this is realized by
  // calling OnHostResizedInPixels() with an empty size. In particular,
  // HWNDMessageHandler::GetClientAreaBounds() returns an empty size when the
  // window is minimized. On Linux, returning empty size in GetBounds() or
  // SetBoundsInPixels() does not work.
  // We also propagate the minimization to the compositor, to makes sure that we
  // don't draw any 'blank' frames that could be noticed in applications such as
  // window manager previews, which show content even when a window is
  // minimized.
  if (is_minimized != was_minimized) {
    if (is_minimized) {
      SetVisible(false);
      content_window()->Hide();
    } else {
      content_window()->Show();
      SetVisible(true);
    }
  }

  if (restored_bounds_in_pixels_.IsEmpty()) {
    if (IsMaximized()) {
      // The request that we become maximized originated from a different
      // process. |bounds_in_pixels_| already contains our maximized bounds. Do
      // a best effort attempt to get restored bounds by setting it to our
      // previously set bounds (and if we get this wrong, we aren't any worse
      // off since we'd otherwise be returning our maximized bounds).
      restored_bounds_in_pixels_ = x11_window_->previous_bounds();
    }
  } else if (!IsMaximized() && !IsFullscreen()) {
    // If we have restored bounds, but WM_STATE no longer claims to be
    // maximized or fullscreen, we should clear our restored bounds.
    restored_bounds_in_pixels_ = gfx::Rect();
  }

  // Now that we have different window properties, we may need to relayout the
  // window. (The windows code doesn't need this because their window change is
  // synchronous.)
  Relayout();
  ResetWindowRegion();
}

void DesktopWindowTreeHostX11::OnXWindowWorkspaceChanged() {
  OnHostWorkspaceChanged();
}

void DesktopWindowTreeHostX11::OnXWindowDamageEvent(
    const gfx::Rect& damage_rect_in_pixels) {
  compositor()->ScheduleRedrawRect(damage_rect_in_pixels);
}

void DesktopWindowTreeHostX11::OnXWindowKeyEvent(ui::KeyEvent* key_event) {
  DispatchKeyEvent(key_event);
}

void DesktopWindowTreeHostX11::OnXWindowMouseEvent(ui::MouseEvent* mouseev) {
  DispatchMouseEvent(mouseev);
}

void DesktopWindowTreeHostX11::OnXWindowTouchEvent(
    ui::TouchEvent* touch_event) {
  DispatchTouchEvent(touch_event);
}

void DesktopWindowTreeHostX11::OnXWindowScrollEvent(
    ui::ScrollEvent* scroll_event) {
  SendEventToSink(scroll_event);
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

void DesktopWindowTreeHostX11::OnXWindowChildCrossingEvent(XEvent* xev) {
  DCHECK(xev);
  ui::MouseEvent mouse_event(xev);
  DispatchMouseEvent(&mouse_event);
}

void DesktopWindowTreeHostX11::OnXWindowSizeChanged(
    const gfx::Size& size_in_pixels) {
  OnHostResizedInPixels(size_in_pixels);
  ResetWindowRegion();
}

void DesktopWindowTreeHostX11::OnXWindowCloseRequested() {
  OnHostCloseRequested();
}

void DesktopWindowTreeHostX11::OnXWindowMoved(const gfx::Point& window_origin) {
  OnHostMovedInPixels(window_origin);
}

void DesktopWindowTreeHostX11::OnXWindowLostPointerGrab() {
  dispatcher()->OnHostLostMouseGrab();
}

void DesktopWindowTreeHostX11::OnXWindowLostCapture() {
  OnHostLostWindowCapture();
}

void DesktopWindowTreeHostX11::OnXWindowIsActiveChanged(bool active) {
  if (active) {
    // TODO(thomasanderson): Remove this window shuffling and use XWindowCache
    // instead.
    ::Window xwindow = x11_window_->window();
    open_windows().remove(xwindow);
    open_windows().insert(open_windows().begin(), xwindow);
  }
  desktop_native_widget_aura_->HandleActivationChanged(active);
  native_widget_delegate_->AsWidget()->GetRootView()->SchedulePaint();
}

gfx::Size DesktopWindowTreeHostX11::GetMinimumSizeForXWindow() {
  return ToPixelRect(gfx::Rect(native_widget_delegate_->GetMinimumSize()))
      .size();
}

gfx::Size DesktopWindowTreeHostX11::GetMaximumSizeForXWindow() {
  return ToPixelRect(gfx::Rect(native_widget_delegate_->GetMaximumSize()))
      .size();
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostX11, private:

void DesktopWindowTreeHostX11::InitX11Window(const Widget::InitParams& params) {
  // Calculate initial bounds
  gfx::Rect bounds_in_pixels = ToPixelRect(params.bounds);
  gfx::Size adjusted_size = AdjustSize(bounds_in_pixels.size());
  bounds_in_pixels.set_size(adjusted_size);

  // Set the background color on startup to make the initial flickering
  // happening between the XWindow is mapped and the first expose event
  // is completely handled less annoying. If possible, we use the content
  // window's background color, otherwise we fallback to white.
  base::Optional<int> background_color;
  const views::LinuxUI* linux_ui = views::LinuxUI::instance();
  if (linux_ui && content_window()) {
    ui::NativeTheme::ColorId target_color;
    switch (params.type) {
      case Widget::InitParams::TYPE_BUBBLE:
        target_color = ui::NativeTheme::kColorId_BubbleBackground;
        break;
      case Widget::InitParams::TYPE_TOOLTIP:
        target_color = ui::NativeTheme::kColorId_TooltipBackground;
        break;
      default:
        target_color = ui::NativeTheme::kColorId_WindowBackground;
        break;
    }
    ui::NativeTheme* theme = linux_ui->GetNativeTheme(content_window());
    background_color = theme->GetSystemColor(target_color);
  }

  // Create window configuration and initialize it
  ui::XWindow::Configuration config =
      ConvertInitParamsToX11WindowConfig(params);
  config.bounds = bounds_in_pixels;
  config.background_color = background_color;
  config.prefer_dark_theme = linux_ui && linux_ui->PreferDarkTheme();
  config.icon = ViewsDelegate::GetInstance()->GetDefaultWindowIcon();
  x11_window_->Init(config);

  // Disable compositing on tooltips as a workaround for
  // https://crbug.com/442111.
  CreateCompositor(viz::FrameSinkId(),
                   params.force_software_compositing ||
                       params.type == Widget::InitParams::TYPE_TOOLTIP);

  if (ui::IsSyncExtensionAvailable()) {
    compositor_observer_ = std::make_unique<SwapWithNewSizeObserverHelper>(
        compositor(), base::BindRepeating(
                          &DesktopWindowTreeHostX11::OnCompleteSwapWithNewSize,
                          base::Unretained(this)));
  }
  OnAcceleratedWidgetAvailable();
}

gfx::Size DesktopWindowTreeHostX11::AdjustSize(
    const gfx::Size& requested_size_in_pixels) {
  std::vector<display::Display> displays =
      display::Screen::GetScreen()->GetAllDisplays();
  // Compare against all monitor sizes. The window manager can move the window
  // to whichever monitor it wants.
  for (const auto& display : displays) {
    if (requested_size_in_pixels == display.GetSizeInPixel()) {
      return gfx::Size(requested_size_in_pixels.width() - 1,
                       requested_size_in_pixels.height() - 1);
    }
  }

  // Do not request a 0x0 window size. It causes an XError.
  gfx::Size size_in_pixels = requested_size_in_pixels;
  size_in_pixels.SetToMax(gfx::Size(1, 1));
  return size_in_pixels;
}

void DesktopWindowTreeHostX11::SetUseNativeFrame(bool use_native_frame) {
  x11_window_->SetUseNativeFrame(use_native_frame);
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
  if (native_widget_delegate_->AsWidget()->IsActive())
    SendEventToSink(event);
}

void DesktopWindowTreeHostX11::ResetWindowRegion() {
  _XRegion* xregion = nullptr;
  if (!x11_window_->use_custom_shape() && !IsMaximized() && !IsFullscreen()) {
    SkPath window_mask;
    Widget* widget = native_widget_delegate_->AsWidget();
    if (widget->non_client_view()) {
      // Some frame views define a custom (non-rectangular) window mask. If
      // so, use it to define the window shape. If not, fall through.
      widget->non_client_view()->GetWindowMask(x11_window_->bounds().size(),
                                               &window_mask);
      if (window_mask.countPoints() > 0) {
        xregion = gfx::CreateRegionFromSkPath(window_mask);
      }
    }
  }
  x11_window_->UpdateWindowRegion(xregion);
}


std::list<XID>& DesktopWindowTreeHostX11::open_windows() {
  if (!open_windows_)
    open_windows_ = new std::list<XID>();
  return *open_windows_;
}

void DesktopWindowTreeHostX11::MapWindow(ui::WindowShowState show_state) {
  if (show_state != ui::SHOW_STATE_DEFAULT &&
      show_state != ui::SHOW_STATE_NORMAL &&
      show_state != ui::SHOW_STATE_INACTIVE &&
      show_state != ui::SHOW_STATE_MAXIMIZED) {
    // It will behave like SHOW_STATE_NORMAL.
    NOTIMPLEMENTED_LOG_ONCE();
  }

  // If SHOW_STATE_INACTIVE, tell the window manager not to focus the window
  // when mapping. This is done by setting the _NET_WM_USER_TIME to 0. See e.g.
  // http://standards.freedesktop.org/wm-spec/latest/ar01s05.html
  bool inactive = show_state == ui::SHOW_STATE_INACTIVE;

  x11_window_->Map(inactive);
}

void DesktopWindowTreeHostX11::SetWindowTransparency() {
  bool has_alpha = x11_window_->has_alpha();
  compositor()->SetBackgroundColor(has_alpha ? SK_ColorTRANSPARENT
                                             : SK_ColorWHITE);
  window()->SetTransparent(has_alpha);
  content_window()->SetTransparent(has_alpha);
}

void DesktopWindowTreeHostX11::Relayout() {
  Widget* widget = native_widget_delegate_->AsWidget();
  NonClientView* non_client_view = widget->non_client_view();
  // non_client_view may be NULL, especially during creation.
  if (non_client_view) {
    non_client_view->client_view()->InvalidateLayout();
    non_client_view->InvalidateLayout();
  }
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostX11, ui::PlatformEventDispatcher implementation:

bool DesktopWindowTreeHostX11::CanDispatchEvent(
    const ui::PlatformEvent& event) {
  return x11_window_->IsTargetedBy(*event);
}

uint32_t DesktopWindowTreeHostX11::DispatchEvent(
    const ui::PlatformEvent& event) {
  XEvent* xev = event;

  TRACE_EVENT1("views", "DesktopWindowTreeHostX11::Dispatch",
               "event->type", event->type);

  x11_window_->ProcessEvent(xev);
  return ui::POST_DISPATCH_STOP_PROPAGATION;
}

void DesktopWindowTreeHostX11::DelayedChangeFrameType(Widget::FrameType type) {
  SetUseNativeFrame(type == Widget::FRAME_TYPE_FORCE_NATIVE);
  // Replace the frame and layout the contents. Even though we don't have a
  // swappable glass frame like on Windows, we still replace the frame because
  // the button assets don't update otherwise.
  native_widget_delegate_->AsWidget()->non_client_view()->UpdateFrame();
}

gfx::Rect DesktopWindowTreeHostX11::ToDIPRect(
    const gfx::Rect& rect_in_pixels) const {
  gfx::RectF rect_in_dip = gfx::RectF(rect_in_pixels);
  GetRootTransform().TransformRectReverse(&rect_in_dip);
  return gfx::ToEnclosingRect(rect_in_dip);
}

gfx::Rect DesktopWindowTreeHostX11::ToPixelRect(
    const gfx::Rect& rect_in_dip) const {
  gfx::RectF rect_in_pixels = gfx::RectF(rect_in_dip);
  GetRootTransform().TransformRect(&rect_in_pixels);
  return gfx::ToEnclosingRect(rect_in_pixels);
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

aura::Window* DesktopWindowTreeHostX11::content_window() {
  return desktop_native_widget_aura_->content_window();
}

void DesktopWindowTreeHostX11::OnCompleteSwapWithNewSize(
    const gfx::Size& size) {
  x11_window_->NotifySwapAfterResize();
}

base::flat_map<std::string, std::string>
DesktopWindowTreeHostX11::GetKeyboardLayoutMap() {
  if (views::LinuxUI::instance())
    return views::LinuxUI::instance()->GetKeyboardLayoutMap();
  return {};
}

void DesktopWindowTreeHostX11::SetVisualId(VisualID visual_id) {
  DCHECK_EQ(x11_window_->window(), x11::None);
  x11_window_->set_visual_id(visual_id);
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
