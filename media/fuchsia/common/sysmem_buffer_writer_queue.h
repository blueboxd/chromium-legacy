// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_COMMON_SYSMEM_BUFFER_WRITER_QUEUE_H_
#define MEDIA_FUCHSIA_COMMON_SYSMEM_BUFFER_WRITER_QUEUE_H_

#include <fuchsia/media/cpp/fidl.h>
#include <fuchsia/sysmem/cpp/fidl.h>

#include <memory>

#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "media/fuchsia/common/stream_processor_helper.h"
#include "media/fuchsia/common/sysmem_buffer_writer.h"

namespace media {

class DecoderBuffer;

// A SysmemBufferWriter wrapper that keeps a queue of pending DecodeBuffers,
// writes them to sysmem buffers and generates StreamProcessor packets.
class SysmemBufferWriterQueue {
 public:
  // Callback passed to StartSender(). |buffer| corresponds to the original
  // buffer from which the |packet| was generated.
  using SendPacketCB =
      base::RepeatingCallback<void(const DecoderBuffer* buffer,
                                   StreamProcessorHelper::IoPacket packet)>;

  // Called when processing DecoderBuffer that's marked as end-of-stream.
  using EndOfStreamCB = base::RepeatingClosure;

  SysmemBufferWriterQueue();
  ~SysmemBufferWriterQueue();

  // Enqueues buffer to the queue.
  void EnqueueBuffer(scoped_refptr<DecoderBuffer> buffer);

  // Sets the buffer writer to use and starts sending outgoing packets using
  // |send_packet_cb|. |end_of_stream_cb| will be called when processing each
  // end-of-stream buffer.
  void Start(std::unique_ptr<SysmemBufferWriter> writer,
             SendPacketCB send_packet_cb,
             EndOfStreamCB end_of_stream_cb);

  // Resets all pending buffers. Keeps the underlying sysmem buffers.
  void ResetQueue();

  // Resets the buffers. Keeps the current pending buffers, so they will still
  // be sent once the new collection is allocated and passed to Start().
  void ResetBuffers();

  // Number of buffers in the sysmem collection or 0 if sysmem buffers has not
  // been allocated (i.e. before Start()).
  size_t num_buffers() const;

 private:
  class PendingBuffer;
  class SysmemBuffer;

  // Pumps pending buffers to SendPacketCB.
  void PumpPackets();

  // Callback called when a packet is destroyed. It marks the buffer as unused
  // and tries to reuse it for other buffers if any.
  void ReleaseBuffer(size_t buffer_index);

  // Buffers that are waiting to be sent.
  std::deque<PendingBuffer> pending_buffers_;

  // Buffers for sysmem buffer collection. Not set until Start() is called.
  std::unique_ptr<SysmemBufferWriter> writer_;

  SendPacketCB send_packet_cb_;
  EndOfStreamCB end_of_stream_cb_;

  base::WeakPtrFactory<SysmemBufferWriterQueue> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SysmemBufferWriterQueue);
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_COMMON_SYSMEM_BUFFER_WRITER_QUEUE_H_
