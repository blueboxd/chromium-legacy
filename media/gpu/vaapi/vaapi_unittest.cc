// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This has to be included first.
// See http://code.google.com/p/googletest/issues/detail?id=371
#include "testing/gtest/include/gtest/gtest.h"

#include <unistd.h>
#include <map>
#include <vector>

#include <va/va.h>

#include "base/files/file.h"
#include "base/files/scoped_file.h"
#include "base/optional.h"
#include "base/process/launch.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"

namespace media {
namespace {

base::Optional<VAProfile> ConvertToVAProfile(VideoCodecProfile profile) {
  // A map between VideoCodecProfile and VAProfile.
  const std::map<VideoCodecProfile, VAProfile> kProfileMap = {
      {H264PROFILE_BASELINE, VAProfileH264Baseline},
      {H264PROFILE_MAIN, VAProfileH264Main},
      {H264PROFILE_HIGH, VAProfileH264High},
      {VP8PROFILE_ANY, VAProfileVP8Version0_3},
      {VP9PROFILE_PROFILE0, VAProfileVP9Profile0},
      {VP9PROFILE_PROFILE1, VAProfileVP9Profile1},
      // TODO(crbug.com/1011454, crbug.com/1011469): Reenable
      // VP9PROFILE_PROFILE2 and _PROFILE3 when P010 is completely supported.
      //{VP9PROFILE_PROFILE2, VAProfileVP9Profile2},
      //{VP9PROFILE_PROFILE3, VAProfileVP9Profile3},
  };
  auto it = kProfileMap.find(profile);
  return it != kProfileMap.end() ? base::make_optional<VAProfile>(it->second)
                                 : base::nullopt;
}

// Converts the given string to VAProfile
base::Optional<VAProfile> StringToVAProfile(const std::string& va_profile) {
  const std::map<std::string, VAProfile> kStringToVAProfile = {
      {"VAProfileNone", VAProfileNone},
      {"VAProfileH264ConstrainedBaseline", VAProfileH264ConstrainedBaseline},
      {"VAProfileH264Baseline", VAProfileH264Baseline},
      {"VAProfileH264Main", VAProfileH264Main},
      {"VAProfileH264High", VAProfileH264High},
      {"VAProfileJPEGBaseline", VAProfileJPEGBaseline},
      {"VAProfileVP8Version0_3", VAProfileVP8Version0_3},
      {"VAProfileVP9Profile0", VAProfileVP9Profile0},
      {"VAProfileVP9Profile1", VAProfileVP9Profile1},
      // TODO(crbug.com/1011454, crbug.com/1011469): Reenable
      // VP9PROFILE_PROFILE2 and _PROFILE3 when P010 is completely supported.
      // {"VAProfileVP9Profile2", VAProfileVP9Profile2},
      // {"VAProfileVP9Profile3", VAProfileVP9Profile3},
  };

  auto it = kStringToVAProfile.find(va_profile);
  return it != kStringToVAProfile.end()
             ? base::make_optional<VAProfile>(it->second)
             : base::nullopt;
}

// Converts the given string to VAProfile
base::Optional<VAEntrypoint> StringToVAEntrypoint(
    const std::string& va_entrypoint) {
  const std::map<std::string, VAEntrypoint> kStringToVAEntrypoint = {
      {"VAEntrypointVLD", VAEntrypointVLD},
      {"VAEntrypointEncSlice", VAEntrypointEncSlice},
      {"VAEntrypointEncPicture", VAEntrypointEncPicture},
      {"VAEntrypointEncSliceLP", VAEntrypointEncSliceLP},
      {"VAEntrypointVideoProc", VAEntrypointVideoProc}};

  auto it = kStringToVAEntrypoint.find(va_entrypoint);
  return it != kStringToVAEntrypoint.end()
             ? base::make_optional<VAEntrypoint>(it->second)
             : base::nullopt;
}
}  // namespace

class VaapiTest : public testing::Test {
 public:
  VaapiTest() = default;
  ~VaapiTest() override = default;
};

std::map<VAProfile, std::vector<VAEntrypoint>> ParseVainfo(
    const std::string& output) {
  const std::vector<std::string> lines =
      base::SplitString(output, "\n", base::WhitespaceHandling::TRIM_WHITESPACE,
                        base::SplitResult::SPLIT_WANT_ALL);
  std::map<VAProfile, std::vector<VAEntrypoint>> info;
  for (const std::string& line : lines) {
    if (!base::StartsWith(line, "VAProfile",
                          base::CompareCase::INSENSITIVE_ASCII)) {
      continue;
    }
    std::vector<std::string> res =
        base::SplitString(line, ":", base::WhitespaceHandling::TRIM_WHITESPACE,
                          base::SplitResult::SPLIT_WANT_ALL);
    if (res.size() != 2) {
      LOG(ERROR) << "Unexpected line: " << line;
      continue;
    }

    auto va_profile = StringToVAProfile(res[0]);
    if (!va_profile)
      continue;
    auto va_entrypoint = StringToVAEntrypoint(res[1]);
    if (!va_entrypoint)
      continue;
    info[*va_profile].push_back(*va_entrypoint);
    DVLOG(3) << line;
  }
  return info;
}

TEST_F(VaapiTest, VaapiSandboxInitialization) {
  // VASupportedProfiles::Get() is called in PreSandboxInitialization().
  // It queries VA-API driver their capabilities.
  VaapiWrapper::PreSandboxInitialization();
}

TEST_F(VaapiTest, VaapiProfiles) {
  // VASupportedProfiles::Get() is called in PreSandboxInitialization().
  // It queries VA-API driver their capabilities.
  VaapiWrapper::PreSandboxInitialization();

  int fds[2];
  PCHECK(pipe(fds) == 0);
  base::File read_pipe(fds[0]);
  base::ScopedFD write_pipe_fd(fds[1]);

  base::LaunchOptions options;
  options.fds_to_remap.emplace_back(write_pipe_fd.get(), STDOUT_FILENO);
  std::vector<std::string> argv = {"vainfo"};
  EXPECT_TRUE(LaunchProcess(argv, options).IsValid());
  write_pipe_fd.reset();

  char buf[4096] = {};
  int n = read_pipe.ReadAtCurrentPos(buf, sizeof(buf));
  PCHECK(n >= 0);
  EXPECT_LT(n, 4096);
  std::string output(buf, n);
  DVLOG(4) << output;
  auto va_info = ParseVainfo(output);

  for (const auto& profile : VaapiWrapper::GetSupportedDecodeProfiles()) {
    auto va_profile = ConvertToVAProfile(profile.profile);
    ASSERT_TRUE(va_profile.has_value());

    bool is_profile_supported =
        base::Contains(va_info[*va_profile], VAEntrypointVLD);
    if (profile.profile == H264PROFILE_BASELINE) {
      // ConstrainedBaseline is the fallback profile for H264PROFILE_BASELINE.
      // This is the same logic as in vaapi_wrapper.cc.
      is_profile_supported |= base::Contains(
          va_info[VAProfileH264ConstrainedBaseline], VAEntrypointVLD);
    }

    EXPECT_TRUE(is_profile_supported) << " profile: " << profile.profile;
  }

  for (const auto& profile : VaapiWrapper::GetSupportedEncodeProfiles()) {
    auto va_profile = ConvertToVAProfile(profile.profile);
    ASSERT_TRUE(va_profile.has_value());
    bool is_profile_supported =
        base::Contains(va_info[*va_profile], VAEntrypointEncSlice) ||
        base::Contains(va_info[*va_profile], VAEntrypointEncSliceLP);
    if (profile.profile == H264PROFILE_BASELINE) {
      // ConstrainedBaseline is the fallback profile for H264PROFILE_BASELINE.
      // This is the same logic as in vaapi_wrapper.cc.
      is_profile_supported |=
          base::Contains(va_info[VAProfileH264ConstrainedBaseline],
                         VAEntrypointEncSlice) ||
          base::Contains(va_info[VAProfileH264ConstrainedBaseline],
                         VAEntrypointEncSliceLP);
    }

    EXPECT_TRUE(is_profile_supported) << " profile: " << profile.profile;
  }

  EXPECT_EQ(VaapiWrapper::IsDecodeSupported(VAProfileJPEGBaseline),
            base::Contains(va_info[VAProfileJPEGBaseline], VAEntrypointVLD));
  EXPECT_EQ(
      VaapiWrapper::IsJpegEncodeSupported(),
      base::Contains(va_info[VAProfileJPEGBaseline], VAEntrypointEncPicture));
}

TEST_F(VaapiTest, DefaultEntrypointIsSupported) {
  for (size_t i = 0; i < VaapiWrapper::kCodecModeMax; ++i) {
    const VaapiWrapper::CodecMode mode =
        static_cast<VaapiWrapper::CodecMode>(i);
    std::map<VAProfile, std::vector<VAEntrypoint>> configurations =
        VaapiWrapper::GetSupportedConfigurationsForCodecModeForTesting(mode);
    for (const auto& profile_and_entrypoints : configurations) {
      const VAEntrypoint default_entrypoint =
          VaapiWrapper::GetDefaultVaEntryPoint(mode,
                                               profile_and_entrypoints.first);
      const auto& supported_entrypoints = profile_and_entrypoints.second;
      EXPECT_TRUE(base::Contains(supported_entrypoints, default_entrypoint))
          << "Default VAEntrypoint " << default_entrypoint
          << " (mode = " << mode << ") is not supported for VAProfile = "
          << profile_and_entrypoints.first;
    }
  }
}
}  // namespace media

int main(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);

  media::VaapiWrapper::PreSandboxInitialization();

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
