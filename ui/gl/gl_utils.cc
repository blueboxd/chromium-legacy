// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains some useful utilities for the ui/gl classes.

#include "ui/gl/gl_utils.h"

#include "base/logging.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/gl_switches.h"

#if defined(OS_ANDROID)
#include "base/posix/eintr_wrapper.h"
#include "third_party/libsync/src/include/sync/sync.h"
#endif

namespace gl {

// Used by chrome://gpucrash and gpu_benchmarking_extension's
// CrashForTesting.
void Crash() {
  DVLOG(1) << "GPU: Simulating GPU crash";
  // Good bye, cruel world.
  volatile int* it_s_the_end_of_the_world_as_we_know_it = nullptr;
  *it_s_the_end_of_the_world_as_we_know_it = 0xdead;
}

#if defined(OS_ANDROID)
base::ScopedFD MergeFDs(base::ScopedFD a, base::ScopedFD b) {
  if (!a.is_valid())
    return b;
  if (!b.is_valid())
    return a;

  base::ScopedFD merged(HANDLE_EINTR(sync_merge("", a.get(), b.get())));
  if (!merged.is_valid())
    LOG(ERROR) << "Failed to merge fences.";
  return merged;
}
#endif

bool UsePassthroughCommandDecoder(const base::CommandLine* command_line) {
  std::string switch_value;
  if (command_line->HasSwitch(switches::kUseCmdDecoder)) {
    switch_value = command_line->GetSwitchValueASCII(switches::kUseCmdDecoder);
  }

  if (switch_value == kCmdDecoderPassthroughName) {
    return true;
  } else if (switch_value == kCmdDecoderValidatingName) {
    return false;
  } else {
    // Unrecognized or missing switch, use the default.
    return base::FeatureList::IsEnabled(
        features::kDefaultPassthroughCommandDecoder);
  }
}
}
