// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CONTENT_NAVIGATION_POLICY_H_
#define CONTENT_COMMON_CONTENT_NAVIGATION_POLICY_H_

#include "content/common/content_export.h"

namespace content {

CONTENT_EXPORT bool IsBackForwardCacheEnabled();
CONTENT_EXPORT bool DeviceHasEnoughMemoryForBackForwardCache();

// Levels of ProactivelySwapBrowsingInstance support.
// These are additive; features enabled at lower levels remain enabled at all
// higher levels.
enum class ProactivelySwapBrowsingInstanceLevel {
  kDisabled = 0,
  // Swap BrowsingInstance and renderer process on cross-site navigations.
  kCrossSiteSwapProcess = 1,
  // Swap BrowsingInstance on cross-site navigations, but try to reuse the
  // current renderer process if possible.
  kCrossSiteReuseProcess = 2,
  // TODO(rakina): Add another level for BrowsingInstance swap on same-site
  // navigations with process reuse.
};
CONTENT_EXPORT bool IsProactivelySwapBrowsingInstanceEnabled();
CONTENT_EXPORT bool IsProactivelySwapBrowsingInstanceWithProcessReuseEnabled();
CONTENT_EXPORT extern const char
    kProactivelySwapBrowsingInstanceLevelParameterName[];

CONTENT_EXPORT bool IsRenderDocumentEnabled();

}  // namespace content

#endif  // CONTENT_COMMON_CONTENT_NAVIGATION_POLICY_H_
