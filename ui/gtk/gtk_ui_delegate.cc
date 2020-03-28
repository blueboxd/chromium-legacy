// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/gtk_ui_delegate.h"

namespace ui {

namespace {

GtkUiDelegate* g_gtk_ui_delegate = nullptr;

}  // namespace

void GtkUiDelegate::SetInstance(GtkUiDelegate* instance) {
  g_gtk_ui_delegate = instance;
}

GtkUiDelegate* GtkUiDelegate::instance() {
  return g_gtk_ui_delegate;
}

}  // namespace ui
