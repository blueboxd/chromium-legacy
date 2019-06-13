// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_icons/status_icon_linux_x11.h"

#include "base/logging.h"

StatusIconLinuxX11::StatusIconLinuxX11() = default;

StatusIconLinuxX11::~StatusIconLinuxX11() = default;

void StatusIconLinuxX11::SetImage(const gfx::ImageSkia& image) {
  NOTIMPLEMENTED();
}

void StatusIconLinuxX11::SetToolTip(const base::string16& tool_tip) {
  NOTIMPLEMENTED();
}

void StatusIconLinuxX11::UpdatePlatformContextMenu(ui::MenuModel* model) {
  NOTIMPLEMENTED();
}
