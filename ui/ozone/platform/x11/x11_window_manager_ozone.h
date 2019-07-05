// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_WINDOW_MANAGER_OZONE_H_
#define UI_OZONE_PLATFORM_X11_X11_WINDOW_MANAGER_OZONE_H_

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

class X11WindowOzone;

class X11WindowManagerOzone {
 public:
  X11WindowManagerOzone();
  ~X11WindowManagerOzone();

  // Sets a given X11WindowOzone as the recipient for events and calls
  // OnLostCapture for another |event_grabber_| if it has been set previously.
  void GrabEvents(X11WindowOzone* window);

  // Unsets a given X11WindowOzone as the recipient for events and calls
  // OnLostCapture.
  void UngrabEvents(X11WindowOzone* window);

  // Gets the current X11WindowOzone recipient of mouse events.
  X11WindowOzone* event_grabber() const { return event_grabber_; }

  // Gets the window corresponding to the AcceleratedWidget |widget|.
  void AddWindow(X11WindowOzone* window);
  void RemoveWindow(X11WindowOzone* window);
  X11WindowOzone* GetWindow(gfx::AcceleratedWidget widget) const;

 private:
  X11WindowOzone* event_grabber_;

  base::flat_map<gfx::AcceleratedWidget, X11WindowOzone*> windows_;

  DISALLOW_COPY_AND_ASSIGN(X11WindowManagerOzone);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_WINDOW_MANAGER_OZONE_H_
