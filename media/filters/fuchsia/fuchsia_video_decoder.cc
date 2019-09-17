// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/fuchsia/fuchsia_video_decoder.h"

#include <fuchsia/media/cpp/fidl.h>
#include <fuchsia/mediacodec/cpp/fidl.h>
#include <fuchsia/sysmem/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>
#include <zircon/rights.h>

#include "base/bind.h"
#include "base/bits.h"
#include "base/callback_helpers.h"
#include "base/fuchsia/default_context.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_metrics.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_native_pixmap.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/cdm_context.h"
#include "media/base/decryptor.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/fuchsia/common/stream_processor_helper.h"
#include "media/fuchsia/common/sysmem_buffer_pool.h"
#include "media/fuchsia/common/sysmem_buffer_writer_queue.h"
#include "third_party/libyuv/include/libyuv/video_common.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/client_native_pixmap_factory.h"
#include "ui/ozone/public/client_native_pixmap_factory_ozone.h"

namespace media {

namespace {

// Value passed to the codec as packet_count_for_client. It's number of output
// buffers that we expect to hold on to in the renderer.
//
// TODO(sergeyu): Figure out the right number of buffers to request. Currently
// the codec doesn't allow to reserve more than 2 client buffers, but it still
// works properly when the client holds to more than that.
const uint32_t kMaxUsedOutputFrames = 8;

// Helper used to hold mailboxes for the output textures. OutputMailbox may
// outlive FuchsiaVideoDecoder if is referenced by a VideoFrame.
class OutputMailbox {
 public:
  OutputMailbox(gpu::SharedImageInterface* shared_image_interface,
                gpu::ContextSupport* gpu_context_support,
                std::unique_ptr<gfx::GpuMemoryBuffer> gmb)
      : shared_image_interface_(shared_image_interface),
        gpu_context_support_(gpu_context_support),
        weak_factory_(this) {
    uint32_t usage = gpu::SHARED_IMAGE_USAGE_RASTER |
                     gpu::SHARED_IMAGE_USAGE_OOP_RASTERIZATION |
                     gpu::SHARED_IMAGE_USAGE_DISPLAY |
                     gpu::SHARED_IMAGE_USAGE_SCANOUT;
    mailbox_ = shared_image_interface_->CreateSharedImage(
        gmb.get(), nullptr, gfx::ColorSpace(), usage);
  }
  ~OutputMailbox() {
    shared_image_interface_->DestroySharedImage(sync_token_, mailbox_);
  }

  const gpu::Mailbox& mailbox() { return mailbox_; }

  // Create a new video frame that wraps the mailbox. |reuse_callback| will be
  // called when the mailbox can be reused.
  scoped_refptr<VideoFrame> CreateFrame(VideoPixelFormat pixel_format,
                                        const gfx::Size& coded_size,
                                        const gfx::Rect& visible_rect,
                                        const gfx::Size& natural_size,
                                        base::TimeDelta timestamp,
                                        base::OnceClosure reuse_callback) {
    DCHECK(!is_used_);
    is_used_ = true;
    reuse_callback_ = std::move(reuse_callback);

    gpu::MailboxHolder mailboxes[VideoFrame::kMaxPlanes];
    mailboxes[0].mailbox = mailbox_;
    mailboxes[0].sync_token = shared_image_interface_->GenUnverifiedSyncToken();

    return VideoFrame::WrapNativeTextures(
        pixel_format, mailboxes,
        BindToCurrentLoop(base::BindOnce(&OutputMailbox::OnFrameDestroyed,
                                         base::Unretained(this))),
        coded_size, visible_rect, natural_size, timestamp);
  }

  // Called by FuchsiaVideoDecoder when it no longer needs this mailbox.
  void Release() {
    if (is_used_) {
      // The mailbox is referenced by a VideoFrame. It will be deleted  as soon
      // as the frame is destroyed.
      DCHECK(reuse_callback_);
      reuse_callback_ = base::Closure();
    } else {
      delete this;
    }
  }

 private:
  void OnFrameDestroyed(const gpu::SyncToken& sync_token) {
    DCHECK(is_used_);
    is_used_ = false;
    sync_token_ = sync_token;

    if (!reuse_callback_) {
      // If the mailbox cannot be reused then we can just delete it.
      delete this;
      return;
    }

    gpu_context_support_->SignalSyncToken(
        sync_token_,
        BindToCurrentLoop(base::BindOnce(&OutputMailbox::OnSyncTokenSignaled,
                                         weak_factory_.GetWeakPtr())));
  }

  void OnSyncTokenSignaled() {
    sync_token_.Clear();
    std::move(reuse_callback_).Run();
  }

  gpu::SharedImageInterface* const shared_image_interface_;
  gpu::ContextSupport* const gpu_context_support_;

  gpu::Mailbox mailbox_;
  gpu::SyncToken sync_token_;

  // Set to true when the mailbox is referenced by a video frame.
  bool is_used_ = false;

  base::OnceClosure reuse_callback_;

  base::WeakPtrFactory<OutputMailbox> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(OutputMailbox);
};

}  // namespace

class FuchsiaVideoDecoder : public VideoDecoder {
 public:
  FuchsiaVideoDecoder(gpu::SharedImageInterface* shared_image_interface,
                      gpu::ContextSupport* gpu_context_support,
                      bool enable_sw_decoding);
  ~FuchsiaVideoDecoder() override;

  // VideoDecoder implementation.
  std::string GetDisplayName() const override;
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) override;
  void Reset(base::OnceClosure closure) override;
  bool NeedsBitstreamConversion() const override;
  bool CanReadWithoutStalling() const override;
  int GetMaxDecodeRequests() const override;

 private:
  // Event handlers for |codec_|.
  void OnStreamFailed(uint64_t stream_lifetime_ordinal,
                      fuchsia::media::StreamError error);
  void OnInputConstraints(
      fuchsia::media::StreamBufferConstraints input_constraints);
  void OnFreeInputPacket(fuchsia::media::PacketHeader free_input_packet);
  void OnOutputConstraints(
      fuchsia::media::StreamOutputConstraints output_constraints);
  void OnOutputFormat(fuchsia::media::StreamOutputFormat output_format);
  void OnOutputPacket(fuchsia::media::Packet output_packet,
                      bool error_detected_before,
                      bool error_detected_during);
  void OnOutputEndOfStream(uint64_t stream_lifetime_ordinal,
                           bool error_detected_before);

  // Called on errors to shutdown the decoder and notify the client.
  void OnError();

  // Callback for |input_buffer_collection_creator_->Create()|.
  void OnInputBufferPoolCreated(fuchsia::media::StreamBufferConstraints,
                                std::unique_ptr<SysmemBufferPool> pool);

  // Callback for |input_buffer_collection_->CreateWriter()|.
  void OnWriterCreated(std::unique_ptr<SysmemBufferWriter> writer);

  // Callbacks for |input_writer_|.
  void SendInputPacket(const DecoderBuffer* buffer,
                       StreamProcessorHelper::IoPacket packet);
  void ProcessEndOfStream();

  // Called by OnOutputConstraints() to initialize input buffers.
  void InitializeOutputBufferCollection(
      fuchsia::media::StreamBufferConstraints constraints,
      fuchsia::sysmem::BufferCollectionTokenPtr collection_token_for_codec,
      fuchsia::sysmem::BufferCollectionTokenPtr collection_token_for_gpu);

  // Called by OutputMailbox to signal that the mailbox and buffer can be
  // reused.
  void OnReuseMailbox(uint32_t buffer_index, uint32_t packet_index);

  // Releases BufferCollection currently used for output buffers if any.
  void ReleaseOutputBuffers();

  gpu::SharedImageInterface* const shared_image_interface_;
  gpu::ContextSupport* const gpu_context_support_;
  const bool enable_sw_decoding_;

  OutputCB output_cb_;

  // Aspect ratio specified in container, or 1.0 if it's not specified. This
  // value is used only if the aspect ratio is not specified in the bitstream.
  float container_pixel_aspect_ratio_ = 1.0;

  // TODO(sergeyu): Use StreamProcessorHelper.
  fuchsia::media::StreamProcessorPtr codec_;
  BufferAllocator sysmem_allocator_;
  std::unique_ptr<gfx::ClientNativePixmapFactory> client_native_pixmap_factory_;

  uint64_t stream_lifetime_ordinal_ = 1;

  // Set to true if we've sent an input packet with the current
  // stream_lifetime_ordinal_.
  bool active_stream_ = false;

  // Input buffers.
  uint64_t input_buffer_lifetime_ordinal_ = 1;
  SysmemBufferWriterQueue input_writer_queue_;
  std::unique_ptr<SysmemBufferPool::Creator> input_buffer_collection_creator_;
  std::unique_ptr<SysmemBufferPool> input_buffer_collection_;
  base::flat_map<size_t, StreamProcessorHelper::IoPacket>
      in_flight_input_packets_;
  std::deque<DecodeCB> decode_callbacks_;

  // Output buffers.
  fuchsia::media::VideoUncompressedFormat output_format_;
  uint64_t output_buffer_lifetime_ordinal_ = 1;
  fuchsia::sysmem::BufferCollectionPtr output_buffer_collection_;
  gfx::SysmemBufferCollectionId output_buffer_collection_id_;
  std::vector<OutputMailbox*> output_mailboxes_;

  int num_used_output_buffers_ = 0;
  int max_used_output_buffers_ = 0;

  // Non-null when flush is pending.
  VideoDecoder::DecodeCB pending_flush_cb_;

  base::WeakPtr<FuchsiaVideoDecoder> weak_this_;
  base::WeakPtrFactory<FuchsiaVideoDecoder> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FuchsiaVideoDecoder);
};

FuchsiaVideoDecoder::FuchsiaVideoDecoder(
    gpu::SharedImageInterface* shared_image_interface,
    gpu::ContextSupport* gpu_context_support,
    bool enable_sw_decoding)
    : shared_image_interface_(shared_image_interface),
      gpu_context_support_(gpu_context_support),
      enable_sw_decoding_(enable_sw_decoding),
      client_native_pixmap_factory_(ui::CreateClientNativePixmapFactoryOzone()),
      weak_factory_(this) {
  DCHECK(shared_image_interface_);
  weak_this_ = weak_factory_.GetWeakPtr();
}

FuchsiaVideoDecoder::~FuchsiaVideoDecoder() {
  ReleaseOutputBuffers();
}

std::string FuchsiaVideoDecoder::GetDisplayName() const {
  return "FuchsiaVideoDecoder";
}

void FuchsiaVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                     bool low_delay,
                                     CdmContext* cdm_context,
                                     InitCB init_cb,
                                     const OutputCB& output_cb,
                                     const WaitingCB& waiting_cb) {
  auto done_callback = BindToCurrentLoop(std::move(init_cb));

  if (config.is_encrypted()) {
    // Caller makes sure |cdm_context| is available if the stream is encrypted.
    if (!cdm_context) {
      LOG(ERROR) << "No cdm context for encrypted stream.";
      std::move(done_callback).Run(false);
      return;
    }

    // If decryptor is decrypt only, return false here to allow decoder selector
    // to choose DecryptingDemuxerStream, which will handle the decryption and
    // pass the clear stream to this decoder.
    Decryptor* decryptor = cdm_context->GetDecryptor();
    if (decryptor && decryptor->CanAlwaysDecrypt()) {
      DVLOG(1) << "Decryptor can always decrypt, return false.";
      std::move(done_callback).Run(false);
      return;
    }
  }

  output_cb_ = output_cb;
  container_pixel_aspect_ratio_ = config.GetPixelAspectRatio();

  fuchsia::mediacodec::CreateDecoder_Params codec_params;
  codec_params.mutable_input_details()->set_format_details_version_ordinal(0);

  switch (config.codec()) {
    case kCodecH264:
      codec_params.mutable_input_details()->set_mime_type("video/h264");
      break;
    case kCodecVP8:
      codec_params.mutable_input_details()->set_mime_type("video/vp8");
      break;
    case kCodecVP9:
      codec_params.mutable_input_details()->set_mime_type("video/vp9");
      break;
    case kCodecHEVC:
      codec_params.mutable_input_details()->set_mime_type("video/hevc");
      break;
    case kCodecAV1:
      codec_params.mutable_input_details()->set_mime_type("video/av1");
      break;

    default:
      std::move(done_callback).Run(false);
      return;
  }

  codec_params.set_promise_separate_access_units_on_input(true);
  codec_params.set_require_hw(!enable_sw_decoding_);

  auto codec_factory = base::fuchsia::ComponentContextForCurrentProcess()
                           ->svc()
                           ->Connect<fuchsia::mediacodec::CodecFactory>();
  codec_factory->CreateDecoder(std::move(codec_params), codec_.NewRequest());

  codec_.set_error_handler([this](zx_status_t status) {
    ZX_LOG(ERROR, status) << "fuchsia.mediacodec.Codec disconnected.";
    OnError();
  });

  codec_.events().OnStreamFailed =
      fit::bind_member(this, &FuchsiaVideoDecoder::OnStreamFailed);
  codec_.events().OnInputConstraints =
      fit::bind_member(this, &FuchsiaVideoDecoder::OnInputConstraints);
  codec_.events().OnFreeInputPacket =
      fit::bind_member(this, &FuchsiaVideoDecoder::OnFreeInputPacket);
  codec_.events().OnOutputConstraints =
      fit::bind_member(this, &FuchsiaVideoDecoder::OnOutputConstraints);
  codec_.events().OnOutputFormat =
      fit::bind_member(this, &FuchsiaVideoDecoder::OnOutputFormat);
  codec_.events().OnOutputPacket =
      fit::bind_member(this, &FuchsiaVideoDecoder::OnOutputPacket);
  codec_.events().OnOutputEndOfStream =
      fit::bind_member(this, &FuchsiaVideoDecoder::OnOutputEndOfStream);

  codec_->EnableOnStreamFailed();

  std::move(done_callback).Run(true);
}

void FuchsiaVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                                 DecodeCB decode_cb) {
  if (!codec_) {
    // Post the callback to the current sequence as DecoderStream doesn't expect
    // Decode() to complete synchronously.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(decode_cb), DecodeStatus::DECODE_ERROR));
    return;
  }

  decode_callbacks_.push_back(std::move(decode_cb));
  input_writer_queue_.EnqueueBuffer(buffer);
}

void FuchsiaVideoDecoder::Reset(base::OnceClosure closure) {
  // Call DecodeCB(ABORTED) for all pending decode requests.
  for (auto& cb : decode_callbacks_) {
    std::move(cb).Run(DecodeStatus::ABORTED);
  }

  input_writer_queue_.ResetQueue();

  if (active_stream_) {
    codec_->CloseCurrentStream(stream_lifetime_ordinal_,
                               /*release_input_buffers=*/false,
                               /*release_output_buffers=*/false);
    stream_lifetime_ordinal_ += 2;
    active_stream_ = false;
  }

  BindToCurrentLoop(std::move(closure)).Run();
}

bool FuchsiaVideoDecoder::NeedsBitstreamConversion() const {
  return true;
}

bool FuchsiaVideoDecoder::CanReadWithoutStalling() const {
  return num_used_output_buffers_ < max_used_output_buffers_;
}

int FuchsiaVideoDecoder::GetMaxDecodeRequests() const {
  // Add one extra request to be able to send new InputBuffer immediately after
  // OnFreeInputPacket().
  return input_writer_queue_.num_buffers() + 1;
}

void FuchsiaVideoDecoder::OnStreamFailed(uint64_t stream_lifetime_ordinal,
                                         fuchsia::media::StreamError error) {
  if (stream_lifetime_ordinal_ != stream_lifetime_ordinal) {
    return;
  }

  OnError();
}

void FuchsiaVideoDecoder::OnInputConstraints(
    fuchsia::media::StreamBufferConstraints stream_constraints) {
  // Buffer lifetime ordinal is an odd number incremented by 2 for each buffer
  // generation as required by StreamProcessor.
  input_buffer_lifetime_ordinal_ += 2;

  input_buffer_collection_.reset();
  input_writer_queue_.ResetBuffers();

  // Create buffer constrains for the input buffer collection.
  base::Optional<fuchsia::sysmem::BufferCollectionConstraints>
      buffer_constraints =
          SysmemBufferWriter::GetRecommendedConstraints(stream_constraints);
  if (!buffer_constraints.has_value()) {
    OnError();
    return;
  }

  // Request SysmemBufferPool with one token to share with the codec.
  input_buffer_collection_creator_ =
      sysmem_allocator_.MakeBufferPoolCreator(1 /* num_shared_token */);
  input_buffer_collection_creator_->Create(
      std::move(buffer_constraints).value(),
      base::BindOnce(&FuchsiaVideoDecoder::OnInputBufferPoolCreated,
                     base::Unretained(this), std::move(stream_constraints)));
}

void FuchsiaVideoDecoder::OnInputBufferPoolCreated(
    fuchsia::media::StreamBufferConstraints constraints,
    std::unique_ptr<SysmemBufferPool> pool) {
  if (!pool) {
    DLOG(ERROR) << "Fail to allocate input buffers for the codec.";
    OnError();
    return;
  }

  input_buffer_collection_ = std::move(pool);

  fuchsia::media::StreamBufferPartialSettings settings;
  settings.set_buffer_lifetime_ordinal(input_buffer_lifetime_ordinal_);
  settings.set_buffer_constraints_version_ordinal(
      constraints.buffer_constraints_version_ordinal());
  settings.set_single_buffer_mode(false);
  settings.set_packet_count_for_server(
      constraints.default_settings().packet_count_for_server());
  settings.set_packet_count_for_client(
      constraints.default_settings().packet_count_for_client());
  settings.set_sysmem_token(std::move(input_buffer_collection_->TakeToken()));
  codec_->SetInputBufferPartialSettings(std::move(settings));

  input_buffer_collection_->CreateWriter(base::BindOnce(
      &FuchsiaVideoDecoder::OnWriterCreated, base::Unretained(this)));
}

void FuchsiaVideoDecoder::OnWriterCreated(
    std::unique_ptr<SysmemBufferWriter> writer) {
  if (!writer) {
    OnError();
    return;
  }

  input_writer_queue_.Start(
      std::move(writer),
      base::BindRepeating(&FuchsiaVideoDecoder::SendInputPacket,
                          base::Unretained(this)),
      base::BindRepeating(&FuchsiaVideoDecoder::ProcessEndOfStream,
                          base::Unretained(this)));
}

void FuchsiaVideoDecoder::SendInputPacket(
    const DecoderBuffer* buffer,
    StreamProcessorHelper::IoPacket packet) {
  fuchsia::media::Packet media_packet;
  media_packet.mutable_header()->set_buffer_lifetime_ordinal(
      input_buffer_lifetime_ordinal_);
  media_packet.mutable_header()->set_packet_index(packet.index());
  media_packet.set_buffer_index(packet.index());
  media_packet.set_timestamp_ish(packet.timestamp().InNanoseconds());
  media_packet.set_stream_lifetime_ordinal(stream_lifetime_ordinal_);
  media_packet.set_start_offset(packet.offset());
  media_packet.set_valid_length_bytes(packet.size());
  media_packet.set_known_end_access_unit(packet.unit_end());
  codec_->QueueInputPacket(std::move(media_packet));

  active_stream_ = true;

  DCHECK(in_flight_input_packets_.find(packet.index()) ==
         in_flight_input_packets_.end());
  in_flight_input_packets_.insert_or_assign(packet.index(), std::move(packet));
}

void FuchsiaVideoDecoder::ProcessEndOfStream() {
  active_stream_ = true;
  codec_->QueueInputEndOfStream(stream_lifetime_ordinal_);
  codec_->FlushEndOfStreamAndCloseStream(stream_lifetime_ordinal_);

  DCHECK(!decode_callbacks_.empty());
  pending_flush_cb_ = std::move(decode_callbacks_.front());
  decode_callbacks_.pop_front();

  // Decode() is not supposed to be called after EOF.
  DCHECK(decode_callbacks_.empty());
}

void FuchsiaVideoDecoder::OnFreeInputPacket(
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

  if (free_input_packet.packet_index() >= input_writer_queue_.num_buffers()) {
    DLOG(ERROR) << "fuchsia.mediacodec sent OnFreeInputPacket() for an unknown "
                   "packet: buffer_lifetime_ordinal="
                << free_input_packet.buffer_lifetime_ordinal()
                << " packet_index=" << free_input_packet.packet_index();
    OnError();
    return;
  }

  auto it = in_flight_input_packets_.find(free_input_packet.packet_index());
  if (it == in_flight_input_packets_.end()) {
    DLOG(ERROR) << "Received OnFreeInputPacket() with invalid packet index.";
    OnError();
    return;
  }

  // Call DecodeCB if this was a last packet for a Decode request.
  bool call_decode_callback = it->second.unit_end();

  {
    // The packet should be destroyed only after it's removed from
    // |in_flight_input_packets_|. Otherwise SysmemBufferWriter may call
    // SendInputPacket() while the packet is still in
    // |in_flight_input_packets_|.
    auto packet = std::move(it->second);
    in_flight_input_packets_.erase(it);
  }

  if (call_decode_callback) {
    DCHECK(!decode_callbacks_.empty());
    std::move(decode_callbacks_.front()).Run(DecodeStatus::OK);
    decode_callbacks_.pop_front();
  }
}

void FuchsiaVideoDecoder::OnOutputConstraints(
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

  const fuchsia::media::StreamBufferConstraints& buffer_constraints =
      output_constraints.buffer_constraints();

  if (!buffer_constraints.has_default_settings() ||
      !buffer_constraints.has_packet_count_for_client_max() ||
      !buffer_constraints.default_settings().has_packet_count_for_server() ||
      !buffer_constraints.default_settings().has_packet_count_for_client()) {
    DLOG(ERROR)
        << "Received OnOutputConstraints() with missing required fields.";
    OnError();
    return;
  }

  ReleaseOutputBuffers();

  // mediacodec API expects odd buffer lifetime ordinal, which is incremented by
  // 2 for each buffer generation.
  output_buffer_lifetime_ordinal_ += 2;

  max_used_output_buffers_ = std::min(
      kMaxUsedOutputFrames, buffer_constraints.packet_count_for_client_max());

  // Create a new sysmem buffer collection token for the output buffers.
  fuchsia::sysmem::BufferCollectionTokenPtr collection_token;
  sysmem_allocator_.raw()->AllocateSharedCollection(
      collection_token.NewRequest());

  // Create sysmem tokens for the gpu process and the codec.
  fuchsia::sysmem::BufferCollectionTokenPtr collection_token_for_codec;
  collection_token->Duplicate(ZX_RIGHT_SAME_RIGHTS,
                              collection_token_for_codec.NewRequest());
  fuchsia::sysmem::BufferCollectionTokenPtr collection_token_for_gpu;
  collection_token->Duplicate(ZX_RIGHT_SAME_RIGHTS,
                              collection_token_for_gpu.NewRequest());

  // Convert the token to a BufferCollection connection.
  sysmem_allocator_.raw()->BindSharedCollection(
      std::move(collection_token), output_buffer_collection_.NewRequest());
  output_buffer_collection_.set_error_handler([this](zx_status_t status) {
    ZX_LOG(ERROR, status) << "fuchsia.sysmem.BufferCollection disconnected.";
    OnError();
  });

  // BufferCollection needs to be synchronized before we can use it.
  output_buffer_collection_->Sync(
      [this,
       buffer_constraints = std::move(
           std::move(*output_constraints.mutable_buffer_constraints())),
       collection_token_for_codec = std::move(collection_token_for_codec),
       collection_token_for_gpu =
           std::move(collection_token_for_gpu)]() mutable {
        InitializeOutputBufferCollection(std::move(buffer_constraints),
                                         std::move(collection_token_for_codec),
                                         std::move(collection_token_for_gpu));
      });
}

void FuchsiaVideoDecoder::OnOutputFormat(
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

  auto* format = output_format.mutable_format_details();
  if (!format->has_domain() || !format->domain().is_video() ||
      !format->domain().video().is_uncompressed()) {
    DLOG(ERROR) << "Received OnOutputFormat() with invalid format.";
    OnError();
    return;
  }

  output_format_ = std::move(format->mutable_domain()->video().uncompressed());
}

void FuchsiaVideoDecoder::OnOutputPacket(fuchsia::media::Packet output_packet,
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

  fuchsia::sysmem::PixelFormatType sysmem_pixel_format =
      output_format_.image_format.pixel_format.type;

  VideoPixelFormat pixel_format;
  gfx::BufferFormat buffer_format;
  switch (sysmem_pixel_format) {
    case fuchsia::sysmem::PixelFormatType::NV12:
      pixel_format = PIXEL_FORMAT_NV12;
      buffer_format = gfx::BufferFormat::YUV_420_BIPLANAR;
      break;

    case fuchsia::sysmem::PixelFormatType::I420:
    case fuchsia::sysmem::PixelFormatType::YV12:
      pixel_format = PIXEL_FORMAT_I420;
      buffer_format = gfx::BufferFormat::YVU_420;
      break;

    default:
      DLOG(ERROR) << "Unsupported pixel format: "
                  << static_cast<int>(sysmem_pixel_format);
      OnError();
      return;
  }

  size_t buffer_index = output_packet.buffer_index();
  if (buffer_index >= output_mailboxes_.size()) {
    DLOG(ERROR)
        << "mediacodec generated output packet with an invalid buffer_index="
        << buffer_index << " for output buffer collection with only "
        << output_mailboxes_.size() << " packets.";
    OnError();
    return;
  }

  auto coded_size = gfx::Size(output_format_.primary_width_pixels,
                              output_format_.primary_height_pixels);

  if (!output_mailboxes_[buffer_index]) {
    gfx::GpuMemoryBufferHandle gmb_handle;
    gmb_handle.type = gfx::NATIVE_PIXMAP;
    gmb_handle.native_pixmap_handle.buffer_collection_id =
        output_buffer_collection_id_;
    gmb_handle.native_pixmap_handle.buffer_index = buffer_index;

    auto gmb = gpu::GpuMemoryBufferImplNativePixmap::CreateFromHandle(
        client_native_pixmap_factory_.get(), std::move(gmb_handle), coded_size,
        buffer_format, gfx::BufferUsage::GPU_READ,
        gpu::GpuMemoryBufferImpl::DestructionCallback());

    output_mailboxes_[buffer_index] = new OutputMailbox(
        shared_image_interface_, gpu_context_support_, std::move(gmb));
  } else {
    shared_image_interface_->UpdateSharedImage(
        gpu::SyncToken(), output_mailboxes_[buffer_index]->mailbox());
  }

  auto display_rect = gfx::Rect(output_format_.primary_display_width_pixels,
                                output_format_.primary_display_height_pixels);

  float pixel_aspect_ratio;
  if (output_format_.has_pixel_aspect_ratio) {
    pixel_aspect_ratio =
        static_cast<float>(output_format_.pixel_aspect_ratio_width) /
        static_cast<float>(output_format_.pixel_aspect_ratio_height);
  } else {
    pixel_aspect_ratio = container_pixel_aspect_ratio_;
  }

  base::TimeDelta timestamp;
  if (output_packet.has_timestamp_ish()) {
    timestamp = base::TimeDelta::FromNanoseconds(output_packet.timestamp_ish());
  }

  num_used_output_buffers_++;

  auto frame = output_mailboxes_[buffer_index]->CreateFrame(
      pixel_format, coded_size, display_rect,
      GetNaturalSize(display_rect, pixel_aspect_ratio), timestamp,
      base::BindOnce(&FuchsiaVideoDecoder::OnReuseMailbox,
                     base::Unretained(this), buffer_index,
                     output_packet.header().packet_index()));

  // Mark the frame as power-efficient when software decoders are disabled. The
  // codec may still decode on hardware even when |enable_sw_decoding_| is set
  // (i.e. POWER_EFFICIENT flag would not be set correctly in that case). It
  // doesn't matter because software decoders can be enabled only for tests.
  frame->metadata()->SetBoolean(VideoFrameMetadata::POWER_EFFICIENT,
                                !enable_sw_decoding_);

  output_cb_.Run(std::move(frame));
}

void FuchsiaVideoDecoder::OnOutputEndOfStream(uint64_t stream_lifetime_ordinal,
                                              bool error_detected_before) {
  if (stream_lifetime_ordinal != stream_lifetime_ordinal_) {
    return;
  }

  stream_lifetime_ordinal_ += 2;
  active_stream_ = false;

  std::move(pending_flush_cb_).Run(DecodeStatus::OK);
}

void FuchsiaVideoDecoder::OnError() {
  codec_.Unbind();

  auto weak_this = weak_this_;

  // Call all decode callback with DECODE_ERROR.
  for (auto& cb : decode_callbacks_) {
    std::move(cb).Run(DecodeStatus::DECODE_ERROR);

    // DecodeCB(DECODE_ERROR) may destroy |this|.
    if (!weak_this)
      return;
  }
  decode_callbacks_.clear();

  input_writer_queue_.ResetBuffers();
  input_writer_queue_.ResetQueue();

  input_buffer_collection_.reset();
  input_buffer_collection_creator_.reset();

  ReleaseOutputBuffers();
}

void FuchsiaVideoDecoder::InitializeOutputBufferCollection(
    fuchsia::media::StreamBufferConstraints constraints,
    fuchsia::sysmem::BufferCollectionTokenPtr collection_token_for_codec,
    fuchsia::sysmem::BufferCollectionTokenPtr collection_token_for_gpu) {
  fuchsia::sysmem::BufferCollectionConstraints buffer_constraints;
  buffer_constraints.usage.none = fuchsia::sysmem::noneUsage;
  buffer_constraints.min_buffer_count_for_camping = max_used_output_buffers_;
  output_buffer_collection_->SetConstraints(
      /*has_constraints=*/true, std::move(buffer_constraints));

  // Register the new collection with the GPU process.
  DCHECK(!output_buffer_collection_id_);
  output_buffer_collection_id_ = gfx::SysmemBufferCollectionId::Create();
  shared_image_interface_->RegisterSysmemBufferCollection(
      output_buffer_collection_id_,
      collection_token_for_gpu.Unbind().TakeChannel());

  // Pass new output buffer settings to the codec.
  fuchsia::media::StreamBufferPartialSettings settings;
  settings.set_buffer_lifetime_ordinal(output_buffer_lifetime_ordinal_);
  settings.set_buffer_constraints_version_ordinal(
      constraints.buffer_constraints_version_ordinal());
  settings.set_packet_count_for_client(max_used_output_buffers_);
  settings.set_packet_count_for_server(
      constraints.packet_count_for_server_recommended());
  settings.set_sysmem_token(std::move(collection_token_for_codec));
  codec_->SetOutputBufferPartialSettings(std::move(settings));
  codec_->CompleteOutputBufferPartialSettings(output_buffer_lifetime_ordinal_);

  DCHECK(output_mailboxes_.empty());
  output_mailboxes_.resize(
      max_used_output_buffers_ +
          constraints.packet_count_for_server_recommended(),
      nullptr);
}

void FuchsiaVideoDecoder::ReleaseOutputBuffers() {
  // Release the buffer collection.
  num_used_output_buffers_ = 0;
  if (output_buffer_collection_) {
    output_buffer_collection_->Close();
    output_buffer_collection_.Unbind();
  }

  // Release all output mailboxes.
  for (OutputMailbox* mailbox : output_mailboxes_) {
    if (mailbox)
      mailbox->Release();
  }
  output_mailboxes_.clear();

  // Tell the GPU process to drop the buffer collection.
  if (output_buffer_collection_id_) {
    shared_image_interface_->ReleaseSysmemBufferCollection(
        output_buffer_collection_id_);
    output_buffer_collection_id_ = {};
  }
}

void FuchsiaVideoDecoder::OnReuseMailbox(uint32_t buffer_index,
                                         uint32_t packet_index) {
  DCHECK(codec_);

  DCHECK_GT(num_used_output_buffers_, 0);
  num_used_output_buffers_--;

  fuchsia::media::PacketHeader header;
  header.set_buffer_lifetime_ordinal(output_buffer_lifetime_ordinal_);
  header.set_packet_index(packet_index);
  codec_->RecycleOutputPacket(std::move(header));
}

std::unique_ptr<VideoDecoder> CreateFuchsiaVideoDecoder(
    gpu::SharedImageInterface* shared_image_interface,
    gpu::ContextSupport* gpu_context_support) {
  return std::make_unique<FuchsiaVideoDecoder>(shared_image_interface,
                                               gpu_context_support,
                                               /*enable_sw_decoding=*/false);
}

std::unique_ptr<VideoDecoder> CreateFuchsiaVideoDecoderForTests(
    gpu::SharedImageInterface* shared_image_interface,
    gpu::ContextSupport* gpu_context_support,
    bool enable_sw_decoding) {
  return std::make_unique<FuchsiaVideoDecoder>(
      shared_image_interface, gpu_context_support, enable_sw_decoding);
}

}  // namespace media
