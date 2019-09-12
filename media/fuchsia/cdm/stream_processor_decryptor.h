// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_CDM_STREAM_PROCESSOR_DECRYPTOR_H_
#define MEDIA_FUCHSIA_CDM_STREAM_PROCESSOR_DECRYPTOR_H_

#include <fuchsia/media/drm/cpp/fidl.h>

#include <memory>

#include "media/base/decryptor.h"
#include "media/fuchsia/common/stream_processor_helper.h"
#include "media/fuchsia/common/sysmem_buffer_pool.h"

namespace media {
class SysmemBufferReader;
class SysmemBufferWriter;

// Class to handle decryption for one stream. All the APIs have the same
// requirement as Decryptor.
class StreamProcessorDecryptor : public StreamProcessorHelper::Client {
 public:
  static std::unique_ptr<StreamProcessorDecryptor> CreateAudioDecryptor(
      fuchsia::media::drm::ContentDecryptionModule* cdm);

  explicit StreamProcessorDecryptor(
      fuchsia::media::StreamProcessorPtr processor);
  ~StreamProcessorDecryptor() override;

  void Decrypt(scoped_refptr<DecoderBuffer> encrypted,
               Decryptor::DecryptCB decrypt_cb);
  void CancelDecrypt();

  // StreamProcessorHelper::Client implementation:
  void AllocateInputBuffers(const fuchsia::media::StreamBufferConstraints&
                                stream_constraints) override;
  void AllocateOutputBuffers(const fuchsia::media::StreamBufferConstraints&
                                 stream_constraints) override;
  void OnProcessEos() override;
  void OnOutputFormat(fuchsia::media::StreamOutputFormat format) override;
  void OnOutputPacket(
      std::unique_ptr<StreamProcessorHelper::IoPacket> packet) override;
  void OnNoKey() override;
  void OnError() override;

 private:
  void OnInputPacketReleased(size_t index);
  void OnInputBufferPoolAvailable(std::unique_ptr<SysmemBufferPool> pool);
  void OnInputBufferPoolWriter(std::unique_ptr<SysmemBufferWriter> writer);
  void OnOutputBufferPoolAvailable(size_t max_used_output_buffers,
                                   std::unique_ptr<SysmemBufferPool> pool);
  void OnOutputBufferPoolReader(std::unique_ptr<SysmemBufferReader> reader);

  StreamProcessorHelper processor_;
  BufferAllocator allocator_;

  // Pending buffers due to input buffer pool not available.
  scoped_refptr<DecoderBuffer> pending_encrypted_buffer_;

  Decryptor::DecryptCB decrypt_cb_;

  std::unique_ptr<SysmemBufferPool::Creator> input_pool_creator_;
  std::unique_ptr<SysmemBufferPool> input_pool_;
  std::unique_ptr<SysmemBufferWriter> input_writer_;

  std::unique_ptr<SysmemBufferPool::Creator> output_pool_creator_;
  std::unique_ptr<SysmemBufferPool> output_pool_;
  std::unique_ptr<SysmemBufferReader> output_reader_;

  scoped_refptr<DecoderBuffer> clear_buffer_;

  DISALLOW_COPY_AND_ASSIGN(StreamProcessorDecryptor);
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_CDM_STREAM_PROCESSOR_DECRYPTOR_H_
