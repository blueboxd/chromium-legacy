// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_desktop_window_tree_host_linux.h"

#include <utility>

#include "base/macros.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/desktop_browser_frame_aura_linux.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "ui/platform_window/extensions/x11_extension.h"

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHostLinux, public:

BrowserDesktopWindowTreeHostLinux::BrowserDesktopWindowTreeHostLinux(
    views::internal::NativeWidgetDelegate* native_widget_delegate,
    views::DesktopNativeWidgetAura* desktop_native_widget_aura,
    BrowserView* browser_view,
    BrowserFrame* browser_frame)
    : DesktopWindowTreeHostLinuxImpl(native_widget_delegate,
                                     desktop_native_widget_aura),
      browser_view_(browser_view),
      browser_frame_(browser_frame) {
  static_cast<DesktopBrowserFrameAuraLinux*>(
      browser_frame->native_browser_frame())
      ->set_host(this);
  browser_frame->set_frame_type(browser_frame->UseCustomFrame()
                                    ? views::Widget::FrameType::kForceCustom
                                    : views::Widget::FrameType::kForceNative);
}

BrowserDesktopWindowTreeHostLinux::~BrowserDesktopWindowTreeHostLinux() =
    default;

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHostLinux,
//     BrowserDesktopWindowTreeHost implementation:

views::DesktopWindowTreeHost*
BrowserDesktopWindowTreeHostLinux::AsDesktopWindowTreeHost() {
  return this;
}

int BrowserDesktopWindowTreeHostLinux::GetMinimizeButtonOffset() const {
  return 0;
}

bool BrowserDesktopWindowTreeHostLinux::UsesNativeSystemMenu() const {
  return false;
}

void BrowserDesktopWindowTreeHostLinux::TabDraggingStatusChanged(
    bool is_dragging) {
  // If there's no tabs left, the browser window is about to close, so don't
  // call SetOverrideRedirect() to prevent the window from flashing.
  if (!browser_view_->tabstrip()->GetModelCount())
    return;

  auto* x11_extension = GetX11Extension();
  if (x11_extension && x11_extension->IsWmTiling())
    x11_extension->SetOverrideRedirect(is_dragging);
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHostLinux,
//     DesktopWindowTreeHostLinuxImpl implementation:

void BrowserDesktopWindowTreeHostLinux::Init(
    const views::Widget::InitParams& params) {
  DesktopWindowTreeHostLinuxImpl::Init(std::move(params));

#if defined(USE_X11)
  // We have now created our backing X11 window. We now need to (possibly)
  // alert Unity that there's a menu bar attached to it.
  global_menu_bar_x11_ =
      std::make_unique<GlobalMenuBarX11>(browser_view_, this);
#endif
}

void BrowserDesktopWindowTreeHostLinux::CloseNow() {
#if defined(USE_X11)
  global_menu_bar_x11_.reset();
#endif
  DesktopWindowTreeHostLinuxImpl::CloseNow();
}

bool BrowserDesktopWindowTreeHostLinux::IsOverrideRedirect(
    bool is_tiling_wm) const {
  return (browser_frame_->tab_drag_kind() == TabDragKind::kAllTabs) &&
         is_tiling_wm;
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHost, public:

// static
BrowserDesktopWindowTreeHost*
BrowserDesktopWindowTreeHost::CreateBrowserDesktopWindowTreeHost(
    views::internal::NativeWidgetDelegate* native_widget_delegate,
    views::DesktopNativeWidgetAura* desktop_native_widget_aura,
    BrowserView* browser_view,
    BrowserFrame* browser_frame) {
  return new BrowserDesktopWindowTreeHostLinux(native_widget_delegate,
                                               desktop_native_widget_aura,
                                               browser_view, browser_frame);
}
