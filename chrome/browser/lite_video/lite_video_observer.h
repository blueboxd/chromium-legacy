// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_OBSERVER_H_
#define CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_OBSERVER_H_

#include "base/macros.h"
#include "base/timer/timer.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace lite_video {
class LiteVideoDecider;
}  // namespace lite_video

class LiteVideoObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<LiteVideoObserver> {
 public:
  static void MaybeCreateForWebContents(content::WebContents* web_contents);

  ~LiteVideoObserver() override;

 private:
  friend class content::WebContentsUserData<LiteVideoObserver>;
  explicit LiteVideoObserver(content::WebContents* web_contents);

  // content::WebContentsObserver.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // The decider capable of making decisions about whether LiteVideos should be
  // applied and the params to use when throttling media requests.
  lite_video::LiteVideoDecider* lite_video_decider_ = nullptr;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_OBSERVER_H_
