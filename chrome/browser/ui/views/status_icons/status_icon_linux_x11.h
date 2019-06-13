// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_LINUX_X11_H_
#define CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_LINUX_X11_H_

#include "base/macros.h"
#include "ui/views/linux_ui/status_icon_linux.h"

// A status icon that uses the XEmbed protocol.
// https://standards.freedesktop.org/xembed-spec/xembed-spec-latest.html
class StatusIconLinuxX11 : public views::StatusIconLinux {
 public:
  StatusIconLinuxX11();
  ~StatusIconLinuxX11() override;

  // StatusIcon:
  void SetImage(const gfx::ImageSkia& image) override;
  void SetToolTip(const base::string16& tool_tip) override;
  void UpdatePlatformContextMenu(ui::MenuModel* model) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(StatusIconLinuxX11);
};

#endif  // CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_LINUX_X11_H_
