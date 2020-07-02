// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/web_test/web_test_shell_platform_delegate.h"

#include "base/command_line.h"
#include "content/shell/common/web_test/web_test_switches.h"

namespace content {

// static
bool WebTestShellPlatformDelegate::IsHeadless() {
  // Headless by default in web tests, unless overridden on the command line.
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableHeadlessMode);
}

}  // namespace content
