// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lite_video/lite_video_switches.h"

#include "base/command_line.h"

namespace lite_video {
namespace switches {

// Overrides the network conditions checks for LiteVideos.
const char kLiteVideoIgnoreNetworkConditions[] =
    "lite-video-ignore-network-conditions";

// Overrides all the LiteVideo decision logic to allow it on every navigation.
// This causes LiteVideos to ignore the hints, user blocklist, and
// network condition.
const char kLiteVideoForceOverrideDecision[] =
    "lite-video-force-override-decision";

bool ShouldIgnoreLiteVideoNetworkConditions() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kLiteVideoIgnoreNetworkConditions);
}

bool ShouldOverrideLiteVideoDecision() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kLiteVideoForceOverrideDecision);
}

}  // namespace switches
}  // namespace lite_video
