// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_LOCAL_VIDEO_CAPTURER_SOURCE_H_
#define CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_LOCAL_VIDEO_CAPTURER_SOURCE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/capture/video_capture_types.h"
#include "media/capture/video_capturer_source.h"
#include "third_party/blink/public/common/media/video_capture.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {

class VideoCaptureImplManager;

// LocalVideoCapturerSource is a delegate used by MediaStreamVideoCapturerSource
// for local video capture. It uses the Render singleton VideoCaptureImplManager
// to start / stop and receive I420 frames from Chrome's video capture
// implementation. This is a main Render thread only object.
class LocalVideoCapturerSource : public media::VideoCapturerSource {
 public:
  static std::unique_ptr<media::VideoCapturerSource> Create(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      int session_id);
  LocalVideoCapturerSource(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      int session_id);
  ~LocalVideoCapturerSource() override;

  // VideoCaptureSource Implementation.
  media::VideoCaptureFormats GetPreferredFormats() override;
  void StartCapture(const media::VideoCaptureParams& params,
                    const blink::VideoCaptureDeliverFrameCB& new_frame_callback,
                    const RunningCallback& running_callback) override;
  void RequestRefreshFrame() override;
  void MaybeSuspend() override;
  void Resume() override;
  void StopCapture() override;
  void OnFrameDropped(media::VideoCaptureFrameDropReason reason) override;
  void OnLog(const std::string& message) override;

 private:
  void OnStateUpdate(blink::VideoCaptureState state);

  // |session_id_| identifies the capture device used for this capture session.
  const media::VideoCaptureSessionId session_id_;

  VideoCaptureImplManager* const manager_;

  base::OnceClosure release_device_cb_;

  // These two are valid between StartCapture() and StopCapture().
  // |running_call_back_| is run when capture is successfully started, and when
  // it is stopped or error happens.
  RunningCallback running_callback_;
  base::OnceClosure stop_capture_cb_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Bound to the main render thread.
  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<LocalVideoCapturerSource> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LocalVideoCapturerSource);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_LOCAL_VIDEO_CAPTURER_SOURCE_H_
