// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_CAST_STREAMING_STREAM_CONSUMER_H_
#define FUCHSIA_CAST_STREAMING_STREAM_CONSUMER_H_

#include <fuchsia/media/cpp/fidl.h>

#include "base/callback.h"
#include "base/time/time.h"
#include "media/base/decoder_buffer.h"
#include "third_party/openscreen/src/cast/streaming/receiver.h"
#include "third_party/openscreen/src/cast/streaming/receiver_session.h"

namespace cast_streaming {

// Attaches to an Open Screen Receiver to receive frames and invokes
// |frame_received_cb_| with each received buffer of encoded data.
class StreamConsumer : public openscreen::cast::Receiver::Consumer {
 public:
  using FrameReceivedCB =
      base::RepeatingCallback<void(scoped_refptr<media::DecoderBuffer>)>;

  // |receiver| sends frames to this object. It must outlive this object.
  // |frame_received_cb| is called on every new frame.
  StreamConsumer(openscreen::cast::Receiver* receiver,
                 FrameReceivedCB frame_received_cb);
  ~StreamConsumer() final;

  StreamConsumer(const StreamConsumer&) = delete;
  StreamConsumer& operator=(const StreamConsumer&) = delete;

 private:
  // openscreen::cast::Receiver::Consumer implementation.
  void OnFramesReady(int next_frame_buffer_size) final;

  openscreen::cast::Receiver* const receiver_;
  const FrameReceivedCB frame_received_cb_;
  base::TimeDelta last_playout_time_;
};

}  // namespace cast_streaming

#endif  // FUCHSIA_CAST_STREAMING_STREAM_CONSUMER_H_
