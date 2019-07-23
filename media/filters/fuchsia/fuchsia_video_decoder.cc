// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/fuchsia/fuchsia_video_decoder.h"

#include <fuchsia/media/cpp/fidl.h>
#include <fuchsia/mediacodec/cpp/fidl.h>
#include <zircon/rights.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/service_directory_client.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/cdm_context.h"
#include "media/base/decryptor.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "third_party/libyuv/include/libyuv/video_common.h"

namespace media {

namespace {

const zx_rights_t kReadOnlyVmoRights =
    ZX_DEFAULT_VMO_RIGHTS &
    ~(ZX_RIGHT_WRITE | ZX_RIGHT_EXECUTE | ZX_RIGHT_SET_PROPERTY);

// Value passed to the codec as packet_count_for_client. It's number of output
// buffers that we expect to hold on to in the renderer.
//
// TODO(sergeyu): Figure out the right number of buffers to request. Currently
// the codec doesn't allow to reserve more than 2 client buffers, but it still
// works properly when the client holds to more than that.
const uint32_t kMaxUsedOutputFrames = 8;

zx::vmo CreateContiguousVmo(size_t size, const zx::handle& bti_handle) {
  zx::vmo vmo;
  zx_status_t status =
      zx_vmo_create_contiguous(bti_handle.get(), size, /*alignment_log2=*/0,
                               vmo.reset_and_get_address());
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "zx_vmo_create_contiguous";
    return zx::vmo();
  }

  return vmo;
}

zx::vmo CreateVmo(size_t size) {
  zx::vmo vmo;
  zx_status_t status = zx::vmo::create(size, 0, &vmo);
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "zx_vmo_create";
    return zx::vmo();
  }

  return vmo;
}

class PendingDecode {
 public:
  PendingDecode(scoped_refptr<DecoderBuffer> buffer,
                VideoDecoder::DecodeCB decode_cb)
      : buffer_(buffer), decode_cb_(std::move(decode_cb)) {
    DCHECK(buffer_);
  }
  ~PendingDecode() {
    if (decode_cb_) {
      std::move(decode_cb_).Run(DecodeStatus::ABORTED);
    }
  }

  PendingDecode(PendingDecode&& other) = default;
  PendingDecode& operator=(PendingDecode&& other) = default;

  const DecoderBuffer& buffer() { return *buffer_; }

  const uint8_t* data() const { return buffer_->data() + buffer_pos_; }
  size_t bytes_left() const { return buffer_->data_size() - buffer_pos_; }
  void AdvanceCurrentPos(size_t bytes) {
    DCHECK_LE(bytes, bytes_left());
    buffer_pos_ += bytes;
  }
  VideoDecoder::DecodeCB TakeDecodeCallback() { return std::move(decode_cb_); }

 private:
  scoped_refptr<DecoderBuffer> buffer_;
  size_t buffer_pos_ = 0;
  VideoDecoder::DecodeCB decode_cb_;

  DISALLOW_COPY_AND_ASSIGN(PendingDecode);
};

class CodecBuffer {
 public:
  CodecBuffer() = default;

  bool Initialize(const fuchsia::media::StreamBufferConstraints& constraints) {
    if (!constraints.has_per_packet_buffer_bytes_recommended()) {
      return false;
    }

    size_ = constraints.per_packet_buffer_bytes_recommended();

    if (constraints.has_is_physically_contiguous_required() &&
        constraints.is_physically_contiguous_required()) {
      if (!constraints.has_very_temp_kludge_bti_handle()) {
        return false;
      }
      vmo_ =
          CreateContiguousVmo(size_, constraints.very_temp_kludge_bti_handle());
    } else {
      vmo_ = CreateVmo(size_);
    }
    return vmo_.is_valid();
  }

  const zx::vmo& vmo() const { return vmo_; }
  size_t size() const { return size_; }

  bool ToFidlCodecBuffer(uint64_t buffer_lifetime_ordinal,
                         uint32_t buffer_index,
                         bool read_only,
                         fuchsia::media::StreamBuffer* buffer) {
    zx::vmo vmo_dup;
    zx_status_t status = vmo_.duplicate(
        read_only ? kReadOnlyVmoRights : ZX_RIGHT_SAME_RIGHTS, &vmo_dup);
    if (status != ZX_OK) {
      ZX_DLOG(ERROR, status) << "zx_handle_duplicate";
      return false;
    }

    fuchsia::media::StreamBufferDataVmo buf_data;
    buf_data.set_vmo_handle(std::move(vmo_dup));

    buf_data.set_vmo_usable_start(0);
    buf_data.set_vmo_usable_size(size_);

    buffer->mutable_data()->set_vmo(std::move(buf_data));
    buffer->set_buffer_lifetime_ordinal(buffer_lifetime_ordinal);
    buffer->set_buffer_index(buffer_index);

    return true;
  }

 private:
  zx::vmo vmo_;
  size_t size_ = 0;

  DISALLOW_COPY_AND_ASSIGN(CodecBuffer);
};

class InputBuffer {
 public:
  InputBuffer() = default;

  ~InputBuffer() { CallDecodeCallbackIfAny(DecodeStatus::ABORTED); }

  bool Initialize(const fuchsia::media::StreamBufferConstraints& constraints) {
    return buffer_.Initialize(constraints);
  }

  CodecBuffer& buffer() { return buffer_; }
  bool is_used() const { return is_used_; }

  // Copies as much data as possible from |pending_decode| to this input buffer.
  size_t FillFromDecodeBuffer(PendingDecode* pending_decode) {
    DCHECK(!is_used_);
    is_used_ = true;

    size_t bytes_to_fill =
        std::min(buffer_.size(), pending_decode->bytes_left());

    zx_status_t status =
        buffer_.vmo().write(pending_decode->data(), 0, bytes_to_fill);
    ZX_CHECK(status == ZX_OK, status) << "zx_vmo_write";

    pending_decode->AdvanceCurrentPos(bytes_to_fill);

    if (pending_decode->bytes_left() == 0) {
      DCHECK(!decode_cb_);
      decode_cb_ = pending_decode->TakeDecodeCallback();
    }

    return bytes_to_fill;
  }

  void CallDecodeCallbackIfAny(DecodeStatus status) {
    if (decode_cb_) {
      std::move(decode_cb_).Run(status);
    }
  }

  void OnDoneDecoding(DecodeStatus status) {
    DCHECK(is_used_);
    is_used_ = false;
    CallDecodeCallbackIfAny(status);
  }

 private:
  CodecBuffer buffer_;

  // Set to true when this buffer is being used by the codec.
  bool is_used_ = false;

  // Decode callback for the DecodeBuffer of which this InputBuffer is a part.
  // This is only set on the final InputBuffer in each DecodeBuffer.
  VideoDecoder::DecodeCB decode_cb_;

  DISALLOW_COPY_AND_ASSIGN(InputBuffer);
};

// Output buffer used to pass decoded frames from the decoder. Ref-counted
// to make it possible to share the buffers with VideoFrames, in case when a
// frame outlives the decoder.UnsafeSharedMemoryRegion
class OutputBuffer : public base::RefCountedThreadSafe<OutputBuffer> {
 public:
  OutputBuffer() = default;

  bool Initialize(const fuchsia::media::StreamBufferConstraints& constraints) {
    if (!buffer_.Initialize(constraints)) {
      return false;
    }

    zx_status_t status = zx::vmar::root_self()->map(
        /*vmar_offset=*/0, buffer_.vmo(), 0, buffer_.size(),
        ZX_VM_REQUIRE_NON_RESIZABLE | ZX_VM_PERM_READ, &mapped_memory_);

    if (status != ZX_OK) {
      ZX_DLOG(ERROR, status) << "zx_vmar_map";
      mapped_memory_ = 0;
      return false;
    }

    return true;
  }

  CodecBuffer& buffer() { return buffer_; }

  const uint8_t* mapped_memory() {
    DCHECK(mapped_memory_);
    return reinterpret_cast<uint8_t*>(mapped_memory_);
  }

 private:
  friend class RefCountedThreadSafe<OutputBuffer>;

  ~OutputBuffer() {
    if (mapped_memory_) {
      zx_status_t status =
          zx::vmar::root_self()->unmap(mapped_memory_, buffer_.size());
      if (status != ZX_OK) {
        ZX_LOG(FATAL, status) << "zx_vmar_unmap";
      }
    }
  }

  CodecBuffer buffer_;

  uintptr_t mapped_memory_ = 0;

  DISALLOW_COPY_AND_ASSIGN(OutputBuffer);
};

}  // namespace

class FuchsiaVideoDecoder : public VideoDecoder {
 public:
  explicit FuchsiaVideoDecoder(bool enable_sw_decoding);
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
  void OnStreamFailed(uint64_t stream_lifetime_ordinal);
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

  void OnError();

  // Called by OnInputConstraints() to initialize input buffers.
  bool InitializeInputBuffers(
      fuchsia::media::StreamBufferConstraints constraints);

  // Pumps |pending_decodes_| to the decoder.
  void PumpInput();

  // Called by OnInputConstraints() to initialize input buffers.
  bool InitializeOutputBuffers(
      fuchsia::media::StreamBufferConstraints constraints);

  // Destruction callback for the output VideoFrame instances.
  void OnFrameDestroyed(scoped_refptr<OutputBuffer> buffer,
                        uint64_t buffer_lifetime_ordinal,
                        uint32_t packet_index);

  const bool enable_sw_decoding_;

  OutputCB output_cb_;

  // Aspect ratio specified in container, or 1.0 if it's not specified. This
  // value is used only if the aspect ratio is not specified in the bitstream.
  float container_pixel_aspect_ratio_ = 1.0;

  fuchsia::media::StreamProcessorPtr codec_;

  uint64_t stream_lifetime_ordinal_ = 1;

  // Set to true if we've sent an input packet with the current
  // stream_lifetime_ordinal_.
  bool active_stream_ = false;

  std::list<PendingDecode> pending_decodes_;
  uint64_t input_buffer_lifetime_ordinal_ = 1;
  std::vector<InputBuffer> input_buffers_;
  int num_used_input_buffers_ = 0;

  fuchsia::media::VideoUncompressedFormat output_format_;
  uint64_t output_buffer_lifetime_ordinal_ = 1;
  std::vector<scoped_refptr<OutputBuffer>> output_buffers_;
  int num_used_output_buffers_ = 0;
  int max_used_output_buffers_ = 0;

  // Non-null when flush is pending.
  VideoDecoder::DecodeCB pending_flush_cb_;

  base::WeakPtr<FuchsiaVideoDecoder> weak_this_;
  base::WeakPtrFactory<FuchsiaVideoDecoder> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FuchsiaVideoDecoder);
};

FuchsiaVideoDecoder::FuchsiaVideoDecoder(bool enable_sw_decoding)
    : enable_sw_decoding_(enable_sw_decoding), weak_factory_(this) {
  weak_this_ = weak_factory_.GetWeakPtr();
}

FuchsiaVideoDecoder::~FuchsiaVideoDecoder() = default;

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

  auto codec_factory =
      base::fuchsia::ServiceDirectoryClient::ForCurrentProcess()
          ->ConnectToService<fuchsia::mediacodec::CodecFactory>();
  codec_factory->CreateDecoder(std::move(codec_params), codec_.NewRequest());

  codec_.set_error_handler(
      [this](zx_status_t status) {
        ZX_LOG(ERROR, status)
            << "The fuchsia.mediacodec.Codec channel was terminated.";
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
  DCHECK_LT(static_cast<int>(pending_decodes_.size()) + num_used_input_buffers_,
            GetMaxDecodeRequests());

  if (!codec_) {
    // Post the callback to the current sequence as DecoderStream doesn't expect
    // Decode() to complete synchronously.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(decode_cb), DecodeStatus::DECODE_ERROR));
    return;
  }

  pending_decodes_.push_back(PendingDecode(buffer, std::move(decode_cb)));
  PumpInput();
}

void FuchsiaVideoDecoder::Reset(base::OnceClosure closure) {
  // Call DecodeCB(ABORTED) for all active decode requests.
  for (auto& buffer : input_buffers_) {
    buffer.CallDecodeCallbackIfAny(DecodeStatus::ABORTED);
  }

  // Will call DecodeCB(ABORTED) for all pending decode requests.
  pending_decodes_.clear();

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
  return input_buffers_.size() + 1;
}

void FuchsiaVideoDecoder::OnStreamFailed(uint64_t stream_lifetime_ordinal) {
  if (stream_lifetime_ordinal_ != stream_lifetime_ordinal) {
    return;
  }

  OnError();
}

void FuchsiaVideoDecoder::OnInputConstraints(
    fuchsia::media::StreamBufferConstraints input_constraints) {
  if (!InitializeInputBuffers(std::move(input_constraints))) {
    DLOG(ERROR) << "Failed to initialize input buffers.";
    OnError();
    return;
  }

  PumpInput();
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

  if (free_input_packet.packet_index() >= input_buffers_.size()) {
    DLOG(ERROR) << "fuchsia.mediacodec sent OnFreeInputPacket() for an unknown "
                   "packet: buffer_lifetime_ordinal="
                << free_input_packet.buffer_lifetime_ordinal()
                << " packet_index=" << free_input_packet.packet_index();
    OnError();
    return;
  }

  DCHECK_GT(num_used_input_buffers_, 0);
  num_used_input_buffers_--;
  input_buffers_[free_input_packet.packet_index()].OnDoneDecoding(
      DecodeStatus::OK);

  // Try to pump input in case it was blocked.
  PumpInput();
}

void FuchsiaVideoDecoder::OnOutputConstraints(
    fuchsia::media::StreamOutputConstraints output_constraints) {
  if (!output_constraints.has_stream_lifetime_ordinal()) {
    DLOG(ERROR) << "Received OnOutputConstraints() with missing required "
                   "fields.";
    OnError();
    return;
  }

  if (output_constraints.stream_lifetime_ordinal() !=
      stream_lifetime_ordinal_) {
    return;
  }

  if (output_constraints.has_buffer_constraints_action_required() &&
      output_constraints.buffer_constraints_action_required()) {
    if (!output_constraints.has_buffer_constraints()) {
      DLOG(ERROR) << "Received OnOutputConstraints() which requires buffer "
                     "constraints action, but without buffer constraints.";
      OnError();
      return;
    }
    if (!InitializeOutputBuffers(
            std::move(*output_constraints.mutable_buffer_constraints()))) {
      DLOG(ERROR) << "Failed to initialize output buffers.";
      OnError();
      return;
    }
  }
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

  auto coded_size = gfx::Size(output_format_.primary_width_pixels,
                              output_format_.primary_height_pixels);

  base::Optional<VideoFrameLayout> layout;
  switch (output_format_.fourcc) {
    case libyuv::FOURCC_NV12:
      layout = VideoFrameLayout::CreateWithPlanes(
          PIXEL_FORMAT_NV12, coded_size,
          std::vector<VideoFrameLayout::Plane>{
              VideoFrameLayout::Plane(output_format_.primary_line_stride_bytes,
                                      output_format_.primary_start_offset,
                                      output_format_.primary_line_stride_bytes *
                                          output_format_.primary_height_pixels),
              VideoFrameLayout::Plane(
                  output_format_.secondary_line_stride_bytes,
                  output_format_.secondary_start_offset,
                  output_format_.secondary_line_stride_bytes *
                      output_format_.secondary_height_pixels)});
      DCHECK(layout);
      break;

    case libyuv::FOURCC_YV12:
      layout = VideoFrameLayout::CreateWithPlanes(
          PIXEL_FORMAT_YV12, coded_size,
          std::vector<VideoFrameLayout::Plane>{
              VideoFrameLayout::Plane(output_format_.primary_line_stride_bytes,
                                      output_format_.primary_start_offset,
                                      output_format_.primary_line_stride_bytes *
                                          output_format_.primary_height_pixels),
              VideoFrameLayout::Plane(
                  output_format_.secondary_line_stride_bytes,
                  output_format_.secondary_start_offset,
                  output_format_.secondary_line_stride_bytes *
                      output_format_.secondary_height_pixels),
              VideoFrameLayout::Plane(
                  output_format_.secondary_line_stride_bytes,
                  output_format_.tertiary_start_offset,
                  output_format_.secondary_line_stride_bytes *
                      output_format_.secondary_height_pixels)});
      DCHECK(layout);
      break;

    default:
      LOG(ERROR) << "unknown fourcc: "
                 << std::string(reinterpret_cast<char*>(&output_format_.fourcc),
                                4);
  }

  if (!layout) {
    codec_->RecycleOutputPacket(fidl::Clone(output_packet.header()));
    return;
  }

  base::TimeDelta timestamp;
  if (output_packet.has_timestamp_ish()) {
    timestamp = base::TimeDelta::FromNanoseconds(output_packet.timestamp_ish());
  }

  auto packet_index = output_packet.header().packet_index();
  auto buffer_index = output_packet.buffer_index();
  auto& buffer = output_buffers_[buffer_index];

  // We're not using single buffer mode, so packet count will be equal to buffer
  // count.
  DCHECK_LT(num_used_output_buffers_, static_cast<int>(output_buffers_.size()));
  num_used_output_buffers_++;

  float pixel_aspect_ratio;
  if (output_format_.has_pixel_aspect_ratio) {
    pixel_aspect_ratio =
        static_cast<float>(output_format_.pixel_aspect_ratio_width) /
        static_cast<float>(output_format_.pixel_aspect_ratio_height);
  } else {
    pixel_aspect_ratio = container_pixel_aspect_ratio_;
  }

  auto display_rect = gfx::Rect(output_format_.primary_display_width_pixels,
                                output_format_.primary_display_height_pixels);

  // TODO(sergeyu): Create ReadOnlySharedMemoryRegion for the VMO and pass
  // it to the frame.
  auto frame = VideoFrame::WrapExternalDataWithLayout(
      *layout, display_rect, GetNaturalSize(display_rect, pixel_aspect_ratio),
      const_cast<uint8_t*>(buffer->mapped_memory()) +
          output_format_.primary_start_offset,
      buffer->buffer().size() - output_format_.primary_start_offset, timestamp);

  // Pass a reference to the buffer to the destruction callback to ensure it's
  // not destroyed while the frame is being used.
  frame->AddDestructionObserver(BindToCurrentLoop(
      base::BindOnce(&FuchsiaVideoDecoder::OnFrameDestroyed, weak_this_, buffer,
                     output_buffer_lifetime_ordinal_, packet_index)));

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

  // Call all decode callback with DECODE_ERROR before clearing input_buffers_
  // and pending_decodes_. Otherwise PendingDecode and InputBuffer destructors
  // would the callbacks with ABORTED.
  for (auto& buffer : input_buffers_) {
    if (buffer.is_used()) {
      buffer.OnDoneDecoding(DecodeStatus::DECODE_ERROR);

      // DecodeCB(DECODE_ERROR) may destroy |this|.
      if (!weak_this) {
        return;
      }
    }
  }

  for (auto& pending_decode : pending_decodes_) {
    pending_decode.TakeDecodeCallback().Run(DecodeStatus::DECODE_ERROR);
    if (!weak_this) {
      return;
    }
  }

  pending_decodes_.clear();

  num_used_input_buffers_ = 0;
  input_buffers_.clear();

  num_used_output_buffers_ = 0;
  output_buffers_.clear();
}

bool FuchsiaVideoDecoder::InitializeInputBuffers(
    fuchsia::media::StreamBufferConstraints constraints) {
  input_buffer_lifetime_ordinal_ += 2;

  if (!constraints.has_default_settings() ||
      !constraints.default_settings().has_packet_count_for_server() ||
      !constraints.default_settings().has_packet_count_for_client()) {
    DLOG(ERROR)
        << "Received InitializeInputBuffers() with missing required fields.";
    OnError();
    return false;
  }

  auto settings = fidl::Clone(constraints.default_settings());
  settings.set_buffer_lifetime_ordinal(input_buffer_lifetime_ordinal_);
  codec_->SetInputBufferSettings(fidl::Clone(settings));

  int total_buffers =
      settings.packet_count_for_server() + settings.packet_count_for_client();
  std::vector<InputBuffer> new_buffers(total_buffers);

  for (int i = 0; i < total_buffers; ++i) {
    fuchsia::media::StreamBuffer codec_buffer;

    if (!new_buffers[i].Initialize(constraints) ||
        !new_buffers[i].buffer().ToFidlCodecBuffer(
            input_buffer_lifetime_ordinal_, i, /*read_only=*/true,
            &codec_buffer)) {
      return false;
    }

    codec_->AddInputBuffer(std::move(codec_buffer));
  }

  num_used_input_buffers_ = 0;
  input_buffers_ = std::move(new_buffers);

  return true;
}

void FuchsiaVideoDecoder::PumpInput() {
  // Nothing to do if a codec error has occurred or input buffers have not been
  // initialized (which happens in response to OnInputConstraints() event).
  if (!codec_ || input_buffers_.empty())
    return;

  while (!pending_decodes_.empty()) {
    // Decode() is not supposed to be called while Decode(EOS) is pending.
    DCHECK(!pending_flush_cb_);

    if (pending_decodes_.front().buffer().end_of_stream()) {
      active_stream_ = true;
      codec_->QueueInputEndOfStream(stream_lifetime_ordinal_);
      codec_->FlushEndOfStreamAndCloseStream(stream_lifetime_ordinal_);
      pending_flush_cb_ = pending_decodes_.front().TakeDecodeCallback();
      pending_decodes_.pop_front();
      continue;
    }

    DCHECK_LE(num_used_input_buffers_, static_cast<int>(input_buffers_.size()));
    if (num_used_input_buffers_ == static_cast<int>(input_buffers_.size())) {
      // No input buffer available.
      return;
    }

    auto input_buffer =
        std::find_if(input_buffers_.begin(), input_buffers_.end(),
                     [](const InputBuffer& buf) { return !buf.is_used(); });
    CHECK(input_buffer != input_buffers_.end());

    num_used_input_buffers_++;
    size_t bytes_filled =
        input_buffer->FillFromDecodeBuffer(&pending_decodes_.front());

    fuchsia::media::Packet packet;
    packet.mutable_header()->set_buffer_lifetime_ordinal(
        input_buffer_lifetime_ordinal_);
    packet.mutable_header()->set_packet_index(input_buffer -
                                              input_buffers_.begin());
    packet.set_buffer_index(packet.header().packet_index());
    packet.set_timestamp_ish(
        pending_decodes_.front().buffer().timestamp().InNanoseconds());
    packet.set_stream_lifetime_ordinal(stream_lifetime_ordinal_);
    packet.set_start_offset(0);
    packet.set_valid_length_bytes(bytes_filled);

    active_stream_ = true;
    codec_->QueueInputPacket(std::move(packet));

    if (pending_decodes_.front().bytes_left() == 0) {
      pending_decodes_.pop_front();
    }
  }
}

bool FuchsiaVideoDecoder::InitializeOutputBuffers(
    fuchsia::media::StreamBufferConstraints constraints) {
  if (!constraints.has_default_settings() ||
      !constraints.has_packet_count_for_client_max() ||
      !constraints.default_settings().has_packet_count_for_server() ||
      !constraints.default_settings().has_packet_count_for_client()) {
    DLOG(ERROR)
        << "Received InitializeOutputBuffers() with missing required fields.";
    OnError();
    return false;
  }

  // mediacodec API expects odd buffer lifetime ordinal, which is incremented by
  // 2 for each buffer generation.
  output_buffer_lifetime_ordinal_ += 2;

  auto settings = fidl::Clone(constraints.default_settings());
  settings.set_buffer_lifetime_ordinal(output_buffer_lifetime_ordinal_);

  max_used_output_buffers_ =
      std::min(kMaxUsedOutputFrames, constraints.packet_count_for_client_max());
  settings.set_packet_count_for_client(max_used_output_buffers_);

  codec_->SetOutputBufferSettings(fidl::Clone(settings));

  int total_buffers =
      settings.packet_count_for_server() + settings.packet_count_for_client();
  std::vector<scoped_refptr<OutputBuffer>> new_buffers(total_buffers);

  for (int i = 0; i < total_buffers; ++i) {
    fuchsia::media::StreamBuffer codec_buffer;
    new_buffers[i] = new OutputBuffer();
    if (!new_buffers[i]->Initialize(constraints) ||
        !new_buffers[i]->buffer().ToFidlCodecBuffer(
            output_buffer_lifetime_ordinal_, i, /*read_only=*/false,
            &codec_buffer)) {
      return false;
    }

    codec_->AddOutputBuffer(std::move(codec_buffer));
  }

  num_used_output_buffers_ = 0;
  output_buffers_ = std::move(new_buffers);

  return true;
}

void FuchsiaVideoDecoder::OnFrameDestroyed(scoped_refptr<OutputBuffer> buffer,
                                           uint64_t buffer_lifetime_ordinal,
                                           uint32_t packet_index) {
  if (!codec_)
    return;

  if (buffer_lifetime_ordinal == output_buffer_lifetime_ordinal_) {
    DCHECK_GT(num_used_output_buffers_, 0);
    num_used_output_buffers_--;
    fuchsia::media::PacketHeader header;
    header.set_buffer_lifetime_ordinal(buffer_lifetime_ordinal);
    header.set_packet_index(packet_index);
    codec_->RecycleOutputPacket(std::move(header));
  }
}

std::unique_ptr<VideoDecoder> CreateFuchsiaVideoDecoder() {
  return std::make_unique<FuchsiaVideoDecoder>(/*enable_sw_decoding=*/false);
}

std::unique_ptr<VideoDecoder> CreateFuchsiaVideoDecoderForTests(
    bool enable_sw_decoding) {
  return std::make_unique<FuchsiaVideoDecoder>(enable_sw_decoding);
}

}  // namespace media
