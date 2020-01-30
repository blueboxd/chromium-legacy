// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CONTENT_NAVIGATION_POLICY_H_
#define CONTENT_COMMON_CONTENT_NAVIGATION_POLICY_H_

#include "content/common/content_export.h"

namespace content {

CONTENT_EXPORT bool IsBackForwardCacheEnabled();
CONTENT_EXPORT bool DeviceHasEnoughMemoryForBackForwardCache();
CONTENT_EXPORT bool IsProactivelySwapBrowsingInstanceEnabled();

}  // namespace content

#endif  // CONTENT_COMMON_CONTENT_NAVIGATION_POLICY_H_
