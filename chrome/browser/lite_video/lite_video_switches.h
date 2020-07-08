// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_SWITCHES_H_
#define CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_SWITCHES_H_

#include <string>

namespace lite_video {
namespace switches {

extern const char kLiteVideoIgnoreNetworkConditions[];
extern const char kLiteVideoForceOverrideDecision[];

// Returns true if checking the network condition should be ignored.
bool ShouldIgnoreLiteVideoNetworkConditions();

// Returns true if the decision logic for whether to allow LiteVideos should be
// overridden and allow LiteVideos to be enabled for every navigation.
bool ShouldOverrideLiteVideoDecision();

}  // namespace switches
}  // namespace lite_video

#endif  // CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_SWITCHES_H_
