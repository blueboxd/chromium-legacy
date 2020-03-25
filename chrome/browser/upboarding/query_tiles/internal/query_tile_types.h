// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_QUERY_TILE_TYPES_H_
#define CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_QUERY_TILE_TYPES_H_

enum class TileInfoRequestStatus {
  // Initial status, request is not sent.
  kInit = 0,
  // Request completed successfully.
  kSuccess = 1,
  // Request failed.
  kFailure = 2,
  // Max value.
  kMaxValue = kFailure,
};

#endif  // CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_QUERY_TILE_TYPES_H_
