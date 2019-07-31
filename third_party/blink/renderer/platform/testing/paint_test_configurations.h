// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_PAINT_TEST_CONFIGURATIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_PAINT_TEST_CONFIGURATIONS_H_

#include <gtest/gtest.h>
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

enum {
  kBlinkGenPropertyTrees = 1 << 0,
  kCompositeAfterPaint = 1 << 1,
  kUnderInvalidationChecking = 1 << 2,
  kFastBorderRadius = 1 << 3,
  kPaintNonFastScrollableRegions = 1 << 4,
};

class PaintTestConfigurations
    : public testing::WithParamInterface<unsigned>,
      private ScopedBlinkGenPropertyTreesForTest,
      private ScopedCompositeAfterPaintForTest,
      private ScopedPaintUnderInvalidationCheckingForTest,
      private ScopedFastBorderRadiusForTest,
      private ScopedPaintNonFastScrollableRegionsForTest {
 public:
  PaintTestConfigurations()
      : ScopedBlinkGenPropertyTreesForTest(GetParam() & kBlinkGenPropertyTrees),
        ScopedCompositeAfterPaintForTest(GetParam() & kCompositeAfterPaint),
        ScopedPaintUnderInvalidationCheckingForTest(GetParam() &
                                                    kUnderInvalidationChecking),
        ScopedFastBorderRadiusForTest(GetParam() & kFastBorderRadius),
        // Because PaintNonFastScrollableRegions is implied by
        // CompositeAfterPaint (see: runtime_eanbled_features.json5), we must
        // enable the scoped flag for (PNFSR || CAP).
        ScopedPaintNonFastScrollableRegionsForTest(
            GetParam() & kPaintNonFastScrollableRegions ||
            GetParam() & kCompositeAfterPaint) {}
  ~PaintTestConfigurations() {
    // Must destruct all objects before toggling back feature flags.
    WebHeap::CollectAllGarbageForTesting();
  }
};

#define INSTANTIATE_PAINT_TEST_SUITE_P(test_class) \
  INSTANTIATE_TEST_SUITE_P(                        \
      All, test_class,                             \
      ::testing::Values(0, kBlinkGenPropertyTrees, \
                        kBlinkGenPropertyTrees | kCompositeAfterPaint))

#define INSTANTIATE_CAP_TEST_SUITE_P(test_class) \
  INSTANTIATE_TEST_SUITE_P(                      \
      All, test_class,                           \
      ::testing::Values(kBlinkGenPropertyTrees | kCompositeAfterPaint))

#define INSTANTIATE_LAYER_LIST_TEST_SUITE_P(test_class)                \
  INSTANTIATE_TEST_SUITE_P(                                            \
      All, test_class,                                                 \
      ::testing::Values(kBlinkGenPropertyTrees,                        \
                        kBlinkGenPropertyTrees | kCompositeAfterPaint, \
                        kBlinkGenPropertyTrees | kFastBorderRadius))

#define INSTANTIATE_SCROLL_HIT_TEST_SUITE_P(test_class) \
  INSTANTIATE_TEST_SUITE_P(                             \
      All, test_class,                                  \
      ::testing::Values(kPaintNonFastScrollableRegions, kCompositeAfterPaint))

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_PAINT_TEST_CONFIGURATIONS_H_
