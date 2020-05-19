// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/x11_error_tracker.h"

#include "base/check.h"
#include "ui/gfx/x/x11_types.h"

namespace {

unsigned char g_x11_error_code = 0;
static gfx::X11ErrorTracker* g_handler = nullptr;

int X11ErrorHandler(Display* display, XErrorEvent* error) {
  g_x11_error_code = error->error_code;
  return 0;
}

}  // namespace

namespace gfx {

X11ErrorTracker::X11ErrorTracker() {
  // This is a non-exhaustive check for incorrect usage. It disallows nested
  // X11ErrorTracker instances on the same thread.
  DCHECK(g_handler == nullptr);
  g_handler = this;
  XSync(GetXDisplay(), False);
  old_handler_ = XSetErrorHandler(X11ErrorHandler);
  g_x11_error_code = 0;
}

X11ErrorTracker::~X11ErrorTracker() {
  g_handler = nullptr;
  XSetErrorHandler(old_handler_);
}

bool X11ErrorTracker::FoundNewError() {
  XSync(GetXDisplay(), False);
  unsigned char error = g_x11_error_code;
  g_x11_error_code = 0;
  return error != 0;
}

}  // namespace gfx
