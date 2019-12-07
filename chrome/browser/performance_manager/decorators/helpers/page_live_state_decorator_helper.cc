// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/decorators/helpers/page_live_state_decorator_helper.h"

#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"

namespace performance_manager {

PageLiveStateDecoratorHelper::PageLiveStateDecoratorHelper() {
  MediaCaptureDevicesDispatcher::GetInstance()
      ->GetMediaStreamCaptureIndicator()
      ->AddObserver(this);
}

PageLiveStateDecoratorHelper::~PageLiveStateDecoratorHelper() {
  MediaCaptureDevicesDispatcher::GetInstance()
      ->GetMediaStreamCaptureIndicator()
      ->RemoveObserver(this);
}

void PageLiveStateDecoratorHelper::OnIsCapturingVideoChanged(
    content::WebContents* contents,
    bool is_capturing_video) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PageLiveStateDecorator::OnIsCapturingVideoChanged(contents,
                                                    is_capturing_video);
}

void PageLiveStateDecoratorHelper::OnIsCapturingAudioChanged(
    content::WebContents* contents,
    bool is_capturing_audio) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PageLiveStateDecorator::OnIsCapturingAudioChanged(contents,
                                                    is_capturing_audio);
}

void PageLiveStateDecoratorHelper::OnIsBeingMirroredChanged(
    content::WebContents* contents,
    bool is_being_mirrored) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PageLiveStateDecorator::OnIsBeingMirroredChanged(contents, is_being_mirrored);
}

void PageLiveStateDecoratorHelper::OnIsCapturingDesktopChanged(
    content::WebContents* contents,
    bool is_capturing_desktop) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PageLiveStateDecorator::OnIsCapturingDesktopChanged(contents,
                                                      is_capturing_desktop);
}

}  // namespace performance_manager
