// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIASTREAM_MEDIA_STREAM_CONSTRAINTS_UTIL_VIDEO_CONTENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIASTREAM_MEDIA_STREAM_CONSTRAINTS_UTIL_VIDEO_CONTENT_H_

#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_constraints_util.h"

namespace blink {

class WebMediaConstraints;

// TODO(crbug.com/704136): Move these helpers out of the Blink exposed API
// when all users of it have been Onion souped.
BLINK_MODULES_EXPORT extern const int kMinScreenCastDimension;
BLINK_MODULES_EXPORT extern const int kMaxScreenCastDimension;
BLINK_MODULES_EXPORT extern const int kDefaultScreenCastWidth;
BLINK_MODULES_EXPORT extern const int kDefaultScreenCastHeight;

BLINK_MODULES_EXPORT extern const double kMaxScreenCastFrameRate;
BLINK_MODULES_EXPORT extern const double kDefaultScreenCastFrameRate;

// This function performs source, source-settings and track-settings selection
// for content video capture based on the given |constraints|.
VideoCaptureSettings BLINK_MODULES_EXPORT
SelectSettingsVideoContentCapture(const blink::WebMediaConstraints& constraints,
                                  blink::mojom::MediaStreamType stream_type,
                                  int screen_width,
                                  int screen_height);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIASTREAM_MEDIA_STREAM_CONSTRAINTS_UTIL_VIDEO_CONTENT_H_
