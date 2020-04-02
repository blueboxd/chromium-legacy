// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_GTK_UI_DELEGATE_X11_H_
#define UI_GTK_GTK_UI_DELEGATE_X11_H_

#include "base/component_export.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/gtk/gtk_ui_delegate.h"

using GdkDisplay = struct _GdkDisplay;

namespace ui {

// GtkUiDelegate implementation for desktop Linux X11 backends.
//
// TODO(crbug.com/1002674): For now, this is used by both Aura (legacy) and
// Ozone X11. Move this into X11 Ozone backend once Linux Chrome migration to
// Ozone is completed.
class COMPONENT_EXPORT(UI_GTK_X) GtkUiDelegateX11 : public GtkUiDelegate {
 public:
  explicit GtkUiDelegateX11(XDisplay* display);
  GtkUiDelegateX11(const GtkUiDelegateX11&) = delete;
  GtkUiDelegateX11& operator=(const GtkUiDelegateX11&) = delete;
  ~GtkUiDelegateX11() override;

  // GtkUiDelegate:
  void OnInitialized() override;
  GdkKeymap* GetGdkKeymap() override;
  GdkWindow* GetGdkWindow(gfx::AcceleratedWidget window_id) override;
  bool SetGdkWindowTransientFor(GdkWindow* window,
                                gfx::AcceleratedWidget parent) override;
  void ShowGtkWindow(GtkWindow* window) override;

 private:
  GdkDisplay* GetGdkDisplay();

  XDisplay* const xdisplay_;
  GdkDisplay* display_ = nullptr;
};

}  // namespace ui

#endif  // UI_GTK_GTK_UI_DELEGATE_X11_H_
