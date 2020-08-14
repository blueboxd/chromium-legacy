// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIDEO_TUTORIALS_VIDEO_TUTORIAL_SERVICE_H_
#define CHROME_BROWSER_VIDEO_TUTORIALS_VIDEO_TUTORIAL_SERVICE_H_

#include "base/supports_user_data.h"
#include "components/keyed_service/core/keyed_service.h"

namespace video_tutorials {

// The central class on chrome client responsible for managing, storing, and
// serving video tutorials in chrome.
class VideoTutorialService : public KeyedService,
                             public base::SupportsUserData {
 public:
  VideoTutorialService() = default;
  ~VideoTutorialService() override = default;

  VideoTutorialService(const VideoTutorialService&) = delete;
  VideoTutorialService& operator=(const VideoTutorialService&) = delete;
};

}  // namespace video_tutorials

#endif  // CHROME_BROWSER_VIDEO_TUTORIALS_VIDEO_TUTORIAL_SERVICE_H_
