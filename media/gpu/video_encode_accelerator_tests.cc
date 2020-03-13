// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "media/base/test_data_util.h"
#include "media/gpu/test/video.h"
#include "media/gpu/test/video_encoder/video_encoder.h"
#include "media/gpu/test/video_encoder/video_encoder_client.h"
#include "media/gpu/test/video_encoder/video_encoder_test_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace test {

namespace {

// Video encoder tests usage message. Make sure to also update the documentation
// under docs/media/gpu/video_encoder_test_usage.md when making changes here.
// TODO(dstaessens): Add video_encoder_test_usage.md
constexpr const char* usage_msg =
    "usage: video_encode_accelerator_tests\n"
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
    "   -v                  enable verbose mode, e.g. -v=2.\n"
    "  --vmodule            enable verbose mode for the specified module,\n"
    "                       e.g. --vmodule=*media/gpu*=2.\n\n"
    "  --gtest_help         display the gtest help and exit.\n"
    "  --help               display this help and exit.\n";

// Default video to be used if no test video was specified.
constexpr base::FilePath::CharType kDefaultTestVideoPath[] =
    FILE_PATH_LITERAL("bear_320x192_40frames.yuv.webm");

media::test::VideoEncoderTestEnvironment* g_env;

// Video encode test class. Performs setup and teardown for each single test.
class VideoEncoderTest : public ::testing::Test {
 public:
  std::unique_ptr<VideoEncoder> CreateVideoEncoder(
      const Video* video,
      VideoEncoderClientConfig config = VideoEncoderClientConfig()) {
    LOG_ASSERT(video);

    auto video_encoder = VideoEncoder::Create(config);

    LOG_ASSERT(video_encoder);
    LOG_ASSERT(video_encoder->Initialize(video));

    return video_encoder;
  }
};

}  // namespace

// TODO(dstaessens): Add more test scenarios:
// - Vary framerate
// - Vary bitrate
// - Flush midstream
// - Forcing key frames

// Encode video from start to end. Wait for the kFlushDone event at the end of
// the stream, that notifies us all frames have been encoded.
TEST_F(VideoEncoderTest, FlushAtEndOfStream) {
  VideoEncoderClientConfig config = VideoEncoderClientConfig();
  config.framerate = g_env->Video()->FrameRate();
  auto encoder = CreateVideoEncoder(g_env->Video(), config);

  encoder->Encode();
  EXPECT_TRUE(encoder->WaitForFlushDone());

  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(), g_env->Video()->NumFrames());
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

  // Parse command line arguments.
  base::CommandLine::SwitchMap switches = cmd_line->GetSwitches();
  for (base::CommandLine::SwitchMap::const_iterator it = switches.begin();
       it != switches.end(); ++it) {
    if (it->first.find("gtest_") == 0 ||               // Handled by GoogleTest
        it->first == "v" || it->first == "vmodule") {  // Handled by Chrome
      continue;
    }

    std::cout << "unknown option: --" << it->first << "\n"
              << media::test::usage_msg;
    return EXIT_FAILURE;
  }

  testing::InitGoogleTest(&argc, argv);

  // Set up our test environment.
  media::test::VideoEncoderTestEnvironment* test_environment =
      media::test::VideoEncoderTestEnvironment::Create(
          video_path, video_metadata_path, base::FilePath());
  if (!test_environment)
    return EXIT_FAILURE;

  media::test::g_env = static_cast<media::test::VideoEncoderTestEnvironment*>(
      testing::AddGlobalTestEnvironment(test_environment));

  return RUN_ALL_TESTS();
}
