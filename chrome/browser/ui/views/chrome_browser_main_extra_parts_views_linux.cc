// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_browser_main_extra_parts_views_linux.h"

#include "chrome/browser/themes/theme_service_aura_linux.h"
#include "chrome/browser/ui/views/theme_profile_key.h"
#include "ui/base/buildflags.h"
#include "ui/views/linux_ui/linux_ui.h"

#if BUILDFLAG(USE_GTK)
#include "chrome/browser/ui/gtk/gtk_ui.h"
#include "ui/gtk/gtk_ui_delegate.h"
#endif

namespace {

views::LinuxUI* BuildLinuxUI() {
  views::LinuxUI* linux_ui = nullptr;
  // GtkUi is the only LinuxUI implementation for now.
#if BUILDFLAG(USE_GTK)
  DCHECK(ui::GtkUiDelegate::instance());
  linux_ui = BuildGtkUi(ui::GtkUiDelegate::instance());
#endif
  return linux_ui;
}

}  // namespace

ChromeBrowserMainExtraPartsViewsLinux::ChromeBrowserMainExtraPartsViewsLinux() =
    default;

ChromeBrowserMainExtraPartsViewsLinux::
    ~ChromeBrowserMainExtraPartsViewsLinux() = default;

void ChromeBrowserMainExtraPartsViewsLinux::ToolkitInitialized() {
  ChromeBrowserMainExtraPartsViews::ToolkitInitialized();

  views::LinuxUI* linux_ui = BuildLinuxUI();
  if (!linux_ui)
    return;

  linux_ui->SetUseSystemThemeCallback(
      base::BindRepeating([](aura::Window* window) {
        if (!window)
          return true;
        return ThemeServiceAuraLinux::ShouldUseSystemThemeForProfile(
            GetThemeProfileForWindow(window));
      }));

  // Update the device scale factor before initializing views
  // because its display::Screen instance depends on it.
  linux_ui->UpdateDeviceScaleFactor();

  views::LinuxUI::SetInstance(linux_ui);
  linux_ui->Initialize();

  DCHECK(ui::LinuxInputMethodContextFactory::instance())
      << "LinuxUI must set LinuxInputMethodContextFactory instance.";
}
