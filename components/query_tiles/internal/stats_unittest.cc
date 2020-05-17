// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/stats.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace query_tiles {
namespace {

const char kImagePreloadingHistogram[] =
    "Search.QueryTiles.ImagePreloadingEvent";

TEST(StatsTest, RecordImageLoading) {
  base::HistogramTester tester;
  stats::RecordImageLoading(stats::ImagePreloadingEvent::kStart);
  tester.ExpectBucketCount(kImagePreloadingHistogram, 0, 1);
}

}  // namespace
}  // namespace query_tiles
