// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"

#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/platform_window/platform_window_handler/wm_move_resize_handler.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/views/linux_ui/linux_ui.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/desktop_aura/window_event_filter.h"
#include "ui/views/widget/widget.h"

namespace views {

DesktopWindowTreeHostLinux::DesktopWindowTreeHostLinux(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura)
    : DesktopWindowTreeHostPlatform(native_widget_delegate,
                                    desktop_native_widget_aura) {}

DesktopWindowTreeHostLinux::~DesktopWindowTreeHostLinux() = default;

void DesktopWindowTreeHostLinux::OnNativeWidgetCreated(
    const Widget::InitParams& params) {
  AddNonClientEventFilter();
  DesktopWindowTreeHostPlatform::OnNativeWidgetCreated(params);
}

void DesktopWindowTreeHostLinux::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  aura::WindowTreeHost::OnDisplayMetricsChanged(display, changed_metrics);

  if ((changed_metrics & DISPLAY_METRIC_DEVICE_SCALE_FACTOR) &&
      display::Screen::GetScreen()->GetDisplayNearestWindow(window()).id() ==
          display.id()) {
    // When the scale factor changes, also pretend that a resize
    // occurred so that the window layout will be refreshed and a
    // compositor redraw will be scheduled.  This is weird, but works.
    // TODO(thomasanderson): Figure out a more direct way of doing
    // this.
    OnHostResizedInPixels(GetBoundsInPixels().size());
  }
}

void DesktopWindowTreeHostLinux::OnClosed() {
  RemoveNonClientEventFilter();
  DesktopWindowTreeHostPlatform::OnClosed();
}

void DesktopWindowTreeHostLinux::AddAdditionalInitProperties(
    const Widget::InitParams& params,
    ui::PlatformWindowInitProperties* properties) {
  // Set the background color on startup to make the initial flickering
  // happening between the XWindow is mapped and the first expose event
  // is completely handled less annoying. If possible, we use the content
  // window's background color, otherwise we fallback to white.
  base::Optional<int> background_color;
  const views::LinuxUI* linux_ui = views::LinuxUI::instance();
  if (linux_ui && content_window()) {
    ui::NativeTheme::ColorId target_color;
    switch (properties->type) {
      case ui::PlatformWindowType::kBubble:
        target_color = ui::NativeTheme::kColorId_BubbleBackground;
        break;
      case ui::PlatformWindowType::kTooltip:
        target_color = ui::NativeTheme::kColorId_TooltipBackground;
        break;
      default:
        target_color = ui::NativeTheme::kColorId_WindowBackground;
        break;
    }
    ui::NativeTheme* theme = linux_ui->GetNativeTheme(content_window());
    background_color = theme->GetSystemColor(target_color);
  }
  properties->prefer_dark_theme = linux_ui && linux_ui->PreferDarkTheme();
  properties->background_color = background_color;
  properties->icon = ViewsDelegate::GetInstance()->GetDefaultWindowIcon();

  properties->wm_class_name = params.wm_class_name;
  properties->wm_class_class = params.wm_class_class;
  properties->wm_role_name = params.wm_role_name;
}

void DesktopWindowTreeHostLinux::AddNonClientEventFilter() {
  DCHECK(!non_client_window_event_filter_);
  std::unique_ptr<WindowEventFilter> window_event_filter =
      std::make_unique<WindowEventFilter>(this);
  auto* wm_move_resize_handler = GetWmMoveResizeHandler(*platform_window());
  if (wm_move_resize_handler)
    window_event_filter->SetWmMoveResizeHandler(
        GetWmMoveResizeHandler(*(platform_window())));

  non_client_window_event_filter_ = std::move(window_event_filter);
  window()->AddPreTargetHandler(non_client_window_event_filter_.get());
}

void DesktopWindowTreeHostLinux::RemoveNonClientEventFilter() {
  if (!non_client_window_event_filter_)
    return;

  window()->RemovePreTargetHandler(non_client_window_event_filter_.get());
  non_client_window_event_filter_.reset();
}

// As DWTHX11 subclasses DWTHPlatform through DWTHLinux now (during transition
// period. see https://crbug.com/990756), we need to guard this factory method.
// TODO(msisov): remove this guard once DWTHX11 is finally merged into
// DWTHPlatform and .
#if !defined(USE_X11)
// static
DesktopWindowTreeHost* DesktopWindowTreeHost::Create(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura) {
  return new DesktopWindowTreeHostLinux(native_widget_delegate,
                                        desktop_native_widget_aura);
}
#endif

}  // namespace views
