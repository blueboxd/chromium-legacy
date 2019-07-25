// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_LOOPBACK_HANDLER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_LOOPBACK_HANDLER_H_

#include <cstdint>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "chromecast/public/cast_media_shlib.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace chromecast {
namespace media {

// Handles loopback audio from the mixer.
class LoopbackHandler {
 public:
  struct Deleter {
    void operator()(LoopbackHandler* obj) { obj->Destroy(); }
  };

  static std::unique_ptr<LoopbackHandler, Deleter> Create(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  virtual void Destroy() = 0;

  virtual void SetDataSize(int data_size_bytes) = 0;

  virtual scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() = 0;

  virtual void AddObserver(CastMediaShlib::LoopbackAudioObserver* observer) = 0;
  virtual void RemoveObserver(
      CastMediaShlib::LoopbackAudioObserver* observer) = 0;

  virtual void SendData(int64_t timestamp,
                        int sample_rate,
                        int num_channels,
                        float* data,
                        int frames) = 0;
  virtual void SendInterrupt() = 0;

 protected:
  virtual ~LoopbackHandler() = default;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_LOOPBACK_HANDLER_H_
