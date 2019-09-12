// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/cdm/stream_processor_decryptor.h"

#include <fuchsia/media/cpp/fidl.h>
#include <fuchsia/media/drm/cpp/fidl.h>

#include "base/bind.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/encryption_pattern.h"
#include "media/base/subsample_entry.h"
#include "media/fuchsia/common/sysmem_buffer_reader.h"
#include "media/fuchsia/common/sysmem_buffer_writer.h"

namespace media {
namespace {

// Decryptor will copy decrypted data immediately once it's available. Client
// just needs one buffer.
const uint32_t kMaxUsedOutputFrames = 1;

std::string GetEncryptionMode(EncryptionMode mode) {
  switch (mode) {
    case EncryptionMode::kCenc:
      return fuchsia::media::drm::ENCRYPTION_MODE_CENC;
    case EncryptionMode::kCbcs:
      return fuchsia::media::drm::ENCRYPTION_MODE_CBCS;
    default:
      NOTREACHED() << "unknown encryption mode " << static_cast<int>(mode);
      return "";
  }
}

fuchsia::media::KeyId GetKeyId(const std::string& key_id) {
  fuchsia::media::KeyId fuchsia_key_id;
  DCHECK_EQ(key_id.size(), fuchsia_key_id.data.size());
  std::copy(key_id.begin(), key_id.end(), fuchsia_key_id.data.begin());
  return fuchsia_key_id;
}

std::vector<fuchsia::media::SubsampleEntry> GetSubsamples(
    const std::vector<SubsampleEntry>& subsamples) {
  std::vector<fuchsia::media::SubsampleEntry> fuchsia_subsamples(
      subsamples.size());

  for (size_t i = 0; i < subsamples.size(); i++) {
    fuchsia_subsamples[i].clear_bytes = subsamples[i].clear_bytes;
    fuchsia_subsamples[i].encrypted_bytes = subsamples[i].cypher_bytes;
  }

  return fuchsia_subsamples;
}

fuchsia::media::EncryptionPattern GetEncryptionPattern(
    EncryptionPattern pattern) {
  fuchsia::media::EncryptionPattern fuchsia_pattern;
  fuchsia_pattern.clear_blocks = pattern.skip_byte_block();
  fuchsia_pattern.encrypted_blocks = pattern.crypt_byte_block();
  return fuchsia_pattern;
}

fuchsia::media::FormatDetails GetFormatDetails(const DecryptConfig* config) {
  DCHECK(config);

  fuchsia::media::EncryptedFormat encrypted_format;
  encrypted_format.set_mode(GetEncryptionMode(config->encryption_mode()))
      .set_key_id(GetKeyId(config->key_id()))
      .set_init_vector(
          std::vector<uint8_t>(config->iv().begin(), config->iv().end()))
      .set_subsamples(GetSubsamples(config->subsamples()));
  if (config->encryption_mode() == EncryptionMode::kCbcs) {
    DCHECK(config->encryption_pattern().has_value());
    encrypted_format.set_pattern(
        GetEncryptionPattern(config->encryption_pattern().value()));
  }

  fuchsia::media::FormatDetails format;
  format.set_format_details_version_ordinal(0);
  format.mutable_domain()->crypto().set_encrypted(std::move(encrypted_format));
  return format;
}

}  // namespace

StreamProcessorDecryptor::StreamProcessorDecryptor(
    fuchsia::media::StreamProcessorPtr processor)
    : processor_(std::move(processor), this) {}

StreamProcessorDecryptor::~StreamProcessorDecryptor() = default;

void StreamProcessorDecryptor::Decrypt(scoped_refptr<DecoderBuffer> encrypted,
                                       Decryptor::DecryptCB decrypt_cb) {
  DCHECK(!encrypted->end_of_stream()) << "EOS frame is always clear.";
  DCHECK(!decrypt_cb_);
  DCHECK(!pending_encrypted_buffer_);

  decrypt_cb_ = std::move(decrypt_cb);

  // Input buffer writer is not available. Wait.
  if (!input_writer_) {
    pending_encrypted_buffer_ = std::move(encrypted);
    return;
  }

  if (!input_pool_->is_live()) {
    DLOG(ERROR) << "Input buffer pool is dead.";
    std::move(decrypt_cb_).Run(Decryptor::kError, nullptr);
    return;
  }

  // Decryptor can only process one buffer at a time, which means there
  // should be always enough unused buffers.
  base::Optional<size_t> buf_index = input_writer_->Acquire();

  // No available input buffer. Just wait for the next available one.
  if (!buf_index.has_value()) {
    pending_encrypted_buffer_ = std::move(encrypted);
    return;
  }

  size_t index = buf_index.value();

  size_t bytes = input_writer_->Write(
      index, base::make_span(encrypted->data(), encrypted->data_size()));
  if (bytes < encrypted->data_size()) {
    // The encrypted data size is too big. Decryptor should consider
    // splitting the buffer and update the IV and subsample entries.
    // TODO(yucliu): Handle large encrypted buffer correctly. For now, just
    // reject the decryption.
    DLOG(ERROR) << "Encrypted data size is too big.";
    std::move(decrypt_cb_).Run(Decryptor::kError, nullptr);
    return;
  }

  const DecryptConfig* decrypt_config = encrypted->decrypt_config();
  DCHECK(decrypt_config);

  auto input_packet = StreamProcessorHelper::IoPacket::CreateInput(
      index, encrypted->data_size(), encrypted->timestamp(),
      GetFormatDetails(decrypt_config),
      base::BindOnce(&StreamProcessorDecryptor::OnInputPacketReleased,
                     base::Unretained(this), index));

  processor_.Process(std::move(input_packet));
}

void StreamProcessorDecryptor::CancelDecrypt() {
  // Close current stream and drop all the cached decoder buffers.
  // Keep input/output_pool_ to avoid buffer re-allocation.
  processor_.Reset();
  pending_encrypted_buffer_ = nullptr;

  // Fire |decrypt_cb_| immediately as required by Decryptor::CancelDecrypt.
  if (decrypt_cb_)
    std::move(decrypt_cb_).Run(Decryptor::kSuccess, nullptr);
}

// StreamProcessorHelper::Client implementation:
void StreamProcessorDecryptor::AllocateInputBuffers(
    const fuchsia::media::StreamBufferConstraints& stream_constraints) {
  base::Optional<fuchsia::sysmem::BufferCollectionConstraints>
      buffer_constraints =
          SysmemBufferWriter::GetRecommendedConstraints(stream_constraints);

  if (!buffer_constraints.has_value()) {
    OnError();
    return;
  }

  input_pool_creator_ =
      allocator_.MakeBufferPoolCreator(1 /* num_shared_token */);

  input_pool_creator_->Create(
      std::move(buffer_constraints).value(),
      base::BindOnce(&StreamProcessorDecryptor::OnInputBufferPoolAvailable,
                     base::Unretained(this)));
}

void StreamProcessorDecryptor::AllocateOutputBuffers(
    const fuchsia::media::StreamBufferConstraints& stream_constraints) {
  if (!stream_constraints.has_packet_count_for_client_max() ||
      !stream_constraints.has_packet_count_for_client_min()) {
    DLOG(ERROR) << "StreamBufferConstraints doesn't contain required fields.";
    OnError();
    return;
  }

  size_t max_used_output_buffers = std::min(
      kMaxUsedOutputFrames, stream_constraints.packet_count_for_client_max());
  max_used_output_buffers = std::max(
      max_used_output_buffers,
      static_cast<size_t>(stream_constraints.packet_count_for_client_min()));

  output_pool_creator_ =
      allocator_.MakeBufferPoolCreator(1 /* num_shared_token */);

  output_pool_creator_->Create(
      SysmemBufferReader::GetRecommendedConstraints(max_used_output_buffers),
      base::BindOnce(&StreamProcessorDecryptor::OnOutputBufferPoolAvailable,
                     base::Unretained(this), max_used_output_buffers));
}

void StreamProcessorDecryptor::OnProcessEos() {
  // Decryptor never pushes EOS frame.
  NOTREACHED();
}

void StreamProcessorDecryptor::OnOutputFormat(
    fuchsia::media::StreamOutputFormat format) {}

void StreamProcessorDecryptor::OnOutputPacket(
    std::unique_ptr<StreamProcessorHelper::IoPacket> packet) {
  DCHECK(output_reader_);
  if (!output_pool_->is_live()) {
    DLOG(ERROR) << "Output buffer pool is dead.";
    return;
  }

  auto clear_buffer = base::MakeRefCounted<DecoderBuffer>(packet->size());
  clear_buffer->set_timestamp(packet->timestamp());

  bool read_success =
      output_reader_->Read(packet->index(), packet->offset(),
                           base::make_span(clear_buffer->writable_data(),
                                           clear_buffer->data_size()));

  if (!read_success) {
    DLOG(ERROR) << "Fail to get decrypted result.";
    std::move(decrypt_cb_).Run(Decryptor::kError, nullptr);
    return;
  }

  std::move(decrypt_cb_).Run(Decryptor::kSuccess, std::move(clear_buffer));
}

void StreamProcessorDecryptor::OnNoKey() {
  if (decrypt_cb_)
    std::move(decrypt_cb_).Run(Decryptor::kNoKey, nullptr);
}

void StreamProcessorDecryptor::OnError() {
  pending_encrypted_buffer_ = nullptr;
  if (decrypt_cb_)
    std::move(decrypt_cb_).Run(Decryptor::kError, nullptr);

  processor_.Reset();
}

void StreamProcessorDecryptor::OnInputPacketReleased(size_t index) {
  input_writer_->Release(index);

  if (!pending_encrypted_buffer_)
    return;

  DCHECK(decrypt_cb_);

  // If there're pending decryption request, handle it now since we have
  // available input buffers.
  Decrypt(std::move(pending_encrypted_buffer_), std::move(decrypt_cb_));
}

void StreamProcessorDecryptor::OnInputBufferPoolAvailable(
    std::unique_ptr<SysmemBufferPool> pool) {
  if (!pool) {
    DLOG(ERROR) << "Fail to allocate input buffer.";
    OnError();
    return;
  }

  input_pool_ = std::move(pool);

  // Provide token before enabling writer. Tokens must be provided to
  // StreamProcessor before getting the allocated buffers.
  processor_.CompleteInputBuffersAllocation(input_pool_->TakeToken());

  input_pool_->CreateWriter(
      base::BindOnce(&StreamProcessorDecryptor::OnInputBufferPoolWriter,
                     base::Unretained(this)));
}

void StreamProcessorDecryptor::OnInputBufferPoolWriter(
    std::unique_ptr<SysmemBufferWriter> writer) {
  if (!writer) {
    LOG(ERROR) << "Fail to enable input buffer writer";
    OnError();
    return;
  }

  DCHECK(!input_writer_);
  input_writer_ = std::move(writer);

  if (pending_encrypted_buffer_) {
    DCHECK(decrypt_cb_);
    Decrypt(std::move(pending_encrypted_buffer_), std::move(decrypt_cb_));
  }
}

void StreamProcessorDecryptor::OnOutputBufferPoolAvailable(
    size_t max_used_output_buffers,
    std::unique_ptr<SysmemBufferPool> pool) {
  if (!pool) {
    LOG(ERROR) << "Fail to allocate output buffer.";
    OnError();
    return;
  }

  output_pool_ = std::move(pool);

  // Provide token before enabling reader. Tokens must be provided to
  // StreamProcessor before getting the allocated buffers.
  processor_.CompleteOutputBuffersAllocation(max_used_output_buffers,
                                             output_pool_->TakeToken());

  output_pool_->CreateReader(
      base::BindOnce(&StreamProcessorDecryptor::OnOutputBufferPoolReader,
                     base::Unretained(this)));
}

void StreamProcessorDecryptor::OnOutputBufferPoolReader(
    std::unique_ptr<SysmemBufferReader> reader) {
  if (!reader) {
    LOG(ERROR) << "Fail to enable output buffer reader.";
    OnError();
    return;
  }

  DCHECK(!output_reader_);
  output_reader_ = std::move(reader);
}

std::unique_ptr<StreamProcessorDecryptor>
StreamProcessorDecryptor::CreateAudioDecryptor(
    fuchsia::media::drm::ContentDecryptionModule* cdm) {
  DCHECK(cdm);

  fuchsia::media::drm::DecryptorParams params;
  params.set_require_secure_mode(false);
  params.mutable_input_details()->set_format_details_version_ordinal(0);
  fuchsia::media::StreamProcessorPtr stream_processor;
  cdm->CreateDecryptor(std::move(params), stream_processor.NewRequest());

  return std::make_unique<StreamProcessorDecryptor>(
      std::move(stream_processor));
}

}  // namespace media
