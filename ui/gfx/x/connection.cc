// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/connection.h"

#include "base/command_line.h"
#include "ui/gfx/x/x11_switches.h"

namespace x11 {

namespace {

XDisplay* OpenNewXDisplay() {
  if (!XInitThreads())
    return nullptr;
  std::string display_str =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kX11Display);
  return XOpenDisplay(display_str.empty() ? nullptr : display_str.c_str());
}

}  // namespace

Connection* Connection::Get() {
  static Connection* instance = new Connection(OpenNewXDisplay());
  return instance;
}

Connection::Connection(XDisplay* display) : XProto(display) {}

}  // namespace x11
