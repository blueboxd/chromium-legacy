// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_GTK_EVENT_LOOP_X11_H_
#define UI_GTK_GTK_EVENT_LOOP_X11_H_

#include "base/macros.h"
#include "ui/base/glib/glib_integers.h"

using GdkEvent = union _GdkEvent;
using GdkEventKey = struct _GdkEventKey;

namespace base {
template <typename Type>
struct DefaultSingletonTraits;
}

namespace ui {

class GtkEventLoopX11 {
 public:
  static GtkEventLoopX11* EnsureInstance();

 private:
  friend struct base::DefaultSingletonTraits<GtkEventLoopX11>;

  GtkEventLoopX11();
  GtkEventLoopX11(const GtkEventLoopX11&) = delete;
  GtkEventLoopX11& operator=(const GtkEventLoopX11&) = delete;
  ~GtkEventLoopX11();

  static void DispatchGdkEvent(GdkEvent* gdk_event, gpointer);
  static void ProcessGdkEventKey(const GdkEventKey& gdk_event_key);
};

}  // namespace ui

#endif  // UI_GTK_GTK_EVENT_LOOP_X11_H_
