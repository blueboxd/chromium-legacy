// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/tile_fetcher.h"

#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace upboarding {
class TileFetcherTest : public testing::Test {
 public:
  TileFetcherTest();
  ~TileFetcherTest() override = default;

  TileFetcherTest(const TileFetcherTest& other) = delete;
  TileFetcherTest& operator=(const TileFetcherTest& other) = delete;
};

}  // namespace upboarding
