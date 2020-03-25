// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upboarding/query_tiles/internal/tile_info_fetcher.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace upboarding {
class TileInfoFetcherTest : public testing::Test {
 public:
  TileInfoFetcherTest();
  ~TileInfoFetcherTest() override = default;

  TileInfoFetcherTest(const TileInfoFetcherTest& other) = delete;
  TileInfoFetcherTest& operator=(const TileInfoFetcherTest& other) = delete;
};

}  // namespace upboarding
