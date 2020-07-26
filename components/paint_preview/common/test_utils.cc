// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/common/test_utils.h"

std::string PersistenceParamToString(
    const ::testing::TestParamInfo<paint_preview::mojom::Persistence>&
        persistence) {
  switch (persistence.param) {
    case paint_preview::mojom::Persistence::kFileSystem:
      return "FileSystem";
    case paint_preview::mojom::Persistence::kMemoryBuffer:
      return "MemoryBuffer";
  }
}
