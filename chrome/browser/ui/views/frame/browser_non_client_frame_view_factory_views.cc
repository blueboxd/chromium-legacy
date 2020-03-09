// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view.h"
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view_layout.h"

#if defined(OS_WIN)
#include "chrome/browser/ui/views/frame/glass_browser_frame_view.h"
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/frame/desktop_linux_browser_frame_view.h"
#include "chrome/browser/ui/views/frame/desktop_linux_browser_frame_view_layout.h"
#include "ui/views/linux_ui/linux_ui.h"
#include "ui/views/linux_ui/nav_button_provider.h"
#endif

namespace chrome {

namespace {

OpaqueBrowserFrameView* CreateOpaqueBrowserFrameView(
    BrowserFrame* frame,
    BrowserView* browser_view) {
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  auto* linux_ui = views::LinuxUI::instance();
  auto* profile = browser_view->browser()->profile();
  auto* theme_service_factory = ThemeServiceFactory::GetForProfile(profile);
  if (linux_ui && theme_service_factory->UsingSystemTheme()) {
    auto nav_button_provider = linux_ui->CreateNavButtonProvider();
    if (nav_button_provider) {
      return new DesktopLinuxBrowserFrameView(
          frame, browser_view,
          new DesktopLinuxBrowserFrameViewLayout(nav_button_provider.get()),
          std::move(nav_button_provider));
    }
  }
#endif
  return new OpaqueBrowserFrameView(frame, browser_view,
                                    new OpaqueBrowserFrameViewLayout());
}

}  // namespace

BrowserNonClientFrameView* CreateBrowserNonClientFrameView(
    BrowserFrame* frame,
    BrowserView* browser_view) {
#if defined(OS_WIN)
  if (frame->ShouldUseNativeFrame())
    return new GlassBrowserFrameView(frame, browser_view);
#endif
  OpaqueBrowserFrameView* view =
      CreateOpaqueBrowserFrameView(frame, browser_view);
  view->InitViews();
  return view;
}

}  // namespace chrome
