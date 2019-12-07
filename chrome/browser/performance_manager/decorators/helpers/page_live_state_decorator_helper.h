// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_HELPERS_PAGE_LIVE_STATE_DECORATOR_HELPER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_HELPERS_PAGE_LIVE_STATE_DECORATOR_HELPER_H_

#include "base/sequence_checker.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"

namespace performance_manager {

class PageLiveStateDecoratorHelper
    : public MediaStreamCaptureIndicator::Observer {
 public:
  PageLiveStateDecoratorHelper();
  ~PageLiveStateDecoratorHelper() override;
  PageLiveStateDecoratorHelper(const PageLiveStateDecoratorHelper& other) =
      delete;
  PageLiveStateDecoratorHelper& operator=(const PageLiveStateDecoratorHelper&) =
      delete;

  // MediaStreamCaptureIndicator::Observer implementation:
  void OnIsCapturingVideoChanged(content::WebContents* contents,
                                 bool is_capturing_video) override;
  void OnIsCapturingAudioChanged(content::WebContents* contents,
                                 bool is_capturing_audio) override;
  void OnIsBeingMirroredChanged(content::WebContents* contents,
                                bool is_being_mirrored) override;
  void OnIsCapturingDesktopChanged(content::WebContents* contents,
                                   bool is_capturing_desktop) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_HELPERS_PAGE_LIVE_STATE_DECORATOR_HELPER_H_
