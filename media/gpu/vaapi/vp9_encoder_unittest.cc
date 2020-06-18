// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vp9_encoder.h"

#include <memory>
#include <numeric>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/optional.h"
#include "media/filters/vp9_parser.h"
#include "media/gpu/vaapi/vp9_rate_control.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libvpx/source/libvpx/vp9/common/vp9_blockd.h"
#include "third_party/libvpx/source/libvpx/vp9/ratectrl_rtc.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;

namespace media {
namespace {

constexpr size_t kDefaultMaxNumRefFrames = kVp9NumRefsPerFrame;

AcceleratedVideoEncoder::Config kDefaultAcceleratedVideoEncoderConfig{
    kDefaultMaxNumRefFrames,
    AcceleratedVideoEncoder::BitrateControl::kConstantBitrate};

VideoEncodeAccelerator::Config kDefaultVideoEncodeAcceleratorConfig(
    PIXEL_FORMAT_I420,
    gfx::Size(1280, 720),
    VP9PROFILE_PROFILE0,
    14000000 /* = maximum bitrate in bits per second for level 3.1 */,
    VideoEncodeAccelerator::kDefaultFramerate,
    base::nullopt /* gop_length */,
    base::nullopt /* h264 output level*/,
    VideoEncodeAccelerator::Config::StorageType::kShmem);

const std::vector<bool> kRefFramesUsedForKeyFrame = {false, false, false};
const std::vector<bool> kRefFramesUsedForInterFrame = {true, true, true};

MATCHER_P4(MatchRtcConfigWithRates,
           size,
           bitrate_allocation,
           framerate,
           num_temporal_layers,
           "") {
  if (arg.target_bandwidth !=
      static_cast<int64_t>(bitrate_allocation.GetSumBps() / 1000.0)) {
    return false;
  }

  if (arg.framerate != static_cast<double>(framerate))
    return false;

  for (size_t i = 0; i < num_temporal_layers; i++) {
    if (arg.layer_target_bitrate[i] !=
        static_cast<int>(bitrate_allocation.GetBitrateBps(0, i) / 1000.0)) {
      return false;
    }
    if (arg.ts_rate_decimator[i] != (1 << i))
      return false;
  }

  return arg.width == size.width() && arg.height == size.height() &&
         base::checked_cast<size_t>(arg.ts_number_layers) ==
             num_temporal_layers &&
         arg.ss_number_layers == 1 && arg.scaling_factor_num[0] == 1 &&
         arg.scaling_factor_den[0] == 1;
}

MATCHER_P2(MatchFrameParam, frame_type, temporal_idx, "") {
  return arg.frame_type == frame_type &&
         (!temporal_idx || arg.temporal_layer_id == *temporal_idx);
}

class MockVP9Accelerator : public VP9Encoder::Accelerator {
 public:
  MockVP9Accelerator() = default;
  ~MockVP9Accelerator() override = default;
  MOCK_METHOD1(GetPicture,
               scoped_refptr<VP9Picture>(AcceleratedVideoEncoder::EncodeJob*));

  MOCK_METHOD5(SubmitFrameParameters,
               bool(AcceleratedVideoEncoder::EncodeJob*,
                    const VP9Encoder::EncodeParams&,
                    scoped_refptr<VP9Picture>,
                    const Vp9ReferenceFrameVector&,
                    const std::array<bool, kVp9NumRefsPerFrame>&));
};

class MockVP9RateControl : public VP9RateControl {
 public:
  MockVP9RateControl() = default;
  ~MockVP9RateControl() override = default;

  MOCK_METHOD1(UpdateRateControl, void(const libvpx::VP9RateControlRtcConfig&));
  MOCK_CONST_METHOD0(GetQP, int());
  MOCK_CONST_METHOD0(GetLoopfilterLevel, int());
  MOCK_METHOD1(ComputeQP, void(const libvpx::VP9FrameParamsQpRTC&));
  MOCK_METHOD1(PostEncodeUpdate, void(uint64_t));
};
}  // namespace

struct VP9EncoderTestParam;

class VP9EncoderTest : public ::testing::TestWithParam<VP9EncoderTestParam> {
 public:
  using BitrateControl = AcceleratedVideoEncoder::BitrateControl;

  VP9EncoderTest() = default;
  ~VP9EncoderTest() override = default;

  void SetUp() override;

 protected:
  using FrameType = Vp9FrameHeader::FrameType;

  void InitializeVP9Encoder(BitrateControl bitrate_control);
  void EncodeSequence(FrameType frame_type);
  void EncodeConstantQuantizationParameterSequence(
      FrameType frame_type,
      const std::vector<bool>& expected_ref_frames_used,
      base::Optional<uint8_t> expected_temporal_idx = base::nullopt);
  void UpdateRatesTest(BitrateControl bitrate_control,
                       size_t num_temporal_layers);

 private:
  std::unique_ptr<AcceleratedVideoEncoder::EncodeJob> CreateEncodeJob(
      bool keyframe);
  void UpdateRatesSequence(const VideoBitrateAllocation& bitrate_allocation,
                           uint32_t framerate,
                           BitrateControl bitrate_control);

  std::unique_ptr<VP9Encoder> encoder_;
  MockVP9Accelerator* mock_accelerator_ = nullptr;
  MockVP9RateControl* mock_rate_ctrl_ = nullptr;
};

void VP9EncoderTest::SetUp() {
  auto mock_accelerator = std::make_unique<MockVP9Accelerator>();
  mock_accelerator_ = mock_accelerator.get();
  auto rate_ctrl = std::make_unique<MockVP9RateControl>();
  mock_rate_ctrl_ = rate_ctrl.get();

  encoder_ = std::make_unique<VP9Encoder>(std::move(mock_accelerator));
  encoder_->set_rate_ctrl_for_testing(std::move(rate_ctrl));
}

std::unique_ptr<AcceleratedVideoEncoder::EncodeJob>
VP9EncoderTest::CreateEncodeJob(bool keyframe) {
  auto input_frame = VideoFrame::CreateFrame(
      kDefaultVideoEncodeAcceleratorConfig.input_format,
      kDefaultVideoEncodeAcceleratorConfig.input_visible_size,
      gfx::Rect(kDefaultVideoEncodeAcceleratorConfig.input_visible_size),
      kDefaultVideoEncodeAcceleratorConfig.input_visible_size,
      base::TimeDelta());
  LOG_ASSERT(input_frame) << " Failed to create VideoFrame";
  return std::make_unique<AcceleratedVideoEncoder::EncodeJob>(
      input_frame, keyframe, base::DoNothing());
}

void VP9EncoderTest::InitializeVP9Encoder(BitrateControl bitrate_control) {
  auto ave_config = kDefaultAcceleratedVideoEncoderConfig;
  ave_config.bitrate_control = bitrate_control;
  if (bitrate_control == BitrateControl::kConstantQuantizationParameter) {
    constexpr size_t kNumTemporalLayers = 1u;
    VideoBitrateAllocation initial_bitrate_allocation;
    initial_bitrate_allocation.SetBitrate(
        0, 0, kDefaultVideoEncodeAcceleratorConfig.initial_bitrate);

    EXPECT_CALL(
        *mock_rate_ctrl_,
        UpdateRateControl(MatchRtcConfigWithRates(
            kDefaultVideoEncodeAcceleratorConfig.input_visible_size,
            initial_bitrate_allocation,
            VideoEncodeAccelerator::kDefaultFramerate, kNumTemporalLayers)))
        .Times(1)
        .WillOnce(Return());
  }
  EXPECT_TRUE(
      encoder_->Initialize(kDefaultVideoEncodeAcceleratorConfig, ave_config));
}

void VP9EncoderTest::EncodeSequence(FrameType frame_type) {
  InSequence seq;
  const bool keyframe = frame_type == FrameType::KEYFRAME;
  auto encode_job = CreateEncodeJob(keyframe);
  scoped_refptr<VP9Picture> picture(new VP9Picture);
  EXPECT_CALL(*mock_accelerator_, GetPicture(encode_job.get()))
      .WillOnce(Invoke(
          [picture](AcceleratedVideoEncoder::EncodeJob*) { return picture; }));
  const auto& expected_ref_frames_used =
      keyframe ? kRefFramesUsedForKeyFrame : kRefFramesUsedForInterFrame;
  EXPECT_CALL(*mock_accelerator_,
              SubmitFrameParameters(
                  encode_job.get(), _, _, _,
                  ::testing::ElementsAreArray(expected_ref_frames_used)))
      .WillOnce(Return(true));
  EXPECT_TRUE(encoder_->PrepareEncodeJob(encode_job.get()));
  // TODO(hiroh): Test for encoder_->reference_frames_.
}

void VP9EncoderTest::EncodeConstantQuantizationParameterSequence(
    FrameType frame_type,
    const std::vector<bool>& expected_ref_frames_used,
    base::Optional<uint8_t> expected_temporal_idx) {
  const bool keyframe = frame_type == FrameType::KEYFRAME;
  InSequence seq;
  auto encode_job = CreateEncodeJob(keyframe);
  scoped_refptr<VP9Picture> picture(new VP9Picture);
  EXPECT_CALL(*mock_accelerator_, GetPicture(encode_job.get()))
      .WillOnce(Invoke(
          [picture](AcceleratedVideoEncoder::EncodeJob*) { return picture; }));

  FRAME_TYPE libvpx_frame_type =
      keyframe ? FRAME_TYPE::KEY_FRAME : FRAME_TYPE::INTER_FRAME;
  EXPECT_CALL(*mock_rate_ctrl_, ComputeQP(MatchFrameParam(
                                    libvpx_frame_type, expected_temporal_idx)))
      .WillOnce(Return());
  constexpr int kDefaultQP = 34;
  constexpr int kDefaultLoopFilterLevel = 8;
  EXPECT_CALL(*mock_rate_ctrl_, GetQP()).WillOnce(Return(kDefaultQP));
  EXPECT_CALL(*mock_rate_ctrl_, GetLoopfilterLevel())
      .WillOnce(Return(kDefaultLoopFilterLevel));
  if (!expected_ref_frames_used.empty()) {
    EXPECT_CALL(*mock_accelerator_,
                SubmitFrameParameters(
                    encode_job.get(), _, _, _,
                    ::testing::ElementsAreArray(expected_ref_frames_used)))
        .WillOnce(Return(true));
  } else {
    EXPECT_CALL(*mock_accelerator_,
                SubmitFrameParameters(encode_job.get(), _, _, _, _))
        .WillOnce(Return(true));
  }
  EXPECT_TRUE(encoder_->PrepareEncodeJob(encode_job.get()));

  // TODO(hiroh): Test for encoder_->reference_frames_.

  constexpr size_t kDefaultEncodedFrameSize = 123456;
  // For BitrateControlUpdate sequence.
  EXPECT_CALL(*mock_rate_ctrl_, PostEncodeUpdate(kDefaultEncodedFrameSize))
      .WillOnce(Return());
  encoder_->BitrateControlUpdate(kDefaultEncodedFrameSize);
}

void VP9EncoderTest::UpdateRatesSequence(
    const VideoBitrateAllocation& bitrate_allocation,
    uint32_t framerate,
    BitrateControl bitrate_control) {
  ASSERT_TRUE(encoder_->current_params_.bitrate_allocation !=
                  bitrate_allocation ||
              encoder_->current_params_.framerate != framerate);

  if (bitrate_control == BitrateControl::kConstantQuantizationParameter) {
    constexpr size_t kNumTemporalLayers = 1u;
    EXPECT_CALL(*mock_rate_ctrl_,
                UpdateRateControl(MatchRtcConfigWithRates(
                    encoder_->visible_size_, bitrate_allocation, framerate,
                    kNumTemporalLayers)))
        .Times(1)
        .WillOnce(Return());
  }

  EXPECT_TRUE(encoder_->UpdateRates(bitrate_allocation, framerate));
  EXPECT_EQ(encoder_->current_params_.bitrate_allocation, bitrate_allocation);
  EXPECT_EQ(encoder_->current_params_.framerate, framerate);
}

void VP9EncoderTest::UpdateRatesTest(BitrateControl bitrate_control,
                                     size_t num_temporal_layers) {
  ASSERT_LE(num_temporal_layers, 3u);
  auto create_allocation =
      [num_temporal_layers](uint32_t bitrate) -> VideoBitrateAllocation {
    VideoBitrateAllocation bitrate_allocation;
    constexpr int kTemporalLayerBitrateScaleFactor[] = {1, 2, 4};
    const int kScaleFactors =
        std::accumulate(std::cbegin(kTemporalLayerBitrateScaleFactor),
                        std::cend(kTemporalLayerBitrateScaleFactor), 0);
    for (size_t ti = 0; ti < num_temporal_layers; ti++) {
      bitrate_allocation.SetBitrate(
          0, ti,
          bitrate * kTemporalLayerBitrateScaleFactor[ti] / kScaleFactors);
    }
    return bitrate_allocation;
  };

  const auto update_rates_and_encode =
      [this, bitrate_control](FrameType frame_type,
                              const VideoBitrateAllocation& bitrate_allocation,
                              uint32_t framerate) {
        UpdateRatesSequence(bitrate_allocation, framerate, bitrate_control);
        if (bitrate_control == BitrateControl::kConstantQuantizationParameter) {
          EncodeConstantQuantizationParameterSequence(frame_type, {},
                                                      base::nullopt);
        } else {
          EncodeSequence(frame_type);
        }
      };

  const uint32_t kBitrate =
      kDefaultVideoEncodeAcceleratorConfig.initial_bitrate;
  const uint32_t kFramerate =
      *kDefaultVideoEncodeAcceleratorConfig.initial_framerate;
  // Call UpdateRates before Encode.
  update_rates_and_encode(FrameType::KEYFRAME, create_allocation(kBitrate / 2),
                          kFramerate);
  // Bitrate change only.
  update_rates_and_encode(FrameType::INTERFRAME, create_allocation(kBitrate),
                          kFramerate);
  // Framerate change only.
  update_rates_and_encode(FrameType::INTERFRAME, create_allocation(kBitrate),
                          kFramerate + 2);
  // Bitrate + Frame changes.
  update_rates_and_encode(FrameType::INTERFRAME,
                          create_allocation(kBitrate * 3 / 4), kFramerate - 5);
}

struct VP9EncoderTestParam {
  VP9EncoderTest::BitrateControl bitrate_control;
} kTestCasesForVP9EncoderTest[] = {
    {VP9EncoderTest::BitrateControl::kConstantBitrate},
    {VP9EncoderTest::BitrateControl::kConstantQuantizationParameter},
};

TEST_P(VP9EncoderTest, Initialize) {
  InitializeVP9Encoder(GetParam().bitrate_control);
}

TEST_P(VP9EncoderTest, Encode) {
  const auto& bitrate_control = GetParam().bitrate_control;
  InitializeVP9Encoder(bitrate_control);
  if (bitrate_control == BitrateControl::kConstantBitrate) {
    EncodeSequence(FrameType::KEYFRAME);
    EncodeSequence(FrameType::INTERFRAME);
  } else {
    EncodeConstantQuantizationParameterSequence(FrameType::KEYFRAME,
                                                kRefFramesUsedForKeyFrame);
    EncodeConstantQuantizationParameterSequence(FrameType::INTERFRAME,
                                                kRefFramesUsedForInterFrame);
  }
}

TEST_P(VP9EncoderTest, UpdateRates) {
  const auto& bitrate_control = GetParam().bitrate_control;
  InitializeVP9Encoder(bitrate_control);
  constexpr size_t kNumTemporalLayers = 1u;
  UpdateRatesTest(bitrate_control, kNumTemporalLayers);
}

TEST_P(VP9EncoderTest, ForceKeyFrame) {
  const auto& bitrate_control = GetParam().bitrate_control;
  InitializeVP9Encoder(GetParam().bitrate_control);
  if (bitrate_control == BitrateControl::kConstantBitrate) {
    EncodeSequence(FrameType::KEYFRAME);
    EncodeSequence(FrameType::INTERFRAME);
    EncodeSequence(FrameType::KEYFRAME);
    EncodeSequence(FrameType::INTERFRAME);
  } else {
    EncodeConstantQuantizationParameterSequence(FrameType::KEYFRAME,
                                                kRefFramesUsedForKeyFrame);
    EncodeConstantQuantizationParameterSequence(FrameType::INTERFRAME,
                                                kRefFramesUsedForInterFrame);
    EncodeConstantQuantizationParameterSequence(FrameType::KEYFRAME,
                                                kRefFramesUsedForKeyFrame);
    EncodeConstantQuantizationParameterSequence(FrameType::INTERFRAME,
                                                kRefFramesUsedForInterFrame);
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         VP9EncoderTest,
                         ::testing::ValuesIn(kTestCasesForVP9EncoderTest));
}  // namespace media
