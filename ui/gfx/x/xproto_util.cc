// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/xproto_util.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "ui/gfx/x/xproto.h"

namespace x11 {

void LogErrorEventDescription(const XErrorEvent& error_event) {
  // This function may make some expensive round trips (XListExtensions,
  // XQueryExtension), but the only effect this function has is LOG(WARNING),
  // so early-return if the log would never be sent anyway.
  if (!LOG_IS_ON(WARNING))
    return;

  char error_str[256];
  char request_str[256];

  XDisplay* dpy = error_event.display;
  XProto conn{dpy};
  XGetErrorText(dpy, error_event.error_code, error_str, sizeof(error_str));

  strncpy(request_str, "Unknown", sizeof(request_str));
  if (error_event.request_code < 128) {
    std::string num = base::NumberToString(error_event.request_code);
    XGetErrorDatabaseText(dpy, "XRequest", num.c_str(), "Unknown", request_str,
                          sizeof(request_str));
  } else {
    if (auto response = conn.ListExtensions({}).Sync()) {
      for (const auto& str : response->names) {
        int ext_code, first_event, first_error;
        const char* name = str.name.c_str();
        XQueryExtension(dpy, name, &ext_code, &first_event, &first_error);
        if (error_event.request_code == ext_code) {
          std::string msg =
              base::StringPrintf("%s.%d", name, error_event.minor_code);
          XGetErrorDatabaseText(dpy, "XRequest", msg.c_str(), "Unknown",
                                request_str, sizeof(request_str));
          break;
        }
      }
    }
  }

  LOG(WARNING) << "X error received: "
               << "serial " << error_event.serial << ", "
               << "error_code " << static_cast<int>(error_event.error_code)
               << " (" << error_str << "), "
               << "request_code " << static_cast<int>(error_event.request_code)
               << ", "
               << "minor_code " << static_cast<int>(error_event.minor_code)
               << " (" << request_str << ")";
}

}  // namespace x11
