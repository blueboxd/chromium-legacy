// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/gtk_ui_delegate_wayland.h"

#include <gdk/gdkwayland.h>
#include <gtk/gtk.h>

#include "base/logging.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

GtkUiDelegateWayland::GtkUiDelegateWayland(WaylandConnection* connection)
    : connection_(connection) {
  DCHECK(connection_);
  gdk_set_allowed_backends("wayland");
}

GtkUiDelegateWayland::~GtkUiDelegateWayland() = default;

void GtkUiDelegateWayland::OnInitialized() {
  // Nothing to do upon initialization for Wayland.
}

GdkKeymap* GtkUiDelegateWayland::GetGdkKeymap() {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

GdkWindow* GtkUiDelegateWayland::GetGdkWindow(
    gfx::AcceleratedWidget window_id) {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

bool GtkUiDelegateWayland::SetGdkWindowTransientFor(
    GdkWindow* window,
    gfx::AcceleratedWidget parent) {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

}  // namespace ui
