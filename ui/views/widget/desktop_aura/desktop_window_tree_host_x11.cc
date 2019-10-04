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

std::list<XID>* DesktopWindowTreeHostX11::open_windows_ = nullptr;

DEFINE_UI_CLASS_PROPERTY_KEY(aura::Window*, kViewsWindowForRootWindow, NULL)

DEFINE_UI_CLASS_PROPERTY_KEY(DesktopWindowTreeHostX11*,
                             kHostForRootWindow,
                             NULL)

namespace {

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
  DesktopWindowTreeHostLinux::Init(params);

  // Set XEventDelegate to receive selection, drag&drop and raw key events.
  //
  // TODO(https://crbug.com/990756): There are two cases of this delegate:
  // XEvents for DragAndDrop client and raw key events. DragAndDrop could be
  // unified so that DragAndrDropClientOzone is used and XEvent are handled on
  // platform level.
  static_cast<ui::X11Window*>(platform_window())->SetXEventDelegate(this);
}

void DesktopWindowTreeHostX11::OnNativeWidgetCreated(
    const Widget::InitParams& params) {
  window()->SetProperty(kViewsWindowForRootWindow, GetContentWindow());
  window()->SetProperty(kHostForRootWindow, this);

  // Ensure that the X11DesktopHandler exists so that it tracks create/destroy
  // notify events.
  X11DesktopHandler::get();

  x11_window_move_client_ = std::make_unique<X11DesktopWindowMoveClient>();
  wm::SetWindowMoveClient(window(), x11_window_move_client_.get());

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

Widget::MoveLoopResult DesktopWindowTreeHostX11::RunMoveLoop(
    const gfx::Vector2d& drag_offset,
    Widget::MoveLoopSource source,
    Widget::MoveLoopEscapeBehavior escape_behavior) {
  wm::WindowMoveSource window_move_source =
      source == Widget::MOVE_LOOP_SOURCE_MOUSE ? wm::WINDOW_MOVE_SOURCE_MOUSE
                                               : wm::WINDOW_MOVE_SOURCE_TOUCH;
  if (x11_window_move_client_->RunMoveLoop(GetContentWindow(), drag_offset,
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

bool DesktopWindowTreeHostX11::ShouldUseDesktopNativeCursorManager() const {
  return true;
}

bool DesktopWindowTreeHostX11::ShouldCreateVisibilityController() const {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostX11, private:

void DesktopWindowTreeHostX11::SetUseNativeFrame(bool use_native_frame) {
  GetXWindow()->SetUseNativeFrame(use_native_frame);
  ResetWindowRegion();
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

void DesktopWindowTreeHostX11::OnBoundsChanged(const gfx::Rect& new_bounds) {
  ResetWindowRegion();
  WindowTreeHostPlatform::OnBoundsChanged(new_bounds);
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
  DesktopWindowTreeHostPlatform::OnActivationChanged(active);
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
        DesktopWindowTreeHostLinux::DispatchEvent(&keydown_event);
      }
      break;
    case KeyRelease:

      // There is no way to deactivate a window in X11 so ignore input if
      // window is supposed to be 'inactive'.
      if (!IsActive() && !HasCapture())
        break;

      if (!ShouldDiscardKeyEvent(xev)) {
        ui::KeyEvent keyup_event(xev);
        DesktopWindowTreeHostLinux::DispatchEvent(&keyup_event);
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
