// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_FEATURES_H_
#define CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_FEATURES_H_

#include "base/feature_list.h"
#include "base/optional.h"

namespace base {
class Value;
}  // namespace base

namespace lite_video {
namespace features {

// Whether the LiteVideo feature that throttles media requests to reduce
// adaptive bitrates of media streams is enabled. Currently disabled by default.
bool IsLiteVideoEnabled();

// Return the origins that are whitelisted for using the LiteVideo optimization
// and the parameters needed to throttle media requests for that origin.
base::Optional<base::Value> GetLiteVideoOriginHintsFromFieldTrial();

// The target for of the round-trip time for media requests used when
// throttling media requests.
int LiteVideoTargetDownlinkRTTLatencyMs();

// The number of kilobytes to be buffered before starting to throttle media
// requests.
int LiteVideoKilobytesToBufferBeforeThrottle();

}  // namespace features
}  // namespace lite_video

#endif  // CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_FEATURES_H_
