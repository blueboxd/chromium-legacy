// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_CAPTURE_SERVICE_CONSTANTS_H_
#define CHROMECAST_MEDIA_AUDIO_CAPTURE_SERVICE_CONSTANTS_H_

#include "chromecast/media/audio/capture_service/capture_service_buildflags.h"

namespace chromecast {
namespace media {
namespace capture_service {

#if BUILDFLAG(USE_UNIX_SOCKETS)
constexpr char kDefaultUnixDomainSocketPath[] = "/tmp/capture-service";
#else
constexpr int kDefaultTcpPort = 12855;
#endif

enum SampleFormat {
  INTERLEAVED_INT16 = 0,
  INTERLEAVED_INT32 = 1,
  INTERLEAVED_FLOAT = 2,
  PLANAR_INT16 = 3,
  PLANAR_INT32 = 4,
  PLANAR_FLOAT = 5,
};

}  // namespace capture_service
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_CAPTURE_SERVICE_CONSTANTS_H_
