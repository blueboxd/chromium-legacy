// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_TYPES_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_TYPES_H_

namespace ash {

// Defines the capture type Capture Mode is currently using.
enum class CaptureModeType {
  kImage,
  kVideo,
};

// Defines the source of the capture used by Capture Mode.
enum class CaptureModeSource {
  kFullscreen,
  kRegion,
  kWindow,
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_TYPES_H_
