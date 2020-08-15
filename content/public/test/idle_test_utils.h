// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_IDLE_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_IDLE_TEST_UTILS_H_

#include "content/public/browser/idle_manager.h"
#include "content/public/browser/render_frame_host.h"

namespace content {
class IdleManagerHelper {
 public:
  static void SetIdleTimeProviderForTest(
      content::RenderFrameHost* frame,
      std::unique_ptr<content::IdleManager::IdleTimeProvider>
          idle_time_provider);
};
}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_IDLE_TEST_UTILS_H_
