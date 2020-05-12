// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/cast_streaming/stream_consumer.h"

#include "base/logging.h"

namespace cast_streaming {

StreamConsumer::StreamConsumer(openscreen::cast::Receiver* receiver,
                               FrameReceivedCB frame_received_cb)
    : receiver_(receiver), frame_received_cb_(std::move(frame_received_cb)) {
  DCHECK(receiver_);
  receiver_->SetConsumer(this);
}

StreamConsumer::~StreamConsumer() {
  receiver_->SetConsumer(nullptr);
}

void StreamConsumer::OnFramesReady(int next_frame_buffer_size) {
  // TODO(crbug.com/1042501): Do not allocate a buffer for every new frame, use
  // a buffer pool.
  std::unique_ptr<uint8_t[]> buffer =
      std::make_unique<uint8_t[]>(next_frame_buffer_size);
  openscreen::cast::EncodedFrame encoded_frame = receiver_->ConsumeNextFrame(
      absl::Span<uint8_t>(buffer.get(), next_frame_buffer_size));

  const bool is_key_frame =
      encoded_frame.dependency ==
      openscreen::cast::EncodedFrame::Dependency::KEY_FRAME;

  base::TimeDelta playout_time = base::TimeDelta::FromMicroseconds(
      encoded_frame.rtp_timestamp
          .ToTimeSinceOrigin<std::chrono::microseconds>(
              receiver_->rtp_timebase())
          .count());

  DVLOG(3) << "[ssrc:" << receiver_->ssrc()
           << "] Received new frame. Timestamp: " << playout_time
           << ", is_key_frame: " << is_key_frame;

  if (playout_time <= last_playout_time_) {
    // TODO(b/156129097): We sometimes receive identical playout time for two
    // frames in a row.
    DVLOG(2) << "[ssrc:" << receiver_->ssrc()
             << "] Droped frame due to identical playout time.";
    return;
  }
  last_playout_time_ = playout_time;

  scoped_refptr<media::DecoderBuffer> decoder_buffer =
      media::DecoderBuffer::FromArray(std::move(buffer),
                                      next_frame_buffer_size);
  decoder_buffer->set_is_key_frame(is_key_frame);
  decoder_buffer->set_timestamp(playout_time);

  frame_received_cb_.Run(decoder_buffer);
}

}  // namespace cast_streaming
