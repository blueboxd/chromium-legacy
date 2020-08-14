// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIDEO_TUTORIALS_TUTORIAL_FACTORY_HELPER_H_
#define CHROME_BROWSER_VIDEO_TUTORIALS_TUTORIAL_FACTORY_HELPER_H_

#include <memory>

namespace video_tutorials {

class VideoTutorialService;

std::unique_ptr<VideoTutorialService> CreateVideoTutorialService();

}  // namespace video_tutorials

#endif  // CHROME_BROWSER_VIDEO_TUTORIALS_TUTORIAL_FACTORY_HELPER_H_
