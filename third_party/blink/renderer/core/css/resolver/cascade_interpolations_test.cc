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

  using Entry = CascadeInterpolations::Entry;

  CascadeInterpolations at_max(Vector<Entry, 4>(max + 1));
  CascadeInterpolations above_max(Vector<Entry, 4>(max + 2));

  EXPECT_EQ(max + 1, at_max.GetEntries().size());
  EXPECT_FALSE(at_max.IsEmpty());

  EXPECT_FALSE(above_max.GetEntries().size());
  EXPECT_TRUE(above_max.IsEmpty());
}

}  // namespace blink
