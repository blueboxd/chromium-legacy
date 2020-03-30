// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/cascade_interpolations.h"

#include <gtest/gtest.h>

namespace blink {

TEST(CascadeInterpolationsTest, Limit) {
  constexpr size_t max = std::numeric_limits<uint16_t>::max();

  static_assert(CascadeInterpolations::kMaxEntryIndex == max,
                "Unexpected max. If the limit increased, evaluate whether it "
                "still makes sense to run this test");

  ActiveInterpolationsMap map;

  CascadeInterpolations interpolations;
  for (size_t i = 0; i <= max; ++i)
    interpolations.Add(&map, CascadeOrigin::kAuthor);

  // At maximum
  EXPECT_FALSE(interpolations.IsEmpty());

  interpolations.Add(&map, CascadeOrigin::kAuthor);

  // Maximum + 1
  EXPECT_TRUE(interpolations.IsEmpty());
}

TEST(CascadeInterpolationsTest, Reset) {
  ActiveInterpolationsMap map;

  CascadeInterpolations interpolations;
  EXPECT_TRUE(interpolations.IsEmpty());

  interpolations.Add(&map, CascadeOrigin::kAuthor);
  EXPECT_FALSE(interpolations.IsEmpty());

  interpolations.Reset();
  EXPECT_TRUE(interpolations.IsEmpty());
}

}  // namespace blink
