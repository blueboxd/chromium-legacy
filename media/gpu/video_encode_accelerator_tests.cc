// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <limits>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "media/base/media_util.h"
#include "media/base/test_data_util.h"
#include "media/base/video_bitrate_allocation.h"
#include "media/base/video_decoder_config.h"
#include "media/gpu/test/video.h"
#include "media/gpu/test/video_encoder/bitstream_file_writer.h"
#include "media/gpu/test/video_encoder/bitstream_validator.h"
#include "media/gpu/test/video_encoder/decoder_buffer_validator.h"
#include "media/gpu/test/video_encoder/video_encoder.h"
#include "media/gpu/test/video_encoder/video_encoder_client.h"
#include "media/gpu/test/video_encoder/video_encoder_test_environment.h"
#include "media/gpu/test/video_frame_helpers.h"
#include "media/gpu/test/video_frame_validator.h"
#include "media/gpu/test/video_test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace test {

namespace {

// Video encoder tests usage message. Make sure to also update the documentation
// under docs/media/gpu/video_encoder_test_usage.md when making changes here.
// TODO(dstaessens): Add video_encoder_test_usage.md
constexpr const char* usage_msg =
    "usage: video_encode_accelerator_tests\n"
    "           [--codec=<codec>] [--disable_validator]\n"
    "           [--output_bitstream] [--output_folder=<filepath>]\n"
    "           [-v=<level>] [--vmodule=<config>] [--gtest_help] [--help]\n"
    "           [<video path>] [<video metadata path>]\n";

// Video encoder tests help message.
constexpr const char* help_msg =
    "Run the video encoder accelerator tests on the video specified by\n"
    "<video path>. If no <video path> is given the default\n"
    "\"bear_320x192_40frames.yuv.webm\" video will be used.\n"
    "\nThe <video metadata path> should specify the location of a json file\n"
    "containing the video's metadata, such as frame checksums. By default\n"
    "<video path>.json will be used.\n"
    "\nThe following arguments are supported:\n"
    "  --codec              codec profile to encode, \"h264\" (baseline),\n"
    "                       \"h264main, \"h264high\", \"vp8\" and \"vp9\".\n"
    "                       H264 Baseline is selected if unspecified.\n"
    "  --disable_validator  disable validation of encoded bitstream.\n\n"
    "  --output_bitstream   save the output bitstream in either H264 AnnexB\n"
    "                       format (for H264) or IVF format (for vp8 and vp9)\n"
    "                       to <output_folder>/<testname>/<filename> +\n"
    "                       .(h264|ivf).\n"
    "  --output_folder      set the basic folder used to store the output\n"
    "                       stream. The default is the current directory.\n"
    "   -v                  enable verbose mode, e.g. -v=2.\n"
    "  --vmodule            enable verbose mode for the specified module,\n"
    "                       e.g. --vmodule=*media/gpu*=2.\n\n"
    "  --gtest_help         display the gtest help and exit.\n"
    "  --help               display this help and exit.\n";

// Default video to be used if no test video was specified.
constexpr base::FilePath::CharType kDefaultTestVideoPath[] =
    FILE_PATH_LITERAL("bear_320x192_40frames.yuv.webm");

// The number of frames to encode for bitrate check test cases.
// TODO(hiroh): Decrease this values to make the test faster.
constexpr size_t kNumFramesToEncodeForBitrateCheck = 300;
// Tolerance factor for how encoded bitrate can differ from requested bitrate.
constexpr double kBitrateTolerance = 0.1;

media::test::VideoEncoderTestEnvironment* g_env;

// Video encode test class. Performs setup and teardown for each single test.
class VideoEncoderTest : public ::testing::Test {
 public:
  std::unique_ptr<VideoEncoder> CreateVideoEncoder(
      Video* video,
      VideoEncoderClientConfig config) {
    LOG_ASSERT(video);

    auto video_encoder =
        VideoEncoder::Create(config, CreateBitstreamProcessors(video, config));
    LOG_ASSERT(video_encoder);

    if (!video_encoder->Initialize(video))
      ADD_FAILURE();

    return video_encoder;
  }

 private:
  std::vector<std::unique_ptr<BitstreamProcessor>> CreateBitstreamProcessors(
      Video* video,
      VideoEncoderClientConfig config) {
    std::vector<std::unique_ptr<BitstreamProcessor>> bitstream_processors;
    if (!g_env->IsBitstreamValidatorEnabled()) {
      return bitstream_processors;
    }

    const gfx::Rect visible_rect(video->Resolution());
    VideoCodec codec = VideoCodecProfileToVideoCodec(config.output_profile);
    switch (codec) {
      case kCodecH264:
        bitstream_processors.emplace_back(
            new H264Validator(config.output_profile, visible_rect));
        break;
      case kCodecVP8:
        bitstream_processors.emplace_back(new VP8Validator(visible_rect));
        break;
      case kCodecVP9:
        bitstream_processors.emplace_back(
            new VP9Validator(config.output_profile, visible_rect));
        break;
      default:
        LOG(ERROR) << "Unsupported profile: "
                   << GetProfileName(config.output_profile);
        break;
    }

    // Attach a bitstream validator to validate all encoded video frames. The
    // bitstream validator uses a software video decoder to validate the
    // encoded buffers by decoding them. Metrics such as the image's SSIM can
    // be calculated for additional quality checks.
    VideoDecoderConfig decoder_config(
        codec, config.output_profile, VideoDecoderConfig::AlphaMode::kIsOpaque,
        VideoColorSpace(), kNoTransformation, visible_rect.size(), visible_rect,
        visible_rect.size(), EmptyExtraData(), EncryptionScheme::kUnencrypted);
    std::vector<std::unique_ptr<VideoFrameProcessor>> video_frame_processors;

    raw_data_helper_ = RawDataHelper::Create(video);
    if (!raw_data_helper_) {
      LOG(ERROR) << "Failed to create raw data helper";
      return bitstream_processors;
    }

    // TODO(hiroh): Add corrupt frame processors.
    VideoFrameValidator::GetModelFrameCB get_model_frame_cb =
        base::BindRepeating(&VideoEncoderTest::GetModelFrame,
                            base::Unretained(this));
    auto psnr_validator = PSNRVideoFrameValidator::Create(get_model_frame_cb);
    auto ssim_validator = SSIMVideoFrameValidator::Create(get_model_frame_cb);
    video_frame_processors.push_back(std::move(psnr_validator));
    video_frame_processors.push_back(std::move(ssim_validator));
    auto bitstream_validator = BitstreamValidator::Create(
        decoder_config, config.num_frames_to_encode - 1,
        std::move(video_frame_processors));
    LOG_ASSERT(bitstream_validator);
    bitstream_processors.emplace_back(std::move(bitstream_validator));

    auto output_bitstream_filepath = g_env->OutputBitstreamFilePath();
    if (output_bitstream_filepath) {
      auto bitstream_writer = BitstreamFileWriter::Create(
          *output_bitstream_filepath, codec, visible_rect.size(),
          config.framerate, config.num_frames_to_encode);
      LOG_ASSERT(bitstream_writer);
      bitstream_processors.emplace_back(std::move(bitstream_writer));
    }

    return bitstream_processors;
  }

  scoped_refptr<const VideoFrame> GetModelFrame(size_t frame_index) {
    LOG_ASSERT(raw_data_helper_);
    return raw_data_helper_->GetFrame(frame_index %
                                      g_env->Video()->NumFrames());
  }

  std::unique_ptr<RawDataHelper> raw_data_helper_;
};

}  // namespace

// TODO(dstaessens): Add more test scenarios:
// - Forcing key frames

// Encode video from start to end. Wait for the kFlushDone event at the end of
// the stream, that notifies us all frames have been encoded.
TEST_F(VideoEncoderTest, FlushAtEndOfStream) {
  VideoEncoderClientConfig config = VideoEncoderClientConfig();
  config.framerate = g_env->Video()->FrameRate();
  config.output_profile = g_env->Profile();
  config.num_frames_to_encode = g_env->Video()->NumFrames();
  auto encoder = CreateVideoEncoder(g_env->Video(), config);

  encoder->Encode();
  EXPECT_TRUE(encoder->WaitForFlushDone());

  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(), g_env->Video()->NumFrames());
  EXPECT_TRUE(encoder->WaitForBitstreamProcessors());
}

// Test initializing the video encoder. The test will be successful if the video
// encoder is capable of setting up the encoder for the specified codec and
// resolution. The test only verifies initialization and doesn't do any
// encoding.
TEST_F(VideoEncoderTest, Initialize) {
  VideoEncoderClientConfig config = VideoEncoderClientConfig();
  auto encoder = CreateVideoEncoder(g_env->Video(), config);

  EXPECT_EQ(encoder->GetEventCount(VideoEncoder::kInitialized), 1u);
}

// Create a video encoder and immediately destroy it without initializing. The
// video encoder will be automatically destroyed when the video encoder goes out
// of scope at the end of the test. The test will pass if no asserts or crashes
// are triggered upon destroying.
TEST_F(VideoEncoderTest, DestroyBeforeInitialize) {
  VideoEncoderClientConfig config = VideoEncoderClientConfig();
  auto video_encoder = VideoEncoder::Create(config);

  EXPECT_NE(video_encoder, nullptr);
}

// Encode multiple videos simultaneously from start to finish.
TEST_F(VideoEncoderTest, FlushAtEndOfStream_MultipleConcurrentEncodes) {
  // The minimal number of concurrent encoders we expect to be supported.
  constexpr size_t kMinSupportedConcurrentEncoders = 3;

  VideoEncoderClientConfig config = VideoEncoderClientConfig();
  config.framerate = g_env->Video()->FrameRate();
  config.output_profile = g_env->Profile();
  config.num_frames_to_encode = g_env->Video()->NumFrames();

  std::vector<std::unique_ptr<VideoEncoder>> encoders(
      kMinSupportedConcurrentEncoders);
  for (size_t i = 0; i < kMinSupportedConcurrentEncoders; ++i)
    encoders[i] = CreateVideoEncoder(g_env->Video(), config);

  for (size_t i = 0; i < kMinSupportedConcurrentEncoders; ++i)
    encoders[i]->Encode();

  for (size_t i = 0; i < kMinSupportedConcurrentEncoders; ++i) {
    EXPECT_TRUE(encoders[i]->WaitForFlushDone());
    EXPECT_EQ(encoders[i]->GetFlushDoneCount(), 1u);
    EXPECT_EQ(encoders[i]->GetFrameReleasedCount(),
              g_env->Video()->NumFrames());
    EXPECT_TRUE(encoders[i]->WaitForBitstreamProcessors());
  }
}

TEST_F(VideoEncoderTest, BitrateCheck) {
  VideoEncoderClientConfig config = VideoEncoderClientConfig();
  config.framerate = g_env->Video()->FrameRate();
  config.output_profile = g_env->Profile();
  config.num_frames_to_encode = kNumFramesToEncodeForBitrateCheck;
  auto encoder = CreateVideoEncoder(g_env->Video(), config);

  encoder->Encode();
  EXPECT_TRUE(encoder->WaitForFlushDone());

  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(), config.num_frames_to_encode);
  EXPECT_TRUE(encoder->WaitForBitstreamProcessors());
  EXPECT_NEAR(encoder->GetStats().Bitrate(), config.bitrate,
              kBitrateTolerance * config.bitrate);
}

TEST_F(VideoEncoderTest, DynamicBitrateChange) {
  VideoEncoderClientConfig config;
  config.framerate = g_env->Video()->FrameRate();
  config.output_profile = g_env->Profile();
  config.num_frames_to_encode = kNumFramesToEncodeForBitrateCheck * 2;
  auto encoder = CreateVideoEncoder(g_env->Video(), config);

  // Encode the video with the first bitrate.
  const uint32_t first_bitrate = config.bitrate;
  encoder->EncodeUntil(VideoEncoder::kFrameReleased,
                       kNumFramesToEncodeForBitrateCheck);
  encoder->WaitForEvent(VideoEncoder::kFrameReleased,
                        kNumFramesToEncodeForBitrateCheck);
  EXPECT_NEAR(encoder->GetStats().Bitrate(), first_bitrate,
              kBitrateTolerance * first_bitrate);

  // Encode the video with the second bitrate.
  const uint32_t second_bitrate = first_bitrate * 3 / 2;
  encoder->ResetStats();
  encoder->UpdateBitrate(second_bitrate, config.framerate);
  encoder->Encode();
  EXPECT_TRUE(encoder->WaitForFlushDone());
  EXPECT_NEAR(encoder->GetStats().Bitrate(), second_bitrate,
              kBitrateTolerance * second_bitrate);

  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(), config.num_frames_to_encode);
  EXPECT_TRUE(encoder->WaitForBitstreamProcessors());
}

TEST_F(VideoEncoderTest, DynamicFramerateChange) {
  VideoEncoderClientConfig config;
  config.framerate = g_env->Video()->FrameRate();
  config.output_profile = g_env->Profile();
  config.num_frames_to_encode = kNumFramesToEncodeForBitrateCheck * 2;
  auto encoder = CreateVideoEncoder(g_env->Video(), config);

  // Encode the video with the first framerate.
  const uint32_t first_framerate = config.framerate;

  encoder->EncodeUntil(VideoEncoder::kFrameReleased,
                       kNumFramesToEncodeForBitrateCheck);
  encoder->WaitForEvent(VideoEncoder::kFrameReleased,
                        kNumFramesToEncodeForBitrateCheck);
  EXPECT_NEAR(encoder->GetStats().Bitrate(), config.bitrate,
              kBitrateTolerance * config.bitrate);

  // Encode the video with the second framerate.
  const uint32_t second_framerate = first_framerate * 3 / 2;
  encoder->ResetStats();
  encoder->UpdateBitrate(config.bitrate, second_framerate);
  encoder->Encode();
  EXPECT_TRUE(encoder->WaitForFlushDone());
  EXPECT_NEAR(encoder->GetStats().Bitrate(), config.bitrate,
              kBitrateTolerance * config.bitrate);

  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(), config.num_frames_to_encode);
  EXPECT_TRUE(encoder->WaitForBitstreamProcessors());
}
}  // namespace test
}  // namespace media

int main(int argc, char** argv) {
  // Set the default test data path.
  media::test::Video::SetTestDataPath(media::GetTestDataPath());

  // Print the help message if requested. This needs to be done before
  // initializing gtest, to overwrite the default gtest help message.
  base::CommandLine::Init(argc, argv);
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  LOG_ASSERT(cmd_line);
  if (cmd_line->HasSwitch("help")) {
    std::cout << media::test::usage_msg << "\n" << media::test::help_msg;
    return 0;
  }

  // Check if a video was specified on the command line.
  base::CommandLine::StringVector args = cmd_line->GetArgs();
  base::FilePath video_path =
      (args.size() >= 1) ? base::FilePath(args[0])
                         : base::FilePath(media::test::kDefaultTestVideoPath);
  base::FilePath video_metadata_path =
      (args.size() >= 2) ? base::FilePath(args[1]) : base::FilePath();
  std::string codec = "h264";
  bool output_bitstream = false;
  base::FilePath output_folder =
      base::FilePath(base::FilePath::kCurrentDirectory);

  // Parse command line arguments.
  bool enable_bitstream_validator = true;
  base::CommandLine::SwitchMap switches = cmd_line->GetSwitches();
  for (base::CommandLine::SwitchMap::const_iterator it = switches.begin();
       it != switches.end(); ++it) {
    if (it->first.find("gtest_") == 0 ||               // Handled by GoogleTest
        it->first == "v" || it->first == "vmodule") {  // Handled by Chrome
      continue;
    }

    if (it->first == "codec") {
      codec = it->second;
    } else if (it->first == "disable_validator") {
      enable_bitstream_validator = false;
    } else if (it->first == "output_bitstream") {
      output_bitstream = true;
    } else if (it->first == "output_folder") {
      output_folder = base::FilePath(it->second);
    } else {
      std::cout << "unknown option: --" << it->first << "\n"
                << media::test::usage_msg;
      return EXIT_FAILURE;
    }
  }

  testing::InitGoogleTest(&argc, argv);

  // Set up our test environment.
  media::test::VideoEncoderTestEnvironment* test_environment =
      media::test::VideoEncoderTestEnvironment::Create(
          video_path, video_metadata_path, enable_bitstream_validator,
          output_folder, codec, output_bitstream);
  if (!test_environment)
    return EXIT_FAILURE;

  media::test::g_env = static_cast<media::test::VideoEncoderTestEnvironment*>(
      testing::AddGlobalTestEnvironment(test_environment));

  return RUN_ALL_TESTS();
}
