// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/common/sysmem_buffer_writer_queue.h"

#include <zircon/rights.h>
#include <algorithm>

#include "base/bits.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/process/process_metrics.h"
#include "media/base/decoder_buffer.h"

namespace media {

class SysmemBufferWriterQueue::PendingBuffer {
 public:
  PendingBuffer(scoped_refptr<DecoderBuffer> buffer) : buffer_(buffer) {
    DCHECK(buffer_);
  }
  ~PendingBuffer() = default;

  PendingBuffer(PendingBuffer&& other) = default;
  PendingBuffer& operator=(PendingBuffer&& other) = default;

  const DecoderBuffer* buffer() { return buffer_.get(); }

  const uint8_t* data() const { return buffer_->data() + buffer_pos_; }
  size_t bytes_left() const { return buffer_->data_size() - buffer_pos_; }
  void AdvanceCurrentPos(size_t bytes) {
    DCHECK_LE(bytes, bytes_left());
    buffer_pos_ += bytes;
  }

 private:
  scoped_refptr<DecoderBuffer> buffer_;
  size_t buffer_pos_ = 0;

  DISALLOW_COPY_AND_ASSIGN(PendingBuffer);
};

SysmemBufferWriterQueue::SysmemBufferWriterQueue() = default;
SysmemBufferWriterQueue::~SysmemBufferWriterQueue() = default;

void SysmemBufferWriterQueue::EnqueueBuffer(
    scoped_refptr<DecoderBuffer> buffer) {
  pending_buffers_.push_back(PendingBuffer(buffer));
  PumpPackets();
}

void SysmemBufferWriterQueue::Start(std::unique_ptr<SysmemBufferWriter> writer,
                                    SendPacketCB send_packet_cb,
                                    EndOfStreamCB end_of_stream_cb) {
  DCHECK(!writer_);

  writer_ = std::move(writer);
  send_packet_cb_ = std::move(send_packet_cb);
  end_of_stream_cb_ = std::move(end_of_stream_cb);

  PumpPackets();
}

void SysmemBufferWriterQueue::PumpPackets() {
  auto weak_this = weak_factory_.GetWeakPtr();

  while (writer_ && !pending_buffers_.empty()) {
    if (pending_buffers_.front().buffer()->end_of_stream()) {
      pending_buffers_.pop_front();
      end_of_stream_cb_.Run();
      if (!weak_this)
        return;
      continue;
    }

    base::Optional<size_t> index_opt = writer_->Acquire();

    if (!index_opt.has_value()) {
      // No input buffer available.
      return;
    }

    size_t buffer_index = index_opt.value();

    size_t bytes_filled = writer_->Write(
        buffer_index, base::make_span(pending_buffers_.front().data(),
                                      pending_buffers_.front().bytes_left()));
    pending_buffers_.front().AdvanceCurrentPos(bytes_filled);

    bool buffer_end = pending_buffers_.front().bytes_left() == 0;

    auto packet = StreamProcessorHelper::IoPacket::CreateInput(
        buffer_index, bytes_filled,
        pending_buffers_.front().buffer()->timestamp(), buffer_end,
        base::BindOnce(&SysmemBufferWriterQueue::ReleaseBuffer,
                       weak_factory_.GetWeakPtr(), buffer_index));

    send_packet_cb_.Run(pending_buffers_.front().buffer(), std::move(packet));
    if (!weak_this)
      return;

    if (buffer_end)
      pending_buffers_.pop_front();
  }
}

void SysmemBufferWriterQueue::ResetQueue() {
  // Invalidate weak pointers to drop all ReleaseBuffer() callbacks.
  weak_factory_.InvalidateWeakPtrs();

  pending_buffers_.clear();
  if (writer_)
    writer_->ReleaseAll();
}

void SysmemBufferWriterQueue::ResetBuffers() {
  writer_.reset();
  send_packet_cb_ = SendPacketCB();
  end_of_stream_cb_ = EndOfStreamCB();
}

void SysmemBufferWriterQueue::ReleaseBuffer(size_t buffer_index) {
  DCHECK(writer_);
  writer_->Release(buffer_index);
  PumpPackets();
}

size_t SysmemBufferWriterQueue::num_buffers() const {
  return writer_ ? writer_->num_buffers() : 0;
}

}  // namespace media
