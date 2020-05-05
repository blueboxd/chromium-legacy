// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/x/gtk_ui_delegate_x11.h"

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "base/check.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/x11.h"
#include "ui/gtk/x/gtk_event_loop_x11.h"

namespace ui {

GtkUiDelegateX11::GtkUiDelegateX11(XDisplay* display) : xdisplay_(display) {
  DCHECK(xdisplay_);
  gdk_set_allowed_backends("x11");
}

GtkUiDelegateX11::~GtkUiDelegateX11() = default;

void GtkUiDelegateX11::OnInitialized() {
  // Ensure the singleton instance of GtkEventLoopX11 is created and started.
  GtkEventLoopX11::EnsureInstance();
}

GdkKeymap* GtkUiDelegateX11::GetGdkKeymap() {
  return gdk_keymap_get_for_display(GetGdkDisplay());
}

GdkWindow* GtkUiDelegateX11::GetGdkWindow(gfx::AcceleratedWidget window_id) {
  GdkDisplay* display = GetGdkDisplay();
  GdkWindow* gdk_window = gdk_x11_window_lookup_for_display(display, window_id);
  if (gdk_window)
    g_object_ref(gdk_window);
  else
    gdk_window = gdk_x11_window_foreign_new_for_display(display, window_id);
  return gdk_window;
}

bool GtkUiDelegateX11::SetGdkWindowTransientFor(GdkWindow* window,
                                                gfx::AcceleratedWidget parent) {
  XSetTransientForHint(xdisplay_, GDK_WINDOW_XID(window), parent);
  return true;
}

GdkDisplay* GtkUiDelegateX11::GetGdkDisplay() {
  if (!display_) {
    GdkDisplay* display = gdk_x11_lookup_xdisplay(xdisplay_);
    display_ = !display ? gdk_display_get_default() : display;
  }
  return display_;
}

void GtkUiDelegateX11::ShowGtkWindow(GtkWindow* window) {
  // We need to call gtk_window_present after making the widgets visible to make
  // sure window gets correctly raised and gets focus.
  DCHECK(X11EventSource::HasInstance());
  gtk_window_present_with_time(window,
                               X11EventSource::GetInstance()->GetTimestamp());
}

}  // namespace ui
