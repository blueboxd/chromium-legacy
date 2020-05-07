// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_TILE_TYPES_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_TILE_TYPES_H_

enum class TileInfoRequestStatus {
  // Initial status, request is not sent.
  kInit = 0,
  // Request completed successfully.
  kSuccess = 1,
  // Request failed. Suggesting a retry with backoff.
  kFailure = 2,
  // Request failed, suggesting a suspend.
  kShouldSuspend = 3,
  // Max value.
  kMaxValue = kShouldSuspend,
};

enum class TileGroupStatus {
  // No errors happen in tile group manager.
  kSuccess = 0,
  // Database and manager component is not fully initialized.
  kUninitialized = 1,
  // Db operations failed.
  kFailureDbOperation = 2,
  // The group status is invalid, reason could be expired or locale not match.
  kInvalidGroup = 3,
  // Max value.
  kMaxValue = kInvalidGroup,
};

using SuccessCallback = base::OnceCallback<void(bool)>;

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_TILE_TYPES_H_
