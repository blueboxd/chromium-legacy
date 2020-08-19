// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <va/va.h>

#include <iostream>
#include <string>

#include "base/command_line.h"
#include "base/files/memory_mapped_file.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "media/filters/ivf_parser.h"
#include "media/gpu/vaapi/test/vaapi_device.h"
#include "media/gpu/vaapi/test/video_decoder.h"
#include "media/gpu/vaapi/test/vp9_decoder.h"
#include "media/gpu/vaapi/va_stubs.h"
#include "ui/gfx/geometry/size.h"

using media::vaapi_test::VaapiDevice;
using media::vaapi_test::VideoDecoder;
using media::vaapi_test::Vp9Decoder;
using media_gpu_vaapi::InitializeStubs;
using media_gpu_vaapi::kModuleVa;
using media_gpu_vaapi::kModuleVa_drm;
using media_gpu_vaapi::StubPathMap;

namespace {

constexpr char kUsageMsg[] =
    "usage: decode_test\n"
    "           --video=<video path>\n"
    "           --profile=<profile of input video>\n"
    "           [--frames=<number of frames to decode>]\n"
    "           [--out-prefix=<path prefix of decoded frame PNGs>]\n"
    "           [--v=<log verbosity>]"
    "           [--help]";

constexpr char kHelpMsg[] =
    "This binary decodes the IVF video in <video> path with specified video\n"
    "<profile> via thinly wrapped libva calls.\n"
    "\nThe following arguments are supported:\n"
    "    --video=<path>\n"
    "        Required. Path to IVF-formatted video to decode.\n"
    "    --profile=<VP9Profile0|VP9Profile2>\n"
    "        Required (case insensitive). Profile of given video.\n"
    "    --frames=<int>\n"
    "        Optional. Number of frames to decode, defaults to all.\n"
    "        Override with a positive integer to decode at most that many.\n"
    "    --out-prefix=<string>\n"
    "        Optional. Save PNGs of decoded frames if and only if a path\n"
    "        prefix (which may specify a directory) is provided here,\n"
    "        resulting in e.g. frame_0.png, frame_1.png, etc. if passed\n"
    "        \"frame\".\n"
    "        If omitted, the output of this binary is error or lack thereof.\n"
    "    --help\n"
    "        Display this help message and exit.\n";

// Converts the provided string to a VAProfile, returning VAProfileNone if not
// supported by the test binary.
VAProfile GetProfile(const std::string& profile_str) {
  if (base::EqualsCaseInsensitiveASCII(profile_str, "vp9profile0"))
    return VAProfileVP9Profile0;
  if (base::EqualsCaseInsensitiveASCII(profile_str, "vp9profile2"))
    return VAProfileVP9Profile2;
  return VAProfileNone;
}

// Gets the appropriate decoder for the given |va_profile|.
std::unique_ptr<VideoDecoder> GetDecoder(
    VAProfile va_profile,
    std::unique_ptr<media::IvfParser> ivf_parser,
    const VaapiDevice& va) {
  switch (va_profile) {
    case VAProfileVP9Profile0:
    case VAProfileVP9Profile2:
      return std::make_unique<Vp9Decoder>(std::move(ivf_parser), va);
    default:
      return nullptr;
  }
}

}  // namespace

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);

  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);

  const base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();

  if (cmd->HasSwitch("help")) {
    std::cout << kUsageMsg << "\n" << kHelpMsg;
    return EXIT_SUCCESS;
  }

  const base::FilePath video_path = cmd->GetSwitchValuePath("video");
  if (video_path.empty()) {
    std::cout << "No input video path provided to decode.\n" << kUsageMsg;
    return EXIT_FAILURE;
  }

  const std::string profile_str = cmd->GetSwitchValueASCII("profile");
  if (profile_str.empty()) {
    std::cout << "No profile provided with which to decode.\n" << kUsageMsg;
    return EXIT_FAILURE;
  }
  const VAProfile profile = GetProfile(profile_str);
  if (profile == VAProfileNone) {
    std::cout << "Profile " << profile_str << " not supported.\n" << kUsageMsg;
    return EXIT_FAILURE;
  }

  std::string output_prefix = cmd->GetSwitchValueASCII("out-prefix");

  const std::string frames = cmd->GetSwitchValueASCII("frames");
  int n_frames;
  if (frames.empty()) {
    n_frames = 0;
  } else if (!base::StringToInt(frames, &n_frames) || n_frames <= 0) {
    LOG(ERROR) << "Number of frames to decode must be positive integer, got "
               << frames;
    return EXIT_FAILURE;
  }

  // Set up video stream and parser.
  base::MemoryMappedFile stream;
  if (!stream.Initialize(video_path)) {
    LOG(ERROR) << "Couldn't open file: " << video_path;
    return EXIT_FAILURE;
  }

  auto ivf_parser = std::make_unique<media::IvfParser>();
  media::IvfFileHeader file_header{};
  if (!ivf_parser->Initialize(stream.data(), stream.length(), &file_header)) {
    LOG(ERROR) << "Couldn't initialize IVF parser for file: " << video_path;
    return EXIT_FAILURE;
  }
  const gfx::Size size(file_header.width, file_header.height);
  VLOG(1) << "video size: " << size.ToString();

  // Initialize VA stubs.
  StubPathMap paths;
  const std::string va_suffix(base::NumberToString(VA_MAJOR_VERSION + 1));
  paths[kModuleVa].push_back(std::string("libva.so.") + va_suffix);
  paths[kModuleVa_drm].push_back(std::string("libva-drm.so.") + va_suffix);
  if (!InitializeStubs(paths)) {
    LOG(ERROR) << "Failed to initialize VA stubs";
    return EXIT_FAILURE;
  }

  // Initialize VA with profile as provided in cmdline args.
  VLOG(1) << "Creating VaapiDevice with profile " << profile;
  const VaapiDevice va(profile);

  std::unique_ptr<VideoDecoder> dec =
      GetDecoder(profile, std::move(ivf_parser), va);
  CHECK(dec);
  VideoDecoder::Result res;
  int i = 0;
  while (true) {
    LOG(INFO) << "Frame " << i << "...";
    res = dec->DecodeNextFrame();

    if (res == VideoDecoder::kEOStream) {
      LOG(INFO) << "End of stream.";
      break;
    }

    if (res == VideoDecoder::kFailed) {
      LOG(ERROR) << "Failed to decode.";
      continue;
    }

    if (!output_prefix.empty()) {
      dec->LastDecodedFrameToPNG(
          base::StringPrintf("%s_%d.png", output_prefix.c_str(), i));
    }

    if (++i == n_frames)
      break;
  };

  LOG(INFO) << "Done reading.";

  return EXIT_SUCCESS;
}
