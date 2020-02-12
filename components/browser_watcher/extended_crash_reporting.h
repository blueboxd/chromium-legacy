// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_WATCHER_EXTENDED_CRASH_REPORTING_H_
#define COMPONENTS_BROWSER_WATCHER_EXTENDED_CRASH_REPORTING_H_

#include <stdint.h>

#include "base/strings/string_piece.h"

namespace browser_watcher {

class ExtendedCrashReporting {
 public:
  // Adds or updates the global extended crash reporting data, if enabled.
  static void SetDataBool(base::StringPiece name, bool value);
  static void SetDataInt(base::StringPiece name, int64_t value);

  // Registers a vectored exception handler that stores exception details to the
  // activity report on crash.
  static void RegisterVEH();

 private:
  // Don't allow constructing this class.
  ExtendedCrashReporting() = delete;
};

}  // namespace browser_watcher

#endif  // COMPONENTS_BROWSER_WATCHER_EXTENDED_CRASH_REPORTING_H_
