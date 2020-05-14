// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/web_test/web_test_shell_platform_delegate.h"

namespace content {

// When other platforms define their own implementation of
// WebTestShellPlatformDelegate, move their implementation of these to their
// platform-specific file so they can see the definition of their own
// WebTestShellData.
#if !defined(OS_MACOSX)
WebTestShellPlatformDelegate::WebTestShellPlatformDelegate() = default;
WebTestShellPlatformDelegate::~WebTestShellPlatformDelegate() = default;
#endif

}  // namespace content
