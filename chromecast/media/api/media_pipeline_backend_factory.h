// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_API_MEDIA_PIPELINE_BACKEND_FACTORY_H_
#define CHROMECAST_MEDIA_API_MEDIA_PIPELINE_BACKEND_FACTORY_H_

#include <memory>

#include "chromecast/public/media/media_pipeline_device_params.h"

namespace chromecast {
namespace media {

class CmaBackend;

// This class creates media backends.
class MediaPipelineBackendFactory {
 public:
  virtual ~MediaPipelineBackendFactory();

  // Creates a CMA backend. Must be called on the same thread as
  // |media_task_runner_|.
  virtual std::unique_ptr<CmaBackend> CreateCmaBackend(
      const MediaPipelineDeviceParams& params) = 0;
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_API_MEDIA_PIPELINE_BACKEND_FACTORY_H_
