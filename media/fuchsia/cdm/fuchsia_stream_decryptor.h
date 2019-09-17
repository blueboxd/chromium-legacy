// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_CDM_FUCHSIA_STREAM_DECRYPTOR_H_
#define MEDIA_FUCHSIA_CDM_FUCHSIA_STREAM_DECRYPTOR_H_

#include <fuchsia/media/drm/cpp/fidl.h>

#include <memory>

#include "media/base/decryptor.h"
#include "media/fuchsia/common/stream_processor_helper.h"
#include "media/fuchsia/common/sysmem_buffer_pool.h"
#include "media/fuchsia/common/sysmem_buffer_writer_queue.h"

namespace media {
class SysmemBufferReader;

// Base class for media stream decryptor implementations.
class FuchsiaStreamDecryptorBase : public StreamProcessorHelper::Client {
 public:
  explicit FuchsiaStreamDecryptorBase(
      fuchsia::media::StreamProcessorPtr processor);
  ~FuchsiaStreamDecryptorBase() override;

 protected:
  // StreamProcessorHelper::Client overrides.
  void AllocateInputBuffers(
      const fuchsia::media::StreamBufferConstraints& stream_constraints) final;

  void DecryptInternal(scoped_refptr<DecoderBuffer> encrypted);
  void ResetStream();

  StreamProcessorHelper processor_;

  BufferAllocator allocator_;

 private:
  void OnInputBufferPoolCreated(std::unique_ptr<SysmemBufferPool> pool);
  void OnWriterCreated(std::unique_ptr<SysmemBufferWriter> writer);
  void SendInputPacket(const DecoderBuffer* buffer,
                       StreamProcessorHelper::IoPacket packet);

  SysmemBufferWriterQueue input_writer_queue_;
  std::unique_ptr<SysmemBufferPool::Creator> input_pool_creator_;
  std::unique_ptr<SysmemBufferPool> input_pool_;

  DISALLOW_COPY_AND_ASSIGN(FuchsiaStreamDecryptorBase);
};

// Stream decryptor used by FuchsiaDecryptor for audio streams.
class FuchsiaClearStreamDecryptor : public FuchsiaStreamDecryptorBase {
 public:
  static std::unique_ptr<FuchsiaClearStreamDecryptor> Create(
      fuchsia::media::drm::ContentDecryptionModule* cdm);

  FuchsiaClearStreamDecryptor(fuchsia::media::StreamProcessorPtr processor);
  ~FuchsiaClearStreamDecryptor() override;

  // Decrypt() behavior should match media::Decryptor interface.
  void Decrypt(scoped_refptr<DecoderBuffer> encrypted,
               Decryptor::DecryptCB decrypt_cb);
  void CancelDecrypt();

 private:
  // StreamProcessorHelper::Client overrides.
  void AllocateOutputBuffers(const fuchsia::media::StreamBufferConstraints&
                                 stream_constraints) override;
  void OnProcessEos() override;
  void OnOutputFormat(fuchsia::media::StreamOutputFormat format) override;
  void OnOutputPacket(StreamProcessorHelper::IoPacket packet) override;
  void OnNoKey() override;
  void OnError() override;

  void OnOutputBufferPoolCreated(size_t max_used_output_buffers,
                                 std::unique_ptr<SysmemBufferPool> pool);
  void OnOutputBufferPoolReaderCreated(
      std::unique_ptr<SysmemBufferReader> reader);

  Decryptor::DecryptCB decrypt_cb_;

  std::unique_ptr<SysmemBufferPool::Creator> output_pool_creator_;
  std::unique_ptr<SysmemBufferPool> output_pool_;
  std::unique_ptr<SysmemBufferReader> output_reader_;

  // Used to re-assemble decrypted output that was split between multiple sysmem
  // buffers.
  Decryptor::Status current_status_ = Decryptor::kSuccess;
  std::vector<uint8_t> output_data_;

  DISALLOW_COPY_AND_ASSIGN(FuchsiaClearStreamDecryptor);
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_CDM_FUCHSIA_STREAM_DECRYPTOR_H_
