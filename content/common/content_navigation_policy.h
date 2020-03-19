// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CONTENT_NAVIGATION_POLICY_H_
#define CONTENT_COMMON_CONTENT_NAVIGATION_POLICY_H_

#include "content/common/content_export.h"

#include <string>

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

// Levels of RenderDocument support. These are additive in that features enabled
// at lower levels remain enabled at all higher levels.
enum class RenderDocumentLevel {
  kDisabled = 0,
  // Do not reused RenderFrameHosts when recovering from crashes.
  kCrashedFrame = 1,
  // Also do not reuse RenderFrameHosts when navigating subframes.
  kSubframe = 2,
};
CONTENT_EXPORT bool CreateNewHostForSameSiteSubframe();
CONTENT_EXPORT RenderDocumentLevel GetRenderDocumentLevel();
CONTENT_EXPORT std::string GetRenderDocumentLevelName(
    RenderDocumentLevel level);
CONTENT_EXPORT extern const char kRenderDocumentLevelParameterName[];

}  // namespace content

#endif  // CONTENT_COMMON_CONTENT_NAVIGATION_POLICY_H_
