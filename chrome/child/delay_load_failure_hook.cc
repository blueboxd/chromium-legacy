// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// windows.h needs to be included before delayimp.h.
#include <windows.h>

#include <delayimp.h>

#include "base/debug/alias.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"

namespace {

// Delay load failure hook that generates a crash report. By default a failure
// to delay load will trigger an exception handled by the delay load runtime and
// this won't generate a crash report.
extern "C" FARPROC WINAPI DelayLoadFailureHook(unsigned reason,
                                               DelayLoadInfo* dll_info) {
  char dll_name[256];
  base::strlcpy(dll_name, dll_info->szDll, base::size(dll_name));
  base::debug::Alias(&dll_name);

  CHECK(false);
  return 0;
}

}  // namespace

// Set the delay load failure hook to the function above.
//
// The |__pfnDliFailureHook2| failure notification hook gets called
// automatically by the delay load runtime in case of failure, see
// https://docs.microsoft.com/en-us/cpp/build/reference/failure-hooks?view=vs-2019
// for more information about this.
extern "C" const PfnDliHook __pfnDliFailureHook2 = DelayLoadFailureHook;
