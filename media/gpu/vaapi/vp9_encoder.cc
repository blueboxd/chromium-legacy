// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vp9_encoder.h"

#include <algorithm>

#include "base/bits.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/vp9_rate_control.h"
#include "third_party/libvpx/source/libvpx/vp9/ratectrl_rtc.h"

namespace media {

namespace {
// Keyframe period.
constexpr size_t kKFPeriod = 3000;

// Arbitrarily chosen bitrate window size for rate control, in ms.
constexpr int kCPBWindowSizeMs = 500;

// Quantization parameter. They are vp9 ac/dc indices and their ranges are
// 0-255. Based on WebRTC's defaults.
constexpr int kMinQP = 4;
// TODO(crbug.com/1060775): Relax this max quantization parameter upper bound
// so that our encoder and bitrate controller can select a higher value in the
// case a requested bitrate is small.
constexpr int kMaxQP = 112;
// This stands for 31 as a real ac value (see rfc 8.6.1 table
// ac_qlookup[3][256]). Note: This needs to be revisited once we have 10&12 bit
// encoder support.
constexpr int kDefaultQP = 24;

// filter level may affect on quality at lower bitrates; for now,
// we set a constant value (== 10) which is what other VA-API
// implementations like libyami and gstreamer-vaapi are using.
constexpr uint8_t kDefaultLfLevel = 10;

// Convert Qindex, whose range is 0-255, to the quantizer parameter used in
// libvpx vp9 rate control, whose range is 0-63.
// Cited from //third_party/libvpx/source/libvpx/vp9/encoder/vp9_quantize.cc.
int QindexToQuantizer(int q_index) {
  constexpr int kQuantizerToQindex[] = {
      0,   4,   8,   12,  16,  20,  24,  28,  32,  36,  40,  44,  48,
      52,  56,  60,  64,  68,  72,  76,  80,  84,  88,  92,  96,  100,
      104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 148, 152,
      156, 160, 164, 168, 172, 176, 180, 184, 188, 192, 196, 200, 204,
      208, 212, 216, 220, 224, 228, 232, 236, 240, 244, 249, 255,
  };

  for (size_t q = 0; q < base::size(kQuantizerToQindex); ++q) {
    if (kQuantizerToQindex[q] >= q_index)
      return q;
  }
  return base::size(kQuantizerToQindex) - 1;
}

// The return value is expressed as a percentage of the average. For example,
// to allocate no more than 4.5 frames worth of bitrate to a keyframe, the
// return value is 450.
uint32_t MaxSizeOfKeyframeAsPercentage(uint32_t optimal_buffer_size,
                                       uint32_t max_framerate) {
  // Set max to the optimal buffer level (normalized by target BR),
  // and scaled by a scale_par.
  // Max target size = scale_par * optimal_buffer_size * targetBR[Kbps].
  // This value is presented in percentage of perFrameBw:
  // perFrameBw = targetBR[Kbps] * 1000 / framerate.
  // The target in % is as follows:
  const double target_size_byte_per_frame = optimal_buffer_size * 0.5;
  const uint32_t target_size_kbyte =
      target_size_byte_per_frame * max_framerate / 1000;
  const uint32_t target_size_kbyte_as_percent = target_size_kbyte * 100;

  // Don't go below 3 times the per frame bandwidth.
  constexpr uint32_t kMinIntraSizePercentage = 300u;
  return std::max(kMinIntraSizePercentage, target_size_kbyte_as_percent);
}

libvpx::VP9RateControlRtcConfig CreateRCConfig(
    const gfx::Size& encode_size,
    const VP9Encoder::EncodeParams& encode_params) {
  libvpx::VP9RateControlRtcConfig rc_cfg{};
  rc_cfg.width = encode_size.width();
  rc_cfg.height = encode_size.height();
  rc_cfg.max_quantizer =
      QindexToQuantizer(encode_params.scaling_settings.max_qp);
  rc_cfg.min_quantizer =
      QindexToQuantizer(encode_params.scaling_settings.min_qp);
  // libvpx::VP9RateControlRtcConfig is kbps.
  rc_cfg.target_bandwidth =
      encode_params.bitrate_allocation.GetSumBps() / 1000.0;
  // These default values come from
  // //third_party/webrtc/modules/video_coding/codecs/vp9/vp9_impl.cc.
  rc_cfg.buf_initial_sz = 500;
  rc_cfg.buf_optimal_sz = 600;
  rc_cfg.buf_sz = 1000;
  rc_cfg.undershoot_pct = 50;
  rc_cfg.overshoot_pct = 50;
  rc_cfg.max_intra_bitrate_pct = MaxSizeOfKeyframeAsPercentage(
      rc_cfg.buf_optimal_sz, encode_params.framerate);
  rc_cfg.framerate = encode_params.framerate;

  // Spatial layer variables.
  rc_cfg.ss_number_layers = 1;
  rc_cfg.max_quantizers[0] = rc_cfg.max_quantizer;
  rc_cfg.min_quantizers[0] = rc_cfg.min_quantizer;
  // TODO(crbug.com/1030199): Fill multiple temporal layers variables.
  // Temporal layer variables.
  rc_cfg.ts_number_layers = 1;
  rc_cfg.scaling_factor_num[0] = 1;
  rc_cfg.scaling_factor_den[0] = 1;
  rc_cfg.layer_target_bitrate[0] = rc_cfg.target_bandwidth;
  rc_cfg.ts_rate_decimator[0] = 1;
  return rc_cfg;
}
}  // namespace

VP9Encoder::EncodeParams::EncodeParams()
    : kf_period_frames(kKFPeriod),
      framerate(0),
      cpb_window_size_ms(kCPBWindowSizeMs),
      cpb_size_bits(0),
      initial_qp(kDefaultQP),
      scaling_settings(kMinQP, kMaxQP),
      error_resilient_mode(false) {}

void VP9Encoder::set_rate_ctrl_for_testing(
    std::unique_ptr<VP9RateControl> rate_ctrl) {
  rate_ctrl_ = std::move(rate_ctrl);
}

void VP9Encoder::Reset() {
  current_params_ = EncodeParams();
  reference_frames_.Clear();
  frame_num_ = 0;
  InitializeFrameHeader();
}

VP9Encoder::VP9Encoder(std::unique_ptr<Accelerator> accelerator)
    : accelerator_(std::move(accelerator)) {}

VP9Encoder::~VP9Encoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool VP9Encoder::Initialize(const VideoEncodeAccelerator::Config& config,
                            const AcceleratedVideoEncoder::Config& ave_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (VideoCodecProfileToVideoCodec(config.output_profile) != kCodecVP9) {
    DVLOGF(1) << "Invalid profile: " << GetProfileName(config.output_profile);
    return false;
  }

  if (config.input_visible_size.IsEmpty()) {
    DVLOGF(1) << "Input visible size could not be empty";
    return false;
  }

  accelerator_->set_bitrate_control(ave_config.bitrate_control);
  visible_size_ = config.input_visible_size;
  coded_size_ = gfx::Size(base::bits::Align(visible_size_.width(), 16),
                          base::bits::Align(visible_size_.height(), 16));
  Reset();

  if (ave_config.bitrate_control ==
      BitrateControl::kConstantQuantizationParameter) {
    // |rate_ctrl_| might be injected for tests.
    if (!rate_ctrl_) {
      rate_ctrl_ = VP9RateControl::Create(
          CreateRCConfig(visible_size_, current_params_));
    }
    if (!rate_ctrl_)
      return false;
  } else {
    DCHECK(!rate_ctrl_) << "|rate_ctrl_| should only be configured when in "
                           "kConstantQuantizationParameter";
  }

  VideoBitrateAllocation initial_bitrate_allocation;
  initial_bitrate_allocation.SetBitrate(0, 0, config.initial_bitrate);
  return UpdateRates(initial_bitrate_allocation,
                     config.initial_framerate.value_or(
                         VideoEncodeAccelerator::kDefaultFramerate));
}

gfx::Size VP9Encoder::GetCodedSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!coded_size_.IsEmpty());

  return coded_size_;
}

size_t VP9Encoder::GetMaxNumOfRefFrames() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return kVp9NumRefFrames;
}

ScalingSettings VP9Encoder::GetScalingSettings() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return current_params_.scaling_settings;
}

bool VP9Encoder::PrepareEncodeJob(EncodeJob* encode_job) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (encode_job->IsKeyframeRequested())
    frame_num_ = 0;

  if (frame_num_ == 0)
    encode_job->ProduceKeyframe();

  frame_num_++;
  frame_num_ %= current_params_.kf_period_frames;

  scoped_refptr<VP9Picture> picture = accelerator_->GetPicture(encode_job);
  DCHECK(picture);

  const bool keyframe = encode_job->IsKeyframeRequested();
  UpdateFrameHeader(keyframe);

  *picture->frame_hdr = current_frame_hdr_;

  // Use last, golden and altref for references.
  const std::array<bool, kVp9NumRefsPerFrame> ref_frames_used = {
      !keyframe, !keyframe, !keyframe};
  if (!accelerator_->SubmitFrameParameters(encode_job, current_params_, picture,
                                           reference_frames_,
                                           ref_frames_used)) {
    LOG(ERROR) << "Failed submitting frame parameters";
    return false;
  }

  UpdateReferenceFrames(picture);
  return true;
}

void VP9Encoder::BitrateControlUpdate(uint64_t encoded_chunk_size_bytes) {
  if (accelerator_->bitrate_control() !=
          BitrateControl::kConstantQuantizationParameter ||
      !rate_ctrl_) {
    DLOG(ERROR) << __func__ << "() is called when no bitrate controller exists";
    return;
  }

  DVLOGF(4) << "|encoded_chunk_size_bytes|=" << encoded_chunk_size_bytes;
  rate_ctrl_->PostEncodeUpdate(encoded_chunk_size_bytes);
}

bool VP9Encoder::UpdateRates(const VideoBitrateAllocation& bitrate_allocation,
                             uint32_t framerate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (bitrate_allocation.GetSumBps() == 0 || framerate == 0)
    return false;

  if (current_params_.bitrate_allocation == bitrate_allocation &&
      current_params_.framerate == framerate) {
    return true;
  }
  VLOGF(2) << "New bitrate: " << bitrate_allocation.GetSumBps()
           << ", New framerate: " << framerate;

  current_params_.bitrate_allocation = bitrate_allocation;
  current_params_.framerate = framerate;

  current_params_.cpb_size_bits =
      current_params_.bitrate_allocation.GetSumBps() *
      current_params_.cpb_window_size_ms / 1000;

  if (!rate_ctrl_)
    return true;

  rate_ctrl_->UpdateRateControl(CreateRCConfig(visible_size_, current_params_));
  return true;
}

void VP9Encoder::InitializeFrameHeader() {
  current_frame_hdr_ = {};
  DCHECK(!visible_size_.IsEmpty());
  current_frame_hdr_.frame_width = visible_size_.width();
  current_frame_hdr_.frame_height = visible_size_.height();
  current_frame_hdr_.render_width = visible_size_.width();
  current_frame_hdr_.render_height = visible_size_.height();
  current_frame_hdr_.quant_params.base_q_idx = kDefaultQP;
  current_frame_hdr_.loop_filter.level = kDefaultLfLevel;
  current_frame_hdr_.show_frame = true;
}

void VP9Encoder::UpdateFrameHeader(bool keyframe) {
  if (keyframe) {
    current_frame_hdr_.frame_type = Vp9FrameHeader::KEYFRAME;
    current_frame_hdr_.refresh_frame_flags = 0xff;
    ref_frame_index_ = 0;
  } else {
    current_frame_hdr_.frame_type = Vp9FrameHeader::INTERFRAME;
    current_frame_hdr_.ref_frame_idx[0] = ref_frame_index_;
    current_frame_hdr_.ref_frame_idx[1] =
        (ref_frame_index_ - 1) & (kVp9NumRefFrames - 1);
    current_frame_hdr_.ref_frame_idx[2] =
        (ref_frame_index_ - 2) & (kVp9NumRefFrames - 1);
    ref_frame_index_ = (ref_frame_index_ + 1) % kVp9NumRefFrames;
    current_frame_hdr_.refresh_frame_flags = 1 << ref_frame_index_;
  }

  if (!rate_ctrl_)
    return;

  libvpx::VP9FrameParamsQpRTC frame_params{};
  frame_params.frame_type =
      keyframe ? FRAME_TYPE::KEY_FRAME : FRAME_TYPE::INTER_FRAME;
  rate_ctrl_->ComputeQP(frame_params);
  // TODO(crbug.com/1030199): Fill temporal layer id.
  current_frame_hdr_.quant_params.base_q_idx = rate_ctrl_->GetQP();
  current_frame_hdr_.loop_filter.level = rate_ctrl_->GetLoopfilterLevel();
  DVLOGF(4) << "|qp|=" << rate_ctrl_->GetQP()
            << ", |filter_level|=" << rate_ctrl_->GetLoopfilterLevel();
}

void VP9Encoder::UpdateReferenceFrames(scoped_refptr<VP9Picture> picture) {
  reference_frames_.Refresh(picture);
}

}  // namespace media
