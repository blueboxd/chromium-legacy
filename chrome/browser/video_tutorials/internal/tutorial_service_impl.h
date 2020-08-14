// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_TUTORIAL_SERVICE_IMPL_H_
#define CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_TUTORIAL_SERVICE_IMPL_H_

#include "chrome/browser/video_tutorials/video_tutorial_service.h"

namespace video_tutorials {

class TutorialServiceImpl : public VideoTutorialService {
 public:
  TutorialServiceImpl() = default;
  ~TutorialServiceImpl() override = default;
};

}  // namespace video_tutorials

#endif  // CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_TUTORIAL_SERVICE_IMPL_H_
