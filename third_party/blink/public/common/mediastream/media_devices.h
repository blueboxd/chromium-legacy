// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_MEDIASTREAM_MEDIA_DEVICES_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_MEDIASTREAM_MEDIA_DEVICES_H_

#include <string>
#include <vector>

#include "base/optional.h"
#include "media/base/video_facing.h"
#include "third_party/blink/public/common/common_export.h"

namespace media {
struct VideoCaptureDeviceDescriptor;
}  // namespace media

namespace blink {

enum MediaDeviceType {
  MEDIA_DEVICE_TYPE_AUDIO_INPUT,
  MEDIA_DEVICE_TYPE_VIDEO_INPUT,
  MEDIA_DEVICE_TYPE_AUDIO_OUTPUT,
  NUM_MEDIA_DEVICE_TYPES,
};

struct BLINK_COMMON_EXPORT WebMediaDeviceInfo {
  WebMediaDeviceInfo();
  WebMediaDeviceInfo(const WebMediaDeviceInfo& other);
  WebMediaDeviceInfo(WebMediaDeviceInfo&& other);
  WebMediaDeviceInfo(
      const std::string& device_id,
      const std::string& label,
      const std::string& group_id,
      bool pan_tilt_zoom_supported = false,
      media::VideoFacingMode video_facing = media::MEDIA_VIDEO_FACING_NONE);
  explicit WebMediaDeviceInfo(
      const media::VideoCaptureDeviceDescriptor& descriptor);
  ~WebMediaDeviceInfo();
  WebMediaDeviceInfo& operator=(const WebMediaDeviceInfo& other);
  WebMediaDeviceInfo& operator=(WebMediaDeviceInfo&& other);

  std::string device_id;
  std::string label;
  std::string group_id;
  bool pan_tilt_zoom_supported = false;
  media::VideoFacingMode video_facing =
      media::VideoFacingMode::MEDIA_VIDEO_FACING_NONE;
};

using WebMediaDeviceInfoArray = std::vector<WebMediaDeviceInfo>;

BLINK_COMMON_EXPORT bool operator==(const WebMediaDeviceInfo& first,
                                    const WebMediaDeviceInfo& second);

inline bool IsValidMediaDeviceType(MediaDeviceType type) {
  return type >= 0 && type < NUM_MEDIA_DEVICE_TYPES;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_MEDIASTREAM_MEDIA_DEVICES_H_
