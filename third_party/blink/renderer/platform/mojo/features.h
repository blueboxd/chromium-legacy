// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_FEATURES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_FEATURES_H_

#include "base/feature_list.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {
PLATFORM_EXPORT extern const base::Feature kHeapMojoUseContextObserver;
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_FEATURES_H_
