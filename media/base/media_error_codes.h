// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_ERROR_CODES_H_
#define MEDIA_BASE_MEDIA_ERROR_CODES_H_

namespace media {

enum class ErrorCode : uint32_t {
  kOk = 0,
  kCodeOnlyForTesting = std::numeric_limits<uint32_t>::max(),
};

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_ERROR_CODES_H_
