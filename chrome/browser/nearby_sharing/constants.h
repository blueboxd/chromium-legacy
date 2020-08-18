// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CONSTANTS_H_
#define CHROME_BROWSER_NEARBY_SHARING_CONSTANTS_H_

#include "base/time/time.h"

// Timeout for reading a response frame from remote device.
constexpr base::TimeDelta kReadResponseFrameTimeout =
    base::TimeDelta::FromSeconds(60);

// The delay before the receiver will disconnect from the sender after rejecting
// an incoming file. The sender is expected to disconnect immediately after
// reading the rejection frame.
constexpr base::TimeDelta kIncomingRejectionDelay =
    base::TimeDelta::FromSeconds(2);

// Timeout for reading a frame from remote device.
constexpr base::TimeDelta kReadFramesTimeout = base::TimeDelta::FromSeconds(15);

// Time to delay running the task to invalidate send and receive surfaces.
constexpr base::TimeDelta kInvalidateDelay =
    base::TimeDelta::FromMilliseconds(500);

#endif  // CHROME_BROWSER_NEARBY_SHARING_CONSTANTS_H_
