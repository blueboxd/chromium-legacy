// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "media/base/test_data_util.h"
#include "media/gpu/test/video_frame_file_writer.h"
#include "media/gpu/test/video_frame_validator.h"
#include "media/gpu/test/video_player/frame_renderer_dummy.h"
#include "media/gpu/test/video_player/frame_renderer_thumbnail.h"
#include "media/gpu/test/video_player/video.h"
#include "media/gpu/test/video_player/video_decoder_client.h"
#include "media/gpu/test/video_player/video_player.h"
#include "media/gpu/test/video_player/video_player_test_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace test {

namespace {

// Video decoder tests usage message. Make sure to also update the documentation
// under docs/media/gpu/video_decoder_test_usage.md when making changes here.
constexpr const char* usage_msg =
    "usage: video_decode_accelerator_tests\n"
    "           [-v=<level>] [--vmodule=<config>] [--disable_validator]\n"
    "           [--output_frames] [output_folder] [--use_vd] [--gtest_help]\n"
    "           [--help] [<video path>] [<video metadata path>]\n";

// Video decoder tests help message.
constexpr const char* help_msg =
    "Run the video decode accelerator tests on the video specified by\n"
    "<video path>. If no <video path> is given the default\n"
    "\"test-25fps.h264\" video will be used.\n"
    "\nThe <video metadata path> should specify the location of a json file\n"
    "containing the video's metadata, such as frame checksums. By default\n"
    "<video path>.json will be used.\n"
    "\nThe following arguments are supported:\n"
    "   -v                  enable verbose mode, e.g. -v=2.\n"
    "  --vmodule            enable verbose mode for the specified module,\n"
    "                       e.g. --vmodule=*media/gpu*=2.\n"
    "  --disable_validator  disable frame validation.\n"
    "  --output_frames      write all decoded video frames to the\n"
    "                       \"<testname>\" folder.\n"
    "  --output_folder      overwrite the default output folder used when\n"
    "                       \"--output_frames\" is specified.\n"
    "  --use_vd             use the new VD-based video decoders, instead of\n"
    "                       the default VDA-based video decoders.\n"
    "  --gtest_help         display the gtest help and exit.\n"
    "  --help               display this help and exit.\n";

media::test::VideoPlayerTestEnvironment* g_env;

// Video decode test class. Performs setup and teardown for each single test.
class VideoDecoderTest : public ::testing::Test {
 public:
  std::unique_ptr<VideoPlayer> CreateVideoPlayer(
      const Video* video,
      VideoDecoderClientConfig config = VideoDecoderClientConfig(),
      std::unique_ptr<FrameRenderer> frame_renderer =
          FrameRendererDummy::Create()) {
    LOG_ASSERT(video);
    std::vector<std::unique_ptr<VideoFrameProcessor>> frame_processors;

    // Force allocate mode if import mode is not supported.
    if (!g_env->ImportSupported())
      config.allocation_mode = AllocationMode::kAllocate;

    // Use the video frame validator to validate decoded video frames if import
    // mode is supported and enabled.
    if (g_env->IsValidatorEnabled() &&
        config.allocation_mode == AllocationMode::kImport) {
      frame_processors.push_back(
          media::test::VideoFrameValidator::Create(video->FrameChecksums()));
    }

    // Write decoded video frames to the '<testname>' folder.
    if (g_env->IsFramesOutputEnabled()) {
      base::FilePath output_folder =
          base::FilePath(g_env->OutputFolder())
              .Append(base::FilePath(g_env->GetTestName()));
      frame_processors.push_back(VideoFrameFileWriter::Create(output_folder));
      VLOG(0) << "Writing video frames to: " << output_folder;
    }

    // Use the new VD-based video decoders if requested.
    config.use_vd = g_env->UseVD();

    return VideoPlayer::Create(video, std::move(frame_renderer),
                               std::move(frame_processors), config);
  }
};

}  // namespace

// Play video from start to end. Wait for the kFlushDone event at the end of the
// stream, that notifies us all frames have been decoded.
TEST_F(VideoDecoderTest, FlushAtEndOfStream) {
  auto tvp = CreateVideoPlayer(g_env->Video());

  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());

  EXPECT_EQ(tvp->GetFlushDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(), g_env->Video()->NumFrames());
  EXPECT_TRUE(tvp->WaitForFrameProcessors());
}

// Flush the decoder immediately after initialization.
TEST_F(VideoDecoderTest, FlushAfterInitialize) {
  auto tvp = CreateVideoPlayer(g_env->Video());

  tvp->Flush();
  EXPECT_TRUE(tvp->WaitForFlushDone());
  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());

  EXPECT_EQ(tvp->GetFlushDoneCount(), 2u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(), g_env->Video()->NumFrames());
  EXPECT_TRUE(tvp->WaitForFrameProcessors());
}

// Reset the decoder immediately after initialization.
TEST_F(VideoDecoderTest, ResetAfterInitialize) {
  auto tvp = CreateVideoPlayer(g_env->Video());

  tvp->Reset();
  EXPECT_TRUE(tvp->WaitForResetDone());
  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());

  EXPECT_EQ(tvp->GetResetDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFlushDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(), g_env->Video()->NumFrames());
  EXPECT_TRUE(tvp->WaitForFrameProcessors());
}

// Reset the decoder when the middle of the stream is reached.
TEST_F(VideoDecoderTest, ResetMidStream) {
  auto tvp = CreateVideoPlayer(g_env->Video());

  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFrameDecoded(g_env->Video()->NumFrames() / 2));
  tvp->Reset();
  EXPECT_TRUE(tvp->WaitForResetDone());
  size_t numFramesDecoded = tvp->GetFrameDecodedCount();
  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());

  EXPECT_EQ(tvp->GetResetDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFlushDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(),
            numFramesDecoded + g_env->Video()->NumFrames());
  EXPECT_TRUE(tvp->WaitForFrameProcessors());
}

// Reset the decoder when the end of the stream is reached.
TEST_F(VideoDecoderTest, ResetEndOfStream) {
  auto tvp = CreateVideoPlayer(g_env->Video());

  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());
  EXPECT_EQ(tvp->GetFrameDecodedCount(), g_env->Video()->NumFrames());
  tvp->Reset();
  EXPECT_TRUE(tvp->WaitForResetDone());
  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());

  EXPECT_EQ(tvp->GetResetDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFlushDoneCount(), 2u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(), g_env->Video()->NumFrames() * 2);
  EXPECT_TRUE(tvp->WaitForFrameProcessors());
}

// Reset the decoder immediately when the end-of-stream flush starts, without
// waiting for a kFlushDone event.
TEST_F(VideoDecoderTest, ResetBeforeFlushDone) {
  auto tvp = CreateVideoPlayer(g_env->Video());

  // Reset when a kFlushing event is received.
  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());
  tvp->Reset();
  EXPECT_TRUE(tvp->WaitForResetDone());

  // Reset will cause the decoder to drop everything it's doing, including the
  // ongoing flush operation. However the flush might have been completed
  // already by the time reset is called. So depending on the timing of the
  // calls we should see 0 or 1 flushes, and the last few video frames might
  // have been dropped.
  EXPECT_LE(tvp->GetFlushDoneCount(), 1u);
  EXPECT_EQ(tvp->GetResetDoneCount(), 1u);
  EXPECT_LE(tvp->GetFrameDecodedCount(), g_env->Video()->NumFrames());
  EXPECT_TRUE(tvp->WaitForFrameProcessors());
}

// Reset the decoder immediately when encountering the first config info in a
// H.264 video stream. After resetting the video is played until the end.
TEST_F(VideoDecoderTest, ResetAfterFirstConfigInfo) {
  // This test is only relevant for H.264 video streams.
  if (g_env->Video()->Profile() < H264PROFILE_MIN ||
      g_env->Video()->Profile() > H264PROFILE_MAX)
    GTEST_SKIP();

  auto tvp = CreateVideoPlayer(g_env->Video());

  tvp->PlayUntil(VideoPlayerEvent::kConfigInfo);
  EXPECT_TRUE(tvp->WaitForEvent(VideoPlayerEvent::kConfigInfo));
  tvp->Reset();
  EXPECT_TRUE(tvp->WaitForResetDone());
  size_t numFramesDecoded = tvp->GetFrameDecodedCount();
  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());

  EXPECT_EQ(tvp->GetResetDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFlushDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(),
            numFramesDecoded + g_env->Video()->NumFrames());
  EXPECT_GE(tvp->GetEventCount(VideoPlayerEvent::kConfigInfo), 1u);
  EXPECT_TRUE(tvp->WaitForFrameProcessors());
}

// Play video from start to end. Multiple buffer decodes will be queued in the
// decoder, without waiting for the result of the previous decode requests.
TEST_F(VideoDecoderTest, FlushAtEndOfStream_MultipleOutstandingDecodes) {
  VideoDecoderClientConfig config;
  config.max_outstanding_decode_requests = 4;
  auto tvp = CreateVideoPlayer(g_env->Video(), config);

  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());

  EXPECT_EQ(tvp->GetFlushDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(), g_env->Video()->NumFrames());
  EXPECT_TRUE(tvp->WaitForFrameProcessors());
}

// Play multiple videos simultaneously from start to finish.
TEST_F(VideoDecoderTest, FlushAtEndOfStream_MultipleConcurrentDecodes) {
  // The minimal number of concurrent decoders we expect to be supported.
  constexpr size_t kMinSupportedConcurrentDecoders = 3;

  std::vector<std::unique_ptr<VideoPlayer>> tvps(
      kMinSupportedConcurrentDecoders);
  for (size_t i = 0; i < kMinSupportedConcurrentDecoders; ++i)
    tvps[i] = CreateVideoPlayer(g_env->Video());

  for (size_t i = 0; i < kMinSupportedConcurrentDecoders; ++i)
    tvps[i]->Play();

  for (size_t i = 0; i < kMinSupportedConcurrentDecoders; ++i) {
    EXPECT_TRUE(tvps[i]->WaitForFlushDone());
    EXPECT_EQ(tvps[i]->GetFlushDoneCount(), 1u);
    EXPECT_EQ(tvps[i]->GetFrameDecodedCount(), g_env->Video()->NumFrames());
    EXPECT_TRUE(tvps[i]->WaitForFrameProcessors());
  }
}

// Play a video from start to finish. Thumbnails of the decoded frames will be
// rendered into a image, whose checksum is compared to a golden value. This
// test is only run on older platforms that don't support the video frame
// validator, which requires import mode. If no thumbnail checksums are present
// in the video metadata the test will be skipped. This test will be deprecated
// once all devices support import mode.
TEST_F(VideoDecoderTest, FlushAtEndOfStream_RenderThumbnails) {
  if (!g_env->IsValidatorEnabled() || g_env->ImportSupported() ||
      g_env->Video()->ThumbnailChecksums().empty()) {
    GTEST_SKIP();
  }

  base::FilePath output_folder =
      base::FilePath(g_env->OutputFolder())
          .Append(base::FilePath(g_env->GetTestName()));

  VideoDecoderClientConfig config;
  config.allocation_mode = AllocationMode::kAllocate;
  auto tvp = CreateVideoPlayer(
      g_env->Video(), config,
      FrameRendererThumbnail::Create(g_env->Video()->ThumbnailChecksums(),
                                     output_folder));

  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());

  EXPECT_EQ(tvp->GetFlushDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(), g_env->Video()->NumFrames());
  EXPECT_TRUE(tvp->WaitForFrameProcessors());
  EXPECT_TRUE(static_cast<FrameRendererThumbnail*>(tvp->GetFrameRenderer())
                  ->ValidateThumbnail());
}

// Play a video from start to finish, using allocate mode. This test is only run
// on platforms that support import mode, as on allocate-mode only platforms all
// tests are run in allocate mode. The test will be skipped when --use_vd is
// specified as the new video decoders only support import mode.
// TODO(dstaessens): Deprecate after switching to new VD-based video decoders.
TEST_F(VideoDecoderTest, FlushAtEndOfStream_Allocate) {
  if (!g_env->ImportSupported() || g_env->UseVD())
    GTEST_SKIP();

  VideoDecoderClientConfig config;
  config.allocation_mode = AllocationMode::kAllocate;
  auto tvp = CreateVideoPlayer(g_env->Video(), config);

  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());

  EXPECT_EQ(tvp->GetFlushDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(), g_env->Video()->NumFrames());
  EXPECT_TRUE(tvp->WaitForFrameProcessors());
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
      (args.size() >= 1) ? base::FilePath(args[0]) : base::FilePath();
  base::FilePath video_metadata_path =
      (args.size() >= 2) ? base::FilePath(args[1]) : base::FilePath();

  // Parse command line arguments.
  bool enable_validator = true;
  bool output_frames = false;
  base::FilePath::StringType output_folder = base::FilePath::kCurrentDirectory;
  bool use_vd = false;
  base::CommandLine::SwitchMap switches = cmd_line->GetSwitches();
  for (base::CommandLine::SwitchMap::const_iterator it = switches.begin();
       it != switches.end(); ++it) {
    if (it->first.find("gtest_") == 0 ||               // Handled by GoogleTest
        it->first == "v" || it->first == "vmodule") {  // Handled by Chrome
      continue;
    }

    if (it->first == "disable_validator") {
      enable_validator = false;
    } else if (it->first == "output_frames") {
      output_frames = true;
    } else if (it->first == "output_folder") {
      output_folder = it->second;
    } else if (it->first == "use_vd") {
      use_vd = true;
    } else {
      std::cout << "unknown option: --" << it->first << "\n"
                << media::test::usage_msg;
      return EXIT_FAILURE;
    }
  }

  testing::InitGoogleTest(&argc, argv);

  // Set up our test environment.
  media::test::VideoPlayerTestEnvironment* test_environment =
      media::test::VideoPlayerTestEnvironment::Create(
          video_path, video_metadata_path, enable_validator, output_frames,
          base::FilePath(output_folder), use_vd);
  if (!test_environment)
    return EXIT_FAILURE;

  media::test::g_env = static_cast<media::test::VideoPlayerTestEnvironment*>(
      testing::AddGlobalTestEnvironment(test_environment));

  return RUN_ALL_TESTS();
}
