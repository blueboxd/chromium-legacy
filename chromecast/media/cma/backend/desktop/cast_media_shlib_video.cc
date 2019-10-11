// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>
#include <string>

#include "base/no_destructor.h"
#include "chromecast/public/cast_media_shlib.h"
#include "chromecast/public/media/media_capabilities_shlib.h"
#include "chromecast/public/video_plane.h"

namespace chromecast {
namespace media {

class DesktopVideoPlane : public VideoPlane {
 public:
  ~DesktopVideoPlane() override = default;

  void SetGeometry(const RectF& display_rect, Transform transform) override {}
};

void CastMediaShlib::Initialize(const std::vector<std::string>& argv) {}

VideoPlane* CastMediaShlib::GetVideoPlane() {
  static base::NoDestructor<DesktopVideoPlane> g_video_plane;
  return g_video_plane.get();
}

bool MediaCapabilitiesShlib::IsSupportedVideoConfig(VideoCodec codec,
                                                    VideoProfile profile,
                                                    int level) {
  return false;
}

}  // namespace media
}  // namespace chromecast
