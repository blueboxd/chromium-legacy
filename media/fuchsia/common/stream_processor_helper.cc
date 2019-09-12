// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/common/stream_processor_helper.h"

#include "base/bind.h"
#include "base/fuchsia/fuchsia_logging.h"

namespace media {

StreamProcessorHelper::IoPacket::IoPacket(size_t index,
                                          size_t offset,
                                          size_t size,
                                          base::TimeDelta timestamp,
                                          fuchsia::media::FormatDetails format,
                                          base::OnceClosure destroy_cb)
    : index_(index),
      offset_(offset),
      size_(size),
      timestamp_(timestamp),
      format_(std::move(format)),
      destroy_cb_(std::move(destroy_cb)) {}

StreamProcessorHelper::IoPacket::~IoPacket() = default;

// static
std::unique_ptr<StreamProcessorHelper::IoPacket>
StreamProcessorHelper::IoPacket::CreateInput(
    size_t index,
    size_t size,
    base::TimeDelta timestamp,
    fuchsia::media::FormatDetails format,
    base::OnceClosure destroy_cb) {
  return std::make_unique<IoPacket>(index, 0 /* offset */, size, timestamp,
                                    std::move(format), std::move(destroy_cb));
}

// static
std::unique_ptr<StreamProcessorHelper::IoPacket>
StreamProcessorHelper::IoPacket::CreateOutput(size_t index,
                                              size_t offset,
                                              size_t size,
                                              base::TimeDelta timestamp,
                                              base::OnceClosure destroy_cb) {
  return std::make_unique<IoPacket>(index, offset, size, timestamp,
                                    fuchsia::media::FormatDetails(),
                                    std::move(destroy_cb));
}

StreamProcessorHelper::StreamProcessorHelper(
    fuchsia::media::StreamProcessorPtr processor,
    Client* client)
    : processor_(std::move(processor)), client_(client), weak_factory_(this) {
  DCHECK(processor_);
  DCHECK(client_);
  weak_this_ = weak_factory_.GetWeakPtr();

  processor_.set_error_handler(
      [this](zx_status_t status) {
        ZX_LOG(ERROR, status)
            << "The fuchsia.media.StreamProcessor channel was terminated.";
        OnError();
      });

  processor_.events().OnStreamFailed =
      fit::bind_member(this, &StreamProcessorHelper::OnStreamFailed);
  processor_.events().OnInputConstraints =
      fit::bind_member(this, &StreamProcessorHelper::OnInputConstraints);
  processor_.events().OnFreeInputPacket =
      fit::bind_member(this, &StreamProcessorHelper::OnFreeInputPacket);
  processor_.events().OnOutputConstraints =
      fit::bind_member(this, &StreamProcessorHelper::OnOutputConstraints);
  processor_.events().OnOutputFormat =
      fit::bind_member(this, &StreamProcessorHelper::OnOutputFormat);
  processor_.events().OnOutputPacket =
      fit::bind_member(this, &StreamProcessorHelper::OnOutputPacket);
  processor_.events().OnOutputEndOfStream =
      fit::bind_member(this, &StreamProcessorHelper::OnOutputEndOfStream);

  processor_->EnableOnStreamFailed();
}

StreamProcessorHelper::~StreamProcessorHelper() = default;

void StreamProcessorHelper::Process(std::unique_ptr<IoPacket> input) {
  DCHECK(input);
  DCHECK(processor_);

  fuchsia::media::Packet packet;
  packet.mutable_header()->set_buffer_lifetime_ordinal(
      input_buffer_lifetime_ordinal_);
  packet.mutable_header()->set_packet_index(input->index());
  packet.set_buffer_index(packet.header().packet_index());
  packet.set_timestamp_ish(input->timestamp().InNanoseconds());
  packet.set_stream_lifetime_ordinal(stream_lifetime_ordinal_);
  packet.set_start_offset(input->offset());
  packet.set_valid_length_bytes(input->size());

  active_stream_ = true;

  if (!input->format().IsEmpty()) {
    processor_->QueueInputFormatDetails(stream_lifetime_ordinal_,
                                        fidl::Clone(input->format()));
  }

  DCHECK(input_packets_.find(input->index()) == input_packets_.end());
  input_packets_[input->index()] = std::move(input);
  processor_->QueueInputPacket(std::move(packet));
}

void StreamProcessorHelper::ProcessEos() {
  DCHECK(processor_);

  active_stream_ = true;
  processor_->QueueInputEndOfStream(stream_lifetime_ordinal_);
  processor_->FlushEndOfStreamAndCloseStream(stream_lifetime_ordinal_);
}

void StreamProcessorHelper::Reset() {
  if (active_stream_) {
    return;
  }

  processor_->CloseCurrentStream(stream_lifetime_ordinal_,
                                 /*release_input_buffers=*/false,
                                 /*release_output_buffers=*/false);
  stream_lifetime_ordinal_ += 2;
  active_stream_ = false;
  input_packets_.clear();
}

void StreamProcessorHelper::OnStreamFailed(uint64_t stream_lifetime_ordinal,
                                           fuchsia::media::StreamError error) {
  if (stream_lifetime_ordinal_ != stream_lifetime_ordinal) {
    return;
  }

  if (error == fuchsia::media::StreamError::DECRYPTOR_NO_KEY) {
    client_->OnNoKey();
    return;
  }

  OnError();
}

void StreamProcessorHelper::OnInputConstraints(
    fuchsia::media::StreamBufferConstraints constraints) {
  // Buffer lifetime ordinal is an odd number incremented by 2 for each buffer
  // generation as required by StreamProcessor.
  input_buffer_lifetime_ordinal_ += 2;

  // Default settings are used in CompleteInputBuffersAllocation to finish
  // StreamProcessor input buffers setup.
  if (!constraints.has_default_settings() ||
      !constraints.default_settings().has_packet_count_for_server() ||
      !constraints.default_settings().has_packet_count_for_client()) {
    DLOG(ERROR)
        << "Received OnInputConstraints() with missing required fields.";
    OnError();
    return;
  }

  DCHECK(input_packets_.empty());
  input_buffer_constraints_ = std::move(constraints);

  client_->AllocateInputBuffers(input_buffer_constraints_);
}

void StreamProcessorHelper::OnFreeInputPacket(
    fuchsia::media::PacketHeader free_input_packet) {
  if (!free_input_packet.has_buffer_lifetime_ordinal() ||
      !free_input_packet.has_packet_index()) {
    DLOG(ERROR) << "Received OnFreeInputPacket() with missing required fields.";
    OnError();
    return;
  }

  if (free_input_packet.buffer_lifetime_ordinal() !=
      input_buffer_lifetime_ordinal_) {
    return;
  }

  auto it = input_packets_.find(free_input_packet.packet_index());
  if (it == input_packets_.end()) {
    DLOG(WARNING) << "Received OnFreeInputPacket() with invalid packet index.";
    return;
  }

  input_packets_.erase(it);
}

void StreamProcessorHelper::OnOutputConstraints(
    fuchsia::media::StreamOutputConstraints output_constraints) {
  if (!output_constraints.has_stream_lifetime_ordinal()) {
    DLOG(ERROR)
        << "Received OnOutputConstraints() with missing required fields.";
    OnError();
    return;
  }

  if (output_constraints.stream_lifetime_ordinal() !=
      stream_lifetime_ordinal_) {
    return;
  }

  if (!output_constraints.has_buffer_constraints_action_required() ||
      !output_constraints.buffer_constraints_action_required()) {
    return;
  }

  if (!output_constraints.has_buffer_constraints()) {
    DLOG(ERROR) << "Received OnOutputConstraints() which requires buffer "
                   "constraints action, but without buffer constraints.";
    OnError();
    return;
  }

  // StreamProcessor API expects odd buffer lifetime ordinal, which is
  // incremented by 2 for each buffer generation.
  output_buffer_lifetime_ordinal_ += 2;

  output_buffer_constraints_ =
      std::move(*output_constraints.mutable_buffer_constraints());

  client_->AllocateOutputBuffers(output_buffer_constraints_);
}

void StreamProcessorHelper::OnOutputFormat(
    fuchsia::media::StreamOutputFormat output_format) {
  if (!output_format.has_stream_lifetime_ordinal() ||
      !output_format.has_format_details()) {
    DLOG(ERROR) << "Received OnOutputFormat() with missing required fields.";
    OnError();
    return;
  }

  if (output_format.stream_lifetime_ordinal() != stream_lifetime_ordinal_) {
    return;
  }

  client_->OnOutputFormat(std::move(output_format));
}

void StreamProcessorHelper::OnOutputPacket(fuchsia::media::Packet output_packet,
                                           bool error_detected_before,
                                           bool error_detected_during) {
  if (!output_packet.has_header() ||
      !output_packet.header().has_buffer_lifetime_ordinal() ||
      !output_packet.header().has_packet_index() ||
      !output_packet.has_buffer_index()) {
    DLOG(ERROR) << "Received OnOutputPacket() with missing required fields.";
    OnError();
    return;
  }

  if (output_packet.header().buffer_lifetime_ordinal() !=
      output_buffer_lifetime_ordinal_) {
    return;
  }

  auto buffer_index = output_packet.buffer_index();
  auto packet_index = output_packet.header().packet_index();
  base::TimeDelta timestamp;
  if (output_packet.has_timestamp_ish()) {
    timestamp = base::TimeDelta::FromNanoseconds(output_packet.timestamp_ish());
  }

  client_->OnOutputPacket(IoPacket::CreateOutput(
      buffer_index, output_packet.start_offset(),
      output_packet.valid_length_bytes(), timestamp,
      base::BindOnce(&StreamProcessorHelper::OnRecycleOutputBuffer, weak_this_,
                     output_buffer_lifetime_ordinal_, packet_index)));
}

void StreamProcessorHelper::OnOutputEndOfStream(
    uint64_t stream_lifetime_ordinal,
    bool error_detected_before) {
  if (stream_lifetime_ordinal != stream_lifetime_ordinal_) {
    return;
  }

  stream_lifetime_ordinal_ += 2;
  active_stream_ = false;

  client_->OnProcessEos();
}

void StreamProcessorHelper::OnError() {
  processor_.Unbind();
  client_->OnError();
}

void StreamProcessorHelper::CompleteInputBuffersAllocation(
    fuchsia::sysmem::BufferCollectionTokenPtr sysmem_token) {
  DCHECK(!input_buffer_constraints_.IsEmpty());
  fuchsia::media::StreamBufferPartialSettings settings;
  settings.set_buffer_lifetime_ordinal(input_buffer_lifetime_ordinal_);
  settings.set_buffer_constraints_version_ordinal(
      input_buffer_constraints_.buffer_constraints_version_ordinal());
  settings.set_single_buffer_mode(false);
  settings.set_packet_count_for_server(
      input_buffer_constraints_.default_settings().packet_count_for_server());
  settings.set_packet_count_for_client(
      input_buffer_constraints_.default_settings().packet_count_for_client());
  settings.set_sysmem_token(std::move(sysmem_token));
  processor_->SetInputBufferPartialSettings(std::move(settings));
}

void StreamProcessorHelper::CompleteOutputBuffersAllocation(
    size_t max_used_output_buffers,
    fuchsia::sysmem::BufferCollectionTokenPtr collection_token) {
  DCHECK(!output_buffer_constraints_.IsEmpty());
  DCHECK_LE(max_used_output_buffers,
            output_buffer_constraints_.packet_count_for_client_max());

  // Pass new output buffer settings to the stream processor.
  fuchsia::media::StreamBufferPartialSettings settings;
  settings.set_buffer_lifetime_ordinal(output_buffer_lifetime_ordinal_);
  settings.set_buffer_constraints_version_ordinal(
      output_buffer_constraints_.buffer_constraints_version_ordinal());
  settings.set_packet_count_for_client(max_used_output_buffers);
  settings.set_packet_count_for_server(
      output_buffer_constraints_.packet_count_for_server_recommended());
  settings.set_sysmem_token(std::move(collection_token));
  processor_->SetOutputBufferPartialSettings(std::move(settings));
  processor_->CompleteOutputBufferPartialSettings(
      output_buffer_lifetime_ordinal_);
}

void StreamProcessorHelper::OnRecycleOutputBuffer(
    uint64_t buffer_lifetime_ordinal,
    uint32_t packet_index) {
  if (!processor_)
    return;

  if (buffer_lifetime_ordinal != output_buffer_lifetime_ordinal_)
    return;

  fuchsia::media::PacketHeader header;
  header.set_buffer_lifetime_ordinal(buffer_lifetime_ordinal);
  header.set_packet_index(packet_index);
  processor_->RecycleOutputPacket(std::move(header));
}

}  // namespace media
